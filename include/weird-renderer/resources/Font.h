#pragma once

#include "weird-engine/vec.h"
#include <array>
#include <iostream>
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
				std::cerr << "Error: could not load image." << std::endl;
				return;
			}

			// Bad fix
			charWidth += spacing;
			charHeight += spacing;

			int columns = width / charWidth;
			int rows = height / charHeight;

			int charCount = charList.length();

			for (size_t i = 0; i < charCount; i++)
			{
				CharData charData;

				int startX = charWidth * (i % columns);
				int startY = (charHeight * (i / columns));

				for (size_t offsetX = 0; offsetX < charWidth; offsetX++)
				{
					for (size_t offsetY = 0; offsetY < charHeight; offsetY++)
					{
						int x = startX + offsetX;
						int y = startY + offsetY;

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
							float localX = offsetX;
							float localY = (charHeight * 0.5f) - offsetY;
							charData.positions.emplace_back(localX, localY);
						}
					}
				}

				charData.dotCount = charData.positions.size();
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
