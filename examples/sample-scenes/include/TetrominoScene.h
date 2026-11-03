#pragma once

#include "GlobalSystems.h"
#include <atomic>
#include <cmath>
#include <weird-engine.h>

namespace TetrominoSceneNamespace
{
	struct State
	{
		// Game state fields can be added here
		float timeSinceLastDrop = 0.0f;
		float dropInterval = 0.5f; // Time between automatic drops in seconds
		std::vector<WeirdEngine::Entity> activeDots;
		std::array<std::array<WeirdEngine::Entity, 20>, 10> grid{}; // 20 rows x 10 columns grid for the game
		bool isGameOver = false;									// Flag to indicate if the game is over
		bool isPaused = false;										// Flag to indicate if the game is paused
		std::array<int, 7> pieceMaterials{};
		int lastMaterial = 2;
		int score = 0;
		int lines = 0;
		int level = 1;
		bool moving = false;
		float timeSinceLastMove = 0.0f;
		float moveInterval = 0.15f;
		int moveDirection = 0;
		int currentPieceType = 0;
		int currentPieceMaterial = 2;
		int nextPieceType = -1;
		int nextPieceMaterial = 2;
		int heldPieceType = -1;
		int heldPieceMaterial = 0;
		bool canHold = true;
		std::array<WeirdEngine::Entity, 4> nextPreviewDots{WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY,
														   WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY};
		std::array<WeirdEngine::Entity, 4> holdPreviewDots{WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY,
														   WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY};
		int ghostMaterial = 0;
		std::array<WeirdEngine::Entity, 4> ghostPreviewDots{WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY,
															WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY};
		WeirdEngine::Entity gameOverText = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity pauseText = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity scoreText = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity linesText = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity levelText = WeirdEngine::INVALID_ENTITY;
		int scoreMaterial = 0;
		int pipeDotMaterial = 0;
		float prevStickX = 0.0f;
		float prevLeftTrigger = 0.0f;
		float prevRightTrigger = 0.0f;

		// Tetromino pipe machine state
		std::vector<WeirdEngine::Entity> pipeDots;
		WeirdEngine::Entity pipePistonEntity = WeirdEngine::INVALID_ENTITY;
		float pistonCurrentS = 10.0f;
		float pistonTargetS = 10.0f;
		std::array<glm::ivec2, 4> activeGridPos{};
		std::array<WeirdEngine::vec2, 4> spawnStartPos{};
		std::array<WeirdEngine::vec2, 4> spawnTargetPos{};
		bool isSpawningPiece = false;
		float spawnAnimTimer = 0.0f;
		float spawnAnimDuration = 0.25f;
		WeirdEngine::Entity pipeNozzleShape = WeirdEngine::INVALID_ENTITY;
	};

	inline std::atomic<bool> g_isGameOverPhysics{false};

	// Coordinate conversion helpers
	// Grid: [0..9] columns, [0..19] rows.
	// World: X in [-5.0, +5.0] (cell centers at -4.5 .. +4.5)
	//        Y in [-10.0, +10.0] (cell centers at -9.5 .. +9.5)
	inline WeirdEngine::vec2 gridToWorld(int gridX, int gridY)
	{
		return WeirdEngine::vec2(static_cast<float>(gridX) - 4.5f, static_cast<float>(gridY) - 9.5f);
	}

	inline WeirdEngine::vec2 gridToWorld(const glm::ivec2& gridPos)
	{
		return gridToWorld(gridPos.x, gridPos.y);
	}

	inline glm::ivec2 worldToGrid(float worldX, float worldY)
	{
		return glm::ivec2(static_cast<int>(std::round(worldX + 4.5f)), static_cast<int>(std::round(worldY + 9.5f)));
	}

	inline glm::ivec2 worldToGrid(const WeirdEngine::vec2& worldPos)
	{
		return worldToGrid(worldPos.x, worldPos.y);
	}

	inline glm::ivec2 worldToGrid(const WeirdEngine::vec3& worldPos)
	{
		return worldToGrid(worldPos.x, worldPos.y);
	}

	State& getState(WeirdEngine::Registry& registry);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupEnvironmentSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void updateTetrominoSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
} // namespace TetrominoSceneNamespace

class TetrominoScene : public WeirdEngine::Scene2D
{
public:
	TetrominoScene()
	{
		addStartSystem(TetrominoSceneNamespace::stateInitSystem);
		addStartSystem(TetrominoSceneNamespace::setupEnvironmentSystem);

		addUpdateSystem(GlobalSystems::sceneControlSystem);
		addUpdateSystem(TetrominoSceneNamespace::updateTetrominoSystem);
	}

protected:
	void onPhysicsShapeCollision(WeirdEngine::Simulation2D& simulation,
								 WeirdEngine::PhysicsShapeCollisionEvent& event) override
	{
		if (!TetrominoSceneNamespace::g_isGameOverPhysics.load(std::memory_order_relaxed))
		{
			event.friction = 0.0f;
			event.penetration = std::sqrt(std::max(0.0f, event.penetration));
		}
	}
};
