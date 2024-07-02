#include "scene.h"
#include "Input.h"

#include <filesystem>
#include <random>

namespace fs = std::filesystem;

constexpr double FIXED_DELTA_TIME = 1 / 1000.0;
constexpr size_t MAX_STEPS = 1000000;
constexpr size_t MAX_SIMULATED_OBJECTS = 100000;

#define PI 3.1416f

// Create a random device to seed the generator
std::random_device rd;

// Use the Mersenne Twister engine seeded with the random device
std::mt19937 gen(rd());

// Define a uniform integer distribution from 1 to 100
std::uniform_int_distribution<> dis(1, 100);

Scene::Scene() :
	m_simulation(MAX_SIMULATED_OBJECTS),
	m_sdfRenderSystem(m_ecs),
	m_renderSystem(m_ecs),
	m_instancedRenderSystem(m_ecs),
	m_rbPhysicsSystem(m_ecs)
{
	// Read scene file and load everything
	loadScene();

	m_rbPhysicsSystem.init(m_ecs, m_simulation);
}


Scene::~Scene()
{

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
}


Entity monkey;
void Scene::update(double delta, double time)
{
	m_simulationDelay += delta;

	int steps = 0;
	while (m_simulationDelay >= FIXED_DELTA_TIME && steps < MAX_STEPS)
	{
		// Handles camera inputs
		camera->Inputs(FIXED_DELTA_TIME);

		m_simulation.step((float)FIXED_DELTA_TIME);
		m_simulationDelay -= FIXED_DELTA_TIME;
		++steps;
	}

	if (steps >= MAX_STEPS)
		std::cout << "Not enough steps for simulation" << std::endl;

	m_rbPhysicsSystem.update(m_ecs, m_simulation);
}


const int m_meshes = 10;
const int m_shapes = 10;
const bool m_useMeshInstancing = true;

void Scene::loadScene()
{
	std::string projectDir = fs::current_path().string();

	std::string spherePath = "/Resources/Models/sphere.gltf";
	std::string cubePath = "/Resources/Models/cube.gltf";
	std::string demoPath = "/Resources/Models/Monkey/monkey.gltf";
	std::string planePath = "/Resources/Models/plane.gltf";

	// Create camera object
	camera = std::make_unique<Camera>(Camera(glm::vec3(0.0f, 5.0f, 15.0f)));

	// Make monke
	Entity monkey = m_ecs.createEntity();
	Transform monkeyTransform;
	monkeyTransform.position = vec3(10, 3.5f, -30);
	monkeyTransform.rotation = vec3(0.0f, PI * 2.75f / 2.0f, 0.6f);
	monkeyTransform.scale = vec3(8.0f);
	m_ecs.addComponent(monkey, monkeyTransform);

	m_ecs.addComponent(monkey, MeshRenderer(m_resourceManager.getMeshId((projectDir + demoPath).c_str(), 1)));
	m_renderSystem.add(monkey);


	// Spawn mesh balls
	for (size_t i = 0; i < m_meshes; i++)
	{
		Entity entity = m_ecs.createEntity();


		float x = dis(gen) - 50;
		float y = dis(gen);
		float z = dis(gen) - 50;

		Transform t;
		t.position = 0.1f * vec3(x, y, z);
		m_ecs.addComponent(entity, t);

		if (m_useMeshInstancing) {

			m_ecs.addComponent(entity, InstancedMeshRenderer(m_resourceManager.getMeshId((projectDir +
				(i % 2 == 0 ? cubePath : spherePath)
				).c_str(), entity, true)));

			m_instancedRenderSystem.add(entity);
		}
		else {
			m_ecs.addComponent(entity, MeshRenderer(m_resourceManager.getMeshId((projectDir + spherePath).c_str(), entity)));
			m_renderSystem.add(entity);
		}

		m_ecs.addComponent(entity, RigidBody());
		m_rbPhysicsSystem.add(entity);
	}

	// Spawn shape balls
	for (size_t i = 0; i < m_shapes; i++)
	{
		float x = dis(gen) - 50;
		float y = dis(gen);
		float z = dis(gen) - 50;

		Transform t;
		t.position = 0.1f * vec3(x, y, z);


		Entity entity = m_ecs.createEntity();
		m_ecs.addComponent(entity, t);

		m_ecs.addComponent(entity, SDFRenderer());
		m_sdfRenderSystem.add(entity);

		m_ecs.addComponent(entity, RigidBody());
		m_rbPhysicsSystem.add(entity);
	}




	Light light;
	light.rotation = normalize(vec3(1.f, 1.5f, 1.f));

	m_lights.push_back(light);
}
