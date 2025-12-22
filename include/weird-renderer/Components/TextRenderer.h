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
    };

    struct UITextRenderer : public TextRenderer{};
}
