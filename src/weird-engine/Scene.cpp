#include "weird-engine/Scene.h"
#include "weird-engine/Input.h"
#include "weird-engine/Profiler.h"

#include <fstream>
#include <iostream>
#include <unordered_map>
#include <json/json.h>

namespace WeirdEngine
{
	void Scene::handlePhysicsStep(void *userData)
	{
		Scene *self = static_cast<Scene *>(userData);
		self->onPhysicsStep();

		self->m_frictionSoundLevelRead.store(self->m_frictionSoundLevel, std::memory_order_release);
		self->m_frictionSoundLevel = 0.0f;
	}

	void Scene::handleCollision(CollisionEvent &event, void *userData)
	{
		// Unsafe cast! Prone to error.
		Scene *self = static_cast<Scene *>(userData);
		self->onCollision(event);
	}

	void Scene::handleShapeCollision(ShapeCollisionEvent &event, void *userData)
	{
		Scene *self = static_cast<Scene*>(userData);
		self->onShapeCollision(event);

		const float m_soundFalloff = 0.1f;
		bool spatialAudio = false;
		auto camPosition = self->getCamera().position; // Mutex?
		float speed = glm::length2(event.velocity);
		float distanceMultiplier = 1.0f / (1.0f + (m_soundFalloff * glm::distance2(camPosition, vec3(event.position, 0.0f))));
		float frictionSample =event.friction * 0.01f * speed * (spatialAudio ? distanceMultiplier : 1.0f);

		self->m_frictionSoundLevel = std::max(frictionSample, self->m_frictionSoundLevel);

		if (event.state == CollisionState::START)
		{
			// float speedFactor = std::sqrt((std::min)(0.001f * speed, 1.0f));
			float penetrationFactor = std::sqrt((std::min)(2.0f * event.penetration, 1.0f));
			float volume = penetrationFactor;
			
			float freqFactor = std::abs(glm::dot(event.normal, (event.velocity)));
			freqFactor *= 0.01f;
			// freqFactor = freqFactor * freqFactor;
			float frequency = 200.0f + (freqFactor * 300.0f);

			self->m_audioQueue.push(WeirdRenderer::SimpleAudioRequest{volume, frequency, false, vec3(event.position, 0.0f) });
		}
	}

	Scene::Scene(const PhysicsSettings& settings)
		: m_simulation2D(MAX_ENTITIES, settings)
		, m_sdfRenderSystem(m_ecs)
		, m_sdfRenderSystem2D(m_ecs)
		, m_UIRenderSystem(m_ecs)
		, m_renderSystem(m_ecs)
		, m_instancedRenderSystem(m_ecs)
		, m_rbPhysicsSystem2D(m_ecs)
		, m_physicsInteractionSystem(m_ecs)
		, m_playerMovementSystem(m_ecs)
		, m_cameraSystem(m_ecs)
		, m_runSimulationInThread(true)
	{
		m_sdfRenderSystem2D.m_dotRadious = 0.5f;
		m_sdfRenderSystem2D.m_charSpacing = 1.0f;

		// Custom component managers
		std::shared_ptr<RigidBodyManager> rbManager = std::make_shared<RigidBodyManager>(m_simulation2D);
		m_ecs.registerComponent<RigidBody2D>(rbManager);

		// Read content from file
		std::string content = "";

		// Read scene file and load everything
		loadScene(content);

		// Initialize simulation
		m_rbPhysicsSystem2D.init(m_ecs, m_simulation2D);

		// Start simulation if different thread
		if (m_runSimulationInThread)
		{
			m_simulation2D.startSimulationThread();
		}

		m_simulation2D.setStepCallback(&handlePhysicsStep, this);
		m_simulation2D.setCollisionCallback(&handleCollision, this);
		m_simulation2D.setShapeCollisionCallback(&handleShapeCollision, this);
	}

	Scene::~Scene()
	{
		m_simulation2D.stopSimulationThread();

		// TODO: Free resources from all entities
		m_resourceManager.freeResources(0);
	}

