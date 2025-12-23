#include "weird-engine/Scene.h"
#include "weird-engine/Input.h"
#include "weird-engine/SceneManager.h"

#include <random>


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
		float frictionSample = event.friction * 0.1f * glm::length2(event.velocity) / (1.0f + (m_soundFalloff * glm::distance2(vec2(camPosition.x, camPosition.y), event.position))); // Apply distance falloff

		g_frictionSound = std::max(frictionSample, g_frictionSound);

		if (event.state == CollisionState::START)
			self->m_collisionSoundQueued = true;
	}


	Scene::Scene()
			: m_simulation2D(MAX_ENTITIES), m_sdfRenderSystem(m_ecs), m_sdfRenderSystem2D(m_ecs), m_UIRenderSystem(m_ecs), m_renderSystem(m_ecs), m_instancedRenderSystem(m_ecs), m_rbPhysicsSystem2D(m_ecs), m_physicsInteractionSystem(m_ecs), m_playerMovementSystem(m_ecs), m_cameraSystem(m_ecs), m_runSimulationInThread(true)
	{
		// Custom component managers
		std::shared_ptr<RigidBodyManager> rbManager = std::make_shared<RigidBodyManager>(m_simulation2D);
		m_ecs.registerComponent<RigidBody2D>(rbManager);

		m_UIRenderSystem.m_shapeBlending = 10.0f;

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

	void Scene::updateRayMarchingShader(WeirdRenderer::Shader &shader)
	{
		// m_sdfRenderSystem2D.updatePalette(shader);

		m_sdfRenderSystem2D.updateCustomShapesShader(shader, m_sdfs);
	}
	
	void Scene::updateUIShader(WeirdRenderer::Shader &shader)
	{
		m_UIRenderSystem.updateCustomShapesShader(shader, m_sdfs);
	}

	void Scene::get2DShapesData(WeirdRenderer::Dot2D *&data, uint32_t &size)
	{
		m_sdfRenderSystem2D.fillDataBuffer(data, size);
	}

	void Scene::getUIData(WeirdRenderer::Dot2D *&uiData, uint32_t &size)
	{
		m_UIRenderSystem.fillDataBuffer(uiData, size);
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

		m_sdfRenderSystem2D.shaderNeedsUpdate() = true;

		return entity;
	}

	Entity Scene::addUIShape(ShapeId shapeId, float* variables, uint16_t material, CombinationType combination, int group)
	{
		Entity entity = m_ecs.createEntity();
		UIShape& shape = m_ecs.addComponent<UIShape>(entity);
		shape.m_distanceFieldId = shapeId;
		shape.m_combination = combination;
		shape.m_hasCollision = false;
		shape.m_groupId = group;
		shape.m_material = material;
		std::copy(variables, variables + 8, shape.m_parameters);

		m_UIRenderSystem.shaderNeedsUpdate() = true;

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



}