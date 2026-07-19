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
		float dotRadious = 5.0f;
		float charSpacing = 10.0f;
		WeirdRenderer::Font font;

		bool shapesNeedUpdate = true;

		SDFRenderSystemContext()
			: font(FONTS_PATH "small.bmp", 3, 4, 1,
				   "ABCDEFGHIJKLMNOPQRSTUVWXYZ[]{}abcdefghijklmnopqrstuvwxyz\\/<>1234567890!\" &_*()__-=_+?|.,:;")
		{
		}
	};

	namespace SDFRenderSystem
	{

		template <typename DotClass, typename ShapeClass, typename TextClass>
		inline void update(ECSManager& ecs, SDFRenderSystemContext& ctx, vec4*& data, uint32_t& size)
		{
			uint32_t normalDots = 0;
			if (auto dotArray = ecs.getComponentArray<DotClass>())
			{
				normalDots = dotArray->getSize();
			}

			uint32_t textDots = 0;
			ecs.forEach<TextClass>(
				[&](Entity entity, TextClass& text)
				{
					if (ecs.isComponentDirty(text))
					{
						// Update dot count
						text.bufferedDotCount = 0;

						for (const auto& c : text.text)
						{
							text.bufferedDotCount += ctx.font.getCharData(c).dotCount;
						}

						int charCount = text.text.length();
						text.width = (charCount * ctx.font.getCharWidth() * 2 * ctx.dotRadious) +
									 ((charCount - 1) * ctx.charSpacing);
						text.height = ctx.font.getCharHeight() * 2 * ctx.dotRadious;

						ecs.setComponentDirty(text, false);
					}

					textDots += text.bufferedDotCount;
				});

			uint32_t dotCount = normalDots + textDots;

			uint32_t shapeCount = 0;
			if (auto shapeArray = ecs.getComponentArray<ShapeClass>())
			{
				shapeCount = shapeArray->getSize();
			}

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
			int dotIdx = 0;
			ecs.forEach<DotClass, Transform>(
				[&](Entity entity, DotClass& dotComp, Transform& t)
				{
					data[dotIdx].x = t.position.x;
					data[dotIdx].y = t.position.y;
					data[dotIdx].z = t.position.z;
					data[dotIdx].w = dotComp.materialId;
					dotIdx++;
				});

			float charWidth = ctx.font.getCharWidth() * 2 * ctx.dotRadious; // should be 40

			// Text
			int dotIndex = 0;
			ecs.forEach<TextClass, Transform>(
				[&](Entity entity, TextClass& text, Transform& t)
				{
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
						auto charData = ctx.font.getCharData(text.text[c]);
						for (size_t j = 0; j < charData.dotCount; j++)
						{
							int idx = normalDots + dotIndex;
							dotIndex++;

							vec2 charOffset = vec2(((charWidth + ctx.charSpacing) * c) + ctx.dotRadious,
												   ctx.dotRadious); // TODO: different lines
							vec2 scaledDotPosition = 2 * ctx.dotRadious * charData.positions[j];
							vec2 position =
								(vec2)t.position + charOffset + scaledDotPosition + alignmentOffset; // + letter
							data[idx].x = position.x;
							data[idx].y = position.y;
							data[idx].z = 1.0f;
							data[idx].w = text.material;
						}
					}
				});

			// Process ShapeClass instances
			int shapeIdx = 0;
			ecs.forEach<ShapeClass>(
				[&](Entity entity, ShapeClass& shapeComp)
				{
					// Assuming ShapeClass has m_parameters[0] through m_parameters[7]
					// Make sure your ShapeClass provides these members.
					data[dotCount + (2 * shapeIdx)].x = shapeComp.parameters[0];
					data[dotCount + (2 * shapeIdx)].y = shapeComp.parameters[1];
					data[dotCount + (2 * shapeIdx)].z = shapeComp.parameters[2];
					data[dotCount + (2 * shapeIdx)].w = shapeComp.parameters[3];

					data[dotCount + (2 * shapeIdx) + 1].x = shapeComp.parameters[4];
					data[dotCount + (2 * shapeIdx) + 1].y = shapeComp.parameters[5];
					data[dotCount + (2 * shapeIdx) + 1].z = shapeComp.parameters[6];
					data[dotCount + (2 * shapeIdx) + 1].w = shapeComp.parameters[7];
					shapeIdx++;
				});
		}

	} // namespace SDFRenderSystem
} // namespace WeirdEngine