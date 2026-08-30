#pragma once

#include "weird-renderer/audio/AudioPresets.h"
#include <weird-engine.h>

#include "globals.h"
#include "weird-engine/math/Default2DSDFs.h"
#include <glm/gtx/norm.hpp>

using namespace WeirdEngine;
class ImageScene : public Scene2D
{
public:
	ImageScene() {};

private:
	std::string binaryString;
	std::string filePath = "cache/image.txt";
	std::string imagePath;

	// Inherited via Scene
	void onStart(Registry& registry, ServiceProvider& services) override
	{
		services.debug().setDebugInput(true);
		services.debug().setDebugFly(true);

		// Initialize audio module with image preset
		auto& audioModule = services.audio().getAudioModule();
		if (audioModule)
		{
			WeirdEngine::WeirdRenderer::setAudioModuleFromPreset(audioModule, "image");
		}

		imagePath = services.resources().assetPath("jimmy.jpg");

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

			x = 15.0f + static_cast<float>(sin(i));
			y = 10 + (1.0f * i);

			std::string materialId;
			while (currentChar < binaryString.size() && binaryString[currentChar] != '-')
			{
				materialId += binaryString[currentChar++];
			}
			currentChar++;

			material = (materialId.size() > 0 && materialId.size() <= 2) ? std::stoi(materialId) : 0;

			Entity entity = registry.createEntity();
			Transform& t = registry.addComponent<Transform>(entity);
			t.position = vec3(x + 0.5f, y + 0.5f, 0);

			Dot& dot = registry.addComponent<Dot>(entity);
			dot.materialId = material;

			RigidBody2D& rb = registry.addComponent<RigidBody2D>(entity);
		}

		for (size_t i = 0; i < ColorPalette::Default.size() && i < 16; ++i)
		{
			auto& m = services.materials2D().get(static_cast<uint16_t>(i));
			m.color = ColorPalette::Default[i];
		}

		auto& wallMat = services.materials2D().createMaterial("wall");
		wallMat.color = ColorPalette::LightGray;

		// Floor
		services.shapes().addShape({.shapeId = DefaultShapes::BOX,
									.variables = {{Primitives::Box::POS_X, 15.0f},
												  {Primitives::Box::POS_Y, -5.0f},
												  {Primitives::Box::SIZE_X, 25.0f},
												  {Primitives::Box::SIZE_Y, 5.0f}},
									.material = wallMat});

		// Wall right
		services.shapes().addShape({.shapeId = DefaultShapes::BOX,
									.variables = {{Primitives::Box::POS_X, 35.0f},
												  {Primitives::Box::POS_Y, 20.0f},
												  {Primitives::Box::SIZE_X, 5.0f},
												  {Primitives::Box::SIZE_Y, 30.0f}},
									.material = wallMat});

		// Wall left
		services.shapes().addShape({.shapeId = DefaultShapes::BOX,
									.variables = {{Primitives::Box::POS_X, -5.0f},
												  {Primitives::Box::POS_Y, 20.0f},
												  {Primitives::Box::SIZE_X, 5.0f},
												  {Primitives::Box::SIZE_Y, 30.0f}},
									.material = wallMat});

		registry.getComponent<Transform>(services.render().getCameraEntity()).position = g_cameraPositon;
	}

	vec3 getColor(const char* path, float x, float y)
	{
		y = 1.0f - y;

		// 1. Load the image to get dimensions
		int width, height, channels;
		unsigned char* img = wstbi_load(path, &width, &height, &channels, 0);

		if (img == nullptr)
		{
			WeirdEngine::Logger::error("Error: could not load image.");
			return vec3();
		}

		// 2. Convert Normalized UV (0.0 - 1.0) to Pixel Coordinates
		// We multiply by width/height.
		// Example: x=0.5, width=100 -> pixel=50
		int pixelX = static_cast<int>(x * width);
		int pixelY = static_cast<int>(y * height);

		// 3. Clamp values (Clamp to Edge)
		// This handles cases where x/y might be < 0.0 or >= 1.0 (like exactly 1.0)
		if (pixelX < 0)
			pixelX = 0;
		if (pixelX >= width)
			pixelX = width - 1;

		if (pixelY < 0)
			pixelY = 0;
		if (pixelY >= height)
			pixelY = height - 1;

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
		return vec3(static_cast<int>(r) / 255.0f, static_cast<int>(g) / 255.0f, static_cast<int>(b) / 255.0f);
	}

	// Function to find the closest color in the palette
	inline int findClosestColorInPalette(const Material2D* materials, uint16_t count, const glm::vec3& color)
	{
		int closestIndex = 0;
		float minDistance = std::numeric_limits<float>::max(); // start with maximum possible distance

		for (uint16_t i = 0; i < count && i < 16; ++i)
		{
			// Use length2 for efficiency (avoids computing square root)
			float distance = glm::length2(color - glm::vec3(materials[i].color));

			if (distance < minDistance)
			{
				minDistance = distance;
				closestIndex = static_cast<int>(i);
			}
		}

		return closestIndex;
	}

	void onUpdate(Registry& registry, ServiceProvider& services) override
	{
		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}

		// Get colors
		if (services.input().getKeyDown(Input::P))
		{
			auto components = registry.getComponentArray<RigidBody2D>();

			// Result string
			std::string result;
			result.reserve(components->getSize());
			for (size_t i = 0; i < components->getSize(); i++)
			{
				RigidBody2D& rb = components->getDataAtIdx(i);
				Entity rbOwner = components->getEntityAtIdx(i);
				Transform& t = registry.getComponent<Transform>(rbOwner);

				int x = static_cast<int>(floor(t.position.x));
				int y = static_cast<int>(floor(30.0f - t.position.y));

				vec2 uv = vec2(x, y) / 30.0f;

				vec3 color = getColor(imagePath.c_str(), uv.x, uv.y);

				int id = findClosestColorInPalette(services.materials2D().getMaterials(),
												   services.materials2D().getMaterialCount(), color);

				result += std::to_string(id) + "-";
			}

			saveToFile(filePath.c_str(), result);
			WeirdEngine::Logger::log("Image saved");
		}
	}
};
