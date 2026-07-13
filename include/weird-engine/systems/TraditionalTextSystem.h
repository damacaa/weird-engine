#pragma once
#include "weird-engine/ecs/ECS.h"
#include "weird-renderer/components/TraditionalTextComponent.h"
#include "weird-engine/components/Transform.h"
#include <vector>

namespace WeirdEngine {
    struct TraditionalTextData {
        Transform transform;
        TraditionalTextComponent* text;
    };

    class TraditionalTextSystem {
    public:
        static void update(ECSManager& ecs, std::vector<TraditionalTextData>& outData);
    };
}
