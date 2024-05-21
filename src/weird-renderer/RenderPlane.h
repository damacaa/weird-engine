#pragma once
#include"Mesh.h"
#include "Shape.h"
class RenderPlane
{
public:
	RenderPlane(int width, int height, Shader& shader);
	void Draw(Shader& shader, Shape* shapes, size_t size, float time) const;
	void Delete();

	unsigned int GetFrameBuffer() const;
private:
	GLuint VAO, VBO, EBO, UBO, FBO;
	unsigned int m_colorTexture, m_colorTextureLocation;
	unsigned int m_depthTexture, m_depthTextureLocation;
};

