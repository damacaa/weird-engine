
#pragma once
namespace WeirdEngine {
    struct PhysicsSettings {
        float gravity = 9.8f;
        float damping = 0.99f;
        float simulationFrequency = 100.0f;
        int relaxationSteps = 10;
    };
}
