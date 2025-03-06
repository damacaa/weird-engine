
#include <iostream>

#include "weird-engine.h"

using namespace WeirdEngine;





class TextScene : public Scene
{
private:

	

	// Inherited via Scene
	void onStart() override
	{
		std::string example("Hello World!");

		print(example);
	}
	void onUpdate() override
	{
	}
	void onRender() override
	{
	}
};

class RenderOrderScene : public Scene
{
private:
	// Inherited via Scene
	void onStart() override
	{
		for (size_t i = 0; i < 10; i++)
		{
			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(15.0f + (0.25f * i), 30.0f, i);

			SDFRenderer& sdfRenderer = m_ecs.addComponent<SDFRenderer>(entity);
			sdfRenderer.materialId = 4 + m_ecs.getComponentArray<SDFRenderer>()->getSize() % 12;

			/*RigidBody2D& rb = m_ecs.addComponent<RigidBody2D>(entity);
			m_simulation2D.fix(rb.simulationId);*/

			
		}

		{
			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(15.0f + (0.25f * 3), 30.25f, 1.5f);

			SDFRenderer& sdfRenderer = m_ecs.addComponent<SDFRenderer>(entity);
			sdfRenderer.materialId = 4 + m_ecs.getComponentArray<SDFRenderer>()->getSize() % 12;
		}
	}
	void onUpdate() override
	{
	}
	void onRender() override
	{
	}
};


class DestroyScene : public Scene
{
public:
	DestroyScene()
		: Scene() {};

private:

	std::array<Entity, 10> m_testEntity;
	bool m_testEntityCreated = false;

	float m_lastSpawnTime = 0.0f;

	void onUpdate() override
	{
		if (m_testEntityCreated && (getTime() - m_lastSpawnTime) > 0.5f && Input::GetKeyDown(Input::U))
		{
			for (size_t i = 0; i < m_testEntity.size(); i++)
			{
				m_ecs.destroyEntity(m_testEntity[i]);
			}

			m_testEntityCreated = false;
		}
		else if (!m_testEntityCreated)
		{
			for (size_t i = 0; i < m_testEntity.size(); i++)
			{
				Entity entity = m_ecs.createEntity();
				Transform& t = m_ecs.addComponent<Transform>(entity);
				t.position = vec3(15.0f + (i % 10), 30.0f + (i / 10), 0.0f);
				t.isDirty = true;

				SDFRenderer& sdfRenderer = m_ecs.addComponent<SDFRenderer>(entity);
				sdfRenderer.materialId = 4 + m_ecs.getComponentArray<SDFRenderer>()->getSize() % 12;

				RigidBody2D& rb = m_ecs.addComponent<RigidBody2D>(entity);
				m_simulation2D.addForce(rb.simulationId, vec2(0, -50));

				m_testEntity[i] = entity;
				m_testEntityCreated = true;
			}


			m_lastSpawnTime = getTime();
		}

		if (m_testEntityCreated && Input::GetKeyDown(Input::I))
		{
			for (size_t i = 0; i < m_testEntity.size(); i++)
			{
				RigidBody2D& rb = m_ecs.getComponent<RigidBody2D>(m_testEntity[i]);

				if (i > 0)
				{
					m_simulation2D.addSpring(rb.simulationId, rb.simulationId - 1, 1000000.0f);
				}
				else
				{
					m_simulation2D.setPosition(rb.simulationId, vec2(0, 15));
					m_simulation2D.fix(rb.simulationId);
				}
			}
		}


	}

	// Inherited via Scene
	void onStart() override
	{
		{
			float variables[8]{ 0.0f, 0.0f };
			addShape(0, variables);
		}
	}
	void onRender() override
	{
	}
};

class RopeScene : public Scene
{
public:
	RopeScene()
		: Scene() {};

private:
	Entity m_star;
	double m_lastSpawnTime = 0.f;

