#pragma once
#include <string>


namespace WeirdEngine
{
	struct TextRenderer
	{
		enum class HorizontalAlignment
		{
			Left,
			Center,
			Right
		};

		enum class VerticalAlignment
		{
			Top,
			Center,
			Bottom
		};

		std::string text;
		uint32_t bufferedDotCount = 0;
		uint16_t material = 0;
		float width = 0;
		float height = 0;
		VerticalAlignment verticalAlignment = VerticalAlignment::Bottom;
		HorizontalAlignment horizontalAlignment = HorizontalAlignment::Left;
	};

	struct UITextRenderer : public TextRenderer
	{
	};
} // namespace WeirdEngine
