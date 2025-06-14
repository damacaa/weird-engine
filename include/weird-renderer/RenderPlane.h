#pragma once
#include"Mesh.h"
#include "Shape.h"
#include "Shape2D.h"

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
