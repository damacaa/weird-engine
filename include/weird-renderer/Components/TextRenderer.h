#pragma once
#include <string>

#include "weird-engine/ecs/Component.h"

namespace WeirdEngine
{
    struct TextRenderer : public Component
    {
        std::string text;
        uint32_t bufferedDotCount = 0;
        bool dirty = true;
        uint16_t material = 0;
    };

    struct UITextRenderer : public TextRenderer{};
}
