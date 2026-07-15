#pragma once

#include <string>
#include <unordered_map>
#include <memory>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "weird-renderer/components/TraditionalTextComponent.h"

namespace WeirdEngine {
namespace WeirdRenderer {

class FontManager {
public:
    FontManager();
    ~FontManager();

    // Disable copy/move for simplicity
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;
    FontManager(FontManager&&) = delete;
    FontManager& operator=(FontManager&&) = delete;

    void updateTextComponent(WeirdEngine::TraditionalTextComponent& textComp);

private:
    FT_Library m_ftLibrary;
    bool m_initialized;
    std::unordered_map<std::string, FT_Face> m_fonts;

    FT_Face getOrLoadFont(const std::string& fontPath);
};

} // namespace WeirdRenderer
} // namespace WeirdEngine