	void Scene::start()
	{
		onCreate();

		if (!m_sceneFilePath.empty())
		{
			loadFromWeirdFile(m_sceneFilePath);
		}

		onStart();

		switch (m_renderMode)
		{
			case WeirdEngine::Scene::RenderMode::RayMarching3D: {
				FlyMovement &fly = m_ecs.addComponent<FlyMovement>(m_mainCamera);
				break;
			}
			case WeirdEngine::Scene::RenderMode::RayMarching2D:
			case WeirdEngine::Scene::RenderMode::RayMarchingBoth: {
				FlyMovement2D &fly = m_ecs.addComponent<FlyMovement2D>(m_mainCamera);
				fly.targetPosition = m_ecs.getComponent<Transform>(m_mainCamera).position;
				break;
			}
			default:
				break;
		}
	}

	//  TODO: pass render target instead of shader. Shaders should be accessed in a different way, through the resource manager
	void Scene::renderModels(WeirdRenderer::RenderTarget &renderTarget, WeirdRenderer::Shader &shader, WeirdRenderer::Shader &instancingShader)
	{
		PROFILE_SCOPE("Render Models");
		WeirdRenderer::Camera &camera = m_ecs.getComponent<ECS::Camera>(m_mainCamera).camera;
		m_renderSystem.render(m_ecs, m_resourceManager, shader, camera, m_lights);

		onRender(renderTarget);

		// m_instancedRenderSystem.render(m_ecs, m_resourceManager, instancingShader, camera, m_lights);
	}

	void Scene::updateRayMarchingShader(WeirdRenderer::Shader &shader)
	{
		m_sdfRenderSystem2D.updateCustomShapesShader(shader, m_sdfs);
	}
	
	void Scene::updateUIShader(WeirdRenderer::Shader &shader)
	{
		m_UIRenderSystem.updateCustomShapesShader(shader, m_sdfs);
	}

	void Scene::forceShaderRefresh()
	{
		m_sdfRenderSystem2D.shaderNeedsUpdate() = true;
		m_UIRenderSystem.shaderNeedsUpdate() = true;
	}

	void Scene::get2DShapesData(WeirdRenderer::Dot2D*& data, uint32_t& size, uint32_t& customShapeCount)
	{
		PROFILE_SCOPE("Fetch World Data");
		customShapeCount = m_ecs.getComponentArray<CustomShape>()->getSize();
		m_sdfRenderSystem2D.fillDataBuffer(data, size);
	}

	void Scene::getUIData(WeirdRenderer::Dot2D *&uiData, uint32_t &size, uint32_t& customShapeCount)
	{
		PROFILE_SCOPE("Fetch UI Data");
		customShapeCount = m_ecs.getComponentArray<UIShape>()->getSize();
		m_UIRenderSystem.fillDataBuffer(uiData, size);
	}

	void Scene::update(double delta, double time)
	{
		PROFILE_SCOPE("Scene Logic Update");

		if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown((Input::R)))
		{
			m_sdfRenderSystem2D.shaderNeedsUpdate() = true;
		}

		// Update systems
		{
			if (m_debugFly)
			{
				m_playerMovementSystem.update(m_ecs, delta);
			}
			// m_cameraSystem.follow(m_ecs, m_mainCamera, 10);

			m_cameraSystem.update(m_ecs);
		}

		// Buttons
		{
			auto componentArray = m_ecs.getComponentArray<ShapeButton>();
			unsigned int size = componentArray->getSize();
			bool mouseIsClicking = Input::GetMouseButton(Input::LeftClick);

			for (size_t i = 0; i < size; i++)
			{
				auto& buttonComponent = componentArray->getDataAtIdx(i);

				if (mouseIsClicking)
				{
					static float parameters[11];

					auto& shape = m_ecs.getComponent<UIShape>(buttonComponent.Owner);
					std::copy(std::begin(shape.parameters), std::end(shape.parameters), std::begin(parameters));

					parameters[8] = getTime();
					parameters[9] = Input::GetMouseX();
					parameters[10] = Input::GetMouseY();

					m_sdfs[shape.distanceFieldId]->propagateValues(parameters);

					float distance = m_sdfs[shape.distanceFieldId]->getValue();

					if (distance < buttonComponent.clickPadding)
					{
						switch (buttonComponent.state)
						{
							case ButtonState::Off:
							case ButtonState::Up:
								buttonComponent.state = ButtonState::Down;

								for (int i = 0; i < 8; ++i)
								{
									// test(i) returns true if the bit is 1
									if (buttonComponent.parameterModifierMask.test(i))
									{
										shape.parameters[i] += buttonComponent.modifierAmount;
									}
								}
								break;
							case ButtonState::Down:
							case ButtonState::Hold:
								buttonComponent.state = ButtonState::Hold;
								break;
						}

						Input::flagUIClick();
					}
				}
				else
				{
					switch (buttonComponent.state)
					{
						case ButtonState::Off:
							break;
						case ButtonState::Down:
						case ButtonState::Hold:
							buttonComponent.state = ButtonState::Up;
							break;
						case ButtonState::Up:
						{
							buttonComponent.state = ButtonState::Off;

							// Reset shape
							auto& shape = m_ecs.getComponent<UIShape>(buttonComponent.Owner);
							for (int i = 0; i < 8; ++i)
							{
								// test(i) returns true if the bit is 1
								if (buttonComponent.parameterModifierMask.test(i))
								{
									shape.parameters[i] -= buttonComponent.modifierAmount;
								}
							}

							break;
						}
					}
				}
			}
		}

