#include "scene.h"
#include "Input.h"
#include "SceneManager.h"

#include <random>


bool g_runSimulation = true;
double g_lastSpawnTime = 0;

vec3 g_cameraPosition(15.0f, 50.f, 60.0f);


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
	//m_sdfRenderSystem.render(m_ecs, shader, rp, m_lights);
	m_sdfRenderSystem2D.render(m_ecs, shader, rp, m_lights);
}


int g_currentMaterial = 0;
void Scene::update(double delta, double time)
{
	// Update systems
	m_playerMovementSystem.update(m_ecs, delta);
	//m_cameraSystem.follow(m_ecs, m_mainCamera, 680);

	m_cameraSystem.update(m_ecs);
	g_cameraPosition = m_ecs.getComponent<Transform>(m_mainCamera).position;

	m_rbPhysicsSystem2D.update(m_ecs, m_simulation2D);
	m_physicsInteractionSystem.update(m_ecs, m_simulation2D);
	m_simulation2D.update(delta);

	m_weirdSandBox.update(m_ecs, m_simulation2D, m_sdfRenderSystem2D);

	if (Input::GetKeyDown(Input::Q))
	{
		SceneManager::getInstance().loadNextScene();
	}

	if (Input::GetKey(Input::E))
		m_weirdSandBox.throwBalls(m_ecs, m_simulation2D);

	if (Input::GetKey(Input::T))
	{
		SimulationID lastInSimulation = m_simulation2D.getSize() - 1;

		int last = m_ecs.getComponentArray<RigidBody2D>()->getSize() - 1;
		SimulationID target = m_ecs.getComponentArray<RigidBody2D>()->getDataAtIdx(last).simulationId;

		auto v = vec2(15, 30) - m_simulation2D.getPosition(target);
		m_simulation2D.addForce(target, 50.0f * normalize(v));
	}


	if (Input::GetMouseButtonDown(Input::LeftClick))
	{
		// Test screen coordinates to 2D world coordinates
		auto& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);

		float x = Input::GetMouseX();
		float y = Input::GetMouseY();

		vec2 pp = Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

		//std::cout << "Click: " << pp.x << ", " << pp.y << std::endl;

		if (false)
		{
			Transform t;
			//t.position = vec3(pp.x + sin(time), pp.y + cos(time), 0.0);
			t.position = vec3(pp.x, pp.y, 0.0);
			Entity entity = m_ecs.createEntity();
			m_ecs.addComponent(entity, t);

			m_ecs.addComponent(entity, SDFRenderer(g_currentMaterial + 4));

			RigidBody2D rb(m_simulation2D);
			m_ecs.addComponent(entity, rb);
			m_simulation2D.addForce(rb.simulationId, 1000.0f * vec2(Input::GetMouseDeltaX(), -Input::GetMouseDeltaY()));
		}
		else
		{
			SimulationID id = m_simulation2D.raycast(pp);
			if (id < m_simulation2D.getSize())
			{
				m_simulation2D.Fix(id);
				std::cout << id << std::endl;
			}else
			{
				std::cout << "Miss" << std::endl;
			}
		}


	}

	if (Input::GetMouseButtonUp(Input::LeftClick))
	{
		g_currentMaterial = (g_currentMaterial + 1) % 12;
	}

}




WeirdRenderer::Camera& Scene::getCamera()
{
	return m_ecs.getComponent<Camera>(m_mainCamera).camera;
}

float Scene::getTime()
{
	return m_simulation2D.getSimulationTime();
}



void Scene::loadScene(std::string sceneFileContent)
{
	json scene = json::parse(sceneFileContent);

	std::string projectDir = fs::current_path().string() + "/SampleProject";


	// Create camera object
	m_mainCamera = m_ecs.createEntity();

	Transform t;
	t.position = g_cameraPosition;
	t.rotation = vec3(0, 0, -1.0f);
	m_ecs.addComponent(m_mainCamera, t);

	ECS::Camera c;
	m_ecs.addComponent(m_mainCamera, c);
	m_ecs.addComponent(m_mainCamera, FlyMovement2D());


	// Add a light
	WeirdRenderer::Light light;
	light.rotation = normalize(vec3(1.f, 0.5f, 0.f));
	m_lights.push_back(light);


	size_t circles = scene["Circles"].get<int>();
	m_weirdSandBox.spawnEntities(m_ecs, m_simulation2D, circles, 0);








}
