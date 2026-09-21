#include "ImageScene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>

#include "globals.h"
#include <glm/gtx/norm.hpp>
#include <SDL3/SDL_dialog.h> // ???

using namespace WeirdEngine;

namespace ImageSceneNamespace
{
	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "ImageScene State is missing: stateInitSystem must run first");
		return *state;
	}

	static int nearestPowerOf2(int val, int maxVal = 32)
	{
		if (val <= 0)
			return 8;

		int p = 1;
		while (p * 2 <= val && p * 2 <= maxVal)
		{
			p *= 2;
		}

		if (p * 2 <= maxVal && std::abs(val - p * 2) < std::abs(val - p))
		{
			p *= 2;
		}

		return (std::max)(8, (std::min)(p, maxVal));
	}

	static size_t calculateBallCount(int width, int height)
	{
		constexpr float PACKING_EFFICIENCY_MULTIPLIER = 1.33f;
		return static_cast<size_t>(std::round(static_cast<float>(width * height) * PACKING_EFFICIENCY_MULTIPLIER));
	}

	static MemoryImage loadAndScaleImage(const std::string& path, int maxDimension = 32)
	{
		int origW = 0, origH = 0, origChannels = 0;
		unsigned char* raw = wstbi_load(path.c_str(), &origW, &origH, &origChannels, 3);
		if (!raw)
		{
			WeirdEngine::Logger::error("Failed to load image: " + path);
			return {};
		}

		int targetW = origW;
		int targetH = origH;

		if (origW > maxDimension || origH > maxDimension)
		{
			float aspect = static_cast<float>(origW) / static_cast<float>(origH);
			if (origW >= origH)
			{
				targetW = maxDimension;
				targetH = nearestPowerOf2(static_cast<int>(std::round(static_cast<float>(maxDimension) / aspect)),
										  maxDimension);
			}
			else
			{
				targetH = maxDimension;
				targetW = nearestPowerOf2(static_cast<int>(std::round(static_cast<float>(maxDimension) * aspect)),
										  maxDimension);
			}
		}
		else
		{
			targetW = nearestPowerOf2(origW, maxDimension);
			targetH = nearestPowerOf2(origH, maxDimension);
		}

		MemoryImage result;
		result.width = targetW;
		result.height = targetH;
		result.rgb.resize(targetW * targetH * 3);

		for (int y = 0; y < targetH; ++y)
		{
			float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(targetH);
			float srcY = v * static_cast<float>(origH) - 0.5f;
			int y0 = (std::clamp)(static_cast<int>(std::floor(srcY)), 0, origH - 1);
			int y1 = (std::clamp)(y0 + 1, 0, origH - 1);
			float yFrac = srcY - std::floor(srcY);

			for (int x = 0; x < targetW; ++x)
			{
				float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(targetW);
				float srcX = u * static_cast<float>(origW) - 0.5f;
				int x0 = (std::clamp)(static_cast<int>(std::floor(srcX)), 0, origW - 1);
				int x1 = (std::clamp)(x0 + 1, 0, origW - 1);
				float xFrac = srcX - std::floor(srcX);

				int outIdx = (y * targetW + x) * 3;
				for (int c = 0; c < 3; ++c)
				{
					float c00 = static_cast<float>(raw[(y0 * origW + x0) * 3 + c]);
					float c10 = static_cast<float>(raw[(y0 * origW + x1) * 3 + c]);
					float c01 = static_cast<float>(raw[(y1 * origW + x0) * 3 + c]);
					float c11 = static_cast<float>(raw[(y1 * origW + x1) * 3 + c]);

					float top = glm::mix(c00, c10, xFrac);
					float bottom = glm::mix(c01, c11, xFrac);
					float val = glm::mix(top, bottom, yFrac);

					result.rgb[outIdx + c] = static_cast<uint8_t>((std::clamp)(val, 0.0f, 255.0f));
				}
			}
		}

		wstbi_image_free(raw);
		return result;
	}

	static int findClosestColorInPalette(const Material2D* materials, uint16_t count, const glm::vec3& color)
	{
		int closestIndex = 0;
		float minDistance = (std::numeric_limits<float>::max)();

		for (uint16_t i = 0; i < count && i < 16; ++i)
		{
			float distance = glm::length2(color - glm::vec3(materials[i].color));
			if (distance < minDistance)
			{
				minDistance = distance;
				closestIndex = static_cast<int>(i);
			}
		}

		return closestIndex;
	}

	static void updateWalls(Registry& registry, ServiceProvider& services, State& state)
	{
		const float W = static_cast<float>(state.imageWidth);
		const float H = static_cast<float>(state.imageHeight);
		const float chamberCenter = 15.0f;
		const float wallHalfW = 5.0f;
		const float wallHalfH = (std::max)(30.0f, H * 1.2f);
		const float wallCenterY = wallHalfH - 5.0f;

		const float leftWallCenterX = (chamberCenter - W * 0.5f) - wallHalfW;
		const float rightWallCenterX = (chamberCenter + W * 0.5f) + wallHalfW;

		if (registry.isEntityValid(state.leftWallEntity) && registry.hasComponent<Shape>(state.leftWallEntity))
		{
			auto& lw = registry.getComponent<Shape>(state.leftWallEntity);
			lw.parameters[Primitives::Box::POS_X] = leftWallCenterX;
			lw.parameters[Primitives::Box::POS_Y] = wallCenterY;
			lw.parameters[Primitives::Box::SIZE_X] = wallHalfW;
			lw.parameters[Primitives::Box::SIZE_Y] = wallHalfH;
			registry.setComponentDirty(lw);
		}

		if (registry.isEntityValid(state.rightWallEntity) && registry.hasComponent<Shape>(state.rightWallEntity))
		{
			auto& rw = registry.getComponent<Shape>(state.rightWallEntity);
			rw.parameters[Primitives::Box::POS_X] = rightWallCenterX;
			rw.parameters[Primitives::Box::POS_Y] = wallCenterY;
			rw.parameters[Primitives::Box::SIZE_X] = wallHalfW;
			rw.parameters[Primitives::Box::SIZE_Y] = wallHalfH;
			registry.setComponentDirty(rw);
		}

		auto& camTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		camTransform.position.x = chamberCenter;
		camTransform.position.y = (std::max)(7.5f, H * 0.45f);
		camTransform.position.z = (std::max)(35.0f, (std::max)(W, H) * 1.35f);
		registry.setComponentDirty(camTransform);
	}

	void spawnBalls(Registry& registry, State& state, bool useSavedMaterials)
	{
		const size_t ballCount = calculateBallCount(state.imageWidth, state.imageHeight);
		const float chamberCenter = 15.0f;

		while (state.balls.size() > ballCount)
		{
			Entity b = state.balls.back();
			state.balls.pop_back();
			if (registry.isEntityValid(b))
			{
				registry.destroyEntity(b);
			}
		}

		const size_t existingCount = state.balls.size();
		for (size_t i = 0; i < existingCount; ++i)
		{
			Entity entity = state.balls[i];
			float x = chamberCenter + static_cast<float>(std::sin(static_cast<float>(i)));
			float y = 10.0f + (1.0f * static_cast<float>(i));

			int material = 1;
			if (useSavedMaterials && i < state.savedMaterials.size())
			{
				material = state.savedMaterials[i];
			}

			auto& t = registry.getComponent<Transform>(entity);
			t.position = vec3(x, y, 0.0f);
			registry.setComponentDirty(t);

			auto& dot = registry.getComponent<Dot>(entity);
			dot.materialId = static_cast<unsigned int>(material);
			registry.setComponentDirty(dot);

			auto& rb = registry.getComponent<RigidBody2D>(entity);
			rb.isFixed = false;
			rb.velocity = vec2(0.0f);
			rb.pendingImpulseForce = vec2(0.0f);
			rb.pendingContinuousForce = vec2(0.0f);
			registry.setComponentDirty(rb);
		}

		for (size_t i = existingCount; i < ballCount; ++i)
		{
			float x = chamberCenter + static_cast<float>(std::sin(static_cast<float>(i)));
			float y = 10.0f + (1.0f * static_cast<float>(i));

			int material = 1;
			if (useSavedMaterials && i < state.savedMaterials.size())
			{
				material = state.savedMaterials[i];
			}

			Entity entity = registry.createEntity();
			auto& t = registry.addComponent<Transform>(entity);
			t.position = vec3(x, y, 0.0f);

			auto& dot = registry.addComponent<Dot>(entity);
			dot.materialId = static_cast<unsigned int>(material);

			auto& rb = registry.addComponent<RigidBody2D>(entity);
			rb.isFixed = false;

			state.balls.push_back(entity);
		}
	}

	static bool checkIfFileExists(const char* filename)
	{
		std::ifstream infile(filename);
		return infile.good();
	}

	static void saveToFile(const char* filename, const std::string& content)
	{
		std::ofstream out(filename);
		out << content;
		out.close();
	}

	static std::string get_file_contents(const char* filename)
	{
		std::ifstream in(filename, std::ios::in | std::ios::binary);
		if (in)
		{
			std::string contents;
			in.seekg(0, std::ios::end);
			contents.resize(in.tellg());
			in.seekg(0, std::ios::beg);
			in.read(&contents[0], contents.size());
			in.close();
			return contents;
		}
		return "";
	}

	void saveMaterials(Registry& registry, ServiceProvider& services, State& state)
	{
		if (state.balls.empty() || state.loadedImage.rgb.empty())
		{
			return;
		}

		const float W = static_cast<float>(state.imageWidth);
		const float H = static_cast<float>(state.imageHeight);
		const float chamberCenter = 15.0f;
		const float minX = chamberCenter - W * 0.5f;
		const float maxX = chamberCenter + W * 0.5f;

		float minY = (std::numeric_limits<float>::max)();
		float maxY = -(std::numeric_limits<float>::max)();

		for (Entity e : state.balls)
		{
			if (registry.isEntityValid(e) && registry.hasComponent<Transform>(e))
			{
				float py = registry.getComponent<Transform>(e).position.y;
				minY = (std::min)(minY, py);
				maxY = (std::max)(maxY, py);
			}
		}

		float spanX = maxX - minX;
		float spanY = maxY - minY;
		if (spanX < 0.001f)
			spanX = W;
		if (spanY < 0.001f)
			spanY = H;

		state.savedMaterials.resize(state.balls.size(), 0);
		std::string cacheContent;
		cacheContent.reserve(state.balls.size() * 4);

		for (size_t i = 0; i < state.balls.size(); ++i)
		{
			Entity e = state.balls[i];
			if (!registry.isEntityValid(e) || !registry.hasComponent<Transform>(e))
			{
				continue;
			}

			Transform& t = registry.getComponent<Transform>(e);

			float u = (t.position.x - minX) / spanX;
			float v = 1.0f - ((t.position.y - minY) / spanY);

			int px = (std::clamp)(static_cast<int>(std::floor(u * W)), 0, state.imageWidth - 1);
			int py = (std::clamp)(static_cast<int>(std::floor(v * H)), 0, state.imageHeight - 1);

			int pixelIdx = (py * state.imageWidth + px) * 3;
			vec3 color(static_cast<float>(state.loadedImage.rgb[pixelIdx]) / 255.0f,
					   static_cast<float>(state.loadedImage.rgb[pixelIdx + 1]) / 255.0f,
					   static_cast<float>(state.loadedImage.rgb[pixelIdx + 2]) / 255.0f);

			int matId = findClosestColorInPalette(services.materials2D().getMaterials(), 16, color);

			state.savedMaterials[i] = matId;
			cacheContent += std::to_string(matId) + "-";

			if (registry.hasComponent<Dot>(e))
			{
				auto& dot = registry.getComponent<Dot>(e);
				dot.materialId = static_cast<unsigned int>(matId);
				registry.setComponentDirty(dot);
			}
		}

		if (!std::filesystem::exists("cache/"))
		{
			std::filesystem::create_directory("cache/");
		}
		saveToFile(state.cacheFilePath.c_str(), cacheContent);
		state.hasSavedMaterials = true;
		WeirdEngine::Logger::log("Materials saved for " + std::to_string(state.balls.size()) + " balls.");
	}

	void loadImage(Registry& registry, ServiceProvider& services, State& state, const std::string& path)
	{
		MemoryImage img = loadAndScaleImage(path, 32);
		if (img.rgb.empty())
		{
			WeirdEngine::Logger::error("Failed to load: " + path);
			return;
		}

		state.currentImagePath = path;
		state.imageWidth = img.width;
		state.imageHeight = img.height;
		state.loadedImage.width = img.width;
		state.loadedImage.height = img.height;
		state.loadedImage.rgb = std::move(img.rgb);

		state.savedMaterials.clear();
		state.hasSavedMaterials = false;

		updateWalls(registry, services, state);
		spawnBalls(registry, state, false);

		WeirdEngine::Logger::log("Image loaded: " + path + " scaled to " + std::to_string(state.imageWidth) + "x" +
								 std::to_string(state.imageHeight));
	}

