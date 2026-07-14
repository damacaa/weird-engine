#pragma once
#include "weird-engine/ecs/Component.h"
#include "weird-renderer/resources/Texture.h"
#include <string>
#include <memory>

namespace WeirdEngine {
    struct TraditionalTextComponent : public Component {
        std::string text;
        std::string fontPath = "/usr/share/fonts/adwaita-mono-fonts/AdwaitaMono-Regular.ttf";
        int materialId = 1;
        float fontSize = 64.0f;
        
        bool dirty = true;
        std::shared_ptr<WeirdRenderer::Texture> sdfTexture;
        int width = 0;
        int height = 0;
    };

    struct UITraditionalTextComponent : public TraditionalTextComponent {
    };
}
