#pragma once

#include "weird-engine/Logger.h"
#include "weird-engine/vec.h"
#include <array>
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#include <string>
#include <vector>

namespace WeirdEngine::WeirdRenderer
{
	class Font
	{
	public:
		struct CharData
		{
			int dotCount = 0;
			std::vector<vec2> positions;
		};

		Font(const std::string& fileName, int charWidth, int charHeight, int spacing, const std::string& charList)
		{
			// Store char dimensions
			m_charWidth = charWidth;
			m_charHeight = charHeight;

			// Make sure texture is not flipped
			wstbi_set_flip_vertically_on_load(false);

			// Load the image
			int width, height, channels;
			unsigned char* img = wstbi_load(fileName.c_str(), &width, &height, &channels, 0);

			if (img == nullptr)
			{
				WeirdEngine::Logger::error("Error: could not load image.");
				return;
			}

			// Bad fix
			charWidth += spacing;
			charHeight += spacing;

			int columns = width / charWidth;
			int rows = height / charHeight;

			int charCount = static_cast<int>(charList.length());

			for (size_t i = 0; i < static_cast<size_t>(charCount); i++)
			{
				CharData charData;

				int startX = charWidth * static_cast<int>(i % columns);
				int startY = (charHeight * static_cast<int>(i / columns));

				for (size_t offsetX = 0; offsetX < static_cast<size_t>(charWidth); offsetX++)
				{
					for (size_t offsetY = 0; offsetY < static_cast<size_t>(charHeight); offsetY++)
					{
						int x = startX + static_cast<int>(offsetX);
						int y = startY + static_cast<int>(offsetY);

						// Calculate the index of the pixel in the image data
						int index = (y * width + x) * channels;

						if (index < 0 || index >= width * height * channels)
						{
							continue;
						}

						// Get the color values
						unsigned char r = img[index];
						unsigned char g = img[index + 1];
						unsigned char b = img[index + 2];
						unsigned char a = (channels == 4) ? img[index + 3] : 255; // Alpha channel (if present)

						if (r < 50)
						{
							float localX = static_cast<float>(offsetX);
							float localY = (charHeight * 0.5f) - static_cast<float>(offsetY);
							charData.positions.emplace_back(localX, localY);
						}
					}
				}

				charData.dotCount = static_cast<int>(charData.positions.size());
				m_charData[charList[i]] = charData;
			}

			// Free the image memory
			wstbi_image_free(img);
		}

		CharData getCharData(int charIndex)
		{
			return m_charData[charIndex];
		}

		int getCharWidth()
		{
			return m_charWidth;
		}
		int getCharHeight()
		{
			return m_charHeight;
		}

	private:
		std::array<CharData, 1024> m_charData;

		int m_charWidth;
		int m_charHeight;
	};
} // namespace WeirdEngine::WeirdRenderer