#ifndef __EMSCRIPTEN__
	static void SDLCALL onOpenFileCallback(void* userdata, const char* const* filelist, int filter)
	{
		auto* state = static_cast<State*>(userdata);
		if (!state || !filelist || !*filelist)
		{
			return;
		}

		std::lock_guard<std::mutex> lock(state->fileMutex);
		state->pendingLoadPath = *filelist;
	}
#endif

	void openImageFileDialog(State& state)
	{
#ifndef __EMSCRIPTEN__
		static SDL_DialogFileFilter filters[1] = {{"Image Files (*.png;*.jpg;*.jpeg;*.bmp)", "png;jpg;jpeg;bmp"}};
		SDL_ShowOpenFileDialog(onOpenFileCallback, &state, nullptr, filters, 1, nullptr, false);
#endif
	}

	void stateInitSystem(Registry& registry, ServiceProvider& services)
	{
		registry.emplaceState<State>();
	}

	void setupChamberSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		services.debug().setDebugInput(false);
		services.debug().setDebugFly(true);

		for (size_t i = 0; i < ColorPalette::Default.size() && i < 16; ++i)
		{
			auto& m = services.materials2D().get(static_cast<uint16_t>(i));
			m.color = ColorPalette::Default[i];
		}

		// Initial chamber shapes
		state.floorEntity = services.shapes().addShape({.shapeId = DefaultShapes::BOX,
														.variables = {{Primitives::Box::POS_X, 15.0f},
																	  {Primitives::Box::POS_Y, -50.0f},
																	  {Primitives::Box::SIZE_X, 2000.0f},
																	  {Primitives::Box::SIZE_Y, 50.0f}},
														.material = 3});

		state.rightWallEntity = services.shapes().addShape({.shapeId = DefaultShapes::BOX,
															.variables = {{Primitives::Box::POS_X, 36.0f},
																		  {Primitives::Box::POS_Y, 20.0f},
																		  {Primitives::Box::SIZE_X, 5.0f},
																		  {Primitives::Box::SIZE_Y, 30.0f}},
															.material = 3});

		state.leftWallEntity = services.shapes().addShape({.shapeId = DefaultShapes::BOX,
														   .variables = {{Primitives::Box::POS_X, -6.0f},
																		 {Primitives::Box::POS_Y, 20.0f},
																		 {Primitives::Box::SIZE_X, 5.0f},
																		 {Primitives::Box::SIZE_Y, 30.0f}},
														   .material = 3});
	}

	void setupControlsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		// Bottom-left UI buttons layout
		constexpr float MARGIN = 20.0f;
		constexpr float BTN_SIZE = 60.0f;
		constexpr float BTN_HALF = BTN_SIZE * 0.5f;
		constexpr float SPACING = 20.0f;

		const float winH = static_cast<float>(Display::height > 0 ? Display::height : 800);
		const float btnY = winH - MARGIN - BTN_HALF;
		const float btn1CenterX = MARGIN + BTN_HALF;
		const float btn2CenterX = MARGIN + BTN_SIZE + SPACING + BTN_HALF;
		const float btn3CenterX = MARGIN + 2.0f * (BTN_SIZE + SPACING) + BTN_HALF;

		state.lastResolutionHash = Display::width + Display::height;

		// 1. Button 1: Square button to find image and load into memory
		{
			state.loadButton = services.shapes().addUIShape({.shapeId = DefaultShapes::BOX,
															 .variables = {{Primitives::Box::POS_X, btn1CenterX},
																		   {Primitives::Box::POS_Y, btnY},
																		   {Primitives::Box::SIZE_X, BTN_HALF},
																		   {Primitives::Box::SIZE_Y, BTN_HALF}},
															 .material = 6});
			auto& btn = registry.addComponent<ShapeButton>(state.loadButton);
			btn.clickPadding = 4.0f;
			btn.modifierAmount = -3.0f;
			btn.parameterModifierMask.set(2);
			btn.parameterModifierMask.set(3);
		}

		// 2. Button 2: Second button to save material for each rigidbody
		{
			state.saveButton = services.shapes().addUIShape({.shapeId = DefaultShapes::CIRCLE,
															 .variables = {{Primitives::Circle::POS_X, btn2CenterX},
																		   {Primitives::Circle::POS_Y, btnY},
																		   {Primitives::Circle::RADIUS, BTN_HALF}},
															 .material = 4});
			auto& btn = registry.addComponent<ShapeButton>(state.saveButton);
			btn.clickPadding = 4.0f;
			btn.modifierAmount = -3.0f;
			btn.parameterModifierMask.set(2);
			btn.parameterModifierMask.set(3);
		}

		// 3. Button 3: Third button (90-degree rotated triangle ▶) that reruns simulation
		{
			state.rerunButton = services.shapes().addUIShape(
				{.shapeId = DefaultShapes::TRIANGLE_ROTATED,
				 .variables = {{0, btn3CenterX}, {1, btnY}, {2, BTN_SIZE}, {3, BTN_SIZE}, {4, 1.5707963f}},
				 .material = 5});

			auto& btn = registry.addComponent<ShapeButton>(state.rerunButton);
			btn.clickPadding = 4.0f;
			btn.modifierAmount = -3.0f;
			btn.parameterModifierMask.set(2);
			btn.parameterModifierMask.set(3);
		}
	}

	void loadInitialImageSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		// Load default image
		std::string defaultPath = services.resources().assetPath("jimmy.jpg");
		loadImage(registry, services, state, defaultPath);

		// If cache exists for default image, load saved materials
		if (checkIfFileExists(state.cacheFilePath.c_str()))
		{
			std::string cache = get_file_contents(state.cacheFilePath.c_str());
			if (!cache.empty() && cache != "0")
			{
				size_t idx = 0;
				state.savedMaterials.clear();
				while (idx < cache.size())
				{
					std::string idStr;
					while (idx < cache.size() && cache[idx] != '-')
					{
						idStr += cache[idx++];
					}
					idx++;
					if (!idStr.empty())
					{
						state.savedMaterials.push_back(std::stoi(idStr));
					}
				}

				bool allZeros =
					!state.savedMaterials.empty() &&
					std::all_of(state.savedMaterials.begin(), state.savedMaterials.end(), [](int m) { return m == 0; });
				if (allZeros)
				{
					state.savedMaterials.clear();
				}
				else if (state.savedMaterials.size() == state.balls.size())
				{
					state.hasSavedMaterials = true;
					spawnBalls(registry, state, true);
				}
			}
		}
	}

	void sceneControlSystem(Registry& registry, ServiceProvider& services)
	{
		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}
	}

	void fileDialogSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		std::string loadPath;
		{
			std::lock_guard<std::mutex> lock(state.fileMutex);
			loadPath = std::exchange(state.pendingLoadPath, {});
		}

		if (!loadPath.empty())
		{
			loadImage(registry, services, state, loadPath);
		}
	}

	void buttonSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		// Button 1: Load Image (Square)
		if (registry.isEntityValid(state.loadButton) && registry.hasComponent<ShapeButton>(state.loadButton))
		{
			auto& btn = registry.getComponent<ShapeButton>(state.loadButton);
			if (btn.state == ButtonState::Down)
			{
				services.audio().playSound({0.04f, 600.0f, false, vec3(0.0f), 1});
				openImageFileDialog(state);
			}
		}

		// Button 2: Save Materials
		if (registry.isEntityValid(state.saveButton) && registry.hasComponent<ShapeButton>(state.saveButton))
		{
			auto& btn = registry.getComponent<ShapeButton>(state.saveButton);
			if (btn.state == ButtonState::Down)
			{
				services.audio().playSound({0.04f, 800.0f, false, vec3(0.0f), 1});
				saveMaterials(registry, services, state);
			}
		}

		// Button 3: Rerun Simulation (Triangle ▶)
		if (registry.isEntityValid(state.rerunButton) && registry.hasComponent<ShapeButton>(state.rerunButton))
		{
			auto& btn = registry.getComponent<ShapeButton>(state.rerunButton);
			if (btn.state == ButtonState::Down)
			{
				services.audio().playSound({0.04f, 1000.0f, false, vec3(0.0f), 1});
				spawnBalls(registry, state, true);
			}
		}
	}

	void shortcutSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		if (services.input().getKeyDown(Input::O))
		{
			openImageFileDialog(state);
		}
		if (services.input().getKeyDown(Input::S))
		{
			saveMaterials(registry, services, state);
		}
		if (services.input().getKeyDown(Input::R))
		{
			spawnBalls(registry, state, true);
		}
	}

	void layoutSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		int hash = Display::width + Display::height;
		if (hash != state.lastResolutionHash)
		{
			state.lastResolutionHash = hash;

			constexpr float MARGIN = 20.0f;
			constexpr float BTN_SIZE = 60.0f;
			constexpr float BTN_HALF = BTN_SIZE * 0.5f;
			const float winH = static_cast<float>(Display::height > 0 ? Display::height : 800);
			const float btnY = winH - MARGIN - BTN_HALF;

			if (registry.isEntityValid(state.loadButton) && registry.hasComponent<UIShape>(state.loadButton))
			{
				auto& ui = registry.getComponent<UIShape>(state.loadButton);
				ui.parameters[1] = btnY;
				registry.setComponentDirty(ui);
			}

			if (registry.isEntityValid(state.saveButton) && registry.hasComponent<UIShape>(state.saveButton))
			{
				auto& ui = registry.getComponent<UIShape>(state.saveButton);
				ui.parameters[1] = btnY;
				registry.setComponentDirty(ui);
			}

			if (registry.isEntityValid(state.rerunButton) && registry.hasComponent<UIShape>(state.rerunButton))
			{
				auto& ui = registry.getComponent<UIShape>(state.rerunButton);
				ui.parameters[1] = btnY;
				registry.setComponentDirty(ui);
			}
		}
	}

	void cameraTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		g_cameraPositon = registry.getComponent<WeirdEngine::Transform>(services.render().getCameraEntity()).position;
	}

	void cameraInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		registry.getComponent<WeirdEngine::Transform>(services.render().getCameraEntity()).position = g_cameraPositon;
	}
} // namespace ImageSceneNamespace