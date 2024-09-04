#include "scene.h"
#include "Input.h"
#include "SceneManager.h"

#include <random>


bool g_runSimulation = true;

constexpr size_t MAX_SIMULATED_OBJECTS = 100000;


Scene::Scene(const char* file) :
	m_simulation(MAX_SIMULATED_OBJECTS),
	m_simulation2D(MAX_SIMULATED_OBJECTS),
	m_sdfRenderSystem(m_ecs),
	m_sdfRenderSystem2D(m_ecs),
	m_renderSystem(m_ecs),
	m_instancedRenderSystem(m_ecs),
	m_rbPhysicsSystem(m_ecs),
	m_rbPhysicsSystem2D(m_ecs),
	m_physicsInteractionSystem(m_ecs),
	m_runSimulationInThread(true)
{
	// Read content from file
	std::string content = get_file_contents(file);

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
	WeirdRenderer::Camera& camera = m_ecs.getComponent<ECS::Camera>(m_camera).m_camera;

	m_renderSystem.render(m_ecs, m_resourceManager, shader, camera, m_lights);

	m_instancedRenderSystem.render(m_ecs, m_resourceManager, instancingShader, camera, m_lights);
}



void Scene::renderShapes(WeirdRenderer::Shader& shader, WeirdRenderer::RenderPlane& rp)
{
	shader.activate();

	m_sdfRenderSystem.render(m_ecs, shader, rp, m_lights);
	m_sdfRenderSystem2D.render(m_ecs, shader, rp, m_lights);
}



double lastSpawnTime = 0;
void Scene::update(double delta, double time)
{
	if (true || !m_runSimulationInThread)
	{
		m_simulation.update(delta);
		m_simulation2D.update(delta);
	}

	// Handles m_camera inputs
	WeirdRenderer::Camera& camera = m_ecs.getComponent<ECS::Camera>(m_camera).m_camera;
	camera.Inputs(delta);

	m_rbPhysicsSystem.update(m_ecs, m_simulation);
	m_rbPhysicsSystem2D.update(m_ecs, m_simulation2D);
	m_physicsInteractionSystem.update(m_ecs, m_simulation2D);

	if (Input::GetKeyDown(Input::Q))
	{
		SceneManager::getInstance().loadNextScene();
	}

	if (Input::GetKey(Input::E) && g_runSimulation && m_simulation2D.getSimulationTime() > lastSpawnTime + 0.05)
	{
		int amount = 1;
		for (size_t i = 0; i < amount; i++)
		{

			float x = 0.f;
			float y = 30 + (1.2 * i);
			float z = 0;

			Transform t;
			t.position = vec3(x + 0.5f, y + 0.5f, z);
			Entity entity = m_ecs.createEntity();
			m_ecs.addComponent(entity, t);

			m_ecs.addComponent(entity, SDFRenderer(m_sdfRenderSystem2D.getEntityCount() % 3));
			m_sdfRenderSystem2D.add(entity);

			m_ecs.addComponent(entity, RigidBody2D());
			m_rbPhysicsSystem2D.add(entity);
			m_rbPhysicsSystem2D.addNewRigidbodiesToSimulation(m_ecs, m_simulation2D);
			m_rbPhysicsSystem2D.addForce(m_ecs, m_simulation2D, entity, vec2(20, 0));
		}

		lastSpawnTime = m_simulation2D.getSimulationTime();
	}


	if (Input::GetKey(Input::T))
	{
		auto v = vec2(15, 30) - m_simulation2D.getPosition(0);
		m_rbPhysicsSystem2D.addForce(m_ecs, m_simulation2D, 0, 10.0f * normalize(v));
	}
}


WeirdRenderer::Camera& Scene::getCamera()
{
	return m_ecs.getComponent<Camera>(m_camera).m_camera;
}

void Scene::loadScene(std::string sceneFileContent)
{
	json scene = json::parse(sceneFileContent);

	std::string projectDir = fs::current_path().string() + "/SampleProject";

	// Create m_camera object
	Entity cameraEntity = m_ecs.createEntity();

	Transform t;
	t.position = vec3(15.0f, 10.f, 10.0f);
	t.rotation = vec3(0.0f);
	m_ecs.addComponent(cameraEntity, t);

	ECS::Camera c(vec3(15.0f, 10.f, 10.0f));
	m_ecs.addComponent(cameraEntity, c);



	//m_camera = std::make_unique<Camera>(Camera(glm::vec3(15.0f, 10.f, 10.0f)));

	// Add a light
	WeirdRenderer::Light light;
	light.rotation = normalize(vec3(1.f, 0.5f, 0.f));
	m_lights.push_back(light);

	size_t circles = scene["Circles"].get<int>();

	// Spawn 2d balls
	for (size_t i = 0; i < circles; i++)
	{
		float x = i % 30;
		float y = (int)(i / 30) + x;
		float z = 0;

		Transform t;
		t.position = vec3(x + 0.5f, y + 0.5f, z);


		Entity entity = m_ecs.createEntity();
		m_ecs.addComponent(entity, t);

		m_ecs.addComponent(entity, SDFRenderer(2));
		m_sdfRenderSystem2D.add(entity);

		if (g_runSimulation)
		{
			m_ecs.addComponent(entity, RigidBody2D());
			m_rbPhysicsSystem2D.add(entity);
		}
	}
}
