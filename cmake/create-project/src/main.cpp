#include <iostream>              // Only needed for debug output, can remove if unused
#include <weird-engine.h>       // Main engine include

using namespace WeirdEngine;

// Example scene demonstrating how to create a rope of connected circles using springs.
class RopeScene : public Scene
{
public:
    RopeScene() : Scene() {}

private:
    Entity m_star;
    double m_lastSpawnTime = 0.0;

    void onStart() override
    {
        // Load fixed-width font for on-screen text
        loadFont(ENGINE_PATH "/src/weird-renderer/fonts/small.bmp", 4, 5,
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ[]{}abcdefghijklmnopqrstuvwxyz\\/<>0123456789!\" ");

        print("Nice rope dude!");

        constexpr int numBalls = 60;
        constexpr int rowWidth = 30;
        constexpr float startY = 20.0f + (numBalls / rowWidth);
        constexpr float stiffness = 20000000.0f;

        // Create 2D rigid bodies in a rope/grid layout
        for (int i = 0; i < numBalls; ++i)
        {
            float x = static_cast<float>(i % rowWidth);
            float y = startY - static_cast<float>(i / rowWidth);
            int material = 4 + (i % 12);

            Entity entity = m_ecs.createEntity();

            auto& t = m_ecs.addComponent<Transform>(entity);
            t.position = vec3(x + 0.5f, y + 0.5f, 0.0f);

            auto& sdf = m_ecs.addComponent<SDFRenderer>(entity);
            sdf.materialId = material;

            m_ecs.addComponent<RigidBody2D>(entity);
        }

        // Connect balls with springs (down and right)
        for (int i = 0; i < numBalls; ++i)
        {
            if (i + rowWidth < numBalls) // Down
                m_simulation2D.addSpring(i, i + rowWidth, stiffness);

            if ((i + 1) % rowWidth != 0) // Right
                m_simulation2D.addSpring(i, i + 1, stiffness);
        }

        // Fix top corners
        if (numBalls >= rowWidth)
        {
            m_simulation2D.fix(0);
            m_simulation2D.fix(rowWidth - 1);
        }

        // Add base shapes (walls, ground, custom)
        float vars0[8] = { 1.0f, 0.5f }; // Floor shape
        addShape(0, vars0);

        float vars1[8] = { 25.0f, 10.0f, 5.0f, 0.5f, 13.0f, 5.0f }; // Custom shape
        m_star = addShape(1, vars1);

        float vars2[8] = { 30.5f, 3.5f, 30.0f, 3.0f };
        addScreenSpaceShape(3, vars2); // UI overlay shape
    }

    void throwBalls(ECSManager& ecs, Simulation2D& sim)
    {
        if (sim.getSimulationTime() <= m_lastSpawnTime + 0.1)
            return;

        constexpr int amount = 10;
        for (int i = 0; i < amount; ++i)
        {
            float y = 60.0f + (1.2f * i);

            Entity entity = ecs.createEntity();

            auto& t = ecs.addComponent<Transform>(entity);
            t.position = vec3(0.5f, y + 0.5f, 0.0f);

            auto& sdf = ecs.addComponent<SDFRenderer>(entity);
            sdf.materialId = 4 + ecs.getComponentArray<SDFRenderer>()->getSize() % 12;

            auto& rb = ecs.addComponent<RigidBody2D>(entity);
            sim.addForce(rb.simulationId, vec2(20.0f, 0.0f));
        }

        m_lastSpawnTime = sim.getSimulationTime();
    }

    void onUpdate() override
    {
        // Animate custom shape over time
        {
            auto& cs = m_ecs.getComponent<CustomShape>(m_star);
            cs.m_parameters[4] = static_cast<int>(std::floor(m_simulation2D.getSimulationTime())) % 5 + 2;
            cs.m_parameters[3] = std::sin(3.1416f * m_simulation2D.getSimulationTime());
            cs.m_isDirty = true;
        }

        if (Input::GetKey(Input::E))
            throwBalls(m_ecs, m_simulation2D);

        if (Input::GetKeyDown(Input::M))
        {
            auto& cam = m_ecs.getComponent<Transform>(m_mainCamera);
            vec2 screen = { Input::GetMouseX(), Input::GetMouseY() };
            vec2 world = ECS::Camera::screenPositionToWorldPosition2D(cam, screen);

            float vars[8] = { world.x, world.y, 5.0f, 0.5f, 13.0f, 5.0f };
            addShape(1, vars);
        }

        if (Input::GetKeyDown(Input::N))
        {
            // Duplicate last SDF, add new shape with it
            m_sdfs.push_back(m_sdfs.back());
            m_simulation2D.setSDFs(m_sdfs);

            auto& cam = m_ecs.getComponent<Transform>(m_mainCamera);
            vec2 screen = { Input::GetMouseX(), Input::GetMouseY() };
            vec2 world = ECS::Camera::screenPositionToWorldPosition2D(cam, screen);

            float vars[8] = { world.x, world.y, 5.0f, 7.5f, 1.0f };
            addShape(m_sdfs.size() - 1, vars);
        }

        if (Input::GetKeyDown(Input::K))
        {
            auto components = m_ecs.getComponentArray<CustomShape>();
            int id = components->getSize() - 1;

            m_simulation2D.removeShape(components->getDataAtIdx(id));
            m_ecs.destroyEntity(components->getDataAtIdx(id).Owner);
        }
    }

    void onRender() override
    {
        // Optional: draw debug overlays here
    }
};

int main()
{
    SceneManager& sceneManager = SceneManager::getInstance();
    sceneManager.registerScene<RopeScene>("rope");
    start(sceneManager);
}
