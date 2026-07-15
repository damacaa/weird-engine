#include "weird-engine/systems/TraditionalTextSystem.h"
#include <iostream>

namespace WeirdEngine {
    template <typename T>
    void updateInternal(ECSManager& ecs, std::vector<TraditionalTextData>& outData) {
        auto textArray = ecs.getComponentManager<T>()->getComponentArray();
        if (!textArray) return;
        
        auto transformArray = ecs.getComponentManager<Transform>()->getComponentArray();

        for (size_t i = 0; i < textArray->getSize(); i++) {
            auto& textComp = textArray->getDataAtIdx(i);
            
            TraditionalTextData data;
            data.transform = transformArray->getDataFromEntity(textComp.Owner);
            data.text = &textComp;
            outData.push_back(data);
        }
    }

    void TraditionalTextSystem::update(ECSManager& ecs, std::vector<TraditionalTextData>& outData) {
        updateInternal<TraditionalTextComponent>(ecs, outData);
    }
    
    void TraditionalTextSystem::updateUI(ECSManager& ecs, std::vector<TraditionalTextData>& outData) {
        updateInternal<UITraditionalTextComponent>(ecs, outData);
    }
}

