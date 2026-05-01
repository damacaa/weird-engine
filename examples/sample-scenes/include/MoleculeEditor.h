#pragma once

#include <weird-engine.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "globals.h"

using namespace WeirdEngine;

class MoleculeEditor : public Scene
{
public:
	MoleculeEditor(const PhysicsSettings& settings)
		: Scene(settings)
	{
	}

private:
	enum class RightMouseMode
	{
		None,
		Drag,
		Constraint
	};

	enum class ToolMode
	{
		Drag,
		Spring,
		Distance,
		Remove,
		TagEditor,
		Material
	};

	enum class LinkType
	{
		Spring,
		Distance
	};

	struct BallInfo
	{
		Entity entity;
		int simulationId;
	};

	struct DistanceLink
	{
		Entity a;
		Entity b;
		int simulationIdA;
		int simulationIdB;
		float restDistance;
		Entity lineEntity;
		LinkType type;
	};

	std::vector<BallInfo> m_balls;
	std::vector<DistanceLink> m_links;
	std::array<Entity, 16> m_materialToggles{};

	Entity m_draggedBall = static_cast<Entity>(-1);
	int m_draggedSimulationId = -1;
	Entity m_constraintStartBall = static_cast<Entity>(-1);
	DistanceLink* m_draggedLink = nullptr;
	float m_linkDragStartX = 0.0f;
	float m_linkDragStartDist = 0.0f;
	std::atomic<float> m_pendingLinkDistance{0.0f};
	std::atomic<int> m_dragLinkSimIdA{-1};
	std::atomic<int> m_dragLinkSimIdB{-1};
	bool m_keepFixedAfterDrag = false;
	bool m_rightWasDown = false;
	bool m_gravityEnabled = false;
	bool m_gridMode = false;
	RightMouseMode m_rightMouseMode = RightMouseMode::None;
	int m_selectedMaterial = 1;

	ToolMode m_toolMode = ToolMode::Drag;
	std::array<Entity, 6> m_toolToggles{};
	Entity m_gravityToggleEntity = static_cast<Entity>(-1);
	Entity m_gridToggleEntity = static_cast<Entity>(-1);

	// Tag editor state
	Entity m_tagSelectedEntity = static_cast<Entity>(-1);
	Entity m_tagCircleOuter = static_cast<Entity>(-1);
	Entity m_tagCircleInner = static_cast<Entity>(-1);
	Entity m_tagLabelEntity = static_cast<Entity>(-1);
	Entity m_tagEditButton = static_cast<Entity>(-1);
	Entity m_tagEditButtonLabel = static_cast<Entity>(-1);

	static constexpr float BALL_HIT_RADIUS = 0.9f;
	static constexpr float LINE_WIDTH = 3.5f;
	static constexpr float SPRING_STIFFNESS = 0.15f;
	static constexpr float CONSTRAINT_STIFFNESS = 0.95f;
	static constexpr float BTN_SIZE = 18.0f;
	static constexpr float START_X = 40.0f;
	static constexpr float MAT_Y = 50.0f;
	static constexpr float MAT_SPACING = 47.0f;

	static constexpr float TOOL_X = 30.0f;
	static constexpr float TOOL_Y_START = 30.0f;
	static constexpr float TOOL_SPACING = 60.0f;
	static constexpr float TOOL_BTN_HALF = 12.0f;
	static constexpr float GRAV_Y = 50.0f;
	static constexpr float GRID_Y = 110.0f;
	static constexpr float GRID_CELL = 1.0f;
	static constexpr float TAG_OUTER_RADIUS = 30.0f;
	static constexpr float TAG_INNER_RADIUS = 25.0f;
	static constexpr int TAG_RING_GROUP = 8;

	void onStart() override
	{
		m_debugInput = false;
		m_debugFly = true;

		g_cameraPositon.x = 0.0f;
		g_cameraPositon.y = 0.0f;
		m_ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;

		// Request neutral simulation behavior for this editor scene.
		m_simulation2D.setGravity(0.0f);
		m_simulation2D.setDamping(1.0f);

		buildMaterialPalette();
		buildToolbar();
		buildTagEditorUI();

		{
			float boundsVars[8]{0.0f, 0.0f, 3000.0f};
			Entity outside = addShape(DefaultShapes::CIRCLE, boundsVars, 17, CombinationType::Addition);

			float boundsVars2[8]{0.0f, 0.0f, 20.0f, 20.0f};
			Entity inside =
				addShape(DefaultShapes::BOX, boundsVars2, DisplaySettings::Black, CombinationType::Subtraction);

			blacklistEntity(outside);
			blacklistEntity(inside);
		}
	}

