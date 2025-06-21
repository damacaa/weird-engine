#include "weird-engine/Scene.h"
#include "weird-engine/Input.h"
#include "weird-engine/SceneManager.h"
#include "weird-engine/math/MathExpressionSerialzation.h"

#include <random>
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

namespace WeirdEngine
{

	void Scene::handleCollision(CollisionEvent &event, void *userData)
	{
		// Unsafe cast! Prone to error.
		Scene *self = static_cast<Scene *>(userData);
		self->onCollision(event);
	}

	// vec3 g_cameraPosition(15.0f, 50.f, 60.0f);
	vec3 g_cameraPosition(0, 1, 20);

	Scene::Scene()
			: m_simulation2D(MAX_ENTITIES), m_sdfRenderSystem(m_ecs), m_sdfRenderSystem2D(m_ecs), m_renderSystem(m_ecs), m_instancedRenderSystem(m_ecs), m_rbPhysicsSystem2D(m_ecs), m_physicsInteractionSystem(m_ecs), m_playerMovementSystem(m_ecs), m_cameraSystem(m_ecs), m_runSimulationInThread(true)
	{

		// Custom component managers
		std::shared_ptr<RigidBodyManager> rbManager = std::make_shared<RigidBodyManager>(m_simulation2D);
		m_ecs.registerComponent<RigidBody2D>(rbManager);

		// Read content from file
		std::string content = "";

		// Read scene file and load everything
		loadScene(content);

		// Initialize simulation
		m_rbPhysicsSystem2D.init(m_ecs, m_simulation2D);

		// Start simulation if different thread
		if (m_runSimulationInThread)
		{
			m_simulation2D.startSimulationThread();
		}

		m_simulation2D.setCollisionCallback(&handleCollision, this);
	}

	Scene::~Scene()
	{
		m_simulation2D.stopSimulationThread();

		// TODO: Free resources from all entities
		m_resourceManager.freeResources(0);
	}

	void Scene::start()
	{
		onCreate();
		onStart();

		if (m_debugFly)
		{
			switch (m_renderMode)
			{
			case WeirdEngine::Scene::RenderMode::Simple3D:
			case WeirdEngine::Scene::RenderMode::RayMarching3D:
			case WeirdEngine::Scene::RenderMode::RayMarchingBoth:
			{
				FlyMovement &fly = m_ecs.addComponent<FlyMovement>(m_mainCamera);
				break;
			}
			case WeirdEngine::Scene::RenderMode::RayMarching2D:
			{
				FlyMovement2D &fly = m_ecs.addComponent<FlyMovement2D>(m_mainCamera);
				// fly.targetPosition = g_cameraPosition;
				break;
			}
			default:
				break;
			}
		}
	}

	//  TODO: pass render target instead of shader. Shaders should be accessed in a different way, through the resource manager
	void Scene::renderModels(WeirdRenderer::RenderTarget &renderTarget, WeirdRenderer::Shader &shader, WeirdRenderer::Shader &instancingShader)
	{
		WeirdRenderer::Camera &camera = m_ecs.getComponent<ECS::Camera>(m_mainCamera).camera;
		m_renderSystem.render(m_ecs, m_resourceManager, shader, camera, m_lights);

		onRender(renderTarget);

		// m_instancedRenderSystem.render(m_ecs, m_resourceManager, instancingShader, camera, m_lights);
	}

	void replaceSubstring(std::string &str, const std::string &from, const std::string &to)
	{
		size_t start_pos = str.find(from);
		if (start_pos != std::string::npos)
		{
			str.replace(start_pos, from.length(), to);
		}
	}

