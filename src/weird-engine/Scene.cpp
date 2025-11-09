#include "weird-engine/Scene.h"
#include "weird-engine/Input.h"
#include "weird-engine/SceneManager.h"
#include "weird-engine/math/Default2DSDFs.h"

#include <random>
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

namespace WeirdEngine
{
	static float g_frictionSound = 0.0f;
	static float g_frictionSoundRead = 0.0f;

	void Scene::handlePhysicsStep(void *userData)
	{
		Scene *self = static_cast<Scene *>(userData);
		self->onPhysicsStep();
		g_frictionSoundRead = g_frictionSound;
		g_frictionSound = 0.0f;
	}

	void Scene::handleCollision(CollisionEvent &event, void *userData)
	{
		// Unsafe cast! Prone to error.
		Scene *self = static_cast<Scene *>(userData);
		self->onCollision(event);
	}

	void Scene::handleShapeCollision(ShapeCollisionEvent &event, void *userData)
	{
		// Unsafe cast! Prone to error.
		Scene *self = static_cast<Scene *>(userData);
		self->onShapeCollision(event);

		const float m_soundFalloff = 0.001f;
		auto camPosition = self->getCamera().position; // Mutex?
		float frictionSample = event.friction / (1.0f + (m_soundFalloff * glm::distance2(vec2(camPosition.x, camPosition.y), event.position))); // Apply distance falloff

		g_frictionSound = std::max(frictionSample, g_frictionSound);

		if (event.state == CollisionState::START)
			self->m_collisionSoundQueued = true;
	}


	Scene::Scene()
			: m_simulation2D(MAX_ENTITIES), m_sdfRenderSystem(m_ecs), m_sdfRenderSystem2D(m_ecs), m_renderSystem(m_ecs), m_instancedRenderSystem(m_ecs), m_rbPhysicsSystem2D(m_ecs), m_physicsInteractionSystem(m_ecs), m_playerMovementSystem(m_ecs), m_cameraSystem(m_ecs), m_runSimulationInThread(true)
	{
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
		onStart();

		if (m_debugFly)
		{
			switch (m_renderMode)
			{
			case WeirdEngine::Scene::RenderMode::Simple3D:
			case WeirdEngine::Scene::RenderMode::RayMarching3D:
			case WeirdEngine::Scene::RenderMode::RayMarchingBoth:
			{
				FlyMovement &fly = m_ecs.addComponent<FlyMovement>(m_mainCamera);
				break;
			}
			case WeirdEngine::Scene::RenderMode::RayMarching2D:
			{
				FlyMovement2D &fly = m_ecs.addComponent<FlyMovement2D>(m_mainCamera);
				fly.targetPosition = m_ecs.getComponent<Transform>(m_mainCamera).position;
				break;
			}
			default:
				break;
			}
		}
	}

	//  TODO: pass render target instead of shader. Shaders should be accessed in a different way, through the resource manager
	void Scene::renderModels(WeirdRenderer::RenderTarget &renderTarget, WeirdRenderer::Shader &shader, WeirdRenderer::Shader &instancingShader)
	{
		WeirdRenderer::Camera &camera = m_ecs.getComponent<ECS::Camera>(m_mainCamera).camera;
		m_renderSystem.render(m_ecs, m_resourceManager, shader, camera, m_lights);

		onRender(renderTarget);

		// m_instancedRenderSystem.render(m_ecs, m_resourceManager, instancingShader, camera, m_lights);
	}

	void replaceSubstring(std::string &str, const std::string &from, const std::string &to)
	{
		size_t start_pos = str.find(from);
		if (start_pos != std::string::npos)
		{
			str.replace(start_pos, from.length(), to);
		}
	}

	void Scene::updateRayMarchingShader(WeirdRenderer::Shader &shader)
	{
		// m_sdfRenderSystem2D.updatePalette(shader);

		updateCustomShapesShader(shader);
	}

	void Scene::get2DShapesData(WeirdRenderer::Dot2D *&data, uint32_t &size)
	{
		m_sdfRenderSystem2D.fillDataBuffer(data, size);
	}

	void Scene::update(double delta, double time)
	{
		if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown((Input::R)))
		{
			m_sdfRenderSystem2D.shaderNeedsUpdate() = true;
		}

		// Update systems
		if (m_debugFly)
		{
			m_playerMovementSystem.update(m_ecs, delta);
		}
		// m_cameraSystem.follow(m_ecs, m_mainCamera, 10);

		m_cameraSystem.update(m_ecs);

		m_rbPhysicsSystem2D.update(m_ecs, m_simulation2D);
		if (m_debugInput)
		{
			m_physicsInteractionSystem.update(m_ecs, m_simulation2D);
		}

		m_simulation2D.update(delta);

		onUpdate(delta);