		// Toggles
		{
			auto componentArray = m_ecs.getComponentArray<ShapeToggle>();
			unsigned int size = componentArray->getSize();
			bool mouseIsClicking = Input::GetMouseButtonDown(Input::LeftClick);

			for (size_t i = 0; i < size; i++)
			{
				auto& buttonComponent = componentArray->getDataAtIdx(i);

				if (mouseIsClicking)
				{
					static float parameters[11];

					auto& shape = m_ecs.getComponent<UIShape>(buttonComponent.Owner);
					std::copy(std::begin(shape.parameters), std::end(shape.parameters), std::begin(parameters));

					parameters[8] = getTime();
					parameters[9] = Input::GetMouseX();
					parameters[10] = Input::GetMouseY();

					m_sdfs[shape.distanceFieldId]->propagateValues(parameters);

					float distance = m_sdfs[shape.distanceFieldId]->getValue();

					if (distance < buttonComponent.clickPadding)
					{
						buttonComponent.active = !buttonComponent.active;
						Input::flagUIClick();
					}
				}

				if (buttonComponent.active)
				{
					switch (buttonComponent.state)
					{
						case ButtonState::Off:
						case ButtonState::Up:
						{
							buttonComponent.state = ButtonState::Down;

							auto& shape = m_ecs.getComponent<UIShape>(buttonComponent.Owner);
							for (int i = 0; i < 8; ++i)
							{
								// test(i) returns true if the bit is 1
								if (buttonComponent.parameterModifierMask.test(i))
								{
									shape.parameters[i] += buttonComponent.modifierAmount;
								}
							}
							break;
						}
						case ButtonState::Down:
							buttonComponent.state = ButtonState::Hold;
							break;
						case ButtonState::Hold:
							break;
					}


				}
				else
				{
					switch (buttonComponent.state)
					{
						case ButtonState::Off:
							break;
						case ButtonState::Down:
						case ButtonState::Hold:
						{
							buttonComponent.state = ButtonState::Up;


						}
							break;
						case ButtonState::Up:
						{
							buttonComponent.state = ButtonState::Off;

							// Reset shape
							auto& shape = m_ecs.getComponent<UIShape>(buttonComponent.Owner);
							for (int i = 0; i < 8; ++i)
							{
								// test(i) returns true if the bit is 1
								if (buttonComponent.parameterModifierMask.test(i))
								{
									shape.parameters[i] -= buttonComponent.modifierAmount;
								}
							}

							break;
						}
					}
				}
			}
		}

		{
			PROFILE_SCOPE("Physics synchronization");
			m_rbPhysicsSystem2D.update(m_ecs, m_simulation2D);

			if (m_debugInput)
			{
				m_physicsInteractionSystem.update(m_ecs, m_simulation2D);
			}

			m_simulation2D.update(delta);
		}

		{
			PROFILE_SCOPE("Scene logic update");
			onUpdate(delta);
		}

