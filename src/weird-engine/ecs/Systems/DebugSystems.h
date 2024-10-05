#pragma once
#pragma once
#include "../ECS.h"
#include "../../Input.h"
#include "../../../weird-physics/Simulation2D.h"



class ProceduralSpawnerSystem : public System
{
private:
	std::string binaryString;
	std::string filePath = "cache/image.txt";
	//std::string imagePath = "SampleProject/Resources/Textures/sample.png";
	std::string imagePath = "SampleProject/Resources/Textures/image.jpg";

	uint32_t m_scene;

public:

	ProceduralSpawnerSystem()
	{
		if (checkIfFileExists(filePath.c_str()))
		{
			binaryString = get_file_contents(filePath.c_str());
		}
		else
		{
			binaryString = "0";
		}
	}

	void update(ECSManager& ecs, Simulation2D& simulation, SDFRenderSystem2D sdfRenderSystem)
	{
		// Get colors
		if (Input::GetKeyDown(Input::P))
		{
			auto components = ecs.getComponentArray<RigidBody2D>();

			// Result string
			std::string result;
			result.reserve(components->getSize());
			for (size_t i = 0; i < components->getSize(); i++)
			{
				RigidBody2D& rb = components->getData(i);
				Transform& t = ecs.getComponent<Transform>(rb.Owner);

				int x = floor(t.position.x);
				int y = floor(30 - t.position.y);

				vec3 color = getColor(imagePath.c_str(), x * 10, y * 10);
				int id = sdfRenderSystem.findClosestColorInPalette(color);

				result += std::to_string(id) + "-";
			}

			saveToFile(filePath.c_str(), result);
			std::cout << "Image saved" << std::endl;
		}
	}





	void SpawnEntities(ECSManager& ecs, PhysicsSystem2D& physicsSystem, SDFRenderSystem2D sdfRenderSystem, size_t circles, uint32_t scene)
	{
		uint32_t currentChar = 0;

		// Spawn 2d balls
		for (size_t i = 0; i < circles; i++)
		{
			float x;
			float y;

			switch (scene)
			{
			case 0:
			{
				x = i % 30;
				y = (int)(i / 30) + 1;
				break;
			}
			case 1:
			{
				x = 15 + sin(i);
				y = 10 + (3 * i);
				break;
			}
			case 2:
			{
				y = (int)(i / 20);
				x =  5 + (i % 20) + sin(y);


				break;
			}
			default:
				break;
			}

			float z = 0;

			Transform t;
			t.position = vec3(x + 0.5f, y + 0.5f, z);

			Entity entity = ecs.createEntity();
			ecs.addComponent(entity, t);

			std::string materialId;
			while (currentChar < binaryString.size() && binaryString[currentChar] != '-')
			{
				materialId += binaryString[currentChar++];
			}
			currentChar++;

			int material = (materialId.size() > 0 && materialId.size() <= 2) ? std::stoi(materialId) : 0;

			ecs.addComponent(entity, SDFRenderer(i % 16));
			sdfRenderSystem.add(entity);

			ecs.addComponent(entity, RigidBody2D());
			//physicsSystem.add(entity);
		}
	}

	double m_lastSpawnTime = 0.f;
	void throwBalls(ECSManager& m_ecs, Simulation2D& m_simulation2D, PhysicsSystem2D& m_rbPhysicsSystem2D, SDFRenderSystem2D& m_sdfRenderSystem2D)
	{
		if (Input::GetKey(Input::E) && m_simulation2D.getSimulationTime() > m_lastSpawnTime + 0.1)
		{
			int amount = 3;
			for (size_t i = 0; i < amount; i++)
			{
				float x = 0.f;
				float y = 60 + (1.2 * i);
				float z = 0;

				Transform t;
				t.position = vec3(x + 0.5f, y + 0.5f, z);
				Entity entity = m_ecs.createEntity();
				m_ecs.addComponent(entity, t);

				m_ecs.addComponent(entity, SDFRenderer(4 + (m_sdfRenderSystem2D.getEntityCount() % 12)));

				m_sdfRenderSystem2D.add(entity);

				m_ecs.addComponent(entity, RigidBody2D());
				m_rbPhysicsSystem2D.add(entity);
				m_rbPhysicsSystem2D.addNewRigidbodiesToSimulation(m_ecs, m_simulation2D);
				m_rbPhysicsSystem2D.addForce(m_ecs, m_simulation2D, entity, vec2(20, 0));
			}

			m_lastSpawnTime = m_simulation2D.getSimulationTime();
		}
	}

private:

	vec3 getColor(const char* path, int x, int y)
	{
		// Load the image		  
		int width, height, channels;
		unsigned char* img = stbi_load(path, &width, &height, &channels, 0);

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
			return vec3();

		// Get the color values
		unsigned char r = img[index];
		unsigned char g = img[index + 1];
		unsigned char b = img[index + 2];
		unsigned char a = (channels == 4) ? img[index + 3] : 255; // Alpha channel (if present)

		// Free the image memory
		stbi_image_free(img);

		return vec3(
			static_cast<int>(r) / 255.0f,
			static_cast<int>(g) / 255.0f,
			static_cast<int>(b) / 255.0f
		);
	}

};