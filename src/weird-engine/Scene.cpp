#include "scene.h"
#include "Input.h"
#include "SceneManager.h"

#include <random>


bool g_runSimulation = true;
double g_lastSpawnTime = 0;


Scene::Scene(const char* filePath) :
	m_simulation(MAX_ENTITIES)
	, m_simulation2D(MAX_ENTITIES)
	, m_sdfRenderSystem(m_ecs)
	, m_sdfRenderSystem2D(m_ecs)
	, m_renderSystem(m_ecs)
	, m_instancedRenderSystem(m_ecs)
	, m_rbPhysicsSystem(m_ecs)
	, m_rbPhysicsSystem2D(m_ecs)
	, m_physicsInteractionSystem(m_ecs)
	, m_playerMovementSystem(m_ecs)
	, m_cameraSystem(m_ecs)
	, m_runSimulationInThread(true)
{
	// Read content from file
	std::string content = get_file_contents(filePath);

	// Read scene file and load everything
	loadScene(content);

	// Initialize simulation
	m_rbPhysicsSystem.init(m_ecs, m_simulation);
	m_rbPhysicsSystem2D.init(m_ecs, m_simulation2D);

	// Start simulation if different thread
	if (m_runSimulationInThread)
	{
		m_simulation.startSimulationThread();
		m_simulation2D.startSimulationThread();
	}
}


Scene::~Scene()
{
	m_simulation.stopSimulationThread();
	m_simulation2D.stopSimulationThread();

	m_resourceManager.freeResources(0);
}


void Scene::renderModels(WeirdRenderer::Shader& shader, WeirdRenderer::Shader& instancingShader)
{
	WeirdRenderer::Camera& camera = m_ecs.getComponent<ECS::Camera>(m_mainCamera).camera;

	m_renderSystem.render(m_ecs, m_resourceManager, shader, camera, m_lights);

	m_instancedRenderSystem.render(m_ecs, m_resourceManager, instancingShader, camera, m_lights);
}



void Scene::renderShapes(WeirdRenderer::Shader& shader, WeirdRenderer::RenderPlane& rp)
{
	shader.activate();

	m_sdfRenderSystem.render(m_ecs, shader, rp, m_lights);
	m_sdfRenderSystem2D.render(m_ecs, shader, rp, m_lights);
}




void throwBalls(ECSManager& m_ecs, Simulation2D& m_simulation2D, PhysicsSystem2D& m_rbPhysicsSystem2D, SDFRenderSystem2D& m_sdfRenderSystem2D)
{
	if (Input::GetKey(Input::E) && g_runSimulation && m_simulation2D.getSimulationTime() > g_lastSpawnTime + 0.1)
	{
		int amount = 1;
		for (size_t i = 0; i < amount; i++)
		{
			float x = 0.f;
			float y = 60 + (1.2 * i);
			float z = 0;

			Transform t;
			t.position = vec3(x + 0.5f, y + 0.5f, z);
			Entity entity = m_ecs.createEntity();
			m_ecs.addComponent(entity, t);

			m_ecs.addComponent(entity, SDFRenderer(2 + m_sdfRenderSystem2D.getEntityCount() % 3));

			m_sdfRenderSystem2D.add(entity);

			m_ecs.addComponent(entity, RigidBody2D());
			m_rbPhysicsSystem2D.add(entity);
			m_rbPhysicsSystem2D.addNewRigidbodiesToSimulation(m_ecs, m_simulation2D);
			m_rbPhysicsSystem2D.addForce(m_ecs, m_simulation2D, entity, vec2(20, 0));
		}

		g_lastSpawnTime = m_simulation2D.getSimulationTime();
	}
}


int currentMaterial = 0;


void Scene::update(double delta, double time)
{
	// Update systems
	m_playerMovementSystem.update(m_ecs, delta);
	m_cameraSystem.update(m_ecs);

	m_simulation2D.update(10.0*delta);
	m_rbPhysicsSystem2D.update(m_ecs, m_simulation2D);
	m_physicsInteractionSystem.update(m_ecs, m_simulation2D);

	m_simulatedImageGenerator.update(m_ecs, m_simulation2D, m_sdfRenderSystem2D);

	if (Input::GetKeyDown(Input::Q))
	{
		SceneManager::getInstance().loadNextScene();
	}



	throwBalls(m_ecs, m_simulation2D, m_rbPhysicsSystem2D, m_sdfRenderSystem2D);




	if (Input::GetKey(Input::T))
	{
		auto v = vec2(15, 30) - m_simulation2D.getPosition(0);
		m_rbPhysicsSystem2D.addForce(m_ecs, m_simulation2D, 0, 10.0f * normalize(v));
	}


	if (Input::GetMouseButton(Input::LeftClick))
	{
		// Test screen coordinates to 2D world coordinates
		auto& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);

		float x = Input::GetMouseX() + sin(time);
		float y = Input::GetMouseY() + cos(time);

		vec2 pp = Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

		Transform t;
		t.position = vec3(pp.x, pp.y, 0.0);
		Entity entity = m_ecs.createEntity();
		m_ecs.addComponent(entity, t);

		m_ecs.addComponent(entity, SDFRenderer(currentMaterial));
		m_sdfRenderSystem2D.add(entity);

		m_ecs.addComponent(entity, RigidBody2D());
		m_rbPhysicsSystem2D.add(entity);
		m_rbPhysicsSystem2D.addNewRigidbodiesToSimulation(m_ecs, m_simulation2D);
		m_rbPhysicsSystem2D.addForce(m_ecs, m_simulation2D, entity, 1000.0f * vec2(Input::GetMouseDeltaX(), -Input::GetMouseDeltaY()));

	}

	if (Input::GetMouseButtonUp(Input::LeftClick))
	{
		currentMaterial = (currentMaterial + 1) % 16;
	}

}




WeirdRenderer::Camera& Scene::getCamera()
{
	return m_ecs.getComponent<Camera>(m_mainCamera).camera;
}

void Scene::loadScene(std::string sceneFileContent)
{
	json scene = json::parse(sceneFileContent);

	std::string projectDir = fs::current_path().string() + "/SampleProject";

	size_t circles = scene["Circles"].get<int>();
	m_simulatedImageGenerator.SpawnEntities(m_ecs, m_rbPhysicsSystem2D, m_sdfRenderSystem2D, circles);

	//// Spawn 2d balls
	//for (size_t i = 0; i < 16*5; i++)
	//{

	//	float x = i % 16;
	//	float y = (int)(i / 16) + 1;

	//	float z = 0;

	//	Transform t;
	//	t.position = vec3(x + 0.5f, y + 0.5f, z);

	//	Entity entity = m_ecs.createEntity();
	//	m_ecs.addComponent(entity, t);



	//	int material = i % 16;

	//	m_ecs.addComponent(entity, SDFRenderer(material));
	//	m_sdfRenderSystem.add(entity);

	//	m_ecs.addComponent(entity, RigidBody2D());
	//}

	// Create camera object
	m_mainCamera = m_ecs.createEntity();

	Transform t;
	t.position = vec3(15.0f, 40.f, 60.0f);
	t.rotation = vec3(0, 0, -1.0f);
	m_ecs.addComponent(m_mainCamera, t);

	ECS::Camera c;
	m_ecs.addComponent(m_mainCamera, c);
	m_ecs.addComponent(m_mainCamera, FlyMovement2D());


	// Add a light
	WeirdRenderer::Light light;
	light.rotation = normalize(vec3(1.f, 0.5f, 0.f));
	m_lights.push_back(light);


}
