#pragma once

#include <weird-engine.h>
#include <cmath>
#include <string>

using namespace WeirdEngine;

// ---------------------------------------------------------------------------
// MoleculeScene
//
// Demonstrates the entity tag system.
//
// A "molecule" is a group of three particles (SDFRenderer + RigidBody2D)
// arranged in a triangle and joined by springs.
//
// Controls:
//   Left-click on a particle  – select it (highlights it, shows its tag)
//   Left-click the "EDIT TAG" button – opens a terminal prompt to type a new
//                                      tag for the selected particle
//   [S] key                   – save current tags to "molecules.weird"
//   Camera: middle-click drag  to pan, scroll wheel to zoom
// ---------------------------------------------------------------------------

namespace
{
    constexpr float SPRING_STIFFNESS   = 800000.0f; // N/m – keeps molecule shape rigid
    constexpr float MOLECULE_RADIUS    = 1.3f;       // distance from centre to particle
    constexpr float BUTTON_CORNER_RADIUS = 0.3f;    // rounded-corner radius of the UI button
    constexpr float TWO_PI             = 6.28318530718f;
}
class MoleculeScene : public Scene
{
public:
    MoleculeScene() : Scene() {}

private:
    // ---- Molecule data -------------------------------------------------------
    struct Molecule
    {
        std::vector<Entity> particles;
    };

    static constexpr Entity INVALID_ENTITY = MAX_ENTITIES;

    std::vector<Molecule> m_molecules;

    // ---- Selection / tag editor state ----------------------------------------
    Entity  m_selectedEntity  = INVALID_ENTITY;
    int     m_selectedMaterial = 0;      // original material before highlight

    // UI button (box SDF) for triggering tag editing
    Entity  m_buttonEntity    = INVALID_ENTITY;
    float   m_buttonCX        = 0.0f;
    float   m_buttonCY        = 0.0f;
    float   m_buttonHW        = 0.0f;   // half-width
    float   m_buttonHH        = 0.0f;   // half-height

