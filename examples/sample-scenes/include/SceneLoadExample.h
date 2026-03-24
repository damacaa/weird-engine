#pragma once

#include <weird-engine.h>
#include <random>
#include <cmath>

#include "globals.h"

using namespace WeirdEngine;

class SceneLoadExample : public Scene
{
public:
    SceneLoadExample(const PhysicsSettings& settings)
        : Scene(settings), m_rng(12345) {}

private:
    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    // Each spawn button maps to a shape type and whether it spawns a UIShape
    struct SpawnButton
    {
        Entity    entity;
        uint16_t  shapeType;   // DefaultShapes::Type value
        bool      isUIShape;
    };

    std::vector<SpawnButton> m_spawnButtons;

    // 16 material toggle entities
    std::array<Entity, 16> m_materialToggles{};
    int m_selectedMaterial = 1; // default: white (palette index 1)

    std::mt19937 m_rng;

    // -------------------------------------------------------------------------
    // Layout constants (screen-pixel coordinates, 800×800 display)
    // -------------------------------------------------------------------------
    static constexpr float k_btnRadius   = 20.0f;
    static constexpr float k_btnSpacing  = 52.0f;
    static constexpr float k_btnStartX   = 35.0f;
    static constexpr float k_worldRowY   = 775.0f; // world-shape spawn buttons
    static constexpr float k_uiRowY      = 715.0f; // UI-shape spawn buttons
    static constexpr float k_toggleY     = 50.0f;  // material toggles (bottom)
    static constexpr float k_toggleSpacingX = 47.0f;
    static constexpr float k_deletionThreshold = 5.0f; // world units

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    void onStart() override
    {
        m_debugInput = false; // disable PhysicsInteractionSystem so mouse is free
        m_debugFly   = true;

        m_ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;

        // --- World-shape spawn buttons (top row) ---
        for (int s = 0; s < DefaultShapes::SIZE; s++)
        {
            float px = k_btnStartX + s * k_btnSpacing;
            float params[8] = { px, k_worldRowY, k_btnRadius, static_cast<float>(1 + s % 15) };
            Entity e = addUIShape(DefaultShapes::CIRCLE, params, static_cast<uint16_t>(1 + s % 15));
            auto& button = m_ecs.addComponent<ShapeButton>(e);
            button.modifierAmount = 1.0f;
            m_spawnButtons.push_back({ e, static_cast<uint16_t>(s), false });
        }

        // --- 16 material toggles (bottom row) ---
        for (int m = 0; m < 16; m++)
        {
            float px = k_btnStartX + m * k_toggleSpacingX;
            float params[8] = { px, k_toggleY, k_btnRadius - 2.0f, static_cast<float>(m) };
            Entity e;
            UIShape& shape = addUIShape(DefaultShapes::CIRCLE, params, e);
            shape.material  = static_cast<uint16_t>(m);

            ShapeToggle& toggle = m_ecs.addComponent<ShapeToggle>(e);
            toggle.clickPadding         = k_btnRadius + 5.0f;
            toggle.parameterModifierMask.set(2); // grow radius when active
            toggle.modifierAmount       = 6.0f;

            m_materialToggles[m] = e;
        }

        // Start with material 1 (white) active
        m_ecs.getComponent<ShapeToggle>(m_materialToggles[m_selectedMaterial]).active = true;
    }