		m_ecs.freeRemovedComponents();
	}

	

	WeirdRenderer::Camera &Scene::getCamera()
	{
		return m_ecs.getComponent<Camera>(m_mainCamera).camera;
	}

	std::vector<WeirdRenderer::Light> &Scene::getLigths()
	{
		return m_lights;
	}

	float Scene::getTime()
	{
		return m_simulation2D.getSimulationTime();
	}

	void Scene::fillShapeDataBuffer(WeirdRenderer::Dot2D *&data, uint32_t &size)
	{
		m_sdfRenderSystem2D.fillDataBuffer(data, size);
	}

	Scene::RenderMode Scene::getRenderMode() const
	{
		return m_renderMode;
	}

	float Scene::getFrictionSound()
	{
		return m_frictionSoundLevelRead.load(std::memory_order_acquire);
	}

	const std::vector<WeirdRenderer::DrawCommand> & Scene::getDrawQueue() const
	{
		return m_drawQueue;
	}

	AudioRingBuffer<WeirdRenderer::SimpleAudioRequest, SOUND_QUEUE_SIZE>& Scene::getAudioQueue()
	{
		return m_audioQueue;
	}

	ShapeId Scene::registerSDF(std::shared_ptr<IMathExpression> sdf)
	{
		m_sdfs.push_back(sdf);
		m_simulation2D.setSDFs(m_sdfs);

		return m_sdfs.size() - 1;
	}

	Entity Scene::addShape(ShapeId shapeId, float* variables, uint16_t material, CombinationType combination, bool hasCollision, int group)
	{
		Entity entity = m_ecs.createEntity();
		CustomShape &shape = m_ecs.addComponent<CustomShape>(entity);
		shape.distanceFieldId = shapeId;
		shape.combination = combination;
		shape.hasCollisions = hasCollision;
		shape.groupIdx = group;
		shape.material = material;
		std::copy(variables, variables + 8, shape.parameters);

		m_sdfRenderSystem2D.shaderNeedsUpdate() = true;

		return entity;
	}

	Entity Scene::addUIShape(ShapeId shapeId, float* variables, uint16_t material, CombinationType combination, int group)
	{
		Entity entity = m_ecs.createEntity();
		UIShape& shape = m_ecs.addComponent<UIShape>(entity);
		shape.distanceFieldId = shapeId;
		shape.combination = combination;
		shape.hasCollisions = false;
		shape.groupIdx = group;
		shape.material = material;
		std::copy(variables, variables + 8, shape.parameters);

		m_UIRenderSystem.shaderNeedsUpdate() = true;

		return entity;
	}

	UIShape& Scene::addUIShape(ShapeId shapeId, float* variables, Entity& entity, int group)
	{
		entity = m_ecs.createEntity();
		UIShape& component = m_ecs.addComponent<UIShape>(entity);
		component.distanceFieldId = shapeId;
		component.hasCollisions = false;
		component.groupIdx = group;
		component.smoothFactor = 100.0f;
		std::copy(variables, variables + 8, component.parameters);

		m_UIRenderSystem.shaderNeedsUpdate() = true;

		return component;
	}

	void Scene::lookAt(Entity entity)
	{
		FlyMovement2D &fly = m_ecs.getComponent<FlyMovement2D>(m_mainCamera);
		Transform &target = m_ecs.getComponent<Transform>(entity);
		float oldZ = fly.targetPosition.z;

		fly.targetPosition = target.position;
		fly.targetPosition.z = oldZ;
	};

	void Scene::playSound(const WeirdRenderer::SimpleAudioRequest& audio)
	{
		m_audioQueue.push(audio);
	}

	void Scene::loadScene(std::string &sceneFileContent)
	{
		// json scene = json::parse(sceneFileContent);

		// load font
		// loadFont(ENGINE_PATH "/src/weird-renderer/fonts/default.bmp", 7, 7, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,;:?!-_'#\"\\/<>() ");

		std::string projectDir = fs::current_path().string() + "/SampleProject";

		// Create camera object
		m_mainCamera = m_ecs.createEntity();

		Transform &t = m_ecs.addComponent<Transform>(m_mainCamera);
		t.rotation = vec3(0, 0, -1.0f);

		ECS::Camera &c = m_ecs.addComponent<ECS::Camera>(m_mainCamera);

		// Add a light
		WeirdRenderer::Light light;
		light.rotation = normalize(vec3(1.f, 0.5f, 0.f));
		m_lights.push_back(light);

		// Shapes
		m_sdfs = DefaultShapes::getSDFS();
		m_simulation2D.setSDFs(m_sdfs);
	}

	void Scene::saveScene(const std::string& filename)
	{
		using json = nlohmann::json;

		json scene;
		scene["version"] = 1;

		// Save camera state
		{
			auto& camTransform = m_ecs.getComponent<Transform>(m_mainCamera);
			scene["camera"]["position"] = { camTransform.position.x, camTransform.position.y, camTransform.position.z };
			scene["camera"]["rotation"] = { camTransform.rotation.x, camTransform.rotation.y, camTransform.rotation.z };
			scene["camera"]["scale"]    = { camTransform.scale.x,    camTransform.scale.y,    camTransform.scale.z };
		}

		// Save entities (skip the main camera entity)
		json entitiesJson = json::array();

		{
			auto transformArray   = m_ecs.getComponentArray<Transform>();
			auto customShapeArray = m_ecs.getComponentArray<CustomShape>();
			auto uiShapeArray     = m_ecs.getComponentArray<UIShape>();
			auto sdfRendererArray = m_ecs.getComponentArray<SDFRenderer>();
			auto rigidBodyArray   = m_ecs.getComponentArray<RigidBody2D>();
			auto textArray        = m_ecs.getComponentArray<TextRenderer>();

			std::unordered_map<Entity, json> entityMap;

			auto collectEntity = [&](Entity e) -> json& {
				if (entityMap.find(e) == entityMap.end())
				{
					entityMap[e] = json::object();
					entityMap[e]["id"] = e;
				}
				return entityMap[e];
			};

			// Transform (skip camera)
			for (size_t i = 0; i < transformArray->getSize(); i++)
			{
				Entity e = transformArray->getEntityAtIdx(i);
				if (e == m_mainCamera) continue;
				auto& t = transformArray->getDataAtIdx(i);
				auto& ej = collectEntity(e);
				ej["transform"] = {
					{"position", { t.position.x, t.position.y, t.position.z }},
					{"rotation", { t.rotation.x, t.rotation.y, t.rotation.z }},
					{"scale",    { t.scale.x,    t.scale.y,    t.scale.z    }}
				};
			}

			// CustomShape (skip UIShape entities – serialised separately)
			for (size_t i = 0; i < customShapeArray->getSize(); i++)
			{
				Entity e = customShapeArray->getEntityAtIdx(i);
				if (e == m_mainCamera) continue;
				if (uiShapeArray->hasData(e)) continue;
				auto& s = customShapeArray->getDataAtIdx(i);
				auto& ej = collectEntity(e);
				ej["customShape"] = {
					{"distanceFieldId", s.distanceFieldId},
					{"combination",     static_cast<int>(s.combination)},
					{"parameters",      json(std::begin(s.parameters), std::end(s.parameters))},
					{"hasCollisions",   s.hasCollisions},
					{"groupIdx",        s.groupIdx},
					{"material",        s.material},
					{"smoothFactor",    s.smoothFactor}
				};
			}

			// UIShape
			for (size_t i = 0; i < uiShapeArray->getSize(); i++)
			{
				Entity e = uiShapeArray->getEntityAtIdx(i);
				if (e == m_mainCamera) continue;
				auto& s = uiShapeArray->getDataAtIdx(i);
				auto& ej = collectEntity(e);
				ej["uiShape"] = {
					{"distanceFieldId", s.distanceFieldId},
					{"combination",     static_cast<int>(s.combination)},
					{"parameters",      json(std::begin(s.parameters), std::end(s.parameters))},
					{"hasCollisions",   s.hasCollisions},
					{"groupIdx",        s.groupIdx},
					{"material",        s.material},
					{"smoothFactor",    s.smoothFactor}
				};
			}

			// SDFRenderer
			for (size_t i = 0; i < sdfRendererArray->getSize(); i++)
			{
				Entity e = sdfRendererArray->getEntityAtIdx(i);
				if (e == m_mainCamera) continue;
				auto& r = sdfRendererArray->getDataAtIdx(i);
				auto& ej = collectEntity(e);
				ej["sdfRenderer"] = {
					{"isStatic",   r.isStatic},
					{"materialId", r.materialId}
				};
			}

			// RigidBody2D – also save the simulation particle position
			for (size_t i = 0; i < rigidBodyArray->getSize(); i++)
			{
				Entity e = rigidBodyArray->getEntityAtIdx(i);
				if (e == m_mainCamera) continue;
				auto& rb = rigidBodyArray->getDataAtIdx(i);
				vec2 simPos = m_simulation2D.getPosition(rb.simulationId);
				auto& ej = collectEntity(e);
				ej["rigidBody2D"] = {
					{"simulationId",    rb.simulationId},
					{"physicsPosition", { simPos.x, simPos.y }}
				};
			}

			// TextRenderer
			for (size_t i = 0; i < textArray->getSize(); i++)
			{
				Entity e = textArray->getEntityAtIdx(i);
				if (e == m_mainCamera) continue;
				auto& tr = textArray->getDataAtIdx(i);
				auto& ej = collectEntity(e);
				ej["textRenderer"] = {
					{"text",     tr.text},
					{"material", tr.material},
					{"width",    tr.width},
					{"height",   tr.height}
				};
			}

			for (auto& [id, ej] : entityMap)
				entitiesJson.push_back(ej);
		}

		scene["entities"] = entitiesJson;

		// Save physics constraints
		{
			json distanceConstraintsJson = json::array();
			for (const auto& dc : m_simulation2D.getDistanceConstraints())
			{
				distanceConstraintsJson.push_back({
					{"A", dc.A}, {"B", dc.B}, {"distance", dc.Distance}, {"k", dc.K}
				});
			}

			json gravitationalConstraintsJson = json::array();
			for (const auto& gc : m_simulation2D.getGravitationalConstraints())
			{
				gravitationalConstraintsJson.push_back({
					{"A", gc.A}, {"B", gc.B}, {"g", gc.g}
				});
			}

			json fixedObjectsJson = json::array();
			for (SimulationID fid : m_simulation2D.getFixedObjects())
				fixedObjectsJson.push_back(fid);

			scene["physics"] = {
				{"distanceConstraints",      distanceConstraintsJson},
				{"gravitationalConstraints", gravitationalConstraintsJson},
				{"fixedObjects",             fixedObjectsJson}
			};
		}

		std::ofstream outFile(filename);
		if (!outFile.is_open())
		{
			std::cerr << "[Scene] Failed to open file for writing: " << filename << "\n";
			return;
		}
		outFile << scene.dump(2);
		std::cout << "[Scene] Scene saved to " << filename << "\n";
	}

	void Scene::loadFromWeirdFile(const std::string& path)
	{
		using json = nlohmann::json;

		std::ifstream inFile(path);
		if (!inFile.is_open())
		{
			std::cerr << "[Scene] Failed to open .weird file: " << path << "\n";
			return;
		}

		json scene;
		try
		{
			inFile >> scene;
		}
		catch (const json::parse_error& e)
		{
			std::cerr << "[Scene] JSON parse error in " << path << ": " << e.what() << "\n";
			return;
		}

		// Restore camera state
		if (scene.contains("camera"))
		{
			auto& camTransform = m_ecs.getComponent<Transform>(m_mainCamera);
			const auto& cam = scene["camera"];
			if (cam.contains("position"))
			{
				camTransform.position = vec3(cam["position"][0], cam["position"][1], cam["position"][2]);
				camTransform.isDirty = true;
			}
			if (cam.contains("rotation"))
				camTransform.rotation = vec3(cam["rotation"][0], cam["rotation"][1], cam["rotation"][2]);
			if (cam.contains("scale"))
				camTransform.scale = vec3(cam["scale"][0], cam["scale"][1], cam["scale"][2]);
		}

		// Map from saved simulationId → newly assigned simulationId
		std::unordered_map<int, SimulationID> simIdMap;

		// Restore entities
		if (scene.contains("entities") && scene["entities"].is_array())
		{
			for (const auto& ej : scene["entities"])
			{
				Entity entity = m_ecs.createEntity();

				if (ej.contains("transform"))
				{
					auto& t = m_ecs.addComponent<Transform>(entity);
					const auto& tj = ej["transform"];
					if (tj.contains("position"))
						t.position = vec3(tj["position"][0], tj["position"][1], tj["position"][2]);
					if (tj.contains("rotation"))
						t.rotation = vec3(tj["rotation"][0], tj["rotation"][1], tj["rotation"][2]);
					if (tj.contains("scale"))
						t.scale = vec3(tj["scale"][0], tj["scale"][1], tj["scale"][2]);
					t.isDirty = true;
				}

				if (ej.contains("customShape"))
				{
					auto& s = m_ecs.addComponent<CustomShape>(entity);
					const auto& sj = ej["customShape"];
					s.distanceFieldId = static_cast<uint16_t>(sj.value("distanceFieldId", 0));
					s.combination     = static_cast<CombinationType>(sj.value("combination", 0));
					s.hasCollisions   = sj.value("hasCollisions", true);
					s.groupIdx        = static_cast<uint16_t>(sj.value("groupIdx", 0));
					s.material        = static_cast<uint16_t>(sj.value("material", 0));
					s.smoothFactor    = sj.value("smoothFactor", 1.0f);
					if (sj.contains("parameters"))
					{
						for (int pi = 0; pi < (int)std::size(s.parameters) && pi < (int)sj["parameters"].size(); pi++)
							s.parameters[pi] = sj["parameters"][pi].get<float>();
					}
					s.isDirty = true;
					m_sdfRenderSystem2D.shaderNeedsUpdate() = true;
				}

				if (ej.contains("uiShape"))
				{
					auto& s = m_ecs.addComponent<UIShape>(entity);
					const auto& sj = ej["uiShape"];
					s.distanceFieldId = static_cast<uint16_t>(sj.value("distanceFieldId", 0));
					s.combination     = static_cast<CombinationType>(sj.value("combination", 0));
					s.hasCollisions   = sj.value("hasCollisions", false);
					s.groupIdx        = static_cast<uint16_t>(sj.value("groupIdx", 0));
					s.material        = static_cast<uint16_t>(sj.value("material", 0));
					s.smoothFactor    = sj.value("smoothFactor", 10.0f);
					if (sj.contains("parameters"))
					{
						for (int pi = 0; pi < (int)std::size(s.parameters) && pi < (int)sj["parameters"].size(); pi++)
							s.parameters[pi] = sj["parameters"][pi].get<float>();
					}
					s.isDirty = true;
					m_UIRenderSystem.shaderNeedsUpdate() = true;
				}

				if (ej.contains("sdfRenderer"))
				{
					auto& r = m_ecs.addComponent<SDFRenderer>(entity);
					const auto& rj = ej["sdfRenderer"];
					r.isStatic   = rj.value("isStatic", false);
					r.materialId = static_cast<unsigned int>(rj.value("materialId", 0));
				}

				if (ej.contains("textRenderer"))
				{
					auto& tr = m_ecs.addComponent<TextRenderer>(entity);
					const auto& trj = ej["textRenderer"];
					tr.text     = trj.value("text", std::string{});
					tr.material = static_cast<uint16_t>(trj.value("material", 0));
					tr.width    = trj.value("width", 0.0f);
					tr.height   = trj.value("height", 0.0f);
					tr.dirty    = true;
				}

				if (ej.contains("rigidBody2D"))
				{
					auto& rb = m_ecs.addComponent<RigidBody2D>(entity);
					const auto& rbj = ej["rigidBody2D"];
					int savedSimId = rbj.value("simulationId", -1);

					if (rbj.contains("physicsPosition"))
					{
						vec2 savedPos(rbj["physicsPosition"][0].get<float>(),
						              rbj["physicsPosition"][1].get<float>());
						m_simulation2D.setPosition(rb.simulationId, savedPos);
					}

					if (savedSimId >= 0)
						simIdMap[savedSimId] = rb.simulationId;
				}
			}
		}

		// Restore physics constraints using the simulationId mapping
		if (scene.contains("physics"))
		{
			const auto& phys = scene["physics"];

			if (phys.contains("distanceConstraints"))
			{
				for (const auto& dcj : phys["distanceConstraints"])
				{
					int savedA = dcj.value("A", -1);
					int savedB = dcj.value("B", -1);
					float dist = dcj.value("distance", 1.0f);
					float k    = dcj.value("k", 1.0f);

					auto itA = simIdMap.find(savedA);
					auto itB = simIdMap.find(savedB);
					if (itA != simIdMap.end() && itB != simIdMap.end())
					{
						m_simulation2D.addRawDistanceConstraint(
							static_cast<int>(itA->second),
							static_cast<int>(itB->second),
							dist, k);
					}
				}
			}

			if (phys.contains("gravitationalConstraints"))
			{
				for (const auto& gcj : phys["gravitationalConstraints"])
				{
					int savedA = gcj.value("A", -1);
					int savedB = gcj.value("B", -1);
					float g    = gcj.value("g", 1.0f);

					auto itA = simIdMap.find(savedA);
					auto itB = simIdMap.find(savedB);
					if (itA != simIdMap.end() && itB != simIdMap.end())
					{
						m_simulation2D.addGravitationalConstraint(
							itA->second, itB->second, g);
					}
				}
			}

			if (phys.contains("fixedObjects"))
			{
				for (const auto& fixedJ : phys["fixedObjects"])
				{
					int savedId = fixedJ.get<int>();
					auto it = simIdMap.find(savedId);
					if (it != simIdMap.end())
						m_simulation2D.fix(it->second);
				}
			}
		}

		std::cout << "[Scene] Scene loaded from " << path << "\n";
	}

}