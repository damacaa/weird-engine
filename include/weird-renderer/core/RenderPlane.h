#pragma once

#include "weird-renderer/resources/Shader.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		class RenderPlane
		{
		public:
			RenderPlane();

			void draw(Shader& shader) const;
			void free();

		private:
			GLuint VAO, VBO, EBO;
		};
	} // namespace WeirdRenderer
} // namespace WeirdEngine
