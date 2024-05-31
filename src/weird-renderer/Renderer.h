#pragma once

#include <vector>

#include "RenderPlane.h"
#include "../weird-engine/Scene.h"

using namespace std;

class Renderer
{
private:

	bool vSyncEnabled = false;

	RenderPlane m_sdfRenderPlane;

	Shader m_defaultShaderProgram;
	Shader m_defaultInstancedShaderProgram;
	Shader m_sdfShaderProgram;

	unsigned int m_width, m_height;



public:
	Renderer(const unsigned int width, const unsigned int height);
	~Renderer();
	void Render(Scene& scene, const double time);
	bool CheckWindowClosed() const;
	void SetWindowTitle(const char* name);

	// TODO: move to private after implementing input system
	GLFWwindow* m_window;

};

