#pragma once
#include "../weird-renderer/Shape.h"
#include "../weird-physics/Simulation.h"
#include "../weird-renderer/RenderPlane.h"



#include "ecs/ECS.h"

#include "ResourceManager.h"

using namespace std;
class Scene
{
public:

	Scene(const char* file);
	~Scene();
	void renderModels(Shader& shader, Shader& instancingShader);
	void renderShapes(Shader& shader, RenderPlane& rp);
	void update(double delta, double time);

	std::unique_ptr<Camera> camera;


	Scene(const Scene&) = default; // Deleted copy constructor
	Scene& operator=(const Scene&) = default; // Deleted copy assignment operator
	Scene(Scene&&) = default; // Defaulted move constructor
	Scene& operator=(Scene&&) = default; // Defaulted move assignment operator

private:
	ECS m_ecs;
	ResourceManager m_resourceManager;

	Simulation m_simulation;
	double m_simulationDelay = 0.0;

	SDFRenderSystem m_sdfRenderSystem;
	RenderSystem m_renderSystem;
	InstancedRenderSystem m_instancedRenderSystem;
	RBPhysicsSystem m_rbPhysicsSystem;

	vector<Light> m_lights;

	void loadScene(std::string sceneFileContent);
};

