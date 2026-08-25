#pragma once

#include "weird-engine/vec.h"
#include <vector>

namespace WeirdEngine
{
	namespace ColorPalette
	{
		inline const glm::vec3 Black = glm::vec3(0.025f, 0.025f, 0.05f);
		inline const glm::vec3 White = glm::vec3(1.0f, 1.0f, 1.0f);
		inline const glm::vec3 Gray = glm::vec3(0.484f, 0.484f, 0.584f);
		inline const glm::vec3 DarkGray = glm::vec3(0.484f, 0.484f, 0.584f);
		inline const glm::vec3 LightGray = glm::vec3(0.752f, 0.762f, 0.78f);
		inline const glm::vec3 Red = glm::vec3(0.8f, 0.2f, 0.2f);
		inline const glm::vec3 Green = glm::vec3(0.1f, 0.95f, 0.1f);
		inline const glm::vec3 Blue = glm::vec3(0.15f, 0.25f, 0.85f);
		inline const glm::vec3 Yellow = glm::vec3(1.0f, 0.9f, 0.2f);
		inline const glm::vec3 Orange = glm::vec3(0.95f, 0.4f, 0.1f);
		inline const glm::vec3 Purple = glm::vec3(0.5f, 0.0f, 1.0f);
		inline const glm::vec3 Cyan = glm::vec3(0.0f, 0.9f, 0.9f);
		inline const glm::vec3 Magenta = glm::vec3(1.0f, 0.3f, 0.6f);
		inline const glm::vec3 LightGreen = glm::vec3(0.5f, 1.0f, 0.5f);
		inline const glm::vec3 Pink = glm::vec3(1.0f, 0.5f, 0.5f);
		inline const glm::vec3 LightBlue = glm::vec3(0.5f, 0.5f, 1.0f);
		inline const glm::vec3 Brown = glm::vec3(0.4f, 0.25f, 0.1f);

		inline const std::vector<glm::vec3> Default = {Black,	   White,  Gray,	  LightGray, Red,  Green,
													   Blue,	   Yellow, Orange,	  Purple,	 Cyan, Magenta,
													   LightGreen, Pink,   LightBlue, Brown};
	} // namespace ColorPalette

	namespace Colors = ColorPalette;
	inline const std::vector<glm::vec3>& DefaultColorPalette = ColorPalette::Default;
} // namespace WeirdEngine
