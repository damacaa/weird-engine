#include "scene.h"
#include <filesystem>

namespace fs = std::filesystem;

constexpr double FIXED_DELTA_TIME = 1 / 1000.0;
constexpr size_t MAX_STEPS = 1000000;

Scene::Scene()
{
	m_data = new Shape[m_size];
	m_models = vector<Model*>();

	// Read scene file and load everything
	LoadScene();

	m_simulation = new Simulation(m_data, m_size);

	//////////////////////////////////////



}


Scene::~Scene()
{
	delete[] m_data;

	for (Model* m : m_models) {
		delete m;
	}
	m_models.clear();

	delete m_camera;
	delete m_simulation;
}


void Scene::RenderModels(Shader& shader, Shader& instancingShader)
{
	shader.Activate();

	// Draw models
	for (const Model* model : m_models) {
		model->Draw(shader, *m_camera, m_lights);
	}

	m_renderSystem->render(m_ecs, shader, *m_camera, m_lights);

	instancingShader.Activate();
	m_instancedRenderSystem->render(m_ecs, instancingShader, *m_camera, m_lights);
}


void Scene::RenderShapes(Shader& shader, RenderPlane* rp) const
{
	shader.setUniform("directionalLightDirection", m_lights[0].rotation);
	rp->Draw(shader, m_data, m_size);
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

	// Copy simulation data to scene
	m_simulation->Copy(m_data);

	// Handles camera inputs
	m_camera->Inputs(delta * 100.0f);



	for (int i = 0; i < 10; ++i) {
		movementSystem->update(m_ecs, delta, time);
	
		/*auto& pos = m_ecs.getComponent<Transform>(i);
		m_models[i]->translation.x = pos.x;
		m_models[i]->translation.y = pos.y;
		m_models[i]->translation.z = pos.z;*/
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



	// Make movement system
	movementSystem = std::make_shared<MovementSystem>();
	m_ecs.addSystem<MovementSystem>(movementSystem);

	m_renderSystem = std::make_shared<RenderSystem>();
	m_ecs.addSystem<MovementSystem>(m_renderSystem);

	m_instancedRenderSystem = std::make_shared<InstancedRenderSystem>();
	m_ecs.addSystem<InstancedMeshRenderer>(m_instancedRenderSystem);


	// Register component
	m_ecs.registerComponent<Transform>(movementSystem);
	m_ecs.registerComponent<MeshRenderer>(m_renderSystem);
	m_ecs.registerComponent<InstancedMeshRenderer>(m_instancedRenderSystem);


	// Create camera object
	m_camera = new Camera(glm::vec3(0.0f, 2.0f, 5.0f));



	// Spawn balls
	for (size_t i = 0; i < m_size; i++)
	{
		//Model* ball = new Model((projectDir + spherePath).c_str());
		//ball->translation = vec3(0);
		//ball->scale = vec3(1.0f);
		//m_models.push_back(ball);

		Entity entity = m_ecs.createEntity();
		m_ecs.addComponent(entity, Transform());
		movementSystem->entities.push_back(entity);

		m_ecs.addComponent(entity, InstancedMeshRenderer(m_resourceManager.GetMesh((projectDir + spherePath).c_str(), 1000 )));
		m_renderSystem->entities.push_back(entity);
	}







	// Make monke
	Model* monkey = new Model((projectDir + demoPath).c_str());
	monkey->translation = vec3(10, 3.5f, -30);
	monkey->scale = vec3(8);
	monkey->rotation = vec3(0.0f, PI * 2.75f / 2.0f, 0.6f);

	m_models.push_back(monkey);


	Model* bigBall = new Model((projectDir + spherePath).c_str());
	bigBall->translation = vec3(20, 1.5f, 20);
	bigBall->scale = vec3(3.0f);
	m_models.push_back(bigBall);

	for (size_t i = 0; i < m_size; i++)
	{
		m_data[i].position.y = i + 1;
		m_data[i].position.x = 3.0f * sin(10.1657873f * i);
		m_data[i].position.z = 3.0f * cos(10.2982364f * i);
	}

	Light light;
	light.rotation = normalize(vec3(1.f, 1.5f, 1.f));

	m_lights.push_back(light);
}
