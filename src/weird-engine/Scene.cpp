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

void test(std::string& str)
{
	std::string toReplace("/*ADD_SHAPES_HERE*/");
	std::string replacement("return vec4(p, 0, 1);");

	size_t pos = str.find(toReplace);
	if (pos != std::string::npos) { // Check if the substring was found
		// Replace the substring
		str.replace(pos, toReplace.length(), replacement);
	}
}

bool newShapeAdded = true;
void Scene::renderShapes(WeirdRenderer::Shader& shader, WeirdRenderer::RenderPlane& rp)
{
	if (newShapeAdded)
	{
		std::string fragCode = shader.getFragmentCode();
		test(fragCode);
		shader.setFragmentCode(fragCode);
		newShapeAdded = false;
	}

	//m_sdfRenderSystem.render(m_ecs, shader, rp, m_lights);
	m_sdfRenderSystem2D.render(m_ecs, shader, rp, m_lights);
}





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

	// Load next scene
	if (Input::GetKeyDown(Input::Q))
	{
		SceneManager::getInstance().loadNextScene();
	}

	// Add balls
	if (Input::GetKey(Input::E))
	{
		m_weirdSandBox.throwBalls(m_ecs, m_simulation2D);
	}

	CustomShape& cs = m_ecs.getComponent<CustomShape>(62);
	//cs.m_parameters[0] = 15.0f + (5.0f * sin(m_simulation2D.getSimulationTime()));
	cs.m_parameters[4] = (static_cast<int>(std::floor(m_simulation2D.getSimulationTime())) % 5) + 2;
	cs.m_parameters[3] = sin(3.1416 * m_simulation2D.getSimulationTime());
	//cs.m_parameters[3] = Input::GetMouseX() / 600.0;
	cs.m_isDirty = true;
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


	// Shapes

	// Floor
	{
		// p.y - a * sinf(0.5f * p.x + u_time);

		// Define variables
		auto amplitude = std::make_shared<FloatVariable>(0);
		auto period = std::make_shared<FloatVariable>(1);

		auto time = std::make_shared<FloatVariable>(8);
		auto x = std::make_shared<FloatVariable>(9);
		auto y = std::make_shared<FloatVariable>(10);

		// Define function
		auto sineContent = std::make_shared<Addition>(std::make_shared<Multiplication>(period, x), time);
		std::shared_ptr<IMathExpression> waveFormula = std::make_shared<Substraction>(y, std::make_shared<Multiplication>(amplitude, std::make_shared<Sine>(sineContent)));

		// Store function
		m_sdfs.push_back(waveFormula);

		std::cout << waveFormula->print() << std::endl;
	}

	// Star
	{

		//vec2 starPosition = p - vec2(25.0f, 30.0f);
		//float infiniteShereDist = length(starPosition) - 5.0f;
		//float displacement = 5.0 * sin(5.0f * atan2f(starPosition.y, starPosition.x) - 5.0f * u_time);

		// Define variables
		auto offsetX = std::make_shared<FloatVariable>(0);
		auto offsetY = std::make_shared<FloatVariable>(1);
		auto radious = std::make_shared<FloatVariable>(2);
		auto displacementStrength = std::make_shared<FloatVariable>(3);
		auto starPoints = std::make_shared<FloatVariable>(4);
		auto speed = std::make_shared<FloatVariable>(5);

		auto time = std::make_shared<FloatVariable>(8);
		auto x = std::make_shared<FloatVariable>(9);
		auto y = std::make_shared<FloatVariable>(10);


		// Define function
		std::shared_ptr<IMathExpression> positionX = std::make_shared<Substraction>(x, offsetX);
		std::shared_ptr<IMathExpression> positionY = std::make_shared<Substraction>(y, offsetY);

		// Circle
		std::shared_ptr<IMathExpression> circleDistance = std::make_shared<Substraction>(std::make_shared<Length>(positionX, positionY), radious);


		std::shared_ptr<IMathExpression> angularDisplacement = std::make_shared<Multiplication>(starPoints, std::make_shared<Atan2>(positionY, positionX));
		std::shared_ptr<IMathExpression> animationTime = std::make_shared<Multiplication>(speed, time);

		std::shared_ptr<IMathExpression> sinContent = std::make_shared<Sine>(std::make_shared<Substraction>(angularDisplacement, animationTime));
		std::shared_ptr<IMathExpression> displacement = std::make_shared<Multiplication>(displacementStrength, sinContent);

		std::shared_ptr<IMathExpression> starDistance = std::make_shared<Addition>(circleDistance, displacement);

		std::cout << starDistance->print() << std::endl;

		// Store function
		m_sdfs.push_back(starDistance);
	}

	m_simulation2D.setSDFs(m_sdfs);

	{
		Entity floor = m_ecs.createEntity();

		float variables[8]{ 1.0f, 0.5f };
		CustomShape shape(0, variables);
		m_ecs.addComponent(floor, shape);
	}

	{
		Entity star = m_ecs.createEntity();

		float variables[8]{ 25.0f, 30.0f, 5.0f, 0.5f, 13.0f, 5.0f };
		CustomShape shape(1, variables);
		m_ecs.addComponent(star, shape);
	}

	{
		Entity star = m_ecs.createEntity();

		float variables[8]{ 25.0f, 30.0f, 5.0f, 0.5f, 13.0f, 5.0f };
		CustomShape shape(1, variables);
		m_ecs.addComponent(star, shape);
	}

}
