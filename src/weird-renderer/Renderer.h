#pragma once

#include <vector>

#include "RenderPlane.h"
#include "../weird-engine/Scene.h"

using namespace std;

class Renderer
{
private:
	GLFWwindow* m_window;
	unsigned int m_windowWidth, m_windowHeight;
	double m_renderScale = 0.25;
	unsigned int m_renderWidth, m_renderHeight;

	bool m_vSyncEnabled = true;
	bool m_renderMeshesOnly = false;

	Shader m_defaultShaderProgram;
	Shader m_defaultInstancedShaderProgram;
	Shader m_sdfShaderProgram;

	RenderPlane m_sdfRenderPlane;

public:
	Renderer(const unsigned int width, const unsigned int height);
	~Renderer();
	void render(Scene& scene, const double time);
	bool checkWindowClosed() const;
	void setWindowTitle(const char* name);

	GLFWwindow* getWindow();
};