	void onUpdate(float delta) override
	{
		g_cameraPositon = m_ecs.getComponent<Transform>(m_mainCamera).position;

		if (Input::GetKeyDown(Input::Q))
		{
			setSceneComplete();
			return;
		}

		if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::S))
		{
			std::cout << "Save scene name: " << std::flush;

			std::string fileName;
			if (!(std::cin >> fileName))
			{
				std::cin.clear();
				std::cin.ignore(10000, '\n');
			}
			else
			{
				if (!fileName.ends_with(".weird"))
				{
					fileName += ".weird";
				}
				saveScene(ASSETS_PATH "Organisms/" + fileName);
			}
		}

		if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::L))
		{
			std::cout << "Load scene name: " << std::flush;

			std::string fileName;
			if (!(std::cin >> fileName))
			{
				std::cin.clear();
				std::cin.ignore(10000, '\n');
			}
			else
			{
				if (!fileName.ends_with(".weird"))
				{
					fileName += ".weird";
				}
				loadMolecule(ASSETS_PATH "Organisms/" + fileName);
			}
		}

		if (Input::GetMouseButtonDown(Input::LeftClick) && !Input::isUIClick())
		{
			spawnBallAtMouse();
		}

		syncMaterialPalette();
		syncToolbar();
		handleRightMouseDragInput();
		handleConstraintLineClicks();
		updateConstraintLines();
		updateTagEditor();
		removeFallenBalls();
	}

	void onPhysicsStep() override
	{
		float dist = m_pendingLinkDistance.exchange(0.0f, std::memory_order_acq_rel);
		if (dist >= 1.0f)
		{
			m_simulation2D.setDistanceConstraintDistance(
				static_cast<SimulationID>(m_dragLinkSimIdA.load(std::memory_order_relaxed)),
				static_cast<SimulationID>(m_dragLinkSimIdB.load(std::memory_order_relaxed)), dist);
		}
	}

	void removeFallenBalls()
	{
		std::vector<Entity> toDelete;
		toDelete.reserve(m_balls.size());

		for (const auto& b : m_balls)
		{
			if (!hasTransform(b.entity))
			{
				toDelete.push_back(b.entity);
				continue;
			}

			const auto& t = m_ecs.getComponent<Transform>(b.entity);
			if (t.position.y < -1000.0f)
			{
				toDelete.push_back(b.entity);
			}
		}

		if (toDelete.empty())
			return;

		auto shouldDelete = [&toDelete](Entity e)
		{ return std::find(toDelete.begin(), toDelete.end(), e) != toDelete.end(); };

		for (Entity e : toDelete)
		{
			m_ecs.destroyEntity(e);
		}

		m_links.erase(std::remove_if(m_links.begin(), m_links.end(),
									 [&](const DistanceLink& link)
									 {
										 bool remove = shouldDelete(link.a) || shouldDelete(link.b);
										 if (remove)
										 {
											 if (m_draggedLink == &link)
												 m_draggedLink = nullptr;
											 m_ecs.destroyEntity(link.lineEntity);
										 }
										 return remove;
									 }),
					  m_links.end());

		m_balls.erase(
			std::remove_if(m_balls.begin(), m_balls.end(), [&](const BallInfo& b) { return shouldDelete(b.entity); }),
			m_balls.end());

		if (shouldDelete(m_draggedBall))
		{
			m_draggedBall = static_cast<Entity>(-1);
			m_draggedSimulationId = -1;
			m_keepFixedAfterDrag = false;
			m_rightMouseMode = RightMouseMode::None;
			m_rightWasDown = false;
		}

		if (shouldDelete(m_constraintStartBall))
		{
			m_constraintStartBall = static_cast<Entity>(-1);
		}

		if (shouldDelete(m_tagSelectedEntity))
		{
			m_tagSelectedEntity = static_cast<Entity>(-1);
		}
	}

	void buildMaterialPalette()
	{
		for (int i = 0; i < 16; i++)
		{
			float px = START_X + i * MAT_SPACING;
			float p[8]{px, MAT_Y, BTN_SIZE - 4.0f};
			Entity e;
			UIShape& sh = addUIShape(DefaultShapes::CIRCLE, p, e);
			sh.material = static_cast<uint16_t>(i);

			auto& tog = m_ecs.addComponent<ShapeToggle>(e);
			tog.clickPadding = BTN_SIZE + 3.0f;
			tog.parameterModifierMask.set(2);
			tog.modifierAmount = 5.0f;

			m_materialToggles[i] = e;
			blacklistEntity(e);
		}

		m_ecs.getComponent<ShapeToggle>(m_materialToggles[m_selectedMaterial]).active = true;
	}

	void syncMaterialPalette()
	{
		int activated = -1;
		for (int i = 0; i < 16; i++)
		{
			auto& t = m_ecs.getComponent<ShapeToggle>(m_materialToggles[i]);
			if (t.active && t.state == ButtonState::Down)
			{
				activated = i;
				break;
			}
		}

		if (activated >= 0)
		{
			m_selectedMaterial = activated;
			for (int i = 0; i < 16; i++)
			{
				if (i != activated)
				{
					m_ecs.getComponent<ShapeToggle>(m_materialToggles[i]).active = false;
				}
			}
		}
		else
		{
			for (int i = 0; i < 16; i++)
			{
				if (m_ecs.getComponent<ShapeToggle>(m_materialToggles[i]).active)
				{
					m_selectedMaterial = i;
					break;
				}
			}
		}
	}

	void buildToolbar()
	{
		const char* labels[] = {"drag", "spring", "distance", "remove", "tag", "material"};
		for (int i = 0; i < 6; i++)
		{
			float y = (Display::height - TOOL_Y_START) - (i * TOOL_SPACING);
			float p[8]{TOOL_X, y, TOOL_BTN_HALF, TOOL_BTN_HALF};
			Entity e = addUIShape(DefaultShapes::BOX, p, static_cast<uint16_t>(2));
			auto& tog = m_ecs.addComponent<ShapeToggle>(e);
			tog.clickPadding = TOOL_BTN_HALF + 8.0f;
			tog.parameterModifierMask.set(2);
			tog.parameterModifierMask.set(3);
			tog.modifierAmount = 3.0f;
			m_toolToggles[i] = e;
			blacklistEntity(e);

			Entity lbl = m_ecs.createEntity();
			auto& lt = m_ecs.addComponent<Transform>(lbl);
			lt.position = vec3(TOOL_X + TOOL_BTN_HALF + 20.0f, y, 0.0f);
			auto& tx = m_ecs.addComponent<UITextRenderer>(lbl);
			tx.text = labels[i];
			tx.material = 1;
			tx.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
			tx.verticalAlignment = TextRenderer::VerticalAlignment::Center;
			blacklistEntity(lbl);
		}
		m_ecs.getComponent<ShapeToggle>(m_toolToggles[0]).active = true;

		float starP[8]{Display::width - GRAV_Y, Display::height - GRAV_Y, GRAV_Y * 0.5f, 5.0f, 10.0f, 0.0f};
		m_gravityToggleEntity = addUIShape(DefaultShapes::STAR, starP, static_cast<uint16_t>(2));
		auto& gravTog = m_ecs.addComponent<ShapeToggle>(m_gravityToggleEntity);
		gravTog.clickPadding = 18.0f;
		// gravTog.parameterModifierMask.set(2);
		gravTog.parameterModifierMask.set(5);
		gravTog.modifierAmount = 10.0f;
		blacklistEntity(m_gravityToggleEntity);

		float gridP[8]{Display::width - GRAV_Y, Display::height - GRID_Y, 12.0f, 12.0f};
		m_gridToggleEntity = addUIShape(DefaultShapes::BOX, gridP, static_cast<uint16_t>(2));
		auto& gridTog = m_ecs.addComponent<ShapeToggle>(m_gridToggleEntity);
		gridTog.clickPadding = 18.0f;
		gridTog.parameterModifierMask.set(2);
		gridTog.parameterModifierMask.set(3);
		gridTog.modifierAmount = 3.0f;
		blacklistEntity(m_gridToggleEntity);
	}

	void syncToolbar()
	{
		int activated = -1;
		for (int i = 0; i < 6; i++)
		{
			auto& t = m_ecs.getComponent<ShapeToggle>(m_toolToggles[i]);
			if (t.active && t.state == ButtonState::Down)
			{
				activated = i;
				break;
			}
		}
		if (activated >= 0)
		{
			m_toolMode = static_cast<ToolMode>(activated);
			for (int i = 0; i < 6; i++)
			{
				if (i != activated)
					m_ecs.getComponent<ShapeToggle>(m_toolToggles[i]).active = false;
			}
		}

		auto& gravTog = m_ecs.getComponent<ShapeToggle>(m_gravityToggleEntity);
		bool wantsGravity = gravTog.active;
		if (wantsGravity != m_gravityEnabled)
		{
			m_gravityEnabled = wantsGravity;
			auto& starShape = m_ecs.getComponent<UIShape>(m_gravityToggleEntity);
			if (m_gravityEnabled)
			{
				m_simulation2D.setGravity(-10.0f);
				m_simulation2D.setDamping(0.01f);
			}
			else
			{
				m_simulation2D.setGravity(0.0f);
				m_simulation2D.setDamping(1.0f);
			}
		}

		m_gridMode = m_ecs.getComponent<ShapeToggle>(m_gridToggleEntity).active;
	}

	void spawnBallAtMouse()
	{
		auto& cam = m_ecs.getComponent<Transform>(m_mainCamera);
		vec2 world = ECS::Camera::screenPositionToWorldPosition2D(cam, vec2(Input::GetMouseX(), Input::GetMouseY()));

		if (m_gridMode)
			world = snapToGrid(world);

		Entity e = m_ecs.createEntity();

		auto& t = m_ecs.addComponent<Transform>(e);
		t.position = vec3(world.x, world.y, 0.0f);
		t.isDirty = true;

		auto& sdf = m_ecs.addComponent<Dot>(e);
		sdf.materialId = static_cast<unsigned int>(m_selectedMaterial);

		auto& rb = m_ecs.addComponent<RigidBody2D>(e);

		m_balls.push_back({e, static_cast<int>(rb.simulationId)});
	}

	vec2 getMouseWorldPosition()
	{
		auto& cam = m_ecs.getComponent<Transform>(m_mainCamera);
		return ECS::Camera::screenPositionToWorldPosition2D(cam, vec2(Input::GetMouseX(), Input::GetMouseY()));
	}

	vec2 snapToGrid(vec2 pos, Entity exclude = static_cast<Entity>(-1))
	{
		vec2 nearest(std::round(pos.x / GRID_CELL) * GRID_CELL, std::round(pos.y / GRID_CELL) * GRID_CELL);

		if (!isCellOccupied(nearest, exclude))
			return nearest;

		// Spiral outward to find the closest free cell.
		for (int radius = 1; radius <= 50; ++radius)
		{
			float bestDist = (std::numeric_limits<float>::max)();
			vec2 bestCell = nearest;
			bool found = false;

			for (int dx = -radius; dx <= radius; ++dx)
			{
				for (int dy = -radius; dy <= radius; ++dy)
				{
					if (std::abs(dx) != radius && std::abs(dy) != radius)
						continue; // only check the outer ring

					vec2 candidate(nearest.x + dx * GRID_CELL, nearest.y + dy * GRID_CELL);

					if (isCellOccupied(candidate, exclude))
						continue;

					float d = glm::length(candidate - pos);
					if (d < bestDist)
					{
						bestDist = d;
						bestCell = candidate;
						found = true;
					}
				}
			}

			if (found)
				return bestCell;
		}

		return nearest; // fallback
	}

	bool isCellOccupied(vec2 cell, Entity exclude)
	{
		const float threshold = GRID_CELL * 0.25f;
		for (const auto& b : m_balls)
		{
			if (b.entity == exclude)
				continue;
			if (!m_ecs.hasComponent<Transform>(b.entity))
				continue;
			const auto& t = m_ecs.getComponent<Transform>(b.entity);
			vec2 p(t.position.x, t.position.y);
			if (std::abs(p.x - cell.x) < threshold && std::abs(p.y - cell.y) < threshold)
				return true;
		}
		return false;
	}

	void handleRightMouseDragInput()
	{
		bool rightDown = Input::GetMouseButton(Input::RightClick);

		if (rightDown && !m_rightWasDown)
		{
			if (m_toolMode == ToolMode::Spring || m_toolMode == ToolMode::Distance || m_toolMode == ToolMode::Remove)
				onConstraintStart();
			else if (m_toolMode == ToolMode::TagEditor)
				onTagEditorRightClick();
			else if (m_toolMode == ToolMode::Material)
				onMaterialRightClick();
			else
				onRightDragStart();
		}
		else if (rightDown && m_rightWasDown)
		{
			if (m_rightMouseMode == RightMouseMode::Drag)
			{
				onRightDragUpdate();
			}
		}
		else if (!rightDown && m_rightWasDown)
		{
			if (m_rightMouseMode == RightMouseMode::Drag)
			{
				onRightDragEnd();
			}
			else if (m_rightMouseMode == RightMouseMode::Constraint)
			{
				onConstraintEnd();
			}

			m_rightMouseMode = RightMouseMode::None;
		}

		m_rightWasDown = rightDown;
	}

	void onRightDragStart()
	{
		m_rightMouseMode = RightMouseMode::Drag;

		Entity hit = pickBallAtMouse();
		if (hit == static_cast<Entity>(-1))
		{
			m_draggedBall = static_cast<Entity>(-1);
			m_draggedSimulationId = -1;
			m_keepFixedAfterDrag = false;
			m_rightMouseMode = RightMouseMode::None;
			return;
		}

		int id = getSimulationId(hit);
		if (id < 0)
		{
			m_draggedBall = static_cast<Entity>(-1);
			m_draggedSimulationId = -1;
			m_keepFixedAfterDrag = false;
			m_rightMouseMode = RightMouseMode::None;
			return;
		}

		m_draggedBall = hit;
		m_draggedSimulationId = id;
		m_keepFixedAfterDrag = false;

		// Dragging starts by fixing and snapping the particle under the mouse.
		m_simulation2D.fix(static_cast<SimulationID>(m_draggedSimulationId));
		vec2 startPos = getMouseWorldPosition();
		if (m_gridMode)
			startPos = snapToGrid(startPos, m_draggedBall);
		m_simulation2D.setPosition(static_cast<SimulationID>(m_draggedSimulationId), startPos);
	}

	void onRightDragUpdate()
	{
		if (m_draggedBall == static_cast<Entity>(-1) || m_draggedSimulationId < 0)
			return;

		if (Input::GetKeyDown(Input::F))
		{
			m_keepFixedAfterDrag = true;
		}

		vec2 dragPos = getMouseWorldPosition();
		if (m_gridMode)
			dragPos = snapToGrid(dragPos, m_draggedBall);
		m_simulation2D.setPosition(static_cast<SimulationID>(m_draggedSimulationId), dragPos);
	}

	void onRightDragEnd()
	{
		if (m_draggedBall == static_cast<Entity>(-1) || m_draggedSimulationId < 0)
			return;

		if (!m_keepFixedAfterDrag)
		{
			m_simulation2D.unFix(static_cast<SimulationID>(m_draggedSimulationId));
		}

		m_draggedBall = static_cast<Entity>(-1);
		m_draggedSimulationId = -1;
		m_keepFixedAfterDrag = false;
	}

	void onConstraintStart()
	{
		m_rightMouseMode = RightMouseMode::Constraint;
		m_constraintStartBall = pickBallAtMouse();
		if (m_constraintStartBall == static_cast<Entity>(-1))
		{
			m_rightMouseMode = RightMouseMode::None;
		}
	}

	void onConstraintEnd()
	{
		if (m_constraintStartBall == static_cast<Entity>(-1))
			return;

		Entity hit = pickBallAtMouse();
		if (hit != static_cast<Entity>(-1) && hit != m_constraintStartBall)
		{
			if (m_toolMode == ToolMode::Remove)
			{
				removeConstraintLink(m_constraintStartBall, hit);
			}
			else
			{
				LinkType type = (m_toolMode == ToolMode::Distance) ? LinkType::Distance : LinkType::Spring;
				addConstraintLink(m_constraintStartBall, hit, type);
			}
		}

		m_constraintStartBall = static_cast<Entity>(-1);
	}

	Entity pickBallAtMouse()
	{
		auto& cam = m_ecs.getComponent<Transform>(m_mainCamera);
		vec2 world = ECS::Camera::screenPositionToWorldPosition2D(cam, vec2(Input::GetMouseX(), Input::GetMouseY()));

		float best = BALL_HIT_RADIUS;
		Entity bestEntity = static_cast<Entity>(-1);

		for (const auto& b : m_balls)
		{
			if (!hasTransform(b.entity))
				continue;

			const auto& t = m_ecs.getComponent<Transform>(b.entity);
			vec2 p(t.position.x, t.position.y);
			float d = length(world - p);
			if (d < best)
			{
				best = d;
				bestEntity = b.entity;
			}
		}

		return bestEntity;
	}

	bool linkExists(Entity a, Entity b)
	{
		for (const auto& link : m_links)
		{
			bool sameDir = (link.a == a && link.b == b);
			bool reverseDir = (link.a == b && link.b == a);
			if (sameDir || reverseDir)
				return true;
		}
		return false;
	}

	void addConstraintLink(Entity a, Entity b, LinkType type)
	{
		if (linkExists(a, b))
			return;

		if (!hasTransform(a) || !hasTransform(b))
			return;

		int idA = getSimulationId(a);
		int idB = getSimulationId(b);
		if (idA < 0 || idB < 0)
			return;

		auto& ta = m_ecs.getComponent<Transform>(a);
		auto& tb = m_ecs.getComponent<Transform>(b);
		vec2 pa(ta.position.x, ta.position.y);
		vec2 pb(tb.position.x, tb.position.y);
		float restDistance = length(pb - pa);
		restDistance = (std::max)(restDistance, 1.0f);

		if (type == LinkType::Distance)
			m_simulation2D.addPositionConstraint(idA, idB, restDistance);
		else
			m_simulation2D.addSpring(idA, idB, SPRING_STIFFNESS, restDistance);

		auto lineColor = (type == LinkType::Distance) ? DisplaySettings::Cyan : DisplaySettings::Orange;

		float lineVars[8]{};
		computeScreenLineParams(pa, pb, lineVars);
		Entity line = addUIShape(DefaultShapes::LINE, lineVars, lineColor);

		auto& btn = m_ecs.addComponent<ShapeButton>(line);
		btn.clickPadding = 8.0f;
		btn.modifierAmount = 0.0f;

		blacklistEntity(line);

		m_links.push_back({a, b, idA, idB, restDistance, line, type});
	}

	void removeConstraintLink(Entity a, Entity b)
	{
		for (size_t i = 0; i < m_links.size();)
		{
			DistanceLink& link = m_links[i];
			bool sameDir = (link.a == a && link.b == b);
			bool reverseDir = (link.a == b && link.b == a);
			if (!sameDir && !reverseDir)
			{
				++i;
				continue;
			}

			if (m_draggedLink == &link)
				m_draggedLink = nullptr;

			m_ecs.destroyEntity(link.lineEntity);
			m_links.erase(m_links.begin() + i);
		}

		int idA = getSimulationId(a);
		int idB = getSimulationId(b);
		if (idA < 0 || idB < 0)
			return;

		m_simulation2D.removeDistanceConstraint(static_cast<SimulationID>(idA), static_cast<SimulationID>(idB));
	}

	void handleConstraintLineClicks()
	{
		// Detect click-down via ShapeButton state.
		// ButtonSystem already calls Input::flagUIClick() when a line is clicked,
		// so ball spawning is suppressed automatically.
		for (auto& link : m_links)
		{
			if (!hasShapeButton(link.lineEntity))
				continue;
			auto& btn = m_ecs.getComponent<ShapeButton>(link.lineEntity);
			if (btn.state == ButtonState::Down)
			{
				m_draggedLink = &link;
				m_linkDragStartX = Input::GetMouseX();
				m_linkDragStartDist = link.restDistance;
				m_dragLinkSimIdA.store(link.simulationIdA, std::memory_order_relaxed);
				m_dragLinkSimIdB.store(link.simulationIdB, std::memory_order_relaxed);
				break;
			}
		}

		if (!Input::GetMouseButton(Input::LeftClick))
		{
			m_draggedLink = nullptr;
			return;
		}

		if (m_draggedLink == nullptr)
			return;

		float dx = (Input::GetMouseX() - m_linkDragStartX) * 0.3f;
		float newDist = std::round((m_linkDragStartDist + dx) * 10.0f) / 10.0f;
		newDist = (std::clamp)(newDist, 1.0f, 10.0f);
		m_draggedLink->restDistance = newDist;

		// Release store: physics thread's acquire-exchange will see the sim ID writes above.
		m_pendingLinkDistance.store(newDist, std::memory_order_release);
	}

	void updateConstraintLines()
	{
		auto& cam = m_ecs.getComponent<Transform>(m_mainCamera);

		for (auto& link : m_links)
		{
			if (!hasTransform(link.a) || !hasTransform(link.b) || !hasUIShape(link.lineEntity))
				continue;

			const auto& ta = m_ecs.getComponent<Transform>(link.a);
			const auto& tb = m_ecs.getComponent<Transform>(link.b);

			vec2 aWorld(ta.position.x, ta.position.y);
			vec2 bWorld(tb.position.x, tb.position.y);
			vec2 aScreen = ECS::Camera::worldPosition2DToScreenPosition(cam, aWorld);
			vec2 bScreen = ECS::Camera::worldPosition2DToScreenPosition(cam, bWorld);

			auto& ui = m_ecs.getComponent<UIShape>(link.lineEntity);
			ui.parameters[0] = aScreen.x;
			ui.parameters[1] = aScreen.y;
			ui.parameters[2] = bScreen.x;
			ui.parameters[3] = bScreen.y;
			ui.parameters[4] = LINE_WIDTH;
		}
	}

	void computeScreenLineParams(const vec2& aWorld, const vec2& bWorld, float outParams[8])
	{
		auto& cam = m_ecs.getComponent<Transform>(m_mainCamera);
		vec2 aScreen = ECS::Camera::worldPosition2DToScreenPosition(cam, aWorld);
		vec2 bScreen = ECS::Camera::worldPosition2DToScreenPosition(cam, bWorld);

		outParams[0] = aScreen.x;
		outParams[1] = aScreen.y;
		outParams[2] = bScreen.x;
		outParams[3] = bScreen.y;
		outParams[4] = LINE_WIDTH;
	}

	int getSimulationId(Entity e)
	{
		for (const auto& b : m_balls)
		{
			if (b.entity == e)
				return b.simulationId;
		}
		return -1;
	}

	// -----------------------------------------------------------------------
	// Tag editor UI
	// -----------------------------------------------------------------------

	void buildTagEditorUI()
	{
		// Tag label – shows "tag: <name>" or "tag: (none)"
		{
			Entity lbl = m_ecs.createEntity();
			auto& lt = m_ecs.addComponent<Transform>(lbl);
			lt.position = vec3(Display::width * 0.5f, 150.0f, 0.0f);
			auto& tx = m_ecs.addComponent<UITextRenderer>(lbl);
			tx.text = "";
			tx.material = 1;
			tx.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;
			tx.verticalAlignment = TextRenderer::VerticalAlignment::Center;
			m_tagLabelEntity = lbl;
			blacklistEntity(lbl);
		}

		// "edit tag" button (a small box)
		{
			static constexpr float BW = 40.0f;
			static constexpr float BH = 14.0f;
			float p[8]{Display::width * 0.5f, 90.0f, BW, BH};
			m_tagEditButton = addUIShape(DefaultShapes::BOX, p, static_cast<uint16_t>(2));
			auto& btn = m_ecs.addComponent<ShapeButton>(m_tagEditButton);
			btn.clickPadding = 6.0f;
			btn.modifierAmount = 0.0f;
			blacklistEntity(m_tagEditButton);
		}
	}

	void updateTagEditor()
	{
		// Hide ring if no entity selected
		if (m_tagSelectedEntity == static_cast<Entity>(-1))
		{
			if (m_tagCircleOuter != static_cast<Entity>(-1))
			{
				m_ecs.getComponent<UIShape>(m_tagCircleOuter).parameters[2] = 0.0f;
				m_ecs.getComponent<UIShape>(m_tagCircleInner).parameters[2] = 0.0f;
			}
			if (m_tagLabelEntity != static_cast<Entity>(-1))
			{
				m_ecs.getComponent<UITextRenderer>(m_tagLabelEntity).text = "";
			}
			return;
		}

		if (!hasTransform(m_tagSelectedEntity))
		{
			m_tagSelectedEntity = static_cast<Entity>(-1);
			return;
		}

		// Lazy-initialize the ring indicator shapes on first use
		if (m_tagCircleOuter == static_cast<Entity>(-1))
		{
			float p[8]{};
			m_tagCircleOuter = addUIShape(DefaultShapes::CIRCLE, p, static_cast<uint16_t>(DisplaySettings::Yellow),
										  CombinationType::Addition, TAG_RING_GROUP);
			blacklistEntity(m_tagCircleOuter);

			m_tagCircleInner = addUIShape(DefaultShapes::CIRCLE, p, static_cast<uint16_t>(DisplaySettings::Yellow),
										  CombinationType::Subtraction, TAG_RING_GROUP);
			blacklistEntity(m_tagCircleInner);
		}

		auto& cam = m_ecs.getComponent<Transform>(m_mainCamera);
		const auto& ht = m_ecs.getComponent<Transform>(m_tagSelectedEntity);
		vec2 world(ht.position.x, ht.position.y);
		vec2 screen = ECS::Camera::worldPosition2DToScreenPosition(cam, world);

		auto& outer = m_ecs.getComponent<UIShape>(m_tagCircleOuter);
		outer.parameters[0] = screen.x;
		outer.parameters[1] = screen.y;
		outer.parameters[2] = TAG_OUTER_RADIUS;

		auto& inner = m_ecs.getComponent<UIShape>(m_tagCircleInner);
		inner.parameters[0] = screen.x;
		inner.parameters[1] = screen.y;
		inner.parameters[2] = TAG_INNER_RADIUS;

		// Update the tag label text
		if (m_tagLabelEntity != static_cast<Entity>(-1))
		{
			std::string currentTag = getEntityTag(m_tagSelectedEntity);
			auto& tx = m_ecs.getComponent<UITextRenderer>(m_tagLabelEntity);
			std::string newText = currentTag.empty() ? "tag: (none)" : ("tag: " + currentTag);
			if (tx.text != newText)
			{
				tx.text = newText;
				tx.dirty = true;
			}
		}

		// Handle "edit tag" button click
		// NOTE: std::cin is intentionally used here for console-based tag input
		// as required by the design of this editor scene.
		if (m_tagEditButton != static_cast<Entity>(-1))
		{
			auto& btn = m_ecs.getComponent<ShapeButton>(m_tagEditButton);
			if (btn.state == ButtonState::Down)
			{
				std::cout << "Enter new tag (empty to remove): " << std::flush;
				std::string newTag;
				std::getline(std::cin, newTag);
				if (newTag.empty())
				{
					removeTag(m_tagSelectedEntity);
				}
				else
				{
					tag(m_tagSelectedEntity, newTag);
				}
			}
		}
	}

	void onTagEditorRightClick()
	{
		m_tagSelectedEntity = pickBallAtMouse();
	}

	void onMaterialRightClick()
	{
		Entity hit = pickBallAtMouse();
		if (hit == static_cast<Entity>(-1))
			return;

		auto& dot = m_ecs.getComponent<Dot>(hit);
		dot.materialId = static_cast<unsigned int>(m_selectedMaterial);
	}

	bool hasTransform(Entity e)
	{
		auto arr = m_ecs.getComponentArray<Transform>();
		return arr->hasData(e);
	}

	bool hasUIShape(Entity e)
	{
		auto arr = m_ecs.getComponentArray<UIShape>();
		return arr->hasData(e);
	}

	bool hasShapeButton(Entity e)
	{
		auto arr = m_ecs.getComponentArray<ShapeButton>();
		return arr->hasData(e);
	}

	void loadMolecule(const std::string& path)
	{
		// Remember constraint count before loading so we can find new ones
		size_t prevConstraintCount = m_simulation2D.getDistanceConstraints().size();

		// Load the file — creates new entities / rigid bodies / constraints
		TagMap loadedTags = loadWeirdFile(path);

		// Apply loaded tags to the scene
		for (const auto& [name, entity] : loadedTags)
		{
			tag(entity, name);
		}

		// Collect new balls: find entities with both Dot and RigidBody2D
		// that are not already tracked
		auto dotArray = m_ecs.getComponentArray<Dot>();
		auto rbArray = m_ecs.getComponentArray<RigidBody2D>();

		// Build a set of already-tracked entities for fast lookup
		std::unordered_set<Entity> existingBalls;
		for (const auto& b : m_balls)
			existingBalls.insert(b.entity);

		// Map from simulationId → entity for newly loaded balls
		std::unordered_map<int, Entity> simIdToEntity;

		for (size_t i = 0; i < rbArray->getSize(); i++)
		{
			Entity e = rbArray->getEntityAtIdx(i);
			if (existingBalls.count(e))
				continue;
			if (!dotArray->hasData(e))
				continue;

			auto& rb = rbArray->getDataAtIdx(i);
			int simId = static_cast<int>(rb.simulationId);
			m_balls.push_back({e, simId});
			simIdToEntity[simId] = e;
		}

		// Collect new constraints and create visual links
		const auto& allConstraints = m_simulation2D.getDistanceConstraints();
		for (size_t i = prevConstraintCount; i < allConstraints.size(); i++)
		{
			const auto& dc = allConstraints[i];

			auto itA = simIdToEntity.find(dc.A);
			auto itB = simIdToEntity.find(dc.B);
			if (itA == simIdToEntity.end() || itB == simIdToEntity.end())
				continue;

			Entity a = itA->second;
			Entity b = itB->second;

			// K == 1.0 means distance constraint, otherwise spring
			LinkType type = (dc.K >= 1.0f) ? LinkType::Distance : LinkType::Spring;
			auto lineColor = (type == LinkType::Distance) ? DisplaySettings::Cyan : DisplaySettings::Orange;

			vec2 pa(0.0f), pb(0.0f);
			if (hasTransform(a) && hasTransform(b))
			{
				auto& ta = m_ecs.getComponent<Transform>(a);
				auto& tb = m_ecs.getComponent<Transform>(b);
				pa = vec2(ta.position.x, ta.position.y);
				pb = vec2(tb.position.x, tb.position.y);
			}

			float lineVars[8]{};
			computeScreenLineParams(pa, pb, lineVars);
			Entity line = addUIShape(DefaultShapes::LINE, lineVars, lineColor);

			auto& btn = m_ecs.addComponent<ShapeButton>(line);
			btn.clickPadding = 8.0f;
			btn.modifierAmount = 0.0f;

			blacklistEntity(line);

			m_links.push_back({a, b, dc.A, dc.B, dc.Distance, line, type});
		}

		std::cout << "[MoleculeEditor] Loaded " << simIdToEntity.size() << " balls and "
				  << (allConstraints.size() - prevConstraintCount) << " links from " << path << "\n";
	}

	void onEntityShapeCollision(WeirdEngine::EntityShapeCollisionEvent& event) override
	{
		event.raw.friction *= 100.0f;
	}
};