	// Inherited via Scene
	void onStart() override
	{
		loadFont(ENGINE_PATH "/src/weird-renderer/fonts/small.bmp", 4, 5, "ABCDEFGHIJKLMNOPQRSTUVWXYZ[]{}abcdefghijklmnopqrstuvwxyz\\/<>0123456789!\" ");

		std::string s("Nice rope dude!");
		print(s);

		constexpr int circles = 60;

		constexpr float startY = 20 + (circles / 30);

		// Spawn 2d balls
		for (size_t i = 0; i < circles; i++)
		{
			float x = i % 30;
			float y = startY - (int)(i / 30);

			int material = 4 + (i % 12);



			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(x + 0.5f, y + 0.5f, 0);

			SDFRenderer& sdfRenderer = m_ecs.addComponent<SDFRenderer>(entity);
			sdfRenderer.materialId = material;

			RigidBody2D& rb = m_ecs.addComponent<RigidBody2D>(entity);
		}

		constexpr float stiffness = 20000000.0f;

		for (size_t i = 0; i < circles; i++)
		{
			// Check it's not last row
			if (i + 30 < circles)
			{
				// Connect down
				m_simulation2D.addSpring(i, i + 30, stiffness);
			}

			// Last column
			if ((i + 1) % 30 == 0)
			{
				continue;
			}
			m_simulation2D.addSpring(i, i + 1, stiffness);

			/*if (i + 30 < circles)
			{
				simulation.addSpring(i, i + 31, stiffness, 1.42f);
				simulation.addSpring(i + 1, i + 30, stiffness, 1.42f);
			}*/
		}

		if (circles >= 30)
		{
			m_simulation2D.fix(0);
			m_simulation2D.fix(29);
		}

		// Add shapes

		{
			float variables[8]{ 1.0f, 0.5f };
			addShape(0, variables);
		}

		{
			float variables[8]{ 25.0f, 10.0f, 5.0f, 0.5f, 13.0f, 5.0f };
			m_star = addShape(1, variables);
		}
	}

	void throwBalls(ECSManager& ecs, Simulation2D& simulation2D)
	{
		if (simulation2D.getSimulationTime() > m_lastSpawnTime + 0.1)
		{
			int amount = 10;
			for (size_t i = 0; i < amount; i++)
			{
				float x = 0.f;
				float y = 60 + (1.2 * i);
				float z = 0;


				Entity entity = ecs.createEntity();
				Transform& t = ecs.addComponent<Transform>(entity);
				t.position = vec3(x + 0.5f, y + 0.5f, z);

				SDFRenderer& sdfRenderer = ecs.addComponent<SDFRenderer>(entity);
				sdfRenderer.materialId = 4 + ecs.getComponentArray<SDFRenderer>()->getSize() % 12;

				RigidBody2D& rb = ecs.addComponent<RigidBody2D>(entity);
				simulation2D.addForce(rb.simulationId, vec2(20, 0));

				// std::cout << rb.simulationId << std::endl;
			}

			m_lastSpawnTime = simulation2D.getSimulationTime();
		}
	}



	void onUpdate() override
	{
		// Change shape
		{
			CustomShape& cs = m_ecs.getComponent<CustomShape>(m_star);
			cs.m_parameters[4] = (static_cast<int>(std::floor(m_simulation2D.getSimulationTime())) % 5) + 2;
			cs.m_parameters[3] = sin(3.1416 * m_simulation2D.getSimulationTime());
			cs.m_isDirty = true;
		}

		// Add balls
		if (Input::GetKey(Input::E))
		{
			throwBalls(m_ecs, m_simulation2D);
		}

		if (Input::GetKeyDown(Input::M))
		{

			// Get mouse coordinates
			auto& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);
			float x = Input::GetMouseX();
			float y = Input::GetMouseY();

			// Transform mouse coordinates to world space
			vec2 mousePositionInWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));



			float variables[8]{ mousePositionInWorld.x, mousePositionInWorld.y, 5.0f, 0.5f, 13.0f, 5.0f };
			addShape(1, variables);


		}

		if (Input::GetKeyDown(Input::N))
		{
			// Test
			{
				m_sdfs.push_back(m_sdfs[m_sdfs.size() - 1]);
			}

			m_simulation2D.setSDFs(m_sdfs);

			auto& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);
			float x = Input::GetMouseX();
			float y = Input::GetMouseY();

			// Transform mouse coordinates to world space
			vec2 mousePositionInWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

			float variables[8]{ mousePositionInWorld.x, mousePositionInWorld.y, 5.0f, 7.5f, 1.0f };

			addShape(m_sdfs.size() - 1, variables);

			//newShapeAdded = true;
		}

		if (Input::GetKeyDown(Input::K))
		{
			auto components = m_ecs.getComponentArray<CustomShape>();
			auto id = components->getSize() - 1;

			m_simulation2D.removeShape(components->getDataAtIdx(id));
			m_ecs.destroyEntity(components->getDataAtIdx(id).Owner);

			//newShapeAdded = true;
		}
	}

	void onRender() override
	{
	}
};

