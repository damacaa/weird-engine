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
			RenderPlane() = default;
			RenderPlane(bool shapeRenderer);

			void Bind() const;
			void Draw(Shader& shader) const;
			void Delete();

			void BindTextureToFrameBuffer(Texture texture, GLenum attachment);
			void BindColorTextureToFrameBuffer(Texture texture);
			void BindDepthTextureToFrameBuffer(Texture texture);

			unsigned int GetFrameBuffer() const;

		private:
			GLuint VAO, VBO, EBO, FBO;
		};
	}
}
