#pragma once

#include <weird-engine.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
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
        HeadSelect
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
    };

    std::vector<BallInfo> m_balls;
    std::vector<DistanceLink> m_links;
    std::array<Entity, 16> m_materialToggles{};

    Entity m_draggedBall = static_cast<Entity>(-1);
    int m_draggedSimulationId = -1;
    Entity m_constraintStartBall = static_cast<Entity>(-1);
    bool m_keepFixedAfterDrag = false;
    bool m_rightWasDown = false;
    bool m_gravityEnabled = false;
    RightMouseMode m_rightMouseMode = RightMouseMode::None;
    int m_selectedMaterial = 1;

    ToolMode m_toolMode = ToolMode::Drag;
    std::array<Entity, 3> m_toolToggles{};
    Entity m_gravityToggleEntity = static_cast<Entity>(-1);
    Entity m_headEntity = static_cast<Entity>(-1);
    Entity m_headCircleOuter = static_cast<Entity>(-1);
    Entity m_headCircleInner = static_cast<Entity>(-1);

    static constexpr float BALL_HIT_RADIUS = 0.9f;
    static constexpr float LINE_WIDTH = 3.5f;
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
    static constexpr float HEAD_OUTER_RADIUS = 30.0f;
    static constexpr float HEAD_INNER_RADIUS = 25.0f;
    static constexpr int   HEAD_RING_GROUP = 8;

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

		{
			float boundsVars[8]{0.0f, 0.0f, 3000.0f};
			Entity outside = addShape(DefaultShapes::CIRCLE, boundsVars, 17, CombinationType::Addition);

            float boundsVars2[8]{0.0f, 0.0f, 20.0f, 20.0f};
			Entity inside = addShape(DefaultShapes::BOX, boundsVars2, DisplaySettings::Black, CombinationType::Subtraction);

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
                saveScene(ASSETS_PATH + fileName);
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
        updateHeadIndicator();
        removeFallenBalls();
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
        {
            return std::find(toDelete.begin(), toDelete.end(), e) != toDelete.end();
        };

        for (Entity e : toDelete)
        {
            m_ecs.destroyEntity(e);
        }

        m_links.erase(
            std::remove_if(m_links.begin(), m_links.end(),
                           [&](const DistanceLink& link)
                           {
                               bool remove = shouldDelete(link.a) || shouldDelete(link.b);
                               if (remove)
                               {
                                   m_ecs.destroyEntity(link.lineEntity);
                               }
                               return remove;
                           }),
            m_links.end());

        m_balls.erase(
            std::remove_if(m_balls.begin(), m_balls.end(),
                           [&](const BallInfo& b)
                           {
                               return shouldDelete(b.entity);
                           }),
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

        if (shouldDelete(m_headEntity))
        {
            m_headEntity = static_cast<Entity>(-1);
        }
    }

    void buildMaterialPalette()
    {
        for (int i = 0; i < 16; i++)
        {
            float px = START_X + i * MAT_SPACING;
            float p[8]{ px, MAT_Y, BTN_SIZE - 4.0f };
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
        const char* labels[] = { "drag", "spring", "head" };
        for (int i = 0; i < 3; i++)
        {
            float y = (Display::height - TOOL_Y_START) - (i * TOOL_SPACING);
            float p[8]{ TOOL_X, y, TOOL_BTN_HALF, TOOL_BTN_HALF };
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

        float starP[8]{ Display::width - GRAV_Y, Display::height - GRAV_Y, GRAV_Y * 0.5f, 5.0f, 10.0f, 0.0f };
        m_gravityToggleEntity = addUIShape(DefaultShapes::STAR, starP, static_cast<uint16_t>(2));
        auto& gravTog = m_ecs.addComponent<ShapeToggle>(m_gravityToggleEntity);
        gravTog.clickPadding = 18.0f;
        // gravTog.parameterModifierMask.set(2);
        gravTog.parameterModifierMask.set(5);
        gravTog.modifierAmount = 10.0f;
        blacklistEntity(m_gravityToggleEntity);
    }

    void syncToolbar()
    {
        int activated = -1;
        for (int i = 0; i < 3; i++)
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
            for (int i = 0; i < 3; i++)
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
                m_simulation2D.setDamping(0.001f);
            }
            else
            {
                m_simulation2D.setGravity(0.0f);
                m_simulation2D.setDamping(1.0f);
            }
        }
    }

    void spawnBallAtMouse()
    {
        auto& cam = m_ecs.getComponent<Transform>(m_mainCamera);
        vec2 world = ECS::Camera::screenPositionToWorldPosition2D(
            cam, vec2(Input::GetMouseX(), Input::GetMouseY()));

        Entity e = m_ecs.createEntity();

        auto& t = m_ecs.addComponent<Transform>(e);
        t.position = vec3(world.x, world.y, 0.0f);
        t.isDirty = true;

        auto& sdf = m_ecs.addComponent<Dot>(e);
        sdf.materialId = static_cast<unsigned int>(m_selectedMaterial);

        auto& rb = m_ecs.addComponent<RigidBody2D>(e);

        m_balls.push_back({ e, static_cast<int>(rb.simulationId) });
    }

    vec2 getMouseWorldPosition()
    {
        auto& cam = m_ecs.getComponent<Transform>(m_mainCamera);
        return ECS::Camera::screenPositionToWorldPosition2D(
            cam, vec2(Input::GetMouseX(), Input::GetMouseY()));
    }

    void handleRightMouseDragInput()
    {
        bool rightDown = Input::GetMouseButton(Input::RightClick);

        if (rightDown && !m_rightWasDown)
        {
            if (m_toolMode == ToolMode::Spring)
                onConstraintStart();
            else if (m_toolMode == ToolMode::HeadSelect)
                onHeadSelectRightClick();
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
        m_simulation2D.setPosition(static_cast<SimulationID>(m_draggedSimulationId), getMouseWorldPosition());
    }

    void onRightDragUpdate()
    {
        if (m_draggedBall == static_cast<Entity>(-1) || m_draggedSimulationId < 0)
            return;

        if (Input::GetKeyDown(Input::F))
        {
            m_keepFixedAfterDrag = true;
        }

        m_simulation2D.setPosition(static_cast<SimulationID>(m_draggedSimulationId), getMouseWorldPosition());
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
            addDistanceConstraint(m_constraintStartBall, hit);
        }

        m_constraintStartBall = static_cast<Entity>(-1);
    }

    Entity pickBallAtMouse()
    {
        auto& cam = m_ecs.getComponent<Transform>(m_mainCamera);
        vec2 world = ECS::Camera::screenPositionToWorldPosition2D(
            cam, vec2(Input::GetMouseX(), Input::GetMouseY()));

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

    void addDistanceConstraint(Entity a, Entity b)
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

        m_simulation2D.addSpring(idA, idB, CONSTRAINT_STIFFNESS, restDistance);

        float lineVars[8]{};
        computeScreenLineParams(pa, pb, lineVars);
        Entity line = addUIShape(DefaultShapes::LINE, lineVars, DisplaySettings::Yellow);

        auto& btn = m_ecs.addComponent<ShapeButton>(line);
        btn.clickPadding = 8.0f;
        btn.modifierAmount = 0.0f;

        blacklistEntity(line);

        m_links.push_back({ a, b, idA, idB, restDistance, line });
    }

    void handleConstraintLineClicks()
    {
        for (auto& link : m_links)
        {
            if (!hasShapeButton(link.lineEntity))
                continue;

            auto& btn = m_ecs.getComponent<ShapeButton>(link.lineEntity);
            if (btn.state != ButtonState::Down)
                continue;

            std::cout << "Constraint distance (current " << link.restDistance << "): " << std::flush;

            float v = 0.0f;
            if (!(std::cin >> v))
            {
                std::cin.clear();
                std::cin.ignore(10000, '\n');
                continue;
            }

            if (v <= 0.0f)
                continue;

            link.restDistance = v;
            m_simulation2D.setDistanceConstraintDistance(
                static_cast<SimulationID>(link.simulationIdA),
                static_cast<SimulationID>(link.simulationIdB),
                link.restDistance);
        }
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
            ui.isDirty = true;
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

    void updateHeadIndicator()
    {
        if (m_headEntity == static_cast<Entity>(-1))
        {
            if (m_headCircleOuter != static_cast<Entity>(-1))
            {
                m_ecs.getComponent<UIShape>(m_headCircleOuter).parameters[2] = 0.0f;
                m_ecs.getComponent<UIShape>(m_headCircleOuter).isDirty = true;
                m_ecs.getComponent<UIShape>(m_headCircleInner).parameters[2] = 0.0f;
                m_ecs.getComponent<UIShape>(m_headCircleInner).isDirty = true;
            }
            return;
        }

        if (!hasTransform(m_headEntity))
        {
            m_headEntity = static_cast<Entity>(-1);
            return;
        }

        // Lazy-initialize the ring indicator shapes on first use
		if (m_headCircleOuter == static_cast<Entity>(-1))
		{
			float p[8]{};
			m_headCircleOuter = addUIShape(DefaultShapes::CIRCLE, p, static_cast<uint16_t>(DisplaySettings::Yellow),
										   CombinationType::Addition, HEAD_RING_GROUP);
			blacklistEntity(m_headCircleOuter);

			m_headCircleInner = addUIShape(DefaultShapes::CIRCLE, p, static_cast<uint16_t>(DisplaySettings::Yellow),
										   CombinationType::Subtraction, HEAD_RING_GROUP);
			blacklistEntity(m_headCircleInner);
		}

		auto& cam = m_ecs.getComponent<Transform>(m_mainCamera);
        const auto& ht = m_ecs.getComponent<Transform>(m_headEntity);
        vec2 world(ht.position.x, ht.position.y);
        vec2 screen = ECS::Camera::worldPosition2DToScreenPosition(cam, world);

        auto& outer = m_ecs.getComponent<UIShape>(m_headCircleOuter);
        outer.parameters[0] = screen.x;
        outer.parameters[1] = screen.y;
        outer.parameters[2] = HEAD_OUTER_RADIUS;
        outer.isDirty = true;

        auto& inner = m_ecs.getComponent<UIShape>(m_headCircleInner);
        inner.parameters[0] = screen.x;
        inner.parameters[1] = screen.y;
        inner.parameters[2] = HEAD_INNER_RADIUS;
        inner.isDirty = true;
    }

    void onHeadSelectRightClick()
    {
        m_headEntity = pickBallAtMouse();
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
};