class MouseCollisionScene : public Scene
{
public:
	MouseCollisionScene()
		: Scene() {};

private:
	Entity m_cursorShape;

	// Inherited via Scene
	void onStart() override
	{
		for (size_t i = 0; i < 600; i++)
		{

			float y = (int)(i / 20);
			float x = 5 + (i % 20) + sin(y);

			int material = 4 + (i % 12);

			float z = 0;



			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(x + 0.5f, y + 0.5f, z);

			if (i < 100)
			{
			}

			SDFRenderer& sdfRenderer = m_ecs.addComponent<SDFRenderer>(entity);
			sdfRenderer.materialId = material;

			RigidBody2D& rb = m_ecs.addComponent<RigidBody2D>(entity);
		}

		// Floor
		{
			float variables[8]{ 0.5f, 1.5f, -1.0f };
			addShape(0, variables);
		}

		{
			float variables[8]{ -15.0f, 50.0f, 5.0f, 5.0f, 2.0f, 10.0f };
			Entity star = addShape(1, variables);

			m_cursorShape = star;
		}
	}

	void onUpdate() override
	{
		// Move wall to mouse
		{
			CustomShape& cs = m_ecs.getComponent<CustomShape>(m_cursorShape);
			auto& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);
			float x = Input::GetMouseX();
			float y = Input::GetMouseY();

			// Transform mouse coordinates to world space
			vec2 mousePositionInWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

			cs.m_parameters[0] = mousePositionInWorld.x;
			cs.m_parameters[1] = mousePositionInWorld.y;
			cs.m_isDirty = true;
		}
	}

	void onRender() override
	{
	}
};

class ImageScene : public Scene
{
public:
	ImageScene()
		: Scene() {};

private:
	std::string binaryString;
	std::string filePath = "cache/image.txt";
	std::string imagePath = "SampleProject/Resources/Textures/image.jpg";

	// Inherited via Scene
	void onStart() override
	{
		// Check if the folder exists
		if (!std::filesystem::exists("cache/"))
		{
			// If it doesn't exist, create the folder
			std::filesystem::create_directory("cache/");
		}

		if (checkIfFileExists(filePath.c_str()))
		{
			binaryString = get_file_contents(filePath.c_str());
		}
		else
		{
			binaryString = "0";
		}

		uint32_t currentChar = 0;

		// Spawn 2d balls
		for (size_t i = 0; i < 1200; i++)
		{
			float x;
			float y;
			int material = 0;

			x = 15 + sin(i);
			y = 10 + (1.0f * i);

			std::string materialId;
			while (currentChar < binaryString.size() && binaryString[currentChar] != '-')
			{
				materialId += binaryString[currentChar++];
			}
			currentChar++;

			material = (materialId.size() > 0 && materialId.size() <= 2) ? std::stoi(materialId) : 0;



			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(x + 0.5f, y + 0.5f, 0);

			SDFRenderer& sdfRenderer = m_ecs.addComponent<SDFRenderer>(entity);
			sdfRenderer.materialId = material;

			RigidBody2D& rb = m_ecs.addComponent<RigidBody2D>(entity);
		}

		// Floor
		{
			float variables[8]{ 15, -5, 25.0f, 5.0f, 0.0f };
			addShape(m_sdfs.size() - 1, variables);
		}

		// Wall right
		{
			float variables[8]{ 30 + 5, 20, 5.0f, 30.0f, 0.0f };
			addShape(m_sdfs.size() - 1, variables);
		}

		// Wall left
		{
			float variables[8]{ 0 - 5, 20, 5.0f, 30.0f, 0.0f };
			addShape(m_sdfs.size() - 1, variables);
		}
	}

	vec3 getColor(const char* path, int x, int y)
	{
		// Load the image
		int width, height, channels;
		unsigned char* img = wstbi_load(path, &width, &height, &channels, 0);

		if (img == nullptr)
		{
			std::cerr << "Error: could not load image." << std::endl;
			return vec3();
		}

		if (x < 0)
		{
			x = 0;
		}
		else if (x >= width)
		{
			x = width - 1;
		}

		if (y < 0)
		{
			y = 0;
		}
		else if (y >= height)
		{
			y = height - 1;
		}

		// Calculate the index of the pixel in the image data
		int index = (y * width + x) * channels;

		if (index < 0 || index >= width * height * channels)
		{
			return vec3();
		}

		// Get the color values
		unsigned char r = img[index];
		unsigned char g = img[index + 1];
		unsigned char b = img[index + 2];
		unsigned char a = (channels == 4) ? img[index + 3] : 255; // Alpha channel (if present)

		// Free the image memory
		wstbi_image_free(img);

		return vec3(
			static_cast<int>(r) / 255.0f,
			static_cast<int>(g) / 255.0f,
			static_cast<int>(b) / 255.0f);
	}

