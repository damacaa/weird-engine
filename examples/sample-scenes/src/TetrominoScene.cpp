#include "TetrominoScene.h"
#include <algorithm>

using namespace WeirdEngine;

namespace TetrominoSceneNamespace
{
	const vec2 HOLD_PREVIEW_POS = vec2(-10.5f, 5.0f);
	const vec2 NEXT_PREVIEW_POS = vec2(10.5f, 5.0f);

	// Simplified pipe geometry: starts off-screen to the left,
	// goes straight horizontally along y = 14.5 to the top center of the board,
	// turns down at the left turn right above the game area,
	// and ends at the vertical nozzle above the top center of the main hole.
	constexpr size_t NUM_RAW_PIPE_WAYPOINTS = 3;
	const std::array<vec2, NUM_RAW_PIPE_WAYPOINTS> RAW_PIPE_WAYPOINTS = {{
		vec2(-60.0f, 14.5f), // P0: Off-screen entrance to the left
		vec2(0.0f, 14.5f),	 // P1: Turn above top center of main board
		vec2(0.0f, 11.2f),	 // P2: Down to nozzle stop above main board
	}};
	const vec2 PIPE_END = RAW_PIPE_WAYPOINTS.back();
	constexpr float PIPE_WIDTH = 0.51f;
	constexpr float PIPE_CORNER_RADIUS = 1.4f;
	constexpr int PIPE_CORNER_SUBDIVS = 4;
	constexpr float PIPE_VALVE_S = 10.0f; // x = -50.0, well off-screen to the left
	constexpr float PIPE_BALL_SPACING = 0.94f;
	constexpr float PISTON_PUSH_SPEED = 11.5f;

	struct PipePathData
	{
		std::vector<vec2> points;
		std::vector<float> cumLengths;
	};

	inline const PipePathData& getPipePath()
	{
		static const PipePathData data = []()
		{
			PipePathData d;
			d.points.push_back(RAW_PIPE_WAYPOINTS.front());
			for (size_t i = 1; i + 1 < NUM_RAW_PIPE_WAYPOINTS; ++i)
			{
				vec2 prev = RAW_PIPE_WAYPOINTS[i - 1];
				vec2 curr = RAW_PIPE_WAYPOINTS[i];
				vec2 next = RAW_PIPE_WAYPOINTS[i + 1];
				vec2 dIn = glm::normalize(curr - prev);
				vec2 dOut = glm::normalize(next - curr);

				vec2 arcCenter = curr - dIn * PIPE_CORNER_RADIUS + dOut * PIPE_CORNER_RADIUS;
				vec2 u = -dOut * PIPE_CORNER_RADIUS;
				vec2 v = dIn * PIPE_CORNER_RADIUS;
				for (int k = 0; k <= PIPE_CORNER_SUBDIVS; ++k)
				{
					float theta =
						(static_cast<float>(k) / static_cast<float>(PIPE_CORNER_SUBDIVS)) * (3.14159265f * 0.5f);
					d.points.push_back(arcCenter + u * std::cos(theta) + v * std::sin(theta));
				}
			}
			d.points.push_back(RAW_PIPE_WAYPOINTS.back());

			d.cumLengths.resize(d.points.size(), 0.0f);
			for (size_t i = 0; i + 1 < d.points.size(); ++i)
			{
				d.cumLengths[i + 1] = d.cumLengths[i] + glm::length(d.points[i + 1] - d.points[i]);
			}
			return d;
		}();
		return data;
	}

	inline float getTotalPipeLength()
	{
		return getPipePath().cumLengths.back();
	}

	inline float samplePipeS(vec2 p)
	{
		const auto& path = getPipePath();
		float bestS = 0.0f;
		float bestDistSq = 1e30f;

		for (size_t i = 0; i + 1 < path.points.size(); ++i)
		{
			vec2 a = path.points[i];
			vec2 b = path.points[i + 1];
			vec2 ab = b - a;
			float segLen = path.cumLengths[i + 1] - path.cumLengths[i];
			if (segLen <= 1e-5f)
			{
				continue;
			}
			vec2 dir = ab / segLen;

			float proj = std::clamp(glm::dot(p - a, dir), 0.0f, segLen);
			vec2 closest = a + dir * proj;
			float distSq = glm::length2(p - closest);

			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				bestS = path.cumLengths[i] + proj;
			}
		}

