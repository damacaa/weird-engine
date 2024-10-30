#pragma once
#include"Mesh.h"
#include "Shape.h"
#include "Shape2D.h"

namespace WeirdRenderer
{
	class RenderPlane
	{
	public:
		RenderPlane() {};
		RenderPlane(bool shapeRenderer);

		void Draw(Shader& shader) const;
		void Draw(Shader& shader, Shape* shapes, size_t size) const;
		void Draw(Shader& shader, Dot2D* shapes, size_t size) const;
		void Delete();

		void BindTextureToFrameBuffer(Texture texture, GLenum attachment);
		void BindColorTextureToFrameBuffer(Texture texture);
		void BindDepthTextureToFrameBuffer(Texture texture);

		unsigned int GetFrameBuffer() const;

	private:
		GLuint VAO, VBO, EBO, UBO, FBO;

		GLuint m_shapeBuffer;
		GLuint m_shapeTexture;
		unsigned int  m_shapeTextureLocation;
	};
}