	void onUpdate() override
	{
		// Get colors
		if (Input::GetKeyDown(Input::P))
		{
			auto components = m_ecs.getComponentArray<RigidBody2D>();

			// Result string
			std::string result;
			result.reserve(components->getSize());
			for (size_t i = 0; i < components->getSize(); i++)
			{
				RigidBody2D& rb = components->getDataAtIdx(i);
				Transform& t = m_ecs.getComponent<Transform>(rb.Owner);

				int x = floor(t.position.x);
				int y = floor(30 - t.position.y);

				vec3 color = getColor(imagePath.c_str(), x * 10, y * 10);
				int id = m_sdfRenderSystem2D.findClosestColorInPalette(color);

				result += std::to_string(id) + "-";
			}

			saveToFile(filePath.c_str(), result);
			std::cout << "Image saved" << std::endl;
		}
	}

	void onRender() override
	{
	}
};

#include <random>
class FireworksScene : public Scene
{
public:
	FireworksScene()
		: Scene() {};

private:
	// Inherited via Scene
	void onStart() override
	{
		// Create a random number generator engine
		std::random_device rd;
		std::mt19937 gen(rd());
		float range = 0.5f;
		std::uniform_real_distribution<> distrib(-range, range);

		for (size_t i = 0; i < 60; i++)
		{
			float y = 50 + distrib(gen);
			float x = 15 + distrib(gen);

			int material = 4 + (i % 12);

			float z = 0;



			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(x + 0.5f, y + 0.5f, z);

			if (i < 100)
			{
			}
			SDFRenderer& sdfRenderer = m_ecs.addComponent<SDFRenderer>(entity);
			sdfRenderer.materialId = material;

			RigidBody2D& rb = m_ecs.addComponent<RigidBody2D>(entity);
		}

		// Floor
		{
			float variables[8]{ 0.5f, 1.5f, -1.0f };
			addShape(0, variables);
		}
	}

	void onUpdate() override
	{
	}

	void onRender() override
	{
	}
};

class SpaceScene : public Scene
{
private:
	std::vector<Entity> m_celestialBodies;
	uint16_t m_current = 0;
	bool m_lookAtBody = true;

	// Inherited via Scene
	void onStart() override
	{
		//m_debugFly = false;

		m_simulation2D.setGravity(0);
		m_simulation2D.setDamping(0);

		loadRandomSystem();
	}

	void loadRandomSystem() {

		std::random_device rd;
		std::mt19937 gen(rd());

		std::uniform_real_distribution<float> floatDistrib(-1, 1);
		std::uniform_real_distribution<float> massDistrib(0.1f, 100.0f);
		std::uniform_int_distribution<int> colorDistrib(2, 15);

		size_t bodyCount = 1000;
		float r = 0;
		for (size_t i = 0; i < bodyCount; i++)
		{

			Entity body = m_ecs.createEntity();


			vec2 pos(floatDistrib(gen), floatDistrib(gen));
			Transform& t = m_ecs.addComponent<Transform>(body);
			t.position = 500.0f * vec3(pos.x, pos.y, 0);


			SDFRenderer& sdfRenderer = m_ecs.addComponent<SDFRenderer>(body);
			sdfRenderer.materialId = 4 + (i % 3);

			RigidBody2D& rb = m_ecs.addComponent<RigidBody2D>(body);
			m_simulation2D.setMass(rb.simulationId, 1.0f);


			for (auto b : m_celestialBodies)
			{
				m_simulation2D.addGravitationalConstraint(
					m_ecs.getComponent<RigidBody2D>(body).simulationId,
					m_ecs.getComponent<RigidBody2D>(b).simulationId,
					100.0f);
			}

			m_simulation2D.addForce(m_ecs.getComponent<RigidBody2D>(body).simulationId, (10.f * vec2(-pos.y, pos.x)) - (10.0f * vec2(pos.x, pos.y)));


			m_celestialBodies.push_back(body);
		}

		lookAt(m_celestialBodies[0]);
	}

