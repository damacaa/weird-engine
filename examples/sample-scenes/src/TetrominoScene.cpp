#include "TetrominoScene.h"
#include <algorithm>

using namespace WeirdEngine;

namespace TetrominoSceneNamespace
{
	const vec2 HOLD_PREVIEW_POS = vec2(-10.5f, 5.0f);
	const vec2 NEXT_PREVIEW_POS = vec2(10.5f, 5.0f);

	struct BlockOffset
	{
		int dx;
		int dy;
	};

	constexpr BlockOffset SHAPES[7][4] = { // I (horizontal bar)
		{{-1, 0}, {0, 0}, {1, 0}, {2, 0}},
		// O (square)
		{{0, 0}, {1, 0}, {0, 1}, {1, 1}},
		// T
		{{-1, 0}, {0, 0}, {1, 0}, {0, 1}},
		// S
		{{-1, 0}, {0, 0}, {0, 1}, {1, 1}},
		// Z
		{{0, 0}, {1, 0}, {-1, 1}, {0, 1}},
		// J
		{{-1, 0}, {0, 0}, {1, 0}, {-1, 1}},
		// L
		{{-1, 0}, {0, 0}, {1, 0}, {1, 1}}};

	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "TetrominoScene State is missing: stateInitSystem must run first");
		return *state;
	}

	int nextMaterialId(State& state)
	{
		int mat = state.lastMaterial++;
		if (state.lastMaterial >= 12)
		{
			state.lastMaterial = 2;
		}
		return mat;
	}

	void updatePreview(Registry& registry, std::array<Entity, 4>& previewDots, int pieceType, int materialId,
					   vec2 centerPos)
	{
		if (pieceType < 0 || pieceType >= 7)
		{
			for (auto& entity : previewDots)
			{
				if (registry.isEntityValid(entity))
				{
					auto& transform = registry.getComponent<Transform>(entity);
					transform.position = vec3(1000.0f, 1000.0f, 0.0f);
				}
			}
			return;
		}

		int minX = 99, maxX = -99, minY = 99, maxY = -99;
		for (int i = 0; i < 4; ++i)
		{
			minX = std::min(minX, SHAPES[pieceType][i].dx);
			maxX = std::max(maxX, SHAPES[pieceType][i].dx);
			minY = std::min(minY, SHAPES[pieceType][i].dy);
			maxY = std::max(maxY, SHAPES[pieceType][i].dy);
		}
		float centerX = static_cast<float>(minX + maxX) * 0.5f;
		float centerY = static_cast<float>(minY + maxY) * 0.5f;

		for (int i = 0; i < 4; ++i)
		{
			if (!registry.isEntityValid(previewDots[i]))
			{
				previewDots[i] = registry.createEntity();
				registry.addComponent<Transform>(previewDots[i]);
				registry.addComponent<Dot>(previewDots[i]);
			}
			auto& transform = registry.getComponent<Transform>(previewDots[i]);
			auto& dot = registry.getComponent<Dot>(previewDots[i]);

			float px = centerPos.x + (static_cast<float>(SHAPES[pieceType][i].dx) - centerX);
			float py = centerPos.y + (static_cast<float>(SHAPES[pieceType][i].dy) - centerY);
			transform.position = vec3(px, py, 0.0f);
			dot.materialId = materialId;
		}
	}

	bool spawnPiece(Registry& registry, State& state, int type, int materialId)
	{
		int cx = 4;
		int cy = 18;
		state.currentPieceType = type;
		state.currentPieceMaterial = materialId;

		// Validate all positions before spawning any entities
		for (int i = 0; i < 4; ++i)
		{
			int gridX = cx + SHAPES[type][i].dx;
			int gridY = cy + SHAPES[type][i].dy;

			if (gridX < 0 || gridX >= 10 || gridY < 0 || gridY >= 20 || state.grid[gridX][gridY] != INVALID_ENTITY)
			{
				state.isGameOver = true;
				return false;
			}
		}

		for (int i = 0; i < 4; ++i)
		{
			int gridX = cx + SHAPES[type][i].dx;
			int gridY = cy + SHAPES[type][i].dy;

			Entity tetromino = registry.createEntity();
			auto& transform = registry.addComponent<Transform>(tetromino);
			vec2 worldPos = gridToWorld(gridX, gridY);
			transform.position = vec3(worldPos.x, worldPos.y, 0.0f);

			auto& dot = registry.addComponent<Dot>(tetromino);
			dot.materialId = materialId;

			state.activeDots.push_back(tetromino);
		}
		return true;
	}

	void updateGhostPreview(Registry& registry, State& state)
	{
		if (state.activeDots.size() < 4 || state.isGameOver)
		{
			for (auto& entity : state.ghostPreviewDots)
			{
				if (registry.isEntityValid(entity))
				{
					auto& transform = registry.getComponent<Transform>(entity);
					transform.position = vec3(1000.0f, 1000.0f, 0.0f);
				}
			}
			return;
		}

		int dropDistance = 0;
		while (true)
		{
			bool canDropFurther = true;
			for (auto& dotEntity : state.activeDots)
			{
				auto& transform = registry.getComponent<Transform>(dotEntity);
				glm::ivec2 gridPos = worldToGrid(transform.position);
				int testY = gridPos.y - (dropDistance + 1);
				if (testY < 0 ||
					(gridPos.x >= 0 && gridPos.x < 10 && testY < 20 && state.grid[gridPos.x][testY] != INVALID_ENTITY))
				{
					canDropFurther = false;
					break;
				}
			}
			if (!canDropFurther)
			{
				break;
			}
			dropDistance++;
		}

		if (dropDistance == 0)
		{
			for (auto& entity : state.ghostPreviewDots)
			{
				if (registry.isEntityValid(entity))
				{
					auto& transform = registry.getComponent<Transform>(entity);
					transform.position = vec3(1000.0f, 1000.0f, 0.0f);
				}
			}
			return;
		}

		for (size_t i = 0; i < state.activeDots.size() && i < 4; ++i)
		{
			if (!registry.isEntityValid(state.ghostPreviewDots[i]))
			{
				state.ghostPreviewDots[i] = registry.createEntity();
				registry.addComponent<Transform>(state.ghostPreviewDots[i]);
				auto& dot = registry.addComponent<Dot>(state.ghostPreviewDots[i]);
				dot.materialId = state.ghostMaterial;
			}

			auto& activeTransform = registry.getComponent<Transform>(state.activeDots[i]);
			glm::ivec2 gridPos = worldToGrid(activeTransform.position);
			vec2 ghostWorldPos = gridToWorld(gridPos.x, gridPos.y - dropDistance);

			auto& ghostTransform = registry.getComponent<Transform>(state.ghostPreviewDots[i]);
			ghostTransform.position = vec3(ghostWorldPos.x, ghostWorldPos.y, 0.0f);

			auto& dot = registry.getComponent<Dot>(state.ghostPreviewDots[i]);
			dot.materialId = state.ghostMaterial;
		}
	}

	void updateGameOverText(Registry& registry, State& state)
	{
		if (!registry.isEntityValid(state.gameOverText) || !registry.hasComponent<UITextRenderer>(state.gameOverText))
		{
			return;
		}

		auto& t = registry.getComponent<Transform>(state.gameOverText);
		t.position = vec3(static_cast<float>(Display::width) * 0.5f, static_cast<float>(Display::height) - 40.0f, 0.0f);

		auto& tr = registry.getComponent<UITextRenderer>(state.gameOverText);
		if (state.isGameOver)
		{
			if (tr.text != "GAME OVER")
			{
				tr.text = "GAME OVER";
				registry.setComponentDirty(tr);
			}
		}
		else
		{
			if (!tr.text.empty())
			{
				tr.text = "";
				registry.setComponentDirty(tr);
			}
		}
	}

	void stateInitSystem(Registry& registry, ServiceProvider& services)
	{
		registry.emplaceState<State>();

		auto& state = getState(registry);
		for (size_t i = 0; i < 10; i++)
		{
			for (size_t j = 0; j < 20; j++)
			{
				state.grid[i][j] = INVALID_ENTITY;
			}
		}

		services.random().setGlobalSeed(services.random().getGlobalSeed() + 1);

		state.nextPieceType = services.random().range(0, 6);
		state.nextPieceMaterial = nextMaterialId(state);
	}

	void setupEnvironmentSystem(Registry& registry, ServiceProvider& services)
	{
		services.debug().setDebugInput(false);
		services.debug().setDebugFly(false);

		auto& background = services.render().getBackground();
		background.type = BackgroundType::Grid;
		background.primaryColor = vec4(0.08f, 0.09f, 0.12f, 1.0f);
		background.secondaryColor = vec4(0.14f, 0.16f, 0.22f, 1.0f);
		background.scale = 10.0f;
		background.intensity = 1.0f;

		// 2D Camera facing the XY play area
		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		cameraTransform.position = vec3(0.0f, 0.0f, 20.0f);

		State& state = getState(registry);

		auto& wallMat = services.materials2D().createMaterial("wall");
		wallMat.color = ColorPalette::LightGray;

		for (int i = 0; i < 10; ++i)
		{
			auto& mat = services.materials2D().createMaterial("hit_" + std::to_string(i));
			mat.color = ColorPalette::Default[(4 + i) % ColorPalette::Default.size()];
			// state.hitMats.push_back(mat.id);
		}

		auto& ghostMat = services.materials2D().createMaterial("ghost");
		ghostMat.color = vec4(1.0f, 1.0f, 1.0f, 0.2f);
		state.ghostMaterial = ghostMat.id;

		auto& gameOverMat = services.materials2D().createMaterial("gameOver");
		gameOverMat.color = ColorPalette::Red;

		state.gameOverText = registry.createEntity();
		auto& textTransform = registry.addComponent<Transform>(state.gameOverText);
		textTransform.position =
			vec3(static_cast<float>(Display::width) * 0.5f, static_cast<float>(Display::height) - 40.0f, 0.0f);

		auto& tr = registry.addComponent<UITextRenderer>(state.gameOverText);
		tr.text = "";
		tr.material = gameOverMat.id;
		tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;
		tr.verticalAlignment = TextRenderer::VerticalAlignment::Top;

		// Floor
		services.shapes().addShape({.shapeId = DefaultShapes::Box,
									.variables = {{DefaultShapes::Box::PosX, 0.0f},
												  {DefaultShapes::Box::PosY, -11.0f},
												  {DefaultShapes::Box::SizeX, 6.0f},
												  {DefaultShapes::Box::SizeY, 1.0f}},
									.material = wallMat});

		// Wall right
		services.shapes().addShape({.shapeId = DefaultShapes::Box,
									.variables = {{DefaultShapes::Box::PosX, 6.0f},
												  {DefaultShapes::Box::PosY, -1.0f},
												  {DefaultShapes::Box::SizeX, 1.0f},
												  {DefaultShapes::Box::SizeY, 11.0f}},
									.material = wallMat});

		// Wall left
		services.shapes().addShape({.shapeId = DefaultShapes::Box,
									.variables = {{DefaultShapes::Box::PosX, -6.0f},
												  {DefaultShapes::Box::PosY, -1.0f},
												  {DefaultShapes::Box::SizeX, 1.0f},
												  {DefaultShapes::Box::SizeY, 11.0f}},
									.material = wallMat});

		// Hold box frame (left)
		services.shapes().addShape({.shapeId = DefaultShapes::BoxLine,
									.variables = {{DefaultShapes::BoxLine::PosX, HOLD_PREVIEW_POS.x},
												  {DefaultShapes::BoxLine::PosY, HOLD_PREVIEW_POS.y},
												  {DefaultShapes::BoxLine::SizeX, 2.5f},
												  {DefaultShapes::BoxLine::SizeY, 2.5f},
												  {DefaultShapes::BoxLine::Thickness, 0.15f}},
									.material = wallMat});

		// Next box frame (right)
		services.shapes().addShape({.shapeId = DefaultShapes::BoxLine,
									.variables = {{DefaultShapes::BoxLine::PosX, NEXT_PREVIEW_POS.x},
												  {DefaultShapes::BoxLine::PosY, NEXT_PREVIEW_POS.y},
												  {DefaultShapes::BoxLine::SizeX, 2.5f},
												  {DefaultShapes::BoxLine::SizeY, 2.5f},
												  {DefaultShapes::BoxLine::Thickness, 0.15f}},
									.material = wallMat});

		updatePreview(registry, state.nextPreviewDots, state.nextPieceType, state.nextPieceMaterial, NEXT_PREVIEW_POS);
		updatePreview(registry, state.holdPreviewDots, state.heldPieceType, state.heldPieceMaterial, HOLD_PREVIEW_POS);
	}

	bool move(TetrominoSceneNamespace::State& state, WeirdEngine::Registry& registry, int offsetX, int offsetY)
	{
		if (state.activeDots.size() > 4 && offsetX != 0)
			return false;

		bool canMove = true;
		for (auto& dotEntity : state.activeDots)
		{
			auto& transform = registry.getComponent<Transform>(dotEntity);

			glm::ivec2 gridPos = worldToGrid(transform.position);
			int newGridX = gridPos.x + offsetX;
			int newGridY = gridPos.y + offsetY;

			if (newGridX < 0 || newGridX >= 10 || newGridY < 0 || newGridY >= 20 ||
				state.grid[newGridX][newGridY] != INVALID_ENTITY)
			{
				canMove = false;
				break;
			}
		}

		if (canMove)
		{
			for (auto& dotEntity : state.activeDots)
			{
				auto& transform = registry.getComponent<Transform>(dotEntity);
				transform.position.x += offsetX;
				transform.position.y += offsetY;
			}
		}

		return canMove;
	}

	void rotate(TetrominoSceneNamespace::State& state, WeirdEngine::Registry& registry, int direction)
	{
		if (state.currentPieceType == 1 || state.activeDots.size() < 4)
		{
			return;
		}

		auto& pivotTransform = registry.getComponent<Transform>(state.activeDots[1]);
		glm::ivec2 center = worldToGrid(pivotTransform.position);

		glm::ivec2 rotatedPositions[4];
		for (size_t idx = 0; idx < state.activeDots.size(); ++idx)
		{
			auto& transform = registry.getComponent<Transform>(state.activeDots[idx]);
			glm::ivec2 gridPos = worldToGrid(transform.position);
			glm::ivec2 localPos = gridPos - center;
			glm::ivec2 rotLocal =
				(direction > 0) ? glm::ivec2(localPos.y, -localPos.x) : glm::ivec2(-localPos.y, localPos.x);
			rotatedPositions[idx] = center + rotLocal;
		}

		const glm::ivec2 kickOffsets[] = {{0, 0}, {1, 0}, {-1, 0}, {2, 0}, {-2, 0}, {0, 1}};
		for (const auto& kick : kickOffsets)
		{
			bool valid = true;
			for (size_t idx = 0; idx < state.activeDots.size(); ++idx)
			{
				int testX = rotatedPositions[idx].x + kick.x;
				int testY = rotatedPositions[idx].y + kick.y;

				if (testX < 0 || testX >= 10 || testY < 0 || testY >= 20 || state.grid[testX][testY] != INVALID_ENTITY)
				{
					valid = false;
					break;
				}
			}

			if (valid)
			{
				for (size_t idx = 0; idx < state.activeDots.size(); ++idx)
				{
					auto& transform = registry.getComponent<Transform>(state.activeDots[idx]);
					glm::ivec2 finalPos = rotatedPositions[idx] + kick;
					transform.position = vec3(gridToWorld(finalPos), 0.0f);
				}
				return;
			}
		}
	}

	void updateTetrominoSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		auto& input = services.input();

		updateGameOverText(registry, state);

		// Game over
		if (state.isGameOver)
		{
			updateGhostPreview(registry, state);
			if (input.getKeyDown(Input::Space) || input.getGamepadButtonDown(Input::GamepadButton::South) ||
				input.getGamepadButtonDown(Input::GamepadButton::Start))
			{
				services.sceneControl().goToNextScene("menu");
			}
			return;
		}

		// Save / Restore (Hold)
		bool holdTriggered = input.getKeyDown(Input::C) || input.getKeyDown(Input::LeftShift) ||
							 input.getKeyDown(Input::H) || input.getKeyDown(Input::S) ||
							 input.getGamepadButtonDown(Input::GamepadButton::LeftShoulder) ||
							 input.getGamepadButtonDown(Input::GamepadButton::RightShoulder);
		if (holdTriggered)
		{
			if (state.canHold && !state.activeDots.empty() && !state.isGameOver)
			{
				state.canHold = false;

				for (auto& dotEntity : state.activeDots)
				{
					if (registry.isEntityValid(dotEntity))
					{
						registry.destroyEntity(dotEntity);
					}
				}
				state.activeDots.clear();

				int prevHeldType = state.heldPieceType;
				int prevHeldMat = state.heldPieceMaterial;

				state.heldPieceType = state.currentPieceType;
				state.heldPieceMaterial = state.currentPieceMaterial;
				updatePreview(registry, state.holdPreviewDots, state.heldPieceType, state.heldPieceMaterial,
							  HOLD_PREVIEW_POS);

				if (prevHeldType == -1)
				{
					if (state.nextPieceType < 0)
					{
						state.nextPieceType = services.random().range(0, 6);
						state.nextPieceMaterial = nextMaterialId(state);
					}

					int typeToSpawn = state.nextPieceType;
					int matToSpawn = state.nextPieceMaterial;

					state.nextPieceType = services.random().range(0, 6);
					state.nextPieceMaterial = nextMaterialId(state);
					updatePreview(registry, state.nextPreviewDots, state.nextPieceType, state.nextPieceMaterial,
								  NEXT_PREVIEW_POS);

					spawnPiece(registry, state, typeToSpawn, matToSpawn);
				}
				else
				{
					spawnPiece(registry, state, prevHeldType, prevHeldMat);
				}

				state.timeSinceLastDrop = 0.0f;
				updateGhostPreview(registry, state);
				updateGameOverText(registry, state);
				return;
			}
		}

		float stickX = input.getGamepadAxis(Input::GamepadAxis::LeftX);
		bool stickLeftDown = (stickX < -0.5f && state.prevStickX >= -0.5f);
		bool stickRightDown = (stickX > 0.5f && state.prevStickX <= 0.5f);
		state.prevStickX = stickX;

		bool leftDown = input.getKeyDown(Input::Left) || input.getKeyDown(Input::A) ||
						input.getGamepadButtonDown(Input::GamepadButton::DpadLeft) || stickLeftDown;
		bool rightDown = input.getKeyDown(Input::Right) || input.getKeyDown(Input::D) ||
						 input.getGamepadButtonDown(Input::GamepadButton::DpadRight) || stickRightDown;

		if (leftDown)
		{
			move(state, registry, -1, 0);
			state.moving = true;
			state.moveDirection = -1;
			state.timeSinceLastMove = -state.moveInterval;
		}
		else if (rightDown)
		{
			move(state, registry, 1, 0);
			state.moving = true;
			state.moveDirection = 1;
			state.timeSinceLastMove = -state.moveInterval;
		}

		bool leftHeld = input.getKey(Input::Left) || input.getKey(Input::A) ||
						input.getGamepadButton(Input::GamepadButton::DpadLeft) || (stickX < -0.5f);
		bool rightHeld = input.getKey(Input::Right) || input.getKey(Input::D) ||
						 input.getGamepadButton(Input::GamepadButton::DpadRight) || (stickX > 0.5f);

		if (!leftHeld && !rightHeld)
		{
			state.moving = false;
		}
		else if (!leftHeld && rightHeld && state.moveDirection == -1)
		{
			state.moving = true;
			state.moveDirection = 1;
			state.timeSinceLastMove = -state.moveInterval;
		}
		else if (!rightHeld && leftHeld && state.moveDirection == 1)
		{
			state.moving = true;
			state.moveDirection = -1;
			state.timeSinceLastMove = -state.moveInterval;
		}

		if (state.moving)
		{
			state.timeSinceLastMove += services.time().deltaTime();

			if (state.timeSinceLastMove > state.moveInterval)
			{
				move(state, registry, state.moveDirection, 0);
				state.timeSinceLastMove = 0.0f;
			}
		}

		bool rotateCW = input.getKeyDown(Input::Up) || input.getKeyDown(Input::W) || input.getKeyDown(Input::X) ||
						input.getGamepadButtonDown(Input::GamepadButton::DpadUp) ||
						input.getGamepadButtonDown(Input::GamepadButton::South) ||
						input.getGamepadButtonDown(Input::GamepadButton::North);

		bool rotateCCW = input.getKeyDown(Input::Z) || input.getGamepadButtonDown(Input::GamepadButton::East) ||
						 input.getGamepadButtonDown(Input::GamepadButton::West);

		if (rotateCW)
		{
			rotate(state, registry, 1);
		}
		else if (rotateCCW)
		{
			rotate(state, registry, -1);
		}

		float stickY = input.getGamepadAxis(Input::GamepadAxis::LeftY);
		bool downHeld =
			input.getKey(Input::Down) || input.getGamepadButton(Input::GamepadButton::DpadDown) || (stickY > 0.5f);

		float speed = 1.0f;
		if (downHeld)
		{
			speed = 10.0f;
		}

		// Pause between drops
		state.timeSinceLastDrop += speed * services.time().deltaTime();
		if (state.timeSinceLastDrop < state.dropInterval)
		{
			updateGhostPreview(registry, state);
			return;
		}

		// New drop, reset timer
		state.timeSinceLastDrop = 0.0f;

		// Nothing active, clear full rows and spawn new
		if (state.activeDots.empty())
		{
			// Check for completed rows and collapse them in a single pass
			int writeY = 0;
			int linesCleared = 0;

			for (int y = 0; y < 20; ++y)
			{
				bool rowComplete = true;
				for (int x = 0; x < 10; ++x)
				{
					if (state.grid[x][y] == INVALID_ENTITY)
					{
						rowComplete = false;
						break;
					}
				}

				if (rowComplete)
				{
					for (int x = 0; x < 10; ++x)
					{
						Entity e = state.grid[x][y];
						state.grid[x][y] = INVALID_ENTITY;
						registry.destroyEntity(e);
					}
					linesCleared++;
				}
				else
				{
					if (linesCleared > 0)
					{
						for (int x = 0; x < 10; ++x)
						{
							Entity e = state.grid[x][y];
							state.grid[x][writeY] = e;
							state.grid[x][y] = INVALID_ENTITY;
							if (e != INVALID_ENTITY)
							{
								auto& transform = registry.getComponent<Transform>(e);
								transform.position.y -= static_cast<float>(linesCleared);
							}
						}
					}
					writeY++;
				}
			}

			// Clear vacated top rows
			for (int y = writeY; y < 20; ++y)
			{
				for (int x = 0; x < 10; ++x)
				{
					state.grid[x][y] = INVALID_ENTITY;
				}
			}

			if (linesCleared > 0)
			{
				state.score += linesCleared * 100;
			}

			// Spawn next tetromino
			if (state.nextPieceType < 0)
			{
				state.nextPieceType = services.random().range(0, 6);
				state.nextPieceMaterial = nextMaterialId(state);
			}

			int typeToSpawn = state.nextPieceType;
			int matToSpawn = state.nextPieceMaterial;

			state.nextPieceType = services.random().range(0, 6);
			state.nextPieceMaterial = nextMaterialId(state);
			updatePreview(registry, state.nextPreviewDots, state.nextPieceType, state.nextPieceMaterial,
						  NEXT_PREVIEW_POS);

			state.canHold = true;

			if (!spawnPiece(registry, state, typeToSpawn, matToSpawn))
			{
				updateGhostPreview(registry, state);
				updateGameOverText(registry, state);
				return;
			}

			updateGhostPreview(registry, state);
			return;
		}

		bool moved = move(state, registry, 0, -1);
		if (!moved)
		{
			for (auto& dotEntity : state.activeDots)
			{
				auto& transform = registry.getComponent<Transform>(dotEntity);

				glm::ivec2 gridPos = worldToGrid(transform.position);
				if (gridPos.x >= 0 && gridPos.x < 10 && gridPos.y >= 0 && gridPos.y < 20)
				{
					state.grid[gridPos.x][gridPos.y] = dotEntity;
				}
			}

			state.activeDots.clear();
		}
		else if (downHeld)
		{
			state.score += 1;
		}

		updateGhostPreview(registry, state);
		updateGameOverText(registry, state);
	}
} // namespace TetrominoSceneNamespace
