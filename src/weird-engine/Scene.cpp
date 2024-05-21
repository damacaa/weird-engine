#include "scene.h"
#include <filesystem>
namespace fs = std::filesystem;


Scene::Scene()
{
	m_data = new Shape[m_size];
	m_models = vector<Model*>();

	// Read scene file and load everything
	LoadScene();

	// Severity	Code	Description	Project	File	Line	Suppression State
	// Error(active)	E0291	no default constructor exists for class "Simulation"	weird - engine	C : \Projects\weird - engine\src\weird - engine\Scene.cpp	7
	//m_simulation = new Simulation(m_data, m_size);
	//m_simulation = std::make_unique<Simulation>(new Simulation(m_data, m_size));

}

Scene::~Scene()
{
	delete[] m_data;
	for (Model* m : m_models) {
		delete m;
	}
	m_models.clear();
	delete m_camera;
	//delete m_simulation;
}

void Scene::Render(Camera& camera, Shader& shader)
{
	// Draw models

	for (const Model* obj : m_models) {

		obj->Draw(shader, camera);
	}

}

void Scene::Update(double delta, double time)
{
	double fixedDeltaTime = 1 / 100.0;
	const size_t MAX_STEPS = 1000000;

	int steps = 0;
	while (simulationDelay >= fixedDeltaTime && steps < MAX_STEPS)
	{
		//m_simulation->Step((float)fixedDeltaTime);
		simulationDelay -= fixedDeltaTime;
		++steps;
	}

	if (steps >= MAX_STEPS)
		std::cout << "Not enough steps for simulation" << std::endl;

	//m_simulation.Copy(m_data);


	int i = 0;
	for (Model* m : m_models)
	{
		m->translation.y = i + 1;
		m->translation.x = 3.0f * sin(i + time);
		m->translation.z = 3.0f * cos(i + time);
		i++;
	}

	for (size_t i = 0; i < m_size; i++)
	{
		m_data[i].position.y = i + 1.5;
		m_data[i].position.x = 3.0f * sin(i - time);
		m_data[i].position.z = 3.0f * cos(i - time);

	}
}

void Scene::LoadScene()
{
	// Creates camera object
	m_camera = new Camera(glm::vec3(0.0f, 2.0f, 5.0f));

	// Load models
	std::string projectDir = fs::current_path().string();
	std::string spherePath = "/Resources/Models/sphere.gltf";
	std::string cubePath = "/Resources/Models/cube.gltf";
	std::string demoPath = "/Resources/Models/demo.gltf";
	std::string planePath = "/Resources/Models/plane.gltf";

	Model* sphere = new Model((projectDir + spherePath).c_str());
	sphere->translation.x = 2.0f;
	sphere->translation.y = 2.1f;
	sphere->translation.z = 3.0f;

	Model* cube = new Model((projectDir + cubePath).c_str());
	cube->translation.x = -5.0f;
	cube->translation.y = 2.0f;
	cube->translation.z = 0.0f;

	m_models.push_back(cube);
	m_models.push_back(sphere);

	for (size_t i = 0; i < 100; i++)
	{
		Model* m = new Model((projectDir + spherePath).c_str());
		m_models.push_back(m);
	}


	for (size_t i = 0; i < m_size; i++)
	{
		m_data[i].position.y = i + 1;
		m_data[i].position.x = 3.0f * sin(10.1657873f * i);
		m_data[i].position.z = 3.0f * cos(10.2982364f * i);
	}
}