	void loadSolarSystem()
	{
		/*Entity sun = m_ecs.createEntity();
		Entity earth = m_ecs.createEntity();
		Entity moon = m_ecs.createEntity();

		{

			t.position = vec3(15, 35, 0);
			m_ecs.addComponent(sun, t);

			m_ecs.addComponent(sun, SDFRenderer(8));

			RigidBody2D rb;
			m_simulation2D.setMass(rb.simulationId, 1000.f);
			m_ecs.addComponent(sun, rb);
		}

		{

			t.position = vec3(45, 35, 0);
			m_ecs.addComponent(earth, t);

			m_ecs.addComponent(earth, SDFRenderer(6));

			RigidBody2D rb;
			m_simulation2D.setMass(rb.simulationId, 1.0f);
			m_ecs.addComponent(earth, rb);
		}

		m_simulation2D.fix(m_ecs.getComponent<RigidBody2D>(sun).simulationId);
		m_simulation2D.addGravitationalConstraint(
			m_ecs.getComponent<RigidBody2D>(sun).simulationId,
			m_ecs.getComponent<RigidBody2D>(earth).simulationId,
			1.0f);

		m_simulation2D.addForce(m_ecs.getComponent<RigidBody2D>(earth).simulationId, vec2(0, 5));

		{

			t.position = vec3(55, 35, 0);
			m_ecs.addComponent(moon, t);

			m_ecs.addComponent(moon, SDFRenderer(2));

			RigidBody2D rb;
			m_simulation2D.setMass(rb.simulationId, 0.001f);
			m_ecs.addComponent(moon, rb);
		}

		m_simulation2D.addGravitationalConstraint(
			m_ecs.getComponent<RigidBody2D>(earth).simulationId,
			m_ecs.getComponent<RigidBody2D>(moon).simulationId,
			1000.0f);

		m_simulation2D.addForce(m_ecs.getComponent<RigidBody2D>(moon).simulationId, vec2(0, 10));

		m_celestialBodies.resize(3);
		m_celestialBodies[0] = sun;
		m_celestialBodies[1] = earth;
		m_celestialBodies[2] = moon;*/
	}



	void onUpdate() override
	{
		if (Input::GetKeyDown(Input::E)) {
			m_current = (m_current + 1) % m_celestialBodies.size();
		}

		if (Input::GetKeyDown(Input::F)) {
			m_lookAtBody = !m_lookAtBody;
		}

		if (m_lookAtBody && m_celestialBodies.size() > 0)
			lookAt(m_celestialBodies[m_current]);
	}

	void onRender() override
	{
	}
};

class ApparentCircularMotionScene : public Scene
{
private:
	// Inherited via Scene
	uint16_t m_count = 8;
	std::vector<Entity> m_circles;

	void onStart() override
	{
		m_simulation2D.setDamping(0.0f);
		m_simulation2D.setGravity(0.0f);

		for (size_t i = 0; i < m_count; i++)
		{
			Entity entity = m_ecs.createEntity();


			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(0, 0, 0);

			SDFRenderer& sdfRenderer = m_ecs.addComponent<SDFRenderer>(entity);
			sdfRenderer.materialId = i % 16;
			RigidBody2D& rb = m_ecs.addComponent<RigidBody2D>(entity);

			m_circles.push_back(entity);

		}
	}

	void onUpdate() override
	{
		auto time = getTime();

		if (time > 1)
		{
			// return;
		}

		for (size_t i = 0; i < m_count; i++)
		{
			float angle = 1 * 3.1416 * i / m_count;
			auto& t = m_ecs.getComponent<Transform>(m_circles[i]);
			t.position = (10.0f * ((float)sin(time + angle)) * vec3(cos(angle), sin(angle), 0)) + vec3(15, 15, 0);
			t.isDirty = true;
		}
	}

	void onRender() override
	{
	}
};

int main()
{

	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<TextScene>("text");
	sceneManager.registerScene<RenderOrderScene>("order");
	sceneManager.registerScene<DestroyScene>("empty");
	sceneManager.registerScene<RopeScene>("rope");
	sceneManager.registerScene<MouseCollisionScene>("cursor-collision");
	sceneManager.registerScene<ImageScene>("image");
	sceneManager.registerScene<FireworksScene>("fireworks");
	sceneManager.registerScene<ApparentCircularMotionScene>("circle");
	// sceneManager.registerScene<SpaceScene>("space");

	start(sceneManager);
}