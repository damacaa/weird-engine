#pragma once
#include "weird-engine/ecs/ECS.h"
#include "weird-engine/ecs/Component.h"
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#include <memory>
#include <limits>

namespace WeirdEngine
{
	class Font {
	public:
		struct CharData {
			int dotCount = 0;
			std::vector<glm::vec2> positions;
		};

		Font(const std::string &fileName, int charWidth, int charHeight, int spacing, const std::string &charList)
		{
			// Store char dimensions
			m_charWidth = charWidth;
			m_charHeight = charHeight;

			// Make sure texture is not flipped
			wstbi_set_flip_vertically_on_load(false);

			// Load the image
			int width, height, channels;
			unsigned char *img = wstbi_load(fileName.c_str(), &width, &height, &channels, 0);

			if (img == nullptr) {
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

				for (size_t offsetX = 0; offsetX < charWidth; offsetX++) {
					for (size_t offsetY = 0; offsetY < charHeight; offsetY++) {
						int x = startX + offsetX;
						int y = startY + offsetY;

						// Calculate the index of the pixel in the image data
						int index = (y * width + x) * channels;

						if (index < 0 || index >= width * height * channels) {
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

		CharData getCharData(int charIndex) {
			return m_charData[charIndex];
		}

		int getCharWidth() { return m_charWidth; }
		int getCharHeight() { return m_charHeight; }

	private:
		std::array<CharData, 1024> m_charData;

		int m_charWidth;
		int m_charHeight;
	};


	using namespace ECS;

	template<typename DotClass, typename ShapeClass, typename TextClass>
	class SDFRenderSystem2D : public System
	{
	private:
		float m_shapeBlending = 1.0f;

	public:

		float m_dotRadious = 5.0f;
		float m_charSpacing = 10.0f;
		Font m_font;

		// loadFont(ENGINE_PATH "/src/weird-renderer/fonts/default.bmp", 7, 7, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,;:?!-_'#\"\\/<>() ");
		SDFRenderSystem2D(ECSManager& ecs): m_font(FONTS_PATH "small.bmp", 3, 4, 1,
		                                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ[]{}abcdefghijklmnopqrstuvwxyz\\/<>1234567890!\" &_*()__-=_+?|.,:;")
		{
			m_dotClassManager = ecs.getComponentManager<DotClass>();
			m_shapeClassManager = ecs.getComponentManager<ShapeClass>();
			m_textClassManager = ecs.getComponentManager<TextClass>();
			m_transformManager = ecs.getComponentManager<Transform>(); // Transform remains non-templated
		}

		void setShapeBlending(float blending)
		{
			m_shapeBlending = blending;
		}

		void updateText(TextClass& text)
		{
			if(text.dirty)
			{
				// Update dot count
				text.bufferedDotCount = 0;

				for (const auto& c : text.text)
				{
					text.bufferedDotCount += m_font.getCharData(c).dotCount;
				}

				int charCount = text.text.length();
				text.width = (charCount * m_font.getCharWidth() * 2 * m_dotRadious) + ((charCount - 1) * m_charSpacing);
				text.height = m_font.getCharHeight() * 2 * m_dotRadious;

				text.dirty = false;
			}
		}

		void fillDataBuffer(WeirdRenderer::Dot2D*& data, uint32_t& size)
		{
			// Get component arrays for the templated types
			auto dotClassArray = m_dotClassManager->getComponentArray();
			auto shapeClassArray = m_shapeClassManager->getComponentArray();
			auto textClassArray = m_textClassManager->getComponentArray();
			auto transformArray = m_transformManager->getComponentArray();

			uint32_t normalDots = dotClassArray->getSize();
			uint32_t textDots = 0;

			for (size_t i = 0; i < textClassArray->getSize(); i++)
			{
				auto& text = textClassArray->getDataAtIdx(i);
				updateText(text);

				textDots += text.bufferedDotCount;
			}
			uint32_t dotCount = normalDots + textDots;
			uint32_t shapeCount = shapeClassArray->getSize();

			// Each ShapeClass will contribute 2 dots to the buffer
			uint32_t newSize = dotCount + (2 * shapeCount);

			if (size != newSize)
			{
				if (data)
				{
					delete[] data;
					data = nullptr;
				}

				size = newSize;
			}

			if (!data && size > 0)
			{
				data = new WeirdRenderer::Dot2D[size];
			}

			// Process DotClass instances
			for (size_t i = 0; i < normalDots; i++)
			{
				auto& dotComp = dotClassArray->getDataAtIdx(i);
				auto& t = transformArray->getDataFromEntity(dotComp.Owner); // Assuming DotClass has an 'Owner' member

				data[i].position = (glm::vec2)t.position;
				data[i].size = t.position.z;
				data[i].material = dotComp.materialId;
			}

			float charWidth = m_font.getCharWidth() * 2 * m_dotRadious; // should be 40

			// Text
			int dotIndex = 0;
			for (size_t i = 0; i < textClassArray->getSize(); i++) {
				auto &text = textClassArray->getDataAtIdx(i);
				auto &t = transformArray->getDataFromEntity(text.Owner);
				int charCount = text.text.length();

				float horizontalOffset = 0.0f;
				switch (text.horizontalAlignment) {
					case TextRenderer::HorizontalAlignment::Left:
						horizontalOffset = 0.0f;
						break;
					case TextRenderer::HorizontalAlignment::Center:
						horizontalOffset = -text.width * 0.5f;
						break;
					case TextRenderer::HorizontalAlignment::Right:
						horizontalOffset = -text.width;
						break;
				}

				float verticalOffset = 0.0f;
				switch (text.verticalAlignment) {
					case TextRenderer::VerticalAlignment::Bottom:
						verticalOffset = 0.0f;
						break;
					case TextRenderer::VerticalAlignment::Center:
						verticalOffset = -text.height * 0.5f;
						break;
					case TextRenderer::VerticalAlignment::Top:
						verticalOffset = -text.height;
						break;
				}

				vec2 alignmentOffset(horizontalOffset, verticalOffset);

				for (size_t c = 0; c < charCount; c++) {
					auto charData = m_font.getCharData(text.text[c]);
					for (size_t j = 0; j < charData.dotCount; j++) {
						int idx = normalDots + dotIndex;
						dotIndex++;

						vec2 charOffset = vec2(((charWidth + m_charSpacing) * c) + m_dotRadious, m_dotRadious); // TODO: different lines
						vec2 scaledDotPosition = 2 * m_dotRadious * charData.positions[j];
						data[idx].position = (vec2)t.position + charOffset + scaledDotPosition + alignmentOffset; // + letter
						data[idx].size = 1.0;
						data[idx].material = text.material;
					}
				}
			}

			// Process ShapeClass instances
			for (size_t i = 0; i < shapeCount; i++)
			{
				auto& shapeComp = shapeClassArray->getDataAtIdx(i);

				// Assuming ShapeClass has m_parameters[0] through m_parameters[7]
				// Make sure your ShapeClass provides these members.
				data[dotCount + (2 * i)].position = glm::vec2(shapeComp.parameters[0], shapeComp.parameters[1]);
				data[dotCount + (2 * i)].size = shapeComp.parameters[2];
				data[dotCount + (2 * i)].material = shapeComp.parameters[3];

				data[dotCount + (2 * i) + 1].position = glm::vec2(shapeComp.parameters[4], shapeComp.parameters[5]);
				data[dotCount + (2 * i) + 1].size = shapeComp.parameters[6];
				data[dotCount + (2 * i) + 1].material = shapeComp.parameters[7];
			}
		}

		void updatePalette(WeirdRenderer::Shader& shader)
		{
			if (m_materialsAreDirty)
			{
				shader.setUniform("u_staticColors", m_colorPalette, 16);
				m_materialsAreDirty = false;
			}
		}

		// Function to find the closest color in the palette
		int findClosestColorInPalette(const glm::vec3& color) {
			int closestIndex = 0;
			float minDistance = std::numeric_limits<float>::max(); // start with maximum possible distance

			for (int i = 0; i < 16; ++i) {
				// Use length2 for efficiency (avoids computing square root)
				float distance = glm::length2(color - m_colorPalette[i]);

				if (distance < minDistance) {
					minDistance = distance;
					closestIndex = i;
				}
			}

			return closestIndex;
		}

		bool& shaderNeedsUpdate()
		{
			return m_shapesNeedUpdate;
		}

		void replaceSubstring(std::string &str, const std::string &from, const std::string &to)
		{
			size_t start_pos = str.find(from);
			if (start_pos != std::string::npos)
			{
				str.replace(start_pos, from.length(), to);
			}
		}

		void updateCustomShapesShader(WeirdRenderer::Shader& shader, std::vector<std::shared_ptr<IMathExpression>> sdfs)
		{
			const auto sdfBalls = m_dotClassManager->getComponentArray();
			const auto componentArray = m_shapeClassManager->getComponentArray();

			if (!m_shapesNeedUpdate)
			{
				return;
			}

			m_shapesNeedUpdate = false;

			std::ostringstream oss;

			oss << "///////////////////////////////////////////\n";

			oss << "int dataOffset =  u_loadedObjects - (2 * u_customShapeCount);\n";

			oss << "int currentGroupColor = -1;\n";

			int currentGroup = -1;
			std::string groupDistanceVariable;

			ShapeClass dummyShape;
			dummyShape.groupIdx = CustomShape::GLOBAL_GROUP - 1;

			// TODO: do this for physics too
			// Sort Shapes by group
			std::vector<size_t> orderedIndices;
			orderedIndices.reserve(componentArray->getSize());

			for (size_t i = 0; i < componentArray->getSize(); i++) {
				orderedIndices.push_back(i);
			}

			// Sort indices by the shape's group value
			std::sort(orderedIndices.begin(), orderedIndices.end(),
				[&](size_t a, size_t b) {
					return componentArray->getDataAtIdx(a).groupIdx <
						   componentArray->getDataAtIdx(b).groupIdx;
				}
			);

			for (size_t idx = 0; idx < componentArray->getSize() + 1; idx++)
			{
				size_t i = idx == componentArray->getSize() ? 0 : orderedIndices.at(idx);

				// Get shape
				const ShapeClass& shape = idx == componentArray->getSize() ? dummyShape :  componentArray->getDataAtIdx(i);

				// Get group
				const int group = shape.groupIdx;

				// Start new group if necessary
				if (group != currentGroup)
				{
					// If this is not the first group, combine current group distance with global minDistance
					if (currentGroup != -1)
					{
						oss << "if(" << groupDistanceVariable << " <= max(minDist, 0)){ finalMaterialId = currentGroupColor;}\n";
						// oss << "{ finalMaterialId = currentGroupColor;}\n";
						oss << "if(minDist >" << groupDistanceVariable << "){ minDist = " << groupDistanceVariable << ";}\n";
					}

					// Next group
					currentGroup = group;
					groupDistanceVariable = "d" + std::to_string(currentGroup);

					// Initialize distance with big value
					oss << "float " << groupDistanceVariable << "= 10000;\n";
				}

				if (group == CustomShape::GLOBAL_GROUP - 1)
					break;

				// Start shape
				oss << "{\n";

				// Calculate data position in array
				oss << "int idx = dataOffset + " << 2 * i << ";\n";

				// Fetch parameters
				oss << "vec4 parameters0 = texelFetch(t_shapeBuffer, idx);\n";
				oss << "vec4 parameters1 = texelFetch(t_shapeBuffer, idx + 1);\n";

				// Get distance function
				auto fragmentCode = sdfs[shape.distanceFieldId]->print();

				// Use screen space coords (DEPRECATED)
				// if (shape.screenSpace)
				// {
				// 	replaceSubstring(fragmentCode, "var9", "var11");
				// 	replaceSubstring(fragmentCode, "var10", "var12");
				// }

				bool globalEffect = group == CustomShape::GLOBAL_GROUP;

				// Shape distance calculation
				oss << "float dist = " << fragmentCode << ";" << std::endl;

				// oss << "#ifdef ORIGIN_AT_BOTTOM_LEFT" << std::endl;
				// oss << "float pixelSize = 10.0; dist = abs(dist - pixelSize) - (pixelSize);" << std::endl;
				// oss << "#endif" << std::endl;





				// Apply globalEffect logic
				oss << "float currentMinDistance = " << (globalEffect ? "minDist" : groupDistanceVariable) << ";" << std::endl;

				// Combine shape distance
				switch (shape.combination)
				{
				case CombinationType::Addition:
				{
					oss << "currentMinDistance = min(currentMinDistance, dist);\n";
					oss << "currentGroupColor = dist <= min(currentMinDistance, dist) ? " << shape.material << ": currentGroupColor;" << std::endl;
					break;
				}
				case CombinationType::Subtraction:
				{
					oss << "currentMinDistance = max(currentMinDistance, -dist);\n";
					break;
				}
				case CombinationType::Intersection:
				{
					oss << "currentMinDistance = max(currentMinDistance, dist);\n";
					break;
				}
				case CombinationType::SmoothAddition:
				{
					oss << "currentMinDistance = fOpUnionSoft(currentMinDistance, dist," << m_shapeBlending << ");\n";
					oss << "currentGroupColor = dist <= min(currentMinDistance, dist) ? " << shape.material << ": currentGroupColor;" << std::endl;
					break;
				}
				case CombinationType::SmoothSubtraction:
				{
					// Smoothly subtract "dist" from currentMinDistance
					oss << "currentMinDistance = fOpSubSoft(currentMinDistance, dist, " << m_shapeBlending << ");\n";
					// Material belongs to the *a* shape if it still �wins� after subtraction
					// oss << "finalMaterialId = dist <= min(minDist, currentMinDistance) ? "
					//	<< shape.m_material << " : finalMaterialId;" << std::endl;
					break;
				}
				default:
					break;
				}

				// Assign back to the correct target
				oss << (globalEffect ? "minDist" : groupDistanceVariable) << " = currentMinDistance;\n";
				oss << "}\n\n";
			}

			// Combine last group
			// if (componentArray->getSize() > 0)
			// {
			// 	oss << "if(minDist >" << groupDistanceVariable << "){ minDist = " << groupDistanceVariable << ";}\n";
			// }

			// Get string
			std::string replacement = oss.str();

			// Set new source code and recompile shader
			shader.setFragmentIncludeCode(1, replacement);

			// std::cout << replacement << std::endl;

#ifndef NDEBUG
			if (Input::GetKey(Input::LeftCtrl) && Input::GetKey(Input::LeftShift) && Input::GetKey(Input::R))
			{
				std::cout << replacement << std::endl;

				// Broken
				std::ofstream outFile("generated_shader.frag");
				if (outFile.is_open()) {
					outFile << shader.getFragmentCode();
					outFile.close();
				}
			}
#endif
		}

	private:
		std::shared_ptr<ComponentManager<DotClass>> m_dotClassManager;
		std::shared_ptr<ComponentManager<ShapeClass>> m_shapeClassManager;
		std::shared_ptr<ComponentManager<TextClass>> m_textClassManager;
		std::shared_ptr<ComponentManager<Transform>> m_transformManager;

		glm::vec3 m_colorPalette[16] = {
			glm::vec3(0.025f, 0.025f, 0.05f), // Black
			glm::vec3(1.0f, 1.0f, 1.0f), // White
			glm::vec3(0.484f, 0.484f, 0.584f), // Dark Gray
			glm::vec3(0.752f, 0.762f, 0.74f), // Light Gray
			glm::vec3(.95f, 0.1f, 0.1f), // Red
			glm::vec3(0.1f, .95f, 0.1f), // Green
			glm::vec3(0.15f, 0.25f, .85f), // Blue
			glm::vec3(1.0f, .9f, 0.2f), // Yellow
			glm::vec3(.95f, 0.4f, 0.1f), // Orange
			glm::vec3(0.5f, 0.0f, 1.0f), // Purple
			glm::vec3(0.0f, .9f, .9f), // Cyan
			glm::vec3(1.0f, 0.3f, .6f), // Magenta
			glm::vec3(0.5f, 1.0f, 0.5f), // Light Green
			glm::vec3(1.0f, 0.5f, 0.5f), // Pink
			glm::vec3(0.5f, 0.5f, 1.0f), // Light Blue
			glm::vec3(0.4f, 0.25f, 0.1f) // Brown
		};

		bool m_materialsAreDirty = true;
		bool m_shapesNeedUpdate = true;
		size_t m_lastShapeCount = 0;
	};
}