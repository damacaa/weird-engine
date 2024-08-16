#include "scene.h"
#include "Input.h"
#include "SceneManager.h"

#include <random>





constexpr size_t MAX_SIMULATED_OBJECTS = 100000;

#define PI 3.1416f

Scene::Scene(const char* file) :
	m_simulation(MAX_SIMULATED_OBJECTS),
	m_simulation2D(MAX_SIMULATED_OBJECTS),
	m_sdfRenderSystem(m_ecs),
	m_sdfRenderSystem2D(m_ecs),
	m_renderSystem(m_ecs),
	m_instancedRenderSystem(m_ecs),
	m_rbPhysicsSystem(m_ecs),
	m_rbPhysicsSystem2D(m_ecs),
	m_runSimulationInThread(false)
{

	std::string content = get_file_contents(file);

	// Read scene file and load everything
	loadScene(content);

	if (m_runSimulationInThread) {
		m_simulation.startSimulationThread();
		m_simulation2D.startSimulationThread();
	}

	// Start simulation
	m_rbPhysicsSystem.init(m_ecs, m_simulation);
	m_rbPhysicsSystem2D.init(m_ecs, m_simulation2D);
}


Scene::~Scene()
{
	if (m_runSimulationInThread) {
		m_simulation.stopSimulationThread();
		m_simulation2D.stopSimulationThread();
	}

	m_resourceManager.freeResources(0);
}


void Scene::renderModels(Shader& shader, Shader& instancingShader)
{
	m_renderSystem.render(m_ecs, m_resourceManager, shader, *camera, m_lights);

	m_instancedRenderSystem.render(m_ecs, m_resourceManager, instancingShader, *camera, m_lights);
}


void Scene::renderShapes(Shader& shader, RenderPlane& rp)
{
	shader.activate();

	m_sdfRenderSystem.render(m_ecs, shader, rp, m_lights);
	m_sdfRenderSystem2D.render(m_ecs, shader, rp, m_lights);

}




void Scene::update(double delta, double time)
{
	if (!m_runSimulationInThread) {
		m_simulation.update(delta);
		m_simulation2D.update(delta);
	}

	// Handles camera inputs
	camera->Inputs(delta);

	m_rbPhysicsSystem.update(m_ecs, m_simulation);
	m_rbPhysicsSystem2D.update(m_ecs, m_simulation2D);

	if (Input::GetKeyDown(Input::Q)) {
		SceneManager::getInstance().loadNextScene();
	}
}



void Scene::loadScene(std::string sceneFileContent)
{

	json scene = json::parse(sceneFileContent);

	size_t meshes = scene["Meshes"].get<int>();
	size_t shapes = scene["Shapes"].get<int>();
	size_t circles = scene["Circles"].get<int>();

	std::string projectDir = fs::current_path().string() + "/SampleProject";

	std::string spherePath = "/Resources/Models/sphere.gltf";
	std::string cubePath = "/Resources/Models/cube.gltf";
	std::string monkeyPath = "/Resources/Models/Monkey/monkey.gltf";
	std::string planePath = "/Resources/Models/plane.gltf";


	// Make monke
	if (false) {

		Entity monkey = m_ecs.createEntity();
		Transform monkeyTransform;
		monkeyTransform.position = vec3(10, 3.5f, -30);
		monkeyTransform.rotation = vec3(0.0f, PI * 2.75f / 2.0f, 0.6f);
		monkeyTransform.scale = vec3(8.0f);
		m_ecs.addComponent(monkey, monkeyTransform);

		m_ecs.addComponent(monkey, MeshRenderer(m_resourceManager.getMeshId((projectDir + monkeyPath).c_str(), 1)));
		m_renderSystem.add(monkey);
	}


	// Create camera object
	camera = std::make_unique<Camera>(Camera(glm::vec3(0.0f, 5.0f, 15.0f)));

	// Add a light
	Light light;
	light.rotation = normalize(vec3(1.f, 1.5f, 1.f));
	m_lights.push_back(light);


	// Create a random device to seed the generator
	std::random_device rdd;

	// Use the Mersenne Twister engine seeded with the random device
	std::mt19937 gen(rdd());

	// Define a uniform integer distribution from 1 to 100
	std::uniform_int_distribution<> diss(1, 100);


	// Spawn mesh balls
	for (size_t i = 0; i < meshes; i++)
	{
		Entity entity = m_ecs.createEntity();


		float x = diss(gen) - 50;
		float y = diss(gen);
		float z = diss(gen) - 50;

		Transform t;
		t.position = 0.1f * vec3(x, y, z);
		m_ecs.addComponent(entity, t);


		/*m_ecs.addComponent(entity, InstancedMeshRenderer(m_resourceManager.getMeshId((projectDir +
			(i % 2 == 0 ? cubePath : spherePath)
			).c_str(), entity, true)));*/

		m_ecs.addComponent(entity, InstancedMeshRenderer(
			m_resourceManager.getMeshId(
				(projectDir + spherePath).c_str(),
				entity,
				true)
		)
		);

		m_instancedRenderSystem.add(entity);


		m_ecs.addComponent(entity, RigidBody());
		m_rbPhysicsSystem.add(entity);
	}

	// Spawn shape balls
	for (size_t i = 0; i < shapes; i++)
	{
		float x = diss(gen) - 50;
		float y = diss(gen);
		float z = diss(gen) - 50;

		Transform t;
		t.position = 0.1f * vec3(x, y, z);


		Entity entity = m_ecs.createEntity();
		m_ecs.addComponent(entity, t);

		m_ecs.addComponent(entity, SDFRenderer());
		m_sdfRenderSystem.add(entity);

		m_ecs.addComponent(entity, RigidBody());
		m_rbPhysicsSystem.add(entity);
	}

	// Spawn 2d balls
	for (size_t i = 0; i < circles; i++)
	{
		/*float x = diss(gen);
		float y = diss(gen);
		float z = 0;*/

		float x = i % 30;
		float y = (int)(i / 30);
		float z = 0;

		Transform t;
		t.position =  vec3(x + 0.5f, y + 10.0f, z);


		Entity entity = m_ecs.createEntity();
		m_ecs.addComponent(entity, t);

		m_ecs.addComponent(entity, SDFRenderer());
		m_sdfRenderSystem2D.add(entity);

		m_ecs.addComponent(entity, RigidBody2D());
		m_rbPhysicsSystem2D.add(entity);
	}
}
