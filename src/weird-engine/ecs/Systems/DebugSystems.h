#pragma once
#pragma once
#include "../ECS.h"
#include "../../Input.h"
#include "../../../weird-physics/Simulation2D.h"


#define SCENE 1
class SimulatedImageGenerator : public System
{
private:

	int seconds;

	bool done = false;
	std::string binaryString;
	std::string filePath = "cache/image.txt";
	//std::string imagePath = "SampleProject/Resources/Textures/sample.png";
	std::string imagePath = "SampleProject/Resources/Textures/image.jpg";

public:

	SimulatedImageGenerator() :
#if (SCENE == 0)
		seconds(5)
#elif SCENE == 1
		seconds(40)
#endif
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
		if (simulation.getSimulationTime() > seconds && !done)
		{
			done = true;

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





	void SpawnEntities(ECSManager& ecs, PhysicsSystem2D& physicsSystem, SDFRenderSystem2D sdfRenderSystem, size_t circles)
	{
		uint32_t currentChar = 0;

		// Spawn 2d balls
		for (size_t i = 0; i < circles; i++)
		{
#if (SCENE == 0)
			float x = i % 30;
			float y = (int)(i / 30) + 1;
#elif SCENE == 1
			float x = 15 + sin(i);
			float y = 10 + (2 * i);
#endif
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

			ecs.addComponent(entity, SDFRenderer(material));
			sdfRenderSystem.add(entity);

			ecs.addComponent(entity, RigidBody2D());
			//physicsSystem.add(entity);
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