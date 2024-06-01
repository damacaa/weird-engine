#include "scene.h"
#include "Input.h"

#include <filesystem>

namespace fs = std::filesystem;

constexpr double FIXED_DELTA_TIME = 1 / 100.0;
constexpr size_t MAX_STEPS = 1000000;

Scene::Scene()
{
	// Register component and bind it to a system
	m_ecs.registerComponent<Transform>();
	m_ecs.registerComponent<MeshRenderer>(m_renderSystem);
	m_ecs.registerComponent<InstancedMeshRenderer>(m_instancedRenderSystem);
	m_ecs.registerComponent<SDFRenderer>(m_sdfRenderSystem);
	m_ecs.registerComponent<RigidBody>(m_rbPhysicsSystem);


	// Read scene file and load everything
	LoadScene();

	unsigned int simulationSize = m_ecs.getComponentManager<RigidBody>()->getComponentArray<RigidBody>()->size;
	m_simulation = new Simulation(simulationSize);
	m_rbPhysicsSystem.init(m_ecs, *m_simulation);


	//m_data = new Shape[m_shapes];

	//////////////////////////////////////



}


Scene::~Scene()
{
	delete m_camera;
	delete m_simulation;
}


void Scene::RenderModels(Shader& shader, Shader& instancingShader)
{
	shader.Activate();
	m_renderSystem.render(m_ecs, shader, *m_camera, m_lights);

	instancingShader.Activate();
	m_instancedRenderSystem.render(m_ecs, instancingShader, *m_camera, m_lights);
}


void Scene::RenderShapes(Shader& shader, RenderPlane& rp)
{
	shader.Activate();
	m_sdfRenderSystem.render(m_ecs, shader, rp, m_lights);
}


void Scene::Update(double delta, double time)
{
	m_simulationDelay += delta;

	int steps = 0;
	while (m_simulationDelay >= FIXED_DELTA_TIME && steps < MAX_STEPS)
	{
		m_simulation->Step((float)FIXED_DELTA_TIME);
		m_simulationDelay -= FIXED_DELTA_TIME;
		++steps;
	}

	if (steps >= MAX_STEPS)
		std::cout << "Not enough steps for simulation" << std::endl;

	// Handles camera inputs
	m_camera->Inputs(delta * 100.0f);

	m_rbPhysicsSystem.update(m_ecs, *m_simulation);

	if (Input::GetKeyDown(Input::KeyCode::T)) {
		auto& t = m_ecs.getComponent<Transform>(0);
		t.isDirty = true;
		t.position = glm::vec3(0, 5, 0);
	}


}


#define PI 3.1416f
void Scene::LoadScene()
{
	// Load models
	std::string projectDir = fs::current_path().string();

	std::string spherePath = "/Resources/Models/sphere.gltf";
	std::string cubePath = "/Resources/Models/cube.gltf";
	std::string demoPath = "/Resources/Models/demo.gltf";
	std::string planePath = "/Resources/Models/plane.gltf";




	// Create camera object
	m_camera = new Camera(glm::vec3(0.0f, 2.0f, 5.0f));

	// Spawn mesh balls
	for (size_t i = 0; i < m_meshes; i++)
	{
		Entity entity = m_ecs.createEntity();
		m_ecs.addComponent(entity, Transform());

		if (m_useMeshInstancing) {
			m_ecs.addComponent(entity, InstancedMeshRenderer(m_resourceManager.GetMesh((projectDir + spherePath).c_str(), true)));
			m_instancedRenderSystem.Add(entity);
		}
		else {
			m_ecs.addComponent(entity, MeshRenderer(m_resourceManager.GetMesh((projectDir + spherePath).c_str())));
			m_renderSystem.Add(entity);
		}

		m_ecs.addComponent(entity, RigidBody());
		m_rbPhysicsSystem.Add(entity);
	}

	// Spawn shape balls
	for (size_t i = 0; i < m_shapes; i++)
	{
		Entity entity = m_ecs.createEntity();
		m_ecs.addComponent(entity, Transform());

		m_ecs.addComponent(entity, SDFRenderer());
		m_sdfRenderSystem.Add(entity);

		m_ecs.addComponent(entity, RigidBody());
		m_rbPhysicsSystem.Add(entity);
	}



	{
		// Make monke
		Entity monkey = m_ecs.createEntity();
		Transform monkeyTransform;
		monkeyTransform.position = vec3(10, 3.5f, -30);
		monkeyTransform.rotation = vec3(0.0f, PI * 2.75f / 2.0f, 0.6f);
		monkeyTransform.scale = vec3(8.0f);
		m_ecs.addComponent(monkey, monkeyTransform);

		m_ecs.addComponent(monkey, MeshRenderer(m_resourceManager.GetMesh((projectDir + demoPath).c_str(), 1)));
		m_renderSystem.Add(monkey);
	}


	/*Model* bigBall = new Model((projectDir + spherePath).c_str());
	bigBall->translation = vec3(20, 1.5f, 20);
	bigBall->scale = vec3(3.0f);*/



	Light light;
	light.rotation = normalize(vec3(1.f, 1.5f, 1.f));

	m_lights.push_back(light);
}
