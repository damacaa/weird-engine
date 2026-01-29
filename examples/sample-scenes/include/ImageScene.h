#pragma once

#include <weird-engine.h>

#include "globals.h"

using namespace WeirdEngine;
class ImageScene : public Scene
{
public:
	ImageScene()
		: Scene() {
	};

private:
	std::string binaryString;
	std::string filePath = "cache/image.txt";
	std::string imagePath = ASSETS_PATH "jimmy.jpg";

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
			addShape(DefaultShapes::BOX, variables, 3);
		}

		// Wall right
		{
			float variables[8]{ 30 + 5, 20, 5.0f, 30.0f, 0.0f };
			addShape(DefaultShapes::BOX, variables, 3);
		}

		// Wall left
		{
			float variables[8]{ 0 - 5, 20, 5.0f, 30.0f, 0.0f };
			addShape(DefaultShapes::BOX, variables, 3);
		}

		m_ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;
	}

	vec3 getColor(const char* path, float x, float y)
	{
		y = 1.0f - y;

		// 1. Load the image to get dimensions
		int width, height, channels;
		unsigned char* img = wstbi_load(path, &width, &height, &channels, 0);

		if (img == nullptr)
		{
			std::cerr << "Error: could not load image." << std::endl;
			return vec3();
		}

		// 2. Convert Normalized UV (0.0 - 1.0) to Pixel Coordinates
		// We multiply by width/height.
		// Example: x=0.5, width=100 -> pixel=50
		int pixelX = static_cast<int>(x * width);
		int pixelY = static_cast<int>(y * height);

		// 3. Clamp values (Clamp to Edge)
		// This handles cases where x/y might be < 0.0 or >= 1.0 (like exactly 1.0)
		if (pixelX < 0)       pixelX = 0;
		if (pixelX >= width)  pixelX = width - 1;

		if (pixelY < 0)       pixelY = 0;
		if (pixelY >= height) pixelY = height - 1;

		// 4. Calculate index
		int index = (pixelY * width + pixelX) * channels;

		// Safety check (though clamping above should prevent this)
		if (index < 0 || index >= width * height * channels)
		{
			wstbi_image_free(img);
			return vec3();
		}

		// 5. Get color values
		unsigned char r = img[index];
		unsigned char g = img[index + 1];
		unsigned char b = img[index + 2];
		// Alpha is available at index+3 if channels==4, but vec3 usually ignores it.

		// 6. Free memory
		wstbi_image_free(img);

		// 7. Return normalized color
		return vec3(
			static_cast<int>(r) / 255.0f,
			static_cast<int>(g) / 255.0f,
			static_cast<int>(b) / 255.0f
		);
	}

	void onUpdate(float delta) override
	{
		if (Input::GetKeyDown(Input::Q))
		{
			setSceneComplete();
		}
		
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

				vec2 uv = vec2(x, y) / 30.0f;

				vec3 color = getColor(imagePath.c_str(), uv.x, uv.y);
				int id = m_sdfRenderSystem2D.findClosestColorInPalette(color);

				result += std::to_string(id) + "-";
			}

			saveToFile(filePath.c_str(), result);
			std::cout << "Image saved" << std::endl;
		}
	}
};