	void Scene::updateCustomShapesShader(WeirdRenderer::Shader &shader)
	{
		auto sdfBalls = m_ecs.getComponentManager<SDFRenderer>()->getComponentArray();
		int32_t atomCount = sdfBalls->getSize();
		auto componentArray = m_ecs.getComponentManager<CustomShape>()->getComponentArray();
		shader.setUniform("u_customShapeCount", componentArray->getSize());

		if (!m_sdfRenderSystem2D.shaderNeedsUpdate())
		{
			return;
		}

		m_sdfRenderSystem2D.shaderNeedsUpdate() = false;

		std::string str = shader.getFragmentCode();

		std::string toReplace("/*ADD_SHAPES_HERE*/");

		std::ostringstream oss;



		oss << "///////////////////////////////////////////\n";

		oss << "int dataOffset =  u_loadedObjects - (2 * u_customShapeCount);\n";

		int currentGroup = -1;


		for (size_t i = 0; i < componentArray->getSize(); i++)
		{
			auto &shape = componentArray->getDataAtIdx(i);

			int group = shape.m_groupId;

			if(group != currentGroup)
			{
				if(currentGroup != -1)
				{
					oss << "d = min(d" << currentGroup << ", d);\n";
				}

				oss << "float d" << group << "= 10000;\n";
				currentGroup = group;
			}

			oss << "{\n";

			oss << "int idx = dataOffset + " << 2 * i << ";\n";

			// Fetch parameters
			oss << "vec4 parameters0 = texelFetch(t_shapeBuffer, idx);\n";
			oss << "vec4 parameters1 = texelFetch(t_shapeBuffer, idx + 1);\n";

			auto fragmentCode = m_sdfs[shape.m_distanceFieldId]->print();

			if (shape.m_screenSpace)
			{
				replaceSubstring(fragmentCode, "var9", "var11");
				replaceSubstring(fragmentCode, "var10", "var12");
			}

			oss << "float dist = " << fragmentCode << ";" << std::endl;

			/*if (shape.m_screenSpace)
			{
				oss << "dist = dist * u_uiScale;" << std::endl;
			}*/

			oss << "dist = dist > 0 ? dist : 0.1 * dist;" << std::endl;

			switch (shape.m_combinationdId)
			{
			case 0: {
				oss << "d" << group << " = min(d" << group << ", dist);\n";
				break;
			}
			case 1: {
				oss << "d" << group << " = max(d" << group << ", -dist);\n";
				break;
			}
			default:
				break;
			}

			/*switch (shape.m_combinationdId)
			{
				case 0: {
					oss << "d = min(d, dist);\n";
					break;
				}
				case 1: {
					oss << "d = max(d, -dist);\n";
					break;
				}
				default:
					break;
			}*/



			// oss << "col = d == (dist) ? getMaterial(p," << (i % 12) + 4 << ") : col;\n";
			oss << "col = d" << currentGroup << " == (dist) ? getMaterial(p," << 3 << ") : col;\n";
			oss << "}\n"
					<< std::endl;
		}

		// oss << "d" << currentGroup << " = d" << currentGroup << " > 0 ? d" << currentGroup << " : 0.1 * d" << currentGroup << ";" << std::endl;

		oss << "d = min(d" << currentGroup << ", d);\n";

		std::string replacement = oss.str();

		std::cout << replacement << std::endl;

		size_t pos = str.find(toReplace);
		if (pos != std::string::npos)
		{ // Check if the substring was found
			// Replace the substring
			str.replace(pos, toReplace.length(), replacement);
		}

		shader.setFragmentCode(str);
	}

	void Scene::updateRayMarchingShader(WeirdRenderer::Shader &shader)
	{
		m_sdfRenderSystem2D.updatePalette(shader);

		updateCustomShapesShader(shader);
	}

	void Scene::get2DShapesData(WeirdRenderer::Dot2D *&data, uint32_t &size)
	{
		m_sdfRenderSystem2D.fillDataBuffer(data, size);
	}

	void Scene::update(double delta, double time)
	{
		if (Input::GetKeyDown((Input::V)))
		{
			m_sdfRenderSystem2D.shaderNeedsUpdate() = true;
		}

		// Update systems
		if (m_debugFly)
		{
			m_playerMovementSystem.update(m_ecs, delta);
		}
		// m_cameraSystem.follow(m_ecs, m_mainCamera, 10);

		m_cameraSystem.update(m_ecs);
		g_cameraPosition = m_ecs.getComponent<Transform>(m_mainCamera).position;

		m_rbPhysicsSystem2D.update(m_ecs, m_simulation2D);
		if (m_debugInput)
		{
			m_physicsInteractionSystem.update(m_ecs, m_simulation2D);
		}

		m_simulation2D.update(delta);

		onUpdate(delta);

		m_ecs.freeRemovedComponents();
	}

	WeirdRenderer::Camera &Scene::getCamera()
	{
		return m_ecs.getComponent<Camera>(m_mainCamera).camera;
	}

	std::vector<WeirdRenderer::Light> &Scene::getLigths()
	{
		return m_lights;
	}

	float Scene::getTime()
	{
		return m_simulation2D.getSimulationTime();
	}

	void Scene::fillShapeDataBuffer(WeirdRenderer::Dot2D *&data, uint32_t &size)
	{
		m_sdfRenderSystem2D.fillDataBuffer(data, size);
	}

	Scene::RenderMode Scene::getRenderMode() const
	{
		return m_renderMode;
	}

