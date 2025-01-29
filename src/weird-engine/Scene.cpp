#include "scene.h"
#include "Input.h"
#include "SceneManager.h"
#include "math/MathExpressionSerialzation.h"

#include <random>

vec3 g_cameraPosition(15.0f, 50.f, 60.0f);

Scene::Scene()
	: m_simulation2D(MAX_ENTITIES)
	, m_sdfRenderSystem(m_ecs)
	, m_sdfRenderSystem2D(m_ecs)
	, m_renderSystem(m_ecs)
	, m_instancedRenderSystem(m_ecs)
	, m_rbPhysicsSystem2D(m_ecs)
	, m_physicsInteractionSystem(m_ecs)
	, m_playerMovementSystem(m_ecs)
	, m_cameraSystem(m_ecs)
	, m_runSimulationInThread(true)
{
	// Read content from file
	std::string content = get_file_contents("SampleProject/Scenes/SampleScene.scene");

	// Read scene file and load everything
	loadScene(content);

	// Initialize simulation
	m_rbPhysicsSystem2D.init(m_ecs, m_simulation2D);

	// Start simulation if different thread
	if (m_runSimulationInThread)
	{
		m_simulation2D.startSimulationThread();
	}
}

Scene::~Scene()
{
	m_simulation2D.stopSimulationThread();

	// TODO: Free resources from all entities
	m_resourceManager.freeResources(0);
}

void Scene::start()
{
	onStart();
}

void Scene::renderModels(WeirdRenderer::Shader& shader, WeirdRenderer::Shader& instancingShader)
{
	// WeirdRenderer::Camera& camera = m_ecs.getComponent<ECS::Camera>(m_mainCamera).camera;

	// m_renderSystem.render(m_ecs, m_resourceManager, shader, camera, m_lights);

	// m_instancedRenderSystem.render(m_ecs, m_resourceManager, instancingShader, camera, m_lights);
}

void Scene::updateCustomShapesShader(WeirdRenderer::Shader& shader)
{
	std::string str = shader.getFragmentCode();

	std::string toReplace("/*ADD_SHAPES_HERE*/");

	std::ostringstream oss;

	auto atomArray = *m_ecs.getComponentManager<SDFRenderer>()->getComponentArray<SDFRenderer>();
	int32_t atomCount = atomArray.getSize();
	auto componentArray = *m_ecs.getComponentManager<CustomShape>()->getComponentArray<CustomShape>();

	oss << "int dataOffset =  u_loadedObjects - (2 * u_customShapeCount);";

	for (size_t i = 0; i < componentArray.getSize(); i++)
	{
		auto& shape = componentArray[i];

		oss << "{";

		oss << "int idx = dataOffset + " << 2 * i << ";\n";

		// Fetch parameters
		oss << "vec4 parameters0 = texelFetch(u_shapeBuffer, idx);";
		oss << "vec4 parameters1 = texelFetch(u_shapeBuffer, idx + 1);";

		auto fragmentCode = m_sdfs[shape.m_distanceFieldId]->print();

		oss << "float dist = " << fragmentCode << ";" << std::endl;
		oss << "d = min(d, dist);\n";
		// oss << "col = d == (dist) ? getMaterial(p," << (i % 12) + 4 << ") : col;\n";
		oss << "col = d == (dist) ? getMaterial(p," << 3 << ") : col;\n";
		oss << "}\n"
			<< std::endl;
	}

	std::string replacement = oss.str();

	size_t pos = str.find(toReplace);
	if (pos != std::string::npos)
	{ // Check if the substring was found
		// Replace the substring
		str.replace(pos, toReplace.length(), replacement);
	}

	shader.setFragmentCode(str);
}

