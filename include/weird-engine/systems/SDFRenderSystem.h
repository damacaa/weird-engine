#pragma once
#include "weird-engine/ecs/ECS.h"
#include "weird-engine/vec.h"
#include "weird-renderer/resources/Font.h"
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

namespace WeirdEngine
{
	// Make this a component?
	struct SDFRenderSystemContext
	{
		float m_dotRadious = 5.0f;
		float m_charSpacing = 10.0f;
		WeirdRenderer::Font m_font;

		bool m_shapesNeedUpdate = true;

		SDFRenderSystemContext()
			: m_font(FONTS_PATH "small.bmp", 3, 4, 1,
					 "ABCDEFGHIJKLMNOPQRSTUVWXYZ[]{}abcdefghijklmnopqrstuvwxyz\\/<>1234567890!\" &_*()__-=_+?|.,:;")
		{
		}
	};

	namespace SDFRenderSystem
	{

		template <typename DotClass, typename ShapeClass, typename TextClass>
		inline void update(ECSManager& ecs, SDFRenderSystemContext& ctx, vec4*& data, uint32_t& size)
		{
			auto updateText = [&](TextClass& text)
			{
				if (text.dirty)
				{
					// Update dot count
					text.bufferedDotCount = 0;

					for (const auto& c : text.text)
					{
						text.bufferedDotCount += ctx.m_font.getCharData(c).dotCount;
					}

					int charCount = text.text.length();
					text.width = (charCount * ctx.m_font.getCharWidth() * 2 * ctx.m_dotRadious) +
								 ((charCount - 1) * ctx.m_charSpacing);
					text.height = ctx.m_font.getCharHeight() * 2 * ctx.m_dotRadious;

					text.dirty = false;
				}
			};

			// Get component arrays for the templated types
			auto dotClassArray = ecs.getComponentManager<DotClass>()->getComponentArray();
			auto shapeClassArray = ecs.getComponentManager<ShapeClass>()->getComponentArray();
			auto textClassArray = ecs.getComponentManager<TextClass>()->getComponentArray();
			auto transformArray = ecs.getComponentManager<Transform>()->getComponentArray();

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
				data = new vec4[size];
			}

			// Process DotClass instances
			for (size_t i = 0; i < normalDots; i++)
			{
				auto& dotComp = dotClassArray->getDataAtIdx(i);
				auto& t = transformArray->getDataFromEntity(dotComp.Owner); // Assuming DotClass has an 'Owner' member

				data[i].x = t.position.x;
				data[i].y = t.position.y;
				data[i].z = t.position.z;
				data[i].w = dotComp.materialId;
			}

			float charWidth = ctx.m_font.getCharWidth() * 2 * ctx.m_dotRadious; // should be 40

			// Text
			int dotIndex = 0;
			for (size_t i = 0; i < textClassArray->getSize(); i++)
			{
				auto& text = textClassArray->getDataAtIdx(i);
				auto& t = transformArray->getDataFromEntity(text.Owner);
				int charCount = text.text.length();

				float horizontalOffset = 0.0f;
				switch (text.horizontalAlignment)
				{
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
				switch (text.verticalAlignment)
				{
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

				for (size_t c = 0; c < charCount; c++)
				{
					auto charData = ctx.m_font.getCharData(text.text[c]);
					for (size_t j = 0; j < charData.dotCount; j++)
					{
						int idx = normalDots + dotIndex;
						dotIndex++;

						vec2 charOffset = vec2(((charWidth + ctx.m_charSpacing) * c) + ctx.m_dotRadious,
											   ctx.m_dotRadious); // TODO: different lines
						vec2 scaledDotPosition = 2 * ctx.m_dotRadious * charData.positions[j];
						vec2 position = (vec2)t.position + charOffset + scaledDotPosition + alignmentOffset; // + letter
						data[idx].x = position.x;
						data[idx].y = position.y;
						data[idx].z = 1.0f;
						data[idx].w = text.material;
					}
				}
			}

			// Process ShapeClass instances
			for (size_t i = 0; i < shapeCount; i++)
			{
				auto& shapeComp = shapeClassArray->getDataAtIdx(i);

				// Assuming ShapeClass has m_parameters[0] through m_parameters[7]
				// Make sure your ShapeClass provides these members.
				data[dotCount + (2 * i)].x = shapeComp.parameters[0];
				data[dotCount + (2 * i)].y = shapeComp.parameters[1];
				data[dotCount + (2 * i)].z = shapeComp.parameters[2];
				data[dotCount + (2 * i)].w = shapeComp.parameters[3];

				data[dotCount + (2 * i) + 1].x = shapeComp.parameters[4];
				data[dotCount + (2 * i) + 1].y = shapeComp.parameters[5];
				data[dotCount + (2 * i) + 1].z = shapeComp.parameters[6];
				data[dotCount + (2 * i) + 1].w = shapeComp.parameters[7];
			}
		}

	} // namespace SDFRenderSystem
} // namespace WeirdEngine