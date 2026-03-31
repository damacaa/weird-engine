#include "weird-engine/Scene.h"
#include "weird-engine/Input.h"
#include "weird-engine/Profiler.h"
#include "weird-engine/SceneSerializer.h"

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
		, m_buttonSystem(m_ecs)
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

		// If a .weird file path was provided (via setSceneFilePath / registerScene),
		// restore saved scene state before the derived class's onStart() runs.
		TagMap loadedTags;
		if (!m_sceneFilePath.empty())
		{
			SceneSerializer::load(*this, m_sceneFilePath);
			loadedTags = m_tagToEntity;
		}

		onStart(loadedTags);

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

		m_buttonSystem.update(m_ecs, m_sdfs, getTime());

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

	void Scene::tag(Entity entity, const std::string& name)
	{
		if (name.empty())
		{
			removeTag(entity);
			return;
		}

		// If the tag is already owned by another entity, remove it from that entity
		auto existingOwner = m_tagToEntity.find(name);
		if (existingOwner != m_tagToEntity.end() && existingOwner->second != entity)
		{
			m_entityToTag.erase(existingOwner->second);
		}

		// Remove any previous tag this entity had
		auto existingTag = m_entityToTag.find(entity);
		if (existingTag != m_entityToTag.end() && existingTag->second != name)
		{
			m_tagToEntity.erase(existingTag->second);
		}

		m_tagToEntity[name] = entity;
		m_entityToTag[entity] = name;
	}

	void Scene::removeTag(Entity entity)
	{
		auto it = m_entityToTag.find(entity);
		if (it == m_entityToTag.end())
			return;
		m_tagToEntity.erase(it->second);
		m_entityToTag.erase(it);
	}

	std::string Scene::getEntityTag(Entity entity) const
	{
		auto it = m_entityToTag.find(entity);
		if (it == m_entityToTag.end())
			return "";
		return it->second;
	}

	Entity Scene::getEntityByTag(const std::string& name) const
	{
		auto it = m_tagToEntity.find(name);
		if (it == m_tagToEntity.end())
			return MAX_ENTITIES;
		return it->second;
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
		SceneSerializer::save(*this, filename);
	}

	Scene::TagMap Scene::loadWeirdFile(const std::string& path, bool blacklistEntities)
	{
		TagMap loadedTags;
		Entity firstNewEntity = m_ecs.getEntityCount();
		SceneSerializer::load(*this, path, &loadedTags);
		if (blacklistEntities)
		{
			Entity lastNewEntity = m_ecs.getEntityCount();
			for (Entity entity = firstNewEntity; entity < lastNewEntity; ++entity)
				m_serializationBlacklist.insert(entity);
		}
		return loadedTags;
	}

	void Scene::loadFromWeirdFile(const std::string& path)
	{
		SceneSerializer::load(*this, path);
	}

}