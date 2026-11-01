#include "TetrominoScene.h"

using namespace WeirdEngine;

namespace TetrominoSceneNamespace
{
	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "TetrominoScene State is missing: stateInitSystem must run first");
		return *state;
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

	void updateTetrominoSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		// Game over
		if (state.isGameOver)
		{
			if (services.input().getKeyDown(Input::Space))
			{
				services.sceneControl().goToNextScene("menu");
			}
			return;
		}

		if (services.input().getKeyDown(Input::Left))
		{
			move(state, registry, -1, 0);
			state.moving = true;
			state.moveDirection = -1;
			state.timeSinceLastMove = -state.moveInterval;
		}
		else if (services.input().getKeyDown(Input::Right))
		{
			move(state, registry, 1, 0);
			state.moving = true;
			state.moveDirection = 1;
			state.timeSinceLastMove = -state.moveInterval;
		}

		bool leftHeld = services.input().getKey(Input::Left);
		bool rightHeld = services.input().getKey(Input::Right);

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

		if (services.input().getKeyDown(Input::Up))
		{
			rotate(state, registry, 1);
		}

		float speed = 1.0f;
		if (services.input().getKey(Input::Down))
		{
			speed = 10.0f;
		}

		// Pause between drops
		state.timeSinceLastDrop += speed * services.time().deltaTime();
		if (state.timeSinceLastDrop < state.dropInterval)
		{
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

			// Spawn a tetromino
			int cx = 4;
			int cy = 18;
			int type = services.random().range(0, 6);
			state.currentPieceType = type;

			for (int i = 0; i < 4; ++i)
			{
				int gridX = cx + SHAPES[type][i].dx;
				int gridY = cy + SHAPES[type][i].dy;

				if (state.grid[gridX][gridY] != INVALID_ENTITY)
				{
					state.isGameOver = true;
					return;
				}

				Entity tetromino = registry.createEntity();
				auto& transform = registry.addComponent<Transform>(tetromino);
				vec2 worldPos = gridToWorld(gridX, gridY);
				transform.position = vec3(worldPos.x, worldPos.y, 0.0f);

				auto& dot = registry.addComponent<Dot>(tetromino);
				dot.materialId = state.lastMaterial;

				state.activeDots.push_back(tetromino);
			}

			state.lastMaterial = state.lastMaterial + 1;
			if (state.lastMaterial >= 12)
			{
				state.lastMaterial = 2;
			}

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
		else if (services.input().getKey(Input::Down))
		{
			state.score += 1;
		}
	}
} // namespace TetrominoSceneNamespace