    void onUpdate(float delta) override
    {
        g_cameraPositon = m_ecs.getComponent<Transform>(m_mainCamera).position;

        if (Input::GetKeyDown(Input::Q))
            setSceneComplete();

        if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::S))
            saveScene("example.weird");

        // --- Exclusive material toggle selection ---------------------------
        syncMaterialToggles();

        // --- Spawn-button left-click handling  (before isUIClick() is consumed) ---
        if (Input::GetMouseButtonDown(Input::LeftClick))
        {
            for (auto& btn : m_spawnButtons)
            {
                auto& shape = m_ecs.getComponent<ShapeButton>(btn.entity);
                if (shape.state == ButtonState::Down)
                {
                    if (btn.isUIShape)
                        spawnRandomUIShape(btn.shapeType);
                    else
                        spawnRandomWorldShape(btn.shapeType);

                    Input::flagUIClick();
                    break;
                }
            }

            if (!Input::isUIClick())
            {
                auto& cam    = m_ecs.getComponent<Transform>(m_mainCamera);
                vec2 worldPos = ECS::Camera::screenPositionToWorldPosition2D(
                    cam, vec2(Input::GetMouseX(), Input::GetMouseY()));

                if (!tryDeleteCustomShapeAt(worldPos))
                    spawnPhysicsEntity(worldPos);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    // Enforce exclusive toggle selection and update m_selectedMaterial
    void syncMaterialToggles()
    {
        // Find first toggle that was just activated (active=true, state==Down)
        // If found, disable all others
        int justActivated = -1;
        for (int i = 0; i < 16; i++)
        {
            auto& t = m_ecs.getComponent<ShapeToggle>(m_materialToggles[i]);
            if (t.active && t.state == ButtonState::Down)
            {
                justActivated = i;
                break;
            }
        }

        if (justActivated >= 0)
        {
            m_selectedMaterial = justActivated;
            for (int i = 0; i < 16; i++)
            {
                if (i == justActivated) continue;
                auto& t = m_ecs.getComponent<ShapeToggle>(m_materialToggles[i]);
                t.active = false;
            }
        }
        else
        {
            // Ensure something is always selected; find active one
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

    // Returns a random float in [lo, hi]
    float rnd(float lo, float hi)
    {
        std::uniform_real_distribution<float> dist(lo, hi);
        return dist(m_rng);
    }

    // Camera centre in world coordinates
    vec2 camCentre()
    {
        auto& t = m_ecs.getComponent<Transform>(m_mainCamera);
        return vec2(t.position.x, t.position.y);
    }

    // Fill 8 parameters with reasonable random values for each shape type,
    // centred on the camera centre
    void fillRandomParams(uint16_t shapeType, float params[8])
    {
        std::memset(params, 0, sizeof(float) * 8);
        vec2 c = camCentre();

        switch (shapeType)
        {
        case DefaultShapes::CIRCLE:
            params[0] = c.x + rnd(-2.0f, 2.0f); // posX
            params[1] = c.y + rnd(-2.0f, 2.0f); // posY
            params[2] = rnd(0.6f, 2.5f);         // radius
            break;

        case DefaultShapes::BOX:
            params[0] = c.x + rnd(-3.0f, 3.0f); // posX
            params[1] = c.y + rnd(-3.0f, 3.0f); // posY
            params[2] = rnd(0.5f, 3.0f);         // half-sizeX
            params[3] = rnd(0.5f, 3.0f);         // half-sizeY
            break;

        case DefaultShapes::BOX_LINE:
            params[0] = c.x + rnd(-3.0f, 3.0f); // posX
            params[1] = c.y + rnd(-3.0f, 3.0f); // posY
            params[2] = rnd(1.0f, 4.0f);         // half-sizeX
            params[3] = rnd(1.0f, 4.0f);         // half-sizeY
            params[4] = rnd(0.05f, 0.4f);        // thickness
            break;

        case DefaultShapes::LINE:
            params[0] = c.x + rnd(-3.0f, -0.5f); // A.x
            params[1] = c.y + rnd(-2.0f, 2.0f);  // A.y
            params[2] = c.x + rnd(0.5f, 3.0f);   // B.x
            params[3] = c.y + rnd(-2.0f, 2.0f);  // B.y
            params[4] = rnd(0.05f, 0.3f);         // width
            break;

        case DefaultShapes::RAMP:
            params[0] = c.x + rnd(-2.0f, 2.0f); // posX
            params[1] = c.y + rnd(-2.0f, 2.0f); // posY
            params[2] = rnd(1.0f, 4.0f);         // width
            params[3] = rnd(1.0f, 4.0f);         // height
            params[4] = rnd(-1.5f, 1.5f);        // skew
            break;

        case DefaultShapes::SINE:
            params[0] = rnd(1.0f, 5.0f);         // amplitude (world units)
            params[1] = rnd(0.05f, 0.25f);        // period
            params[2] = rnd(0.3f, 1.5f);          // speed
            params[3] = c.y + rnd(-5.0f, 5.0f);  // y-offset
            break;

        case DefaultShapes::STAR:
            params[0] = c.x + rnd(-2.0f, 2.0f); // posX
            params[1] = c.y + rnd(-2.0f, 2.0f); // posY
            params[2] = rnd(0.8f, 2.5f);         // radius
            params[3] = rnd(0.1f, 0.6f);         // displacement strength
            params[4] = rnd(3.0f, 8.0f);         // star points
            params[5] = rnd(0.5f, 2.0f);         // spin speed
            break;

        default:
            params[0] = c.x;
            params[1] = c.y;
            params[2] = 1.0f;
            break;
        }
    }

    // Spawn a world CustomShape with random params
    void spawnRandomWorldShape(uint16_t shapeType)
    {
        float params[8] = {};
        fillRandomParams(shapeType, params);
        addShape(shapeType, params, static_cast<uint16_t>(m_selectedMaterial));
    }

    // Spawn a UIShape with random params (screen-space circle buttons use pixel coords)
    void spawnRandomUIShape(uint16_t shapeType)
    {
        // For UI shapes the coordinates are in screen pixels (0-800).
        // Place it in the middle of the screen with moderate random offsets.
        float params[8] = {};
        std::memset(params, 0, sizeof(params));

        constexpr float scr = 800.0f;
        constexpr float mid = scr * 0.5f;

        switch (shapeType)
        {
        case DefaultShapes::CIRCLE:
            params[0] = mid + rnd(-100.0f, 100.0f);
            params[1] = mid + rnd(-100.0f, 100.0f);
            params[2] = rnd(20.0f, 80.0f);
            break;

        case DefaultShapes::BOX:
            params[0] = mid + rnd(-100.0f, 100.0f);
            params[1] = mid + rnd(-100.0f, 100.0f);
            params[2] = rnd(15.0f, 80.0f);
            params[3] = rnd(15.0f, 80.0f);
            break;

        case DefaultShapes::BOX_LINE:
            params[0] = mid + rnd(-100.0f, 100.0f);
            params[1] = mid + rnd(-100.0f, 100.0f);
            params[2] = rnd(20.0f, 100.0f);
            params[3] = rnd(20.0f, 100.0f);
            params[4] = rnd(2.0f, 12.0f);
            break;

        case DefaultShapes::LINE:
            params[0] = mid + rnd(-150.0f, -20.0f);
            params[1] = mid + rnd(-80.0f, 80.0f);
            params[2] = mid + rnd(20.0f, 150.0f);
            params[3] = mid + rnd(-80.0f, 80.0f);
            params[4] = rnd(2.0f, 10.0f);
            break;

        case DefaultShapes::RAMP:
            params[0] = mid + rnd(-100.0f, 100.0f);
            params[1] = mid + rnd(-100.0f, 100.0f);
            params[2] = rnd(20.0f, 100.0f);
            params[3] = rnd(20.0f, 100.0f);
            params[4] = rnd(-40.0f, 40.0f);
            break;

        case DefaultShapes::SINE:
            params[0] = rnd(30.0f, 100.0f);      // amplitude
            params[1] = rnd(0.005f, 0.03f);       // period (smaller in screen space)
            params[2] = rnd(0.3f, 1.5f);
            params[3] = mid + rnd(-100.0f, 100.0f);
            break;

        case DefaultShapes::STAR:
            params[0] = mid + rnd(-100.0f, 100.0f);
            params[1] = mid + rnd(-100.0f, 100.0f);
            params[2] = rnd(20.0f, 80.0f);
            params[3] = rnd(5.0f, 25.0f);
            params[4] = rnd(3.0f, 8.0f);
            params[5] = rnd(0.5f, 2.0f);
            break;

        default:
            params[0] = mid;
            params[1] = mid;
            params[2] = 30.0f;
            break;
        }

        addUIShape(shapeType, params, static_cast<uint16_t>(m_selectedMaterial));
    }

    // Right-click: find the nearest world CustomShape under the cursor and delete it.
    // Returns true if a shape was deleted.
    bool tryDeleteCustomShapeAt(vec2 worldPos)
    {
        auto csArray  = m_ecs.getComponentArray<CustomShape>();
        auto uiArray  = m_ecs.getComponentArray<UIShape>();

        float  bestDist   = k_deletionThreshold;
        Entity bestEntity = static_cast<Entity>(-1);

        for (size_t i = 0; i < csArray->getSize(); i++)
        {
            Entity e = csArray->getEntityAtIdx(i);
            if (uiArray->hasData(e)) continue; // skip UIShapes

            auto& cs = csArray->getDataAtIdx(i);

            float p[11] = {};
            std::copy(std::begin(cs.parameters), std::end(cs.parameters), p);
            p[9]  = worldPos.x;
            p[10] = worldPos.y;
            m_sdfs[cs.distanceFieldId]->propagateValues(p);
            float dist = m_sdfs[cs.distanceFieldId]->getValue();

            if (dist < bestDist)
            {
                bestDist   = dist;
                bestEntity = e;
            }
        }

        if (bestEntity != static_cast<Entity>(-1))
        {
            m_ecs.destroyEntity(bestEntity);
            m_sdfRenderSystem2D.shaderNeedsUpdate() = true;
            return true;
        }
        return false;
    }

    // Right-click on empty world: spawn an SDFRenderer + RigidBody2D ball
    void spawnPhysicsEntity(vec2 worldPos)
    {
        Entity e = m_ecs.createEntity();

        Transform& t  = m_ecs.addComponent<Transform>(e);
        t.position     = vec3(worldPos.x, worldPos.y, 0.0f);

        SDFRenderer& sdf   = m_ecs.addComponent<SDFRenderer>(e);
        sdf.materialId      = static_cast<unsigned int>(m_selectedMaterial);

        m_ecs.addComponent<RigidBody2D>(e);
    }
};