	Entity Scene::addShape(int shapeId, float* variables, int combination, bool hasCollision, int group)
	{
		Entity entity = m_ecs.createEntity();
		CustomShape &shape = m_ecs.addComponent<CustomShape>(entity);
		shape.m_distanceFieldId = shapeId;
		shape.m_combinationdId = combination;
		shape.m_hasCollision = hasCollision;
		shape.m_groupId = group;
		std::copy(variables, variables + 8, shape.m_parameters);

		// CustomShape shape(shapeId, variables); // check old constructor for references

		m_sdfRenderSystem2D.shaderNeedsUpdate() = true;

		return entity;
	}

	Entity Scene::addScreenSpaceShape(int shapeId, float *variables)
	{
		Entity entity = m_ecs.createEntity();
		CustomShape &shape = m_ecs.addComponent<CustomShape>(entity);
		shape.m_screenSpace = true;
		shape.m_distanceFieldId = shapeId;
		std::copy(variables, variables + 8, shape.m_parameters);

		// CustomShape shape(shapeId, variables); // check old constructor for references

		return entity;
	}

	void Scene::lookAt(Entity entity)
	{
		FlyMovement2D &fly = m_ecs.getComponent<FlyMovement2D>(m_mainCamera);
		Transform &target = m_ecs.getComponent<Transform>(entity);
		float oldZ = fly.targetPosition.z;

		fly.targetPosition = target.position;
		fly.targetPosition.z = oldZ;
	};

	void Scene::loadScene(std::string &sceneFileContent)
	{
		// json scene = json::parse(sceneFileContent);

		// load font
		loadFont(ENGINE_PATH "/src/weird-renderer/fonts/default.bmp", 7, 7, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,;:?!-_'#\"\\/<>() ");

		std::string projectDir = fs::current_path().string() + "/SampleProject";

		// Create camera object
		m_mainCamera = m_ecs.createEntity();

		Transform &t = m_ecs.addComponent<Transform>(m_mainCamera);
		t.position = g_cameraPosition;
		t.rotation = vec3(0, 0, -1.0f);

		ECS::Camera &c = m_ecs.addComponent<ECS::Camera>(m_mainCamera);

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

	constexpr int INVALID_INDEX = -1;

	void Scene::print(const std::string &text)
	{
		float offset = 0;
		for (auto i : text)
		{
			int idx = getIndex(i);

			// std::cout << idx << std::endl;

			for (auto vec2 : m_letters[idx])
			{
				float x = 2 + vec2.x + offset;
				float y = vec2.y;

				Entity entity = m_ecs.createEntity();
				Transform &t = m_ecs.addComponent<Transform>(entity);
				t.position = vec3(x, y, -10.0f);

				SDFRenderer &sdfRenderer = m_ecs.addComponent<SDFRenderer>(entity);
				sdfRenderer.materialId = 4 + idx % 12;
			}

			offset += m_charWidth;
		}
	}

	void Scene::loadFont(const char *imagePath, int charWidth, int charHeight, const char *characters)
	{
		// Set all to INVALID_INDEX initially
		for (int &val : m_CharLookUpTable)
		{
			val = INVALID_INDEX;
		}

		// Fill look up table
		for (size_t i = 0; characters[i] != '\0'; ++i)
		{
			m_CharLookUpTable[characters[i]] = i;
		}

		// Store char dimensions
		m_charWidth = charWidth;
		m_charHeight = charHeight;

		// Load the image
		int width, height, channels;
		unsigned char *img = wstbi_load(imagePath, &width, &height, &channels, 0);

		if (img == nullptr)
		{
			std::cerr << "Error: could not load image." << std::endl;
			return;
		}

		int columns = width / charWidth;
		int rows = height / charHeight;

		int charCount = columns * rows;

		m_letters.clear();
		m_letters.resize(charCount);

		for (size_t i = 0; i < charCount; i++)
		{
			int startX = charWidth * (i % columns);
			int startY = (charHeight * (i / columns));

			for (size_t offsetX = 0; offsetX < charWidth; offsetX++)
			{
				for (size_t offsetY = 0; offsetY < charHeight; offsetY++)
				{

					int x = startX + offsetX;
					int y = startY + offsetY;

					// Calculate the index of the pixel in the image data
					int index = (y * width + x) * channels;

					if (index < 0 || index >= width * height * channels)
					{
						continue;
					}

					// Get the color values
					unsigned char r = img[index];
					unsigned char g = img[index + 1];
					unsigned char b = img[index + 2];
					unsigned char a = (channels == 4) ? img[index + 3] : 255; // Alpha channel (if present)

					if (r < 50)
					{
						float localX = offsetX;
						float localY = charHeight - offsetY;

						m_letters[i].emplace_back(localX, localY);
					}
				}
			}
		}

		// Free the image memory
		wstbi_image_free(img);
	}
}