#pragma once
#pragma once
#include "../ECS.h"
#include "../../Input.h"
#include "../../../weird-physics/Simulation2D.h"


class SimulatedImageGenerator : public System
{
private:

	int seconds = 30;
	bool done = false;
	std::string binaryString;
	std::string filePath = "cache/image.txt";

public:

	SimulatedImageGenerator()
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

				vec3 color = getColor("SampleProject/Resources/Textures/sample.png", x, y);
				int id = sdfRenderSystem.findClosestColorInPalette(color);
				//std::cout << id;

				result += id;
			}

			saveToFile(filePath.c_str(), result);
			std::cout << "Image saved" << std::endl;
		}
	}

	void SpawnEntities(ECSManager& ecs, PhysicsSystem2D& physicsSystem, SDFRenderSystem2D sdfRenderSystem, size_t circles)
	{
		// Spawn 2d balls

		for (size_t i = 0; i < circles; i++)
		{
			/*float x = i % 30;
			float y = (int)(i / 30) + 20;*/
			float x = 15 + sin(i);
			float y = 10 + (2 * i);
			float z = 0;

			if (sin(i) != sin(i))
			{
				int wtf = 0;
			}

			Transform t;
			t.position = vec3(x + 0.5f, y + 0.5f, z);
			//t.isDirty = true;


			Entity entity = ecs.createEntity();
			ecs.addComponent(entity, t);


			//m_ecs.addComponent(entity, SDFRenderer(2));
			int material = binaryString[i] == '0' ? 0 : 1;

			//material = 0;

			//if (i % 2 == 0)
			ecs.addComponent(entity, SDFRenderer(material));

			sdfRenderSystem.add(entity);


			ecs.addComponent(entity, RigidBody2D());
			physicsSystem.add(entity);

		}
	}

private:

	vec3 getColor(const char* path, int x, int y) {
		// Load the image		  
		int width, height, channels;
		unsigned char* img = stbi_load(path, &width, &height, &channels, 0);

		if (img == nullptr) {
			std::cerr << "Error: could not load image." << std::endl;
			return vec3();
		}

		// Coordinates of the pixel you want to get
		//int x = 10; // x-coordinate
		//int y = 20; // y-coordinate

		// Calculate the index of the pixel in the image data
		int index = (y * width + x) * channels;

		if (index < 0 || index >= width * height * channels)
			return vec3();

		// Get the color values
		unsigned char r = img[index];
		unsigned char g = img[index + 1];
		unsigned char b = img[index + 2];
		unsigned char a = (channels == 4) ? img[index + 3] : 255; // Alpha channel (if present)

		// Output the color values
		/*std::cout << "Pixel at (" << x << ", " << y << "): "
			<< "R: " << static_cast<int>(r) << ", "
			<< "G: " << static_cast<int>(g) << ", "
			<< "B: " << static_cast<int>(b) << ", "
			<< "A: " << static_cast<int>(a) << std::endl;*/

			// Free the image memory
		stbi_image_free(img);

		return vec3(
			static_cast<int>(r),
			static_cast<int>(g),
			static_cast<int>(b)
		);
	}

};