void Scene::renderShapes(WeirdRenderer::Shader& shader, WeirdRenderer::RenderPlane& rp)
{
	onRender();

	{
		std::shared_ptr<ComponentArray<CustomShape>> componentArray = m_ecs.getComponentManager<CustomShape>()->getComponentArray<CustomShape>();
		shader.setUniform("u_customShapeCount", componentArray->getSize());
	}

	if (m_sdfRenderSystem2D.shaderNeedsUpdate())
	{
		updateCustomShapesShader(shader);
	}

	m_sdfRenderSystem2D.render(m_ecs, shader, rp, m_lights);
}

void Scene::update(double delta, double time)
{
	// Update systems
	m_playerMovementSystem.update(m_ecs, delta);
	// m_cameraSystem.follow(m_ecs, m_mainCamera, 10);

	m_cameraSystem.update(m_ecs);
	g_cameraPosition = m_ecs.getComponent<Transform>(m_mainCamera).position;

	m_rbPhysicsSystem2D.update(m_ecs, m_simulation2D);
	m_physicsInteractionSystem.update(m_ecs, m_simulation2D);
	m_simulation2D.update(delta);

	onUpdate();
}

WeirdRenderer::Camera& Scene::getCamera()
{
	return m_ecs.getComponent<Camera>(m_mainCamera).camera;
}

float Scene::getTime()
{
	return m_simulation2D.getSimulationTime();
}

Entity Scene::addShape(int shapeId, float* variables)
{
	Entity entity = m_ecs.createEntity();
	CustomShape shape(shapeId, variables);
	m_ecs.addComponent(entity, shape);

	return entity;
}

void Scene::loadScene(std::string& sceneFileContent)
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
	}

	// Star
	{

		// vec2 starPosition = p - vec2(25.0f, 30.0f);
		// float infiniteShereDist = length(starPosition) - 5.0f;
		// float displacement = 5.0 * sin(5.0f * atan2f(starPosition.y, starPosition.x) - 5.0f * u_time);

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

		// Store function
		m_sdfs.push_back(starDistance);
	}

	// Circle
	{

		// vec2 starPosition = p - vec2(25.0f, 30.0f);
		// float infiniteShereDist = length(starPosition) - 5.0f;
		// float displacement = 5.0 * sin(5.0f * atan2f(starPosition.y, starPosition.x) - 5.0f * u_time);

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

		// Store function
		m_sdfs.push_back(circleDistance);
	}

	// Box
	{
		// Define variables
		auto offsetX = std::make_shared<FloatVariable>(0);
		auto offsetY = std::make_shared<FloatVariable>(1);
		auto bX = std::make_shared<FloatVariable>(2);
		auto bY = std::make_shared<FloatVariable>(3);
		auto r = std::make_shared<FloatVariable>(4);

		auto time = std::make_shared<FloatVariable>(8);
		auto x = std::make_shared<FloatVariable>(9);
		auto y = std::make_shared<FloatVariable>(10);

		// Define function
		std::shared_ptr<IMathExpression> positionX = std::make_shared<Substraction>(x, offsetX);
		std::shared_ptr<IMathExpression> positionY = std::make_shared<Substraction>(y, offsetY);

		std::shared_ptr<IMathExpression> dX = std::make_shared<Substraction>(std::make_shared<Abs>(positionX), bX);
		std::shared_ptr<IMathExpression> dY = std::make_shared<Substraction>(std::make_shared<Abs>(positionY), bY);

		std::shared_ptr<IMathExpression> aX = std::make_shared<Max>(0.0f, dX);
		std::shared_ptr<IMathExpression> aY = std::make_shared<Max>(0.0f, dY);

		std::shared_ptr<IMathExpression> length = std::make_shared<Length>(aX, aY);

		std::shared_ptr<IMathExpression> minMax = std::make_shared<Min>(0.0f, std::make_shared<Max>(dX, dY));

		std::shared_ptr<IMathExpression> boxDistance = std::make_shared<Addition>(length, minMax);

		// Store function
		m_sdfs.push_back(boxDistance);
	}

	m_simulation2D.setSDFs(m_sdfs);
}
