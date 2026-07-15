#include "weird-renderer/fonts/FontManager.h"
#include <algorithm>
#include <iostream>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{

		FontManager::FontManager()
			: m_ftLibrary(nullptr)
			, m_initialized(false)
		{
			if (FT_Init_FreeType(&m_ftLibrary))
			{
				std::cerr << "Could not init freetype library" << std::endl;
			}
			else
			{
				m_initialized = true;
			}
		}

		FontManager::~FontManager()
		{
			for (auto& pair : m_fonts)
			{
				FT_Done_Face(pair.second);
			}
			m_fonts.clear();

			if (m_initialized)
			{
				FT_Done_FreeType(m_ftLibrary);
			}
		}

		FT_Face FontManager::getOrLoadFont(const std::string& fontPath)
		{
			if (!m_initialized)
				return nullptr;

			auto it = m_fonts.find(fontPath);
			if (it != m_fonts.end())
			{
				return it->second;
			}

			FT_Face face;
			if (FT_New_Face(m_ftLibrary, fontPath.c_str(), 0, &face))
			{
				std::cerr << "Failed to load font " << fontPath << std::endl;
				return nullptr;
			}

			m_fonts[fontPath] = face;
			return face;
		}

		void FontManager::updateTextComponent(WeirdEngine::TraditionalTextComponent& textComp)
		{
			if (!textComp.dirty)
				return;

			FT_Face face = getOrLoadFont(textComp.fontPath);
			if (!face)
			{
				textComp.dirty = false;
				return;
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
			for (char c : textComp.text)
			{
				if (FT_Load_Char(face, c, FT_LOAD_DEFAULT))
					continue;
				FT_Render_Glyph(face->glyph, FT_RENDER_MODE_SDF);

				int xpos = penX + face->glyph->bitmap_left;
				int ypos = face->glyph->bitmap_top;

				if (c == textComp.text[0])
				{
					minX = xpos;
				}
				else
				{
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

			if (totalWidth > 0 && maxHeight > 0)
			{
				std::vector<unsigned char> buffer(totalWidth * maxHeight, 0);

				penX = 0;
				for (char c : textComp.text)
				{
					if (FT_Load_Char(face, c, FT_LOAD_DEFAULT))
						continue;
					FT_Render_Glyph(face->glyph, FT_RENDER_MODE_SDF);

					FT_Bitmap* bmp = &face->glyph->bitmap;
					int xpos = penX + face->glyph->bitmap_left - minX;
					int ypos = maxBearingY - face->glyph->bitmap_top; // 0 is top

					for (unsigned int row = 0; row < bmp->rows; row++)
					{
						for (unsigned int col = 0; col < bmp->width; col++)
						{
							int bufX = xpos + col;
							int bufY = ypos + row;
							if (bufX >= 0 && bufX < totalWidth && bufY >= 0 && bufY < maxHeight)
							{
								unsigned char existing = buffer[bufY * totalWidth + bufX];
								unsigned char newPix = bmp->buffer[row * bmp->pitch + col];
								buffer[bufY * totalWidth + bufX] = std::max(existing, newPix);
							}
						}
					}
					penX += (face->glyph->advance.x >> 6);
				}

				textComp.sdfTexture = std::make_shared<WeirdRenderer::Texture>(
					totalWidth, maxHeight, WeirdRenderer::Texture::TextureType::SingleChannel, buffer.data());
			}
			else
			{
				textComp.sdfTexture = nullptr;
			}

			textComp.dirty = false;
		}

	} // namespace WeirdRenderer
} // namespace WeirdEngine
