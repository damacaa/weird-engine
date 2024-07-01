#pragma once
#include"Mesh.h"
#include "Shape.h"
class RenderPlane
{
public:
	RenderPlane() {};
	RenderPlane(int width, int height, Shader& shader, bool shapeRenderer);

	void Draw(Shader& shader) const;
	void Draw(Shader& shader, Shape* shapes, size_t size) const;
	void Delete();

	unsigned int GetFrameBuffer() const;

private:
	GLuint VAO, VBO, EBO, UBO, FBO;
	unsigned int m_colorTexture, m_colorTextureLocation;
	unsigned int m_depthTexture, m_depthTextureLocation;
};

