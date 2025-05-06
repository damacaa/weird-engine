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

			void Draw(Shader& shader) const;
			void Delete();

		private:
			GLuint VAO, VBO, EBO;
		};
	}
}
