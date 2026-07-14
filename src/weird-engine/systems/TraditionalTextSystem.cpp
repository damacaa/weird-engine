#include "weird-engine/systems/TraditionalTextSystem.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>

namespace WeirdEngine {
    static FT_Library ftLibrary;
    static bool ftInitialized = false;

    void TraditionalTextSystem::update(ECSManager& ecs, std::vector<TraditionalTextData>& outData) {
        if (!ftInitialized) {
            if (FT_Init_FreeType(&ftLibrary)) {
                std::cerr << "Could not init freetype library" << std::endl;
                return;
            }
            ftInitialized = true;
        }

        auto textArray = ecs.getComponentManager<TraditionalTextComponent>()->getComponentArray();
        auto transformArray = ecs.getComponentManager<Transform>()->getComponentArray();

        for (size_t i = 0; i < textArray->getSize(); i++) {
            auto& textComp = textArray->getDataAtIdx(i);
            
            if (textComp.dirty) {
                FT_Face face;
                if (FT_New_Face(ftLibrary, textComp.fontPath.c_str(), 0, &face)) {
                    std::cerr << "Failed to load font " << textComp.fontPath << std::endl;
                    continue;
                }
                
                FT_Set_Pixel_Sizes(face, 0, static_cast<int>(textComp.fontSize));

                int totalWidth = 0;
                int maxHeight = 0;
                int maxBearingY = 0;
                int minY = 0;
                int minX = 0;
                int maxX = 0;
                
                // First pass to compute bounds
                int penX = 0;
                for (char c : textComp.text) {
                    if (FT_Load_Char(face, c, FT_LOAD_DEFAULT)) continue;
                    FT_Render_Glyph(face->glyph, FT_RENDER_MODE_SDF);
                    
                    int xpos = penX + face->glyph->bitmap_left;
                    int ypos = face->glyph->bitmap_top;
                    
                    if (c == textComp.text[0]) {
                        minX = xpos;
                    } else {
                        minX = std::min(minX, xpos);
                    }
                    maxX = std::max(maxX, xpos + (int)face->glyph->bitmap.width);
                    maxBearingY = std::max(maxBearingY, ypos);
                    minY = std::min(minY, ypos - (int)face->glyph->bitmap.rows);
                    
                    penX += (face->glyph->advance.x >> 6);
                }
                
                totalWidth = maxX - minX;
                maxHeight = maxBearingY - minY;
                
                textComp.width = totalWidth;
                textComp.height = maxHeight;
                
                if (totalWidth > 0 && maxHeight > 0) {
                    std::vector<unsigned char> buffer(totalWidth * maxHeight, 0); 
                    
                    penX = 0;
                    for (char c : textComp.text) {
                        if (FT_Load_Char(face, c, FT_LOAD_DEFAULT)) continue;
                        FT_Render_Glyph(face->glyph, FT_RENDER_MODE_SDF);
                        
                        FT_Bitmap* bmp = &face->glyph->bitmap;
                        int xpos = penX + face->glyph->bitmap_left - minX;
                        int ypos = maxBearingY - face->glyph->bitmap_top; // 0 is top
                        
                        for(unsigned int row = 0; row < bmp->rows; row++) {
                            for(unsigned int col = 0; col < bmp->width; col++) {
                                int bufX = xpos + col;
                                int bufY = ypos + row;
                                if (bufX >= 0 && bufX < totalWidth && bufY >= 0 && bufY < maxHeight) {
                                    unsigned char existing = buffer[bufY * totalWidth + bufX];
                                    unsigned char newPix = bmp->buffer[row * bmp->pitch + col];
                                    buffer[bufY * totalWidth + bufX] = std::max(existing, newPix);
                                }
                            }
                        }
                        penX += (face->glyph->advance.x >> 6);
                    }
                    
                    textComp.sdfTexture = std::make_shared<WeirdRenderer::Texture>(totalWidth, maxHeight, WeirdRenderer::Texture::TextureType::SingleChannel, buffer.data());
                }
                
                FT_Done_Face(face);
                textComp.dirty = false;
            }

            TraditionalTextData data;
            data.transform = transformArray->getDataFromEntity(textComp.Owner);
            data.text = &textComp;
            outData.push_back(data);
        }
    }
}