		return bestS;
	}

	inline void pointAndTangentOnPipe(float s, vec2& outPos, vec2& outTangent)
	{
		const auto& path = getPipePath();
		float clampedS = std::clamp(s, 0.0f, path.cumLengths.back());

		size_t segIdx = 0;
		for (size_t i = 0; i + 1 < path.points.size(); ++i)
		{
			if (clampedS <= path.cumLengths[i + 1] || i + 2 == path.points.size())
			{
				segIdx = i;
				break;
			}
		}

		vec2 a = path.points[segIdx];
		vec2 b = path.points[segIdx + 1];
		float segLen = std::max(1e-5f, path.cumLengths[segIdx + 1] - path.cumLengths[segIdx]);
		outTangent = (b - a) / segLen;
		outPos = a + outTangent * (clampedS - path.cumLengths[segIdx]);
	}

	inline vec2 pointOnPipe(float s)
	{
		vec2 pos{};
		vec2 tangent{};
		pointAndTangentOnPipe(s, pos, tangent);
		return pos;
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

	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "TetrominoScene State is missing: stateInitSystem must run first");
		return *state;
	}

	int getPieceMaterial(const State& state, int pieceType)
	{
		if (pieceType >= 0 && pieceType < static_cast<int>(state.pieceMaterials.size()))
		{
			return state.pieceMaterials[pieceType];
		}
		return state.pieceMaterials[0];
	}

	int nextMaterialId(State& state)
	{
		return getPieceMaterial(state, state.nextPieceType);
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
					registry.setComponentDirty(transform);
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
				auto& rb = registry.addComponent<RigidBody2D>(previewDots[i]);
				rb.type = BodyType::Kinematic;
				rb.velocity = vec2(0.0f, 0.0f);
				rb.enableCollision = false;
				rb.enableShapeCollision = false;
			}
			auto& transform = registry.getComponent<Transform>(previewDots[i]);
			auto& dot = registry.getComponent<Dot>(previewDots[i]);

			float px = centerPos.x + (static_cast<float>(SHAPES[pieceType][i].dx) - centerX);
			float py = centerPos.y + (static_cast<float>(SHAPES[pieceType][i].dy) - centerY);
			vec3 targetPos(px, py, 0.0f);
			if (glm::length2(vec2(transform.position) - vec2(targetPos)) > 0.0001f)
			{
				transform.position = targetPos;
				registry.setComponentDirty(transform);
			}
			if (dot.materialId != static_cast<unsigned int>(materialId))
			{
				dot.materialId = materialId;
				registry.setComponentDirty(dot);
			}
		}
	}

	void spawnDotOnPipeAtS(Registry& registry, State& state, int materialId, float s)
	{
		vec2 pos = pointOnPipe(s);

		Entity dotEntity = registry.createEntity();
		auto& transform = registry.addComponent<Transform>(dotEntity);
		transform.position = vec3(pos.x, pos.y, 0.0f);

		auto& dot = registry.addComponent<Dot>(dotEntity);
		dot.materialId = materialId;

		auto& rb = registry.addComponent<RigidBody2D>(dotEntity);
		rb.type = BodyType::Dynamic;
		rb.velocity = vec2(0.0f, 0.0f);
		rb.enableCollision = true;
		rb.enableShapeCollision = true;

		state.pipeDots.push_back(dotEntity);
	}

	void sortPipeDotsByExitDistance(Registry& registry, State& state)
	{
		state.pipeDots.erase(std::remove_if(state.pipeDots.begin(), state.pipeDots.end(),
											[&](Entity e) { return !registry.isEntityValid(e); }),
							 state.pipeDots.end());

		std::sort(state.pipeDots.begin(), state.pipeDots.end(),
				  [&](Entity a, Entity b)
				  {
					  vec2 posA = vec2(registry.getComponent<Transform>(a).position);
					  vec2 posB = vec2(registry.getComponent<Transform>(b).position);
					  return samplePipeS(posA) > samplePipeS(posB);
				  });
	}

	void pushDotsIntoPipe(Registry& registry, State& state, int materialId, int count = 4)
	{
		float backBallS = PIPE_VALVE_S + 1.0f;
		if (!state.pipeDots.empty())
		{
			float minExistingS = getTotalPipeLength();
			for (Entity e : state.pipeDots)
			{
				if (registry.isEntityValid(e))
				{
					const auto& t = registry.getComponent<Transform>(e);
					minExistingS = std::min(minExistingS, samplePipeS(vec2(t.position)));
				}
			}
			backBallS = minExistingS;
		}

		for (int i = 1; i <= count; ++i)
		{
			float s = std::max(0.5f, backBallS - static_cast<float>(i) * PIPE_BALL_SPACING);
			spawnDotOnPipeAtS(registry, state, materialId, s);
		}

		// Move the kinematic stopper/piston behind the newly spawned balls so it pushes the column forward
		state.pistonCurrentS = std::max(0.1f, backBallS - static_cast<float>(count + 1) * PIPE_BALL_SPACING);
		state.pistonTargetS = PIPE_VALVE_S;
		if (registry.isEntityValid(state.pipePistonEntity))
		{
			vec2 pistonPos{};
			vec2 pistonTangent{};
			pointAndTangentOnPipe(state.pistonCurrentS, pistonPos, pistonTangent);

			auto& transform = registry.getComponent<Transform>(state.pipePistonEntity);
			transform.position = vec3(pistonPos.x, pistonPos.y, 0.0f);
			registry.setComponentDirty(transform);

			auto& rb = registry.getComponent<RigidBody2D>(state.pipePistonEntity);
			rb.velocity = pistonTangent * PISTON_PUSH_SPEED;
			registry.setComponentDirty(rb);
		}
	}

	void updatePipePiston(Registry& registry, ServiceProvider& services, State& state)
	{
		if (state.isGameOver || !registry.isEntityValid(state.pipePistonEntity))
		{
			return;
		}

		float dt = std::min(static_cast<float>(services.time().deltaTime()), 0.05f);
		vec2 pistonPos{};
		vec2 pistonTangent{};

		if (state.pistonCurrentS < state.pistonTargetS)
		{
			state.pistonCurrentS = std::min(state.pistonTargetS, state.pistonCurrentS + PISTON_PUSH_SPEED * dt);
			pointAndTangentOnPipe(state.pistonCurrentS, pistonPos, pistonTangent);

			auto& transform = registry.getComponent<Transform>(state.pipePistonEntity);
			transform.position = vec3(pistonPos.x, pistonPos.y, 0.0f);
			registry.setComponentDirty(transform);

			auto& rb = registry.getComponent<RigidBody2D>(state.pipePistonEntity);
			rb.velocity =
				(state.pistonCurrentS < state.pistonTargetS) ? (pistonTangent * PISTON_PUSH_SPEED) : vec2(0.0f);
			registry.setComponentDirty(rb);
		}
		else
		{
			pointAndTangentOnPipe(state.pistonTargetS, pistonPos, pistonTangent);
			auto& transform = registry.getComponent<Transform>(state.pipePistonEntity);
			if (glm::length2(vec2(transform.position) - pistonPos) > 0.0001f)
			{
				transform.position = vec3(pistonPos.x, pistonPos.y, 0.0f);
				registry.setComponentDirty(transform);
			}
			auto& rb = registry.getComponent<RigidBody2D>(state.pipePistonEntity);
			if (glm::length2(rb.velocity) > 0.0001f)
			{
				rb.velocity = vec2(0.0f);
				registry.setComponentDirty(rb);
			}
		}
	}

	void triggerGameOver(Registry& registry, ServiceProvider& services, State& state)
	{
		state.isGameOver = true;
		state.isPaused = false;
		state.isSpawningPiece = false;
		services.physics().resume();
		g_isGameOverPhysics.store(true, std::memory_order_relaxed);

		services.physics().setGravity(-9.8f);
		services.physics().setFriction(0.2f);

		if (registry.isEntityValid(state.pipeNozzleShape) && registry.hasComponent<Shape>(state.pipeNozzleShape))
		{
			auto& nozzle = registry.getComponent<Shape>(state.pipeNozzleShape);
			nozzle.hasCollisions = true;
			nozzle.parameters[DefaultShapes::Line::Width] = 0.65f;
			registry.setComponentDirty(nozzle);
		}

		registry.forEach<RigidBody2D>(
			[&](Entity entity, RigidBody2D& rb)
			{
				rb.type = BodyType::Dynamic;
				rb.isFixed = false;
				rb.enableCollision = true;
				rb.enableShapeCollision = true;
				rb.pendingContinuousForce = vec2(0.0f);
				if (glm::length2(rb.velocity) < 0.05f)
				{
					rb.velocity = vec2(services.random().range(-1.5f, 1.5f), services.random().range(-0.5f, 1.5f));
				}
				registry.setComponentDirty(rb);
			});
	}

	bool canPlacePieceAtSpawn(const State& state, int type)
	{
		constexpr int cx = 4;
		constexpr int cy = 18;
		for (int i = 0; i < 4; ++i)
		{
			int gridX = cx + SHAPES[type][i].dx;
			int gridY = cy + SHAPES[type][i].dy;
			if (gridX < 0 || gridX >= 10 || gridY < 0 || gridY >= 20 || state.grid[gridX][gridY] != INVALID_ENTITY)
			{
				return false;
			}
		}
		return true;
	}

	bool spawnPieceFromPipe(Registry& registry, ServiceProvider& services, State& state, int type, int materialId)
	{
		constexpr int cx = 4;
		constexpr int cy = 18;
		state.currentPieceType = type;
		state.currentPieceMaterial = materialId;

		if (state.pipeDots.size() < 4)
		{
			pushDotsIntoPipe(registry, state, state.pipeDotMaterial, 4);
		}

		sortPipeDotsByExitDistance(registry, state);

		if (!canPlacePieceAtSpawn(state, type))
		{
			triggerGameOver(registry, services, state);

			// On game over when the next piece is blocked, color the 4 dots that would have formed it
			// and shoot them down through the nozzle so they crash into the existing rigidbodies.
			for (size_t i = 0; i < 4 && i < state.pipeDots.size(); ++i)
			{
				Entity dotEntity = state.pipeDots[i];
				if (!registry.isEntityValid(dotEntity))
				{
					continue;
				}

				auto& dot = registry.getComponent<Dot>(dotEntity);
				dot.materialId = materialId;
				registry.setComponentDirty(dot);

				auto& transform = registry.getComponent<Transform>(dotEntity);
				transform.position = vec3(PIPE_END.x, PIPE_END.y + static_cast<float>(i) * 0.95f, 0.0f);
				registry.setComponentDirty(transform);

				auto& rb = registry.getComponent<RigidBody2D>(dotEntity);
				rb.type = BodyType::Dynamic;
				rb.isFixed = false;
				rb.enableCollision = true;
				rb.enableShapeCollision = true;
				rb.pendingContinuousForce = vec2(0.0f);
				float downSpeed = 28.0f + static_cast<float>(3 - i) * 2.0f;
				rb.velocity = vec2(services.random().range(-0.15f, 0.15f), -downSpeed);
				registry.setComponentDirty(rb);
			}

			if (state.pipeDots.size() >= 4)
			{
				state.pipeDots.erase(state.pipeDots.begin(), state.pipeDots.begin() + 4);
			}
			else
			{
				state.pipeDots.clear();
			}

			return false;
		}

		for (int i = 0; i < 4; ++i)
		{
			Entity dotEntity = state.pipeDots[i];
			int gridX = cx + SHAPES[type][i].dx;
			int gridY = cy + SHAPES[type][i].dy;

			auto& transform = registry.getComponent<Transform>(dotEntity);
			state.activeGridPos[i] = glm::ivec2(gridX, gridY);
			state.spawnStartPos[i] = vec2(transform.position);
			state.spawnTargetPos[i] = gridToWorld(gridX, gridY);

			auto& dot = registry.getComponent<Dot>(dotEntity);
			dot.materialId = materialId;
			registry.setComponentDirty(dot);

			auto& rb = registry.getComponent<RigidBody2D>(dotEntity);
			rb.type = BodyType::Kinematic;
			rb.velocity = vec2(0.0f, 0.0f);
			rb.enableCollision = false;
			rb.enableShapeCollision = false;
			registry.setComponentDirty(rb);

			state.activeDots.push_back(dotEntity);
		}

		state.pipeDots.erase(state.pipeDots.begin(), state.pipeDots.begin() + 4);

		// Push 4 new dynamic dots at the beginning of the pipe
		pushDotsIntoPipe(registry, state, state.pipeDotMaterial, 4);
		updatePipePiston(registry, services, state);

		state.isSpawningPiece = true;
		state.spawnAnimTimer = 0.0f;
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
			for (size_t i = 0; i < 4; ++i)
			{
				glm::ivec2 gridPos = state.activeGridPos[i];
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

			glm::ivec2 gridPos = state.activeGridPos[i];
			vec2 ghostWorldPos = gridToWorld(gridPos.x, gridPos.y - dropDistance);

			auto& ghostTransform = registry.getComponent<Transform>(state.ghostPreviewDots[i]);
			ghostTransform.position = vec3(ghostWorldPos.x, ghostWorldPos.y, 0.0f);

			auto& dot = registry.getComponent<Dot>(state.ghostPreviewDots[i]);
			dot.materialId = state.ghostMaterial;
		}
	}

	void syncKinematicPositions(Registry& registry, State& state)
	{
		if (state.isGameOver)
		{
			return;
		}

		if (!state.isSpawningPiece && state.activeDots.size() == 4)
		{
			for (size_t i = 0; i < 4; ++i)
			{
				vec2 expected = gridToWorld(state.activeGridPos[i]);
				auto& transform = registry.getComponent<Transform>(state.activeDots[i]);
				if (glm::length2(vec2(transform.position) - expected) > 0.0001f)
				{
					transform.position = vec3(expected, 0.0f);
					registry.setComponentDirty(transform);
				}
			}
		}

		for (int x = 0; x < 10; ++x)
		{
			for (int y = 0; y < 20; ++y)
			{
				Entity e = state.grid[x][y];
				if (e != INVALID_ENTITY && registry.isEntityValid(e))
				{
					vec2 expected = gridToWorld(x, y);
					auto& transform = registry.getComponent<Transform>(e);
					if (glm::length2(vec2(transform.position) - expected) > 0.0001f)
					{
						transform.position = vec3(expected, 0.0f);
						registry.setComponentDirty(transform);
					}
				}
			}
		}
	}

	void updateGameOverText(Registry& registry, State& state)
	{
		if (!registry.isEntityValid(state.gameOverText) || !registry.hasComponent<UITextRenderer>(state.gameOverText))
		{
			return;
		}

		auto& t = registry.getComponent<Transform>(state.gameOverText);
		t.position = vec3(static_cast<float>(Display::width) * 0.5f, static_cast<float>(Display::height) * 0.5f, 0.0f);

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

	void updatePauseText(Registry& registry, State& state)
	{
		if (!registry.isEntityValid(state.pauseText) || !registry.hasComponent<UITextRenderer>(state.pauseText))
		{
			return;
		}

		auto& t = registry.getComponent<Transform>(state.pauseText);
		t.position = vec3(static_cast<float>(Display::width) * 0.5f, static_cast<float>(Display::height) * 0.5f, 0.0f);

		auto& tr = registry.getComponent<UITextRenderer>(state.pauseText);
		if (state.isPaused && !state.isGameOver)
		{
			if (tr.text != "PAUSED")
			{
				tr.text = "PAUSED";
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

	void updateScoreText(Registry& registry, State& state)
	{
		float startY = static_cast<float>(Display::height) - 40.0f;
		float lineSpacing = 55.0f;
		float posX = 30.0f;

		if (registry.isEntityValid(state.scoreText) && registry.hasComponent<UITextRenderer>(state.scoreText))
		{
			auto& t = registry.getComponent<Transform>(state.scoreText);
			t.position = vec3(posX, startY, 0.0f);
			registry.setComponentDirty(t);

			auto& tr = registry.getComponent<UITextRenderer>(state.scoreText);
			std::string newText = "SCORE: " + std::to_string(state.score);
			if (tr.text != newText)
			{
				tr.text = newText;
				registry.setComponentDirty(tr);
			}
		}

		if (registry.isEntityValid(state.linesText) && registry.hasComponent<UITextRenderer>(state.linesText))
		{
			auto& t = registry.getComponent<Transform>(state.linesText);
			t.position = vec3(posX, startY - lineSpacing, 0.0f);
			registry.setComponentDirty(t);

			auto& tr = registry.getComponent<UITextRenderer>(state.linesText);
			std::string newText = "LINES: " + std::to_string(state.lines);
			if (tr.text != newText)
			{
				tr.text = newText;
				registry.setComponentDirty(tr);
			}
		}

		if (registry.isEntityValid(state.levelText) && registry.hasComponent<UITextRenderer>(state.levelText))
		{
			auto& t = registry.getComponent<Transform>(state.levelText);
			t.position = vec3(posX, startY - 2.0f * lineSpacing, 0.0f);
			registry.setComponentDirty(t);

			auto& tr = registry.getComponent<UITextRenderer>(state.levelText);
			std::string newText = "LEVEL: " + std::to_string(state.level);
			if (tr.text != newText)
			{
				tr.text = newText;
				registry.setComponentDirty(tr);
			}
		}
	}

	void stateInitSystem(Registry& registry, ServiceProvider& services)
	{
		registry.emplaceState<State>();
		g_isGameOverPhysics.store(false, std::memory_order_relaxed);

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

		state.score = 0;
		state.lines = 0;
		state.level = 1;
		state.dropInterval = 0.5f;
		state.isPaused = false;
		services.physics().resume();

		services.physics().setGravity(0.0f);
		services.physics().setDamping(0.004f);
		services.physics().setFriction(0.0f);
	}

	void setupEnvironmentSystem(Registry& registry, ServiceProvider& services)
	{
		services.debug().setDebugInput(false);
		services.debug().setDebugFly(false);

		auto& background = services.render().getBackground();
		background.type = BackgroundType::Checkers;
		background.primaryColor = ColorPalette::fromHex("#473543FF");
		background.secondaryColor = ColorPalette::fromHex("#3b1d29");
		background.scale = 10.0f;
		background.intensity = 1.0f;

		// 2D Camera facing the XY play area
		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		cameraTransform.position = vec3(0.0f, 5.0f, 20.0f);

		State& state = getState(registry);

		auto& wallMat = services.materials2D().createMaterial("wall");
		wallMat.color = ColorPalette::fromHex("#e68f51");

		for (int i = 0; i < 7; ++i)
		{
			auto& mat = services.materials2D().createMaterial("hit_" + std::to_string(i));
			mat.color = ColorPalette::Default[(7 + i) % ColorPalette::Default.size()];
			state.pieceMaterials[i] = mat.id;
		}

		auto& ghostMat = services.materials2D().createMaterial("ghost");
		ghostMat.color = vec4(1.0f, 1.0f, 1.0f, 0.2f);
		state.ghostMaterial = ghostMat.id;

		auto& gameOverMat = services.materials2D().createMaterial("gameOver");
		gameOverMat.color = ColorPalette::fromHex("#A81E00FF");

		auto& scoreMat = services.materials2D().createMaterial("score");
		scoreMat.color = ColorPalette::fromHex("#ffcea4");
		state.scoreMaterial = scoreMat.id;

		auto& pipeDotMat = services.materials2D().createMaterial("pipeDot");
		pipeDotMat.color = ColorPalette::fromHex("#ff4000");
		state.pipeDotMaterial = pipeDotMat.id;

		state.gameOverText = registry.createEntity();
		auto& textTransform = registry.addComponent<Transform>(state.gameOverText);
		textTransform.position =
			vec3(static_cast<float>(Display::width) * 0.5f, static_cast<float>(Display::height) - 40.0f, 0.0f);

		auto& tr = registry.addComponent<UITextRenderer>(state.gameOverText);
		tr.text = "";
		tr.material = gameOverMat.id;
		tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;
		tr.verticalAlignment = TextRenderer::VerticalAlignment::Top;

		state.pauseText = registry.createEntity();
		auto& pauseTransform = registry.addComponent<Transform>(state.pauseText);
		pauseTransform.position =
			vec3(static_cast<float>(Display::width) * 0.5f, static_cast<float>(Display::height) * 0.5f, 0.0f);

		auto& pauseTr = registry.addComponent<UITextRenderer>(state.pauseText);
		pauseTr.text = "";
		pauseTr.material = scoreMat.id;
		pauseTr.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;
		pauseTr.verticalAlignment = TextRenderer::VerticalAlignment::Center;

		float startY = static_cast<float>(Display::height) - 40.0f;
		float lineSpacing = 55.0f;
		float posX = 30.0f;

		state.scoreText = registry.createEntity();
		auto& scoreTransform = registry.addComponent<Transform>(state.scoreText);
		scoreTransform.position = vec3(posX, startY, 0.0f);

		auto& scoreTr = registry.addComponent<UITextRenderer>(state.scoreText);
		scoreTr.text = "SCORE: 0";
		scoreTr.material = scoreMat.id;
		scoreTr.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
		scoreTr.verticalAlignment = TextRenderer::VerticalAlignment::Center;
		registry.setComponentDirty(scoreTr);

		state.linesText = registry.createEntity();
		auto& linesTransform = registry.addComponent<Transform>(state.linesText);
		linesTransform.position = vec3(posX, startY - lineSpacing, 0.0f);

		auto& linesTr = registry.addComponent<UITextRenderer>(state.linesText);
		linesTr.text = "LINES: 0";
		linesTr.material = scoreMat.id;
		linesTr.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
		linesTr.verticalAlignment = TextRenderer::VerticalAlignment::Center;
		registry.setComponentDirty(linesTr);

		state.levelText = registry.createEntity();
		auto& levelTransform = registry.addComponent<Transform>(state.levelText);
		levelTransform.position = vec3(posX, startY - 2.0f * lineSpacing, 0.0f);

		auto& levelTr = registry.addComponent<UITextRenderer>(state.levelText);
		levelTr.text = "LEVEL: 1";
		levelTr.material = scoreMat.id;
		levelTr.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
		levelTr.verticalAlignment = TextRenderer::VerticalAlignment::Center;
		registry.setComponentDirty(levelTr);

		// Big box
		services.shapes().addShape({.shapeId = DefaultShapes::Box,
									.variables = {{DefaultShapes::Box::PosX, 0.0f},
												  {DefaultShapes::Box::PosY, 0.0f},
												  {DefaultShapes::Box::SizeX, 60.0f},
												  {DefaultShapes::Box::SizeY, 100.0f}},
									.material = wallMat});

		// Big hole
		services.shapes().addShape({.shapeId = DefaultShapes::RoundedBox,
									.variables = {{DefaultShapes::RoundedBox::PosX, 0.0f},
												  {DefaultShapes::RoundedBox::PosY, 0.0f},
												  {DefaultShapes::RoundedBox::SizeX, 5.0f},
												  {DefaultShapes::RoundedBox::SizeY, 10.0f},
												  {DefaultShapes::RoundedBox::Radius, 0.5f}},
									.material = wallMat,
									.combination = CombinationType::Subtraction});

		// Hold box hole (left)
		services.shapes().addShape({.shapeId = DefaultShapes::RoundedBox,
									.variables =
										{
											{DefaultShapes::RoundedBox::PosX, HOLD_PREVIEW_POS.x},
											{DefaultShapes::RoundedBox::PosY, HOLD_PREVIEW_POS.y},
											{DefaultShapes::RoundedBox::SizeX, 2.5f},
											{DefaultShapes::RoundedBox::SizeY, 2.5f},
											{DefaultShapes::RoundedBox::Radius, 0.5f},
										},
									.material = wallMat,
									.combination = CombinationType::Subtraction});

		// Next box hole (right)
		services.shapes().addShape({.shapeId = DefaultShapes::RoundedBox,
									.variables =
										{
											{DefaultShapes::RoundedBox::PosX, NEXT_PREVIEW_POS.x},
											{DefaultShapes::RoundedBox::PosY, NEXT_PREVIEW_POS.y},
											{DefaultShapes::RoundedBox::SizeX, 2.5f},
											{DefaultShapes::RoundedBox::SizeY, 2.5f},
											{DefaultShapes::RoundedBox::Radius, 0.5f},
										},
									.material = wallMat,
									.combination = CombinationType::Subtraction});

		// Pipe segments feeding dots from off-screen left to PIPE_END
		const auto& pipePath = getPipePath();
		for (size_t i = 0; i + 1 < pipePath.points.size(); ++i)
		{
			services.shapes().addShape({.shapeId = DefaultShapes::Line,
										.variables =
											{
												{DefaultShapes::Line::StartX, pipePath.points[i].x},
												{DefaultShapes::Line::StartY, pipePath.points[i].y},
												{DefaultShapes::Line::EndX, pipePath.points[i + 1].x},
												{DefaultShapes::Line::EndY, pipePath.points[i + 1].y},
												{DefaultShapes::Line::Width, PIPE_WIDTH},
											},
										.material = wallMat,
										.combination = CombinationType::Subtraction,
										.hasCollision = true});
		}

		// Nozzle opening from the pipe end into the top of the main hole
		// (hasCollision is false during gameplay so dynamic dots stop at PIPE_END; enabled on Game Over)
		state.pipeNozzleShape = services.shapes().addShape({.shapeId = DefaultShapes::Line,
															.variables =
																{
																	{DefaultShapes::Line::StartX, PIPE_END.x},
																	{DefaultShapes::Line::StartY, PIPE_END.y},
																	{DefaultShapes::Line::EndX, PIPE_END.x},
																	{DefaultShapes::Line::EndY, 9.8f},
																	{DefaultShapes::Line::Width, PIPE_WIDTH},
																},
															.material = wallMat,
															.combination = CombinationType::Subtraction,
															.hasCollision = false});

		// Completely fill the pipe from PIPE_END down to PIPE_VALVE_S with Dynamic rigidbody dots
		state.nextPieceMaterial = getPieceMaterial(state, state.nextPieceType);
		float curS = getTotalPipeLength();
		vec2 prevPos = pointOnPipe(curS);
		spawnDotOnPipeAtS(registry, state, state.pipeDotMaterial, curS);

		while (curS > PIPE_VALVE_S + 0.5f)
		{
			float lo = std::max(0.0f, curS - 2.0f);
			float hi = curS;
			for (int it = 0; it < 25; ++it)
			{
				float mid = 0.5f * (lo + hi);
				if (glm::length(pointOnPipe(mid) - prevPos) < PIPE_BALL_SPACING)
				{
					hi = mid;
				}
				else
				{
					lo = mid;
				}
			}
			curS = 0.5f * (lo + hi);
			if (curS < PIPE_VALVE_S + 0.5f)
			{
				break;
			}
			prevPos = pointOnPipe(curS);

			spawnDotOnPipeAtS(registry, state, state.pipeDotMaterial, curS);
		}

		// Create the kinematic stopper/piston at the pipe entrance to prevent backflow and push new batches
		state.pistonCurrentS = PIPE_VALVE_S;
		state.pistonTargetS = PIPE_VALVE_S;
		state.pipePistonEntity = registry.createEntity();
		vec2 pistonPos = pointOnPipe(state.pistonCurrentS);
		auto& pistonTransform = registry.addComponent<Transform>(state.pipePistonEntity);
		pistonTransform.position = vec3(pistonPos.x, pistonPos.y, 0.0f);

		auto& pistonDot = registry.addComponent<Dot>(state.pipePistonEntity);
		pistonDot.materialId = wallMat.id;

		auto& pistonRb = registry.addComponent<RigidBody2D>(state.pipePistonEntity);
		pistonRb.type = BodyType::Kinematic;
		pistonRb.velocity = vec2(0.0f, 0.0f);
		pistonRb.enableCollision = true;
		pistonRb.enableShapeCollision = false;

		updatePreview(registry, state.nextPreviewDots, state.nextPieceType, state.nextPieceMaterial, NEXT_PREVIEW_POS);
		updatePreview(registry, state.holdPreviewDots, state.heldPieceType, state.heldPieceMaterial, HOLD_PREVIEW_POS);
	}

	bool move(TetrominoSceneNamespace::State& state, WeirdEngine::Registry& registry, int offsetX, int offsetY)
	{
		if (state.activeDots.size() != 4)
			return false;

		bool canMove = true;
		for (size_t i = 0; i < 4; ++i)
		{
			glm::ivec2 gridPos = state.activeGridPos[i];
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
			for (size_t i = 0; i < 4; ++i)
			{
				state.activeGridPos[i].x += offsetX;
				state.activeGridPos[i].y += offsetY;
				state.spawnTargetPos[i] = gridToWorld(state.activeGridPos[i]);

				if (!state.isSpawningPiece)
				{
					auto& transform = registry.getComponent<Transform>(state.activeDots[i]);
					transform.position = vec3(state.spawnTargetPos[i], 0.0f);
					registry.setComponentDirty(transform);
				}
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

		glm::ivec2 center = state.activeGridPos[1];

		glm::ivec2 rotatedPositions[4];
		for (size_t idx = 0; idx < 4; ++idx)
		{
			glm::ivec2 gridPos = state.activeGridPos[idx];
			glm::ivec2 localPos = gridPos - center;
			glm::ivec2 rotLocal =
				(direction > 0) ? glm::ivec2(localPos.y, -localPos.x) : glm::ivec2(-localPos.y, localPos.x);
			rotatedPositions[idx] = center + rotLocal;
		}

		const glm::ivec2 kickOffsets[] = {{0, 0}, {1, 0}, {-1, 0}, {2, 0}, {-2, 0}, {0, 1}};
		for (const auto& kick : kickOffsets)
		{
			bool valid = true;
			for (size_t idx = 0; idx < 4; ++idx)
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
				for (size_t idx = 0; idx < 4; ++idx)
				{
					glm::ivec2 finalPos = rotatedPositions[idx] + kick;
					state.activeGridPos[idx] = finalPos;
					state.spawnTargetPos[idx] = gridToWorld(finalPos);

					if (!state.isSpawningPiece)
					{
						auto& transform = registry.getComponent<Transform>(state.activeDots[idx]);
						transform.position = vec3(state.spawnTargetPos[idx], 0.0f);
						registry.setComponentDirty(transform);
					}
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
		updateScoreText(registry, state);
		updatePauseText(registry, state);

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

		// Pause toggle: key P
		if (input.getKeyDown(Input::P))
		{
			state.isPaused = !state.isPaused;
			if (state.isPaused)
			{
				services.physics().pause();
			}
			else
			{
				services.physics().resume();
			}
			updatePauseText(registry, state);
		}

		if (state.isPaused)
		{
			return;
		}

		syncKinematicPositions(registry, state);
		updatePipePiston(registry, services, state);
		updatePreview(registry, state.nextPreviewDots, state.nextPieceType, state.nextPieceMaterial, NEXT_PREVIEW_POS);
		if (state.heldPieceType >= 0)
		{
			updatePreview(registry, state.holdPreviewDots, state.heldPieceType, state.heldPieceMaterial,
						  HOLD_PREVIEW_POS);
		}

		// Save / Restore (Hold)
		bool holdTriggered = input.getKeyDown(Input::C) || input.getKeyDown(Input::LeftShift) ||
							 input.getKeyDown(Input::H) || input.getKeyDown(Input::S) ||
							 input.getGamepadButtonDown(Input::GamepadButton::LeftShoulder) ||
							 input.getGamepadButtonDown(Input::GamepadButton::RightShoulder);
		if (holdTriggered)
		{
			if (state.canHold && state.activeDots.size() == 4 && !state.isGameOver)
			{
				state.canHold = false;

				int prevHeldType = state.heldPieceType;
				int prevHeldMat = state.heldPieceMaterial;

				if (prevHeldType == -1)
				{
					state.heldPieceType = state.currentPieceType;
					state.heldPieceMaterial = state.currentPieceMaterial;

					for (int i = 0; i < 4; ++i)
					{
						state.holdPreviewDots[i] = state.activeDots[i];
					}
					state.activeDots.clear();

					updatePreview(registry, state.holdPreviewDots, state.heldPieceType, state.heldPieceMaterial,
								  HOLD_PREVIEW_POS);

					int typeToSpawn = state.nextPieceType;
					int matToSpawn = state.nextPieceMaterial;

					if (!spawnPieceFromPipe(registry, services, state, typeToSpawn, matToSpawn))
					{
						updateGhostPreview(registry, state);
						updateScoreText(registry, state);
						updateGameOverText(registry, state);
						return;
					}

					sortPipeDotsByExitDistance(registry, state);
					state.nextPieceType = services.random().range(0, 6);
					state.nextPieceMaterial = getPieceMaterial(state, state.nextPieceType);
					updatePreview(registry, state.nextPreviewDots, state.nextPieceType, state.nextPieceMaterial,
								  NEXT_PREVIEW_POS);
				}
				else
				{
					if (!canPlacePieceAtSpawn(state, prevHeldType))
					{
						triggerGameOver(registry, services, state);
						updateGhostPreview(registry, state);
						updateScoreText(registry, state);
						updateGameOverText(registry, state);
						return;
					}

					state.heldPieceType = state.currentPieceType;
					state.heldPieceMaterial = state.currentPieceMaterial;

					std::array<Entity, 4> oldHoldDots = state.holdPreviewDots;
					for (int i = 0; i < 4; ++i)
					{
						state.holdPreviewDots[i] = state.activeDots[i];
					}
					updatePreview(registry, state.holdPreviewDots, state.heldPieceType, state.heldPieceMaterial,
								  HOLD_PREVIEW_POS);

					state.activeDots.assign(oldHoldDots.begin(), oldHoldDots.end());
					state.currentPieceType = prevHeldType;
					state.currentPieceMaterial = prevHeldMat;

					constexpr int cx = 4;
					constexpr int cy = 18;
					for (int i = 0; i < 4; ++i)
					{
						int gridX = cx + SHAPES[prevHeldType][i].dx;
						int gridY = cy + SHAPES[prevHeldType][i].dy;

						auto& transform = registry.getComponent<Transform>(state.activeDots[i]);
						state.activeGridPos[i] = glm::ivec2(gridX, gridY);
						state.spawnStartPos[i] = vec2(transform.position);
						state.spawnTargetPos[i] = gridToWorld(gridX, gridY);

						auto& dot = registry.getComponent<Dot>(state.activeDots[i]);
						dot.materialId = prevHeldMat;
						registry.setComponentDirty(dot);
					}

					state.isSpawningPiece = true;
					state.spawnAnimTimer = 0.0f;
				}

				state.timeSinceLastDrop = 0.0f;
				updateGhostPreview(registry, state);
				updateScoreText(registry, state);
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

		// Hard drop: immediately drop piece to ghost position, lock, and award bonus score
		bool hardDrop = input.getKeyDown(Input::Space);
		if (hardDrop && !state.isSpawningPiece && state.activeDots.size() == 4)
		{
			int dropDistance = 0;
			while (true)
			{
				bool canDropFurther = true;
				for (size_t i = 0; i < 4; ++i)
				{
					glm::ivec2 gridPos = state.activeGridPos[i];
					int testY = gridPos.y - (dropDistance + 1);
					if (testY < 0 || (gridPos.x >= 0 && gridPos.x < 10 && testY < 20 &&
									  state.grid[gridPos.x][testY] != INVALID_ENTITY))
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

			if (dropDistance > 0)
			{
				move(state, registry, 0, -dropDistance);
				state.score += dropDistance * 2;
			}

			for (size_t i = 0; i < state.activeDots.size() && i < 4; ++i)
			{
				Entity dotEntity = state.activeDots[i];
				glm::ivec2 gridPos = state.activeGridPos[i];
				if (gridPos.x >= 0 && gridPos.x < 10 && gridPos.y >= 0 && gridPos.y < 20)
				{
					state.grid[gridPos.x][gridPos.y] = dotEntity;
					auto& transform = registry.getComponent<Transform>(dotEntity);
					transform.position = vec3(gridToWorld(gridPos), 0.0f);
					registry.setComponentDirty(transform);
				}
			}

			state.activeDots.clear();
			state.timeSinceLastDrop = state.dropInterval;
			updateGhostPreview(registry, state);
			updateScoreText(registry, state);
			updateGameOverText(registry, state);
			return;
		}

		// Smoothly animate the 4 dots from the pipe into the tetromino formation
		if (state.isSpawningPiece)
		{
			state.spawnAnimTimer += services.time().deltaTime();
			float t = std::clamp(state.spawnAnimTimer / state.spawnAnimDuration, 0.0f, 1.0f);
			float oneMinusT = 1.0f - t;
			float eased = 1.0f - (oneMinusT * oneMinusT * oneMinusT);

			for (size_t i = 0; i < state.activeDots.size() && i < 4; ++i)
			{
				state.spawnTargetPos[i] = gridToWorld(state.activeGridPos[i]);
				vec2 pos = glm::mix(state.spawnStartPos[i], state.spawnTargetPos[i], eased);

				auto& transform = registry.getComponent<Transform>(state.activeDots[i]);
				transform.position = vec3(pos.x, pos.y, 0.0f);
				registry.setComponentDirty(transform);
			}

			if (t >= 1.0f)
			{
				state.isSpawningPiece = false;
				state.timeSinceLastDrop = 0.0f;
			}

			updateGhostPreview(registry, state);
			updateScoreText(registry, state);
			return;
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
			updateScoreText(registry, state);
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
								transform.position = vec3(gridToWorld(x, writeY), 0.0f);
								registry.setComponentDirty(transform);
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
				constexpr int LINE_SCORES[] = {0, 100, 300, 500, 800};
				int baseScore = (linesCleared <= 4) ? LINE_SCORES[linesCleared] : (linesCleared * 200);
				state.score += baseScore * state.level;
				state.lines += linesCleared;
				state.level = 1 + (state.lines / 10);
				state.dropInterval = std::max(0.08f, 0.5f - static_cast<float>(state.level - 1) * 0.04f);

				// Let destroyed rigidbodies be freed at end-of-frame before spawning new pipe rigidbodies
				state.timeSinceLastDrop = state.dropInterval;
				updateScoreText(registry, state);
				return;
			}

			int typeToSpawn = state.nextPieceType;
			int matToSpawn = state.nextPieceMaterial;

			state.canHold = true;

			if (!spawnPieceFromPipe(registry, services, state, typeToSpawn, matToSpawn))
			{
				updateGhostPreview(registry, state);
				updateScoreText(registry, state);
				updateGameOverText(registry, state);
				return;
			}

			sortPipeDotsByExitDistance(registry, state);
			state.nextPieceType = services.random().range(0, 6);
			state.nextPieceMaterial = getPieceMaterial(state, state.nextPieceType);
			updatePreview(registry, state.nextPreviewDots, state.nextPieceType, state.nextPieceMaterial,
						  NEXT_PREVIEW_POS);

			updateGhostPreview(registry, state);
			updateScoreText(registry, state);
			return;
		}

		bool moved = move(state, registry, 0, -1);
		if (!moved)
		{
			for (size_t i = 0; i < state.activeDots.size() && i < 4; ++i)
			{
				Entity dotEntity = state.activeDots[i];
				glm::ivec2 gridPos = state.activeGridPos[i];
				if (gridPos.x >= 0 && gridPos.x < 10 && gridPos.y >= 0 && gridPos.y < 20)
				{
					state.grid[gridPos.x][gridPos.y] = dotEntity;
					auto& transform = registry.getComponent<Transform>(dotEntity);
					transform.position = vec3(gridToWorld(gridPos), 0.0f);
					registry.setComponentDirty(transform);
				}
			}

			state.activeDots.clear();
		}
		else if (downHeld)
		{
			state.score += 1;
		}

		updateGhostPreview(registry, state);
		updateScoreText(registry, state);
		updateGameOverText(registry, state);
	}
} // namespace TetrominoSceneNamespace
