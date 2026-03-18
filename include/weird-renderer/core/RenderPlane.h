#pragma once
#include "weird-renderer/resources/Mesh.h"
#include "weird-renderer/scene/Shape.h"
#include "weird-renderer/scene/Shape2D.h"

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
	}
}