    // ---- Scene setup ---------------------------------------------------------
    void onStart() override
    {
        m_debugInput = false; // disable default physics-interaction spawner

        // Load font for on-screen text
        loadFont(ENGINE_PATH "/src/weird-renderer/fonts/small.bmp", 4, 5,
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ[]{}abcdefghijklmnopqrstuvwxyz\\/<>0123456789!\" ");

        // Floor (sine wave, amplitude=0 → flat)
        float floorVars[8] = { 0.0f, 1.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        addShape(0, floorVars, 3);

        // Create three molecules at different positions
        createMolecule(8.0f,  14.0f, 0);
        createMolecule(18.0f, 14.0f, 3);
        createMolecule(13.0f, 20.0f, 6);

        // ---- UI "EDIT TAG" button (box SDF, no physics collision) ----------
        m_buttonCX = 24.0f;
        m_buttonCY = 4.0f;
        m_buttonHW = 4.5f;
        m_buttonHH = 1.2f;

        float btnVars[8] = { m_buttonCX, m_buttonCY, m_buttonHW, m_buttonHH, BUTTON_CORNER_RADIUS, 0.0f, 0.0f, 0.0f };
        m_buttonEntity = addShape(3, btnVars, 6, CombinationType::Addition, false);

        // Restore tags saved in a previous run (if available)
        auto savedTags = loadTagsFromFile("molecules.weird");
        for (const auto& [name, entity] : savedTags)
        {
            tag(entity, name);
        }
        if (!savedTags.empty())
        {
            std::cout << "[MoleculeScene] Loaded " << savedTags.size()
                      << " tag(s) from molecules.weird\n";
        }

        // Render the initial tag display (instructions + tag line if a tag was loaded)
        updateTagDisplay();
    }

    // ---- Molecule helpers ----------------------------------------------------
    void createMolecule(float cx, float cy, int colorOffset)
    {
        Molecule mol;

        for (int i = 0; i < 3; ++i)
        {
            float angle = TWO_PI * i / 3.0f;

            Entity e = m_ecs.createEntity();

            auto& t = m_ecs.addComponent<Transform>(e);
            t.position = vec3(cx + MOLECULE_RADIUS * std::cos(angle),
                              cy + MOLECULE_RADIUS * std::sin(angle), 0.0f);

            auto& sdf = m_ecs.addComponent<SDFRenderer>(e);
            sdf.materialId = 4 + (colorOffset + i) % 12;

            m_ecs.addComponent<RigidBody2D>(e);
            mol.particles.push_back(e);
        }

        // Connect all pairs of particles with stiff springs
        auto& rb0 = m_ecs.getComponent<RigidBody2D>(mol.particles[0]);
        auto& rb1 = m_ecs.getComponent<RigidBody2D>(mol.particles[1]);
        auto& rb2 = m_ecs.getComponent<RigidBody2D>(mol.particles[2]);
        m_simulation2D.addSpring(rb0.simulationId, rb1.simulationId, SPRING_STIFFNESS);
        m_simulation2D.addSpring(rb1.simulationId, rb2.simulationId, SPRING_STIFFNESS);
        m_simulation2D.addSpring(rb0.simulationId, rb2.simulationId, SPRING_STIFFNESS);

        m_molecules.push_back(std::move(mol));
    }

    // Return the entity whose RigidBody2D has the given simulation ID,
    // or INVALID_ENTITY if not found.
    Entity findEntityBySimId(SimulationID simId)
    {
        auto rbArray = m_ecs.getComponentArray<RigidBody2D>();
        for (size_t i = 0; i < (size_t)rbArray->getSize(); ++i)
        {
            const auto& rb = rbArray->getDataAtIdx(i);
            if (rb.simulationId == simId)
                return rb.Owner;
        }
        return INVALID_ENTITY;
    }

    // ---- UI helpers ---------------------------------------------------------
    bool isPointInButton(vec2 pos) const
    {
        return std::abs(pos.x - m_buttonCX) <= m_buttonHW &&
               std::abs(pos.y - m_buttonCY) <= m_buttonHH;
    }

    // Highlight / de-highlight the selected particle by changing its material.
    void setSelectionHighlight(Entity entity, bool highlighted)
    {
        if (!m_ecs.getComponentManager<SDFRenderer>()->hasComponent(entity))
            return;

        auto& sdf = m_ecs.getComponent<SDFRenderer>(entity);
        if (highlighted)
        {
            m_selectedMaterial = sdf.materialId;
            sdf.materialId = 15; // bright highlight colour
        }
        else
        {
            sdf.materialId = m_selectedMaterial;
        }
    }

    // Rebuild the dynamic tag-display text (appears just above the instructions).
    void updateTagDisplay()
    {
        // Reprint both the static instruction line and the current tag line.
        clearText();

        print("SELECT DOT  CLICK BUTTON TO EDIT TAG  S SAVES");

        if (m_selectedEntity != INVALID_ENTITY)
        {
            std::string t = getTag(m_selectedEntity);
            std::string line = "TAG: " + (t.empty() ? "(none)" : t);
            printAtRow(line, 1);
        }
    }

    // ---- Per-frame update ---------------------------------------------------
    void onUpdate(float delta) override
    {
        auto& camTransform = m_ecs.getComponent<Transform>(m_mainCamera);

        if (Input::GetMouseButtonDown(Input::LeftClick))
        {
            vec2 worldPos = ECS::Camera::screenPositionToWorldPosition2D(
                camTransform,
                vec2(Input::GetMouseX(), Input::GetMouseY()));

            if (isPointInButton(worldPos))
            {
                // ---- "EDIT TAG" button clicked --------------------------------
                if (m_selectedEntity != INVALID_ENTITY)
                {
                    std::string currentTag = getTag(m_selectedEntity);
                    std::cout << "\n[Tag editor] Entity " << m_selectedEntity
                              << "  current tag: "
                              << (currentTag.empty() ? "(none)" : currentTag)
                              << "\nEnter new tag (empty to remove): ";
                    std::string newTag;
                    std::getline(std::cin, newTag);

                    // tag() handles uniqueness and empty-string removal
                    tag(m_selectedEntity, newTag);

                    if (newTag.empty())
                        std::cout << "[Tag editor] Tag removed.\n";
                    else
                        std::cout << "[Tag editor] Tag set to \"" << newTag << "\".\n";

                    updateTagDisplay();
                }
                else
                {
                    std::cout << "[Tag editor] No particle selected. "
                                 "Click a dot first.\n";
                }
            }
            else
            {
                // ---- Try to select a particle --------------------------------
                SimulationID simId = m_simulation2D.raycast(worldPos);

                // De-highlight the previously selected entity
                if (m_selectedEntity != INVALID_ENTITY)
                    setSelectionHighlight(m_selectedEntity, false);

                if (simId < m_simulation2D.getSize())
                {
                    m_selectedEntity = findEntityBySimId(simId);
                    if (m_selectedEntity != INVALID_ENTITY)
                        setSelectionHighlight(m_selectedEntity, true);
                }
                else
                {
                    m_selectedEntity = INVALID_ENTITY;
                }

                updateTagDisplay();
            }
        }

        // [S] – save tags to file
        if (Input::GetKeyDown(Input::S))
        {
            saveTagsToFile("molecules.weird");
        }
    }

    // ---- Persistence --------------------------------------------------------
    // When this scene is started from a .weird file the base class applies the
    // saved tags and then calls this override so we can refresh the display.
    void onStartFromFile(const std::map<std::string, Entity>& tags) override
    {
        updateTagDisplay();
    }
};