		m_ecs.freeRemovedComponents();
	}

	void Scene::updateCustomShapesShader(WeirdRenderer::Shader &shader)
	{
		const auto sdfBalls = m_ecs.getComponentManager<SDFRenderer>()->getComponentArray();
		int32_t ballsCount = sdfBalls->getSize();
		const auto componentArray = m_ecs.getComponentManager<CustomShape>()->getComponentArray();
		shader.setUniform("u_customShapeCount", componentArray->getSize());

		if (!m_sdfRenderSystem2D.shaderNeedsUpdate())
		{
			return;
		}

		m_sdfRenderSystem2D.shaderNeedsUpdate() = false;

		std::ostringstream oss;

		oss << "///////////////////////////////////////////\n";

		oss << "int dataOffset =  u_loadedObjects - (2 * u_customShapeCount);\n";

		int currentGroup = -1;
		std::string groupDistanceVariable;

		for (size_t i = 0; i < componentArray->getSize(); i++)
		{
			// Get shape
			const auto &shape = componentArray->getDataAtIdx(i);

			// Get group
			const int group = shape.m_groupId;

			// Start new group if necessary
			if(group != currentGroup)
			{
				// If this is not the first group, combine current group distance with global minDistance
				if(currentGroup != -1)
				{
					oss << "if(minDist >"<< groupDistanceVariable <<"){ minDist = "<< groupDistanceVariable <<";}\n";
				}

				// Next group
				currentGroup = group;
				groupDistanceVariable = "d" + std::to_string(currentGroup);

				// Initialize distance with big value
				oss << "float " << groupDistanceVariable << "= 10000;\n";
			}

			// Start shape
			oss << "{\n";

			// Calculate data position in array
			oss << "int idx = dataOffset + " << 2 * i << ";\n";

			// Fetch parameters
			oss << "vec4 parameters0 = texelFetch(t_shapeBuffer, idx);\n";
			oss << "vec4 parameters1 = texelFetch(t_shapeBuffer, idx + 1);\n";

			// Get distance function
			auto fragmentCode = m_sdfs[shape.m_distanceFieldId]->print();

			// Use screen space coords (DEPRECATED)
			if (shape.m_screenSpace)
			{
				replaceSubstring(fragmentCode, "var9", "var11");
				replaceSubstring(fragmentCode, "var10", "var12");
			}

			bool globalEffect = group == CustomShape::GLOBAL_GROUP;

			// Shape distance calculation
			oss << "float dist = " << fragmentCode << ";" << std::endl;

			// Apply globalEffect logic
			oss << "float currentMinDistance = " << (globalEffect ? "minDist" : groupDistanceVariable) << ";" << std::endl;

			// Combine shape distance
			switch (shape.m_combination)
			{
				case CombinationType::Addition:
				{
					oss << "currentMinDistance = min(currentMinDistance, dist);\n";
					oss << "finalMaterialId = dist <= min(minDist, currentMinDistance) ? "  << shape.m_material << ": finalMaterialId;" << std::endl;
					break;
				}
				case CombinationType::Subtraction:
				{
					oss << "currentMinDistance = max(currentMinDistance, -dist);\n";
					break;
				}
				case CombinationType::Intersection:
				{
					oss << "currentMinDistance = max(currentMinDistance, dist);\n";
					break;
				}
				case CombinationType::SmoothAddition:
				{
					oss << "currentMinDistance = fOpUnionSoft(currentMinDistance, dist, 1.0);\n";
					oss << "finalMaterialId = dist <= min(minDist, currentMinDistance) ? "  << shape.m_material << ": finalMaterialId;" << std::endl;
					break;
				}
				case CombinationType::SmoothSubtraction:
				{
					// Smoothly subtract "dist" from currentMinDistance
					oss << "currentMinDistance = fOpSubSoft(currentMinDistance, dist, 1.0);\n";
					// Material belongs to the *a* shape if it still “wins” after subtraction
					// oss << "finalMaterialId = dist <= min(minDist, currentMinDistance) ? "
					//	<< shape.m_material << " : finalMaterialId;" << std::endl;
					break;
				}
				default:
					break;
			}

			// Assign back to the correct target
			oss << (globalEffect ? "minDist" : groupDistanceVariable) << " = currentMinDistance;\n";
			oss << "}\n\n";
		}

		// Combine last group
		if (componentArray->getSize() > 0)
		{
			oss << "if(minDist >" << groupDistanceVariable << "){ minDist = " << groupDistanceVariable << ";}\n";
		}

		// Get string
		std::string replacement = oss.str();

		// Set new source code and recompile shader
		shader.setFragmentIncludeCode(1, replacement);

#ifndef NDEBUG
		if (Input::GetKey(Input::LeftCtrl) && Input::GetKey(Input::LeftShift) && Input::GetKey(Input::R))
		{
			std::cout << replacement << std::endl;

			// Broken
			std::ofstream outFile("generated_shader.frag");
			if (outFile.is_open()) {
				outFile << shader.getFragmentCode();
				outFile.close();
			}
		}
#endif
	}

	void Scene::onPhysicsStep()
	{
		m_collisionSoundQueued = false || m_collisionSoundQueued; // TODO
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
		return g_frictionSoundRead;
	}

	const std::vector<WeirdRenderer::DrawCommand> & Scene::getDrawQueue() const
	{
		return m_drawQueue;
	}

	Entity Scene::addShape(ShapeId shapeId, float* variables, uint16_t material, CombinationType combination, bool hasCollision, int group)
	{
		Entity entity = m_ecs.createEntity();
		CustomShape &shape = m_ecs.addComponent<CustomShape>(entity);
		shape.m_distanceFieldId = shapeId;
		shape.m_combination = combination;
		shape.m_hasCollision = hasCollision;
		shape.m_groupId = group;
		shape.m_material = material;
		std::copy(variables, variables + 8, shape.m_parameters);

		// CustomShape shape(shapeId, variables); // check old constructor for references

		m_sdfRenderSystem2D.shaderNeedsUpdate() = true;

		return entity;
	}

	void Scene::lookAt(Entity entity)
	{
		FlyMovement2D &fly = m_ecs.getComponent<FlyMovement2D>(m_mainCamera);
		Transform &target = m_ecs.getComponent<Transform>(entity);
		float oldZ = fly.targetPosition.z;

		fly.targetPosition = target.position;
		fly.targetPosition.z = oldZ;
	};

	void Scene::loadScene(std::string &sceneFileContent)
	{
		// json scene = json::parse(sceneFileContent);

		// load font
		loadFont(ENGINE_PATH "/src/weird-renderer/fonts/default.bmp", 7, 7, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,;:?!-_'#\"\\/<>() ");

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
		m_sdfs = getSDFS();
		m_simulation2D.setSDFs(m_sdfs);
	}

	constexpr int INVALID_INDEX = -1;

	void Scene::print(const std::string &text)
	{
		float offset = 0;
		for (auto i : text)
		{
			int idx = getIndex(i);

			// std::cout << idx << std::endl;

			for (auto vec2 : m_letters[idx])
			{
				float x = 2 + vec2.x + offset;
				float y = vec2.y;

				Entity entity = m_ecs.createEntity();
				Transform &t = m_ecs.addComponent<Transform>(entity);
				t.position = vec3(x, y, -10.0f);

				SDFRenderer &sdfRenderer = m_ecs.addComponent<SDFRenderer>(entity);
				sdfRenderer.materialId = 4 + idx % 12;
			}

			offset += m_charWidth;
		}
	}

	void Scene::loadFont(const char *imagePath, int charWidth, int charHeight, const char *characters)
	{
		// Set all to INVALID_INDEX initially
		for (int &val : m_CharLookUpTable)
		{
			val = INVALID_INDEX;
		}

		// Fill look up table
		for (size_t i = 0; characters[i] != '\0'; ++i)
		{
			m_CharLookUpTable[characters[i]] = i;
		}

		// Store char dimensions
		m_charWidth = charWidth;
		m_charHeight = charHeight;

		// Load the image
		int width, height, channels;
		unsigned char *img = wstbi_load(imagePath, &width, &height, &channels, 0);

		if (img == nullptr)
		{
			std::cerr << "Error: could not load image." << std::endl;
			return;
		}

		int columns = width / charWidth;
		int rows = height / charHeight;

		int charCount = columns * rows;

		m_letters.clear();
		m_letters.resize(charCount);

		for (size_t i = 0; i < charCount; i++)
		{
			int startX = charWidth * (i % columns);
			int startY = (charHeight * (i / columns));

			for (size_t offsetX = 0; offsetX < charWidth; offsetX++)
			{
				for (size_t offsetY = 0; offsetY < charHeight; offsetY++)
				{

					int x = startX + offsetX;
					int y = startY + offsetY;

					// Calculate the index of the pixel in the image data
					int index = (y * width + x) * channels;

					if (index < 0 || index >= width * height * channels)
					{
						continue;
					}

					// Get the color values
					unsigned char r = img[index];
					unsigned char g = img[index + 1];
					unsigned char b = img[index + 2];
					unsigned char a = (channels == 4) ? img[index + 3] : 255; // Alpha channel (if present)

					if (r < 50)
					{
						float localX = offsetX;
						float localY = charHeight - offsetY;

						m_letters[i].emplace_back(localX, localY);
					}
				}
			}
		}

		// Free the image memory
		wstbi_image_free(img);
	}
}