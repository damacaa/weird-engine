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
	Scene();
	~Scene();
	void renderModels(Shader& shader, Shader& instancingShader);
	void renderShapes(Shader& shader, RenderPlane& rp);
	void update(double delta, double time);

	std::unique_ptr<Camera> camera;

private:
	ECS m_ecs;
	ResourceManager m_resourceManager;

	Simulation m_simulation;
	double m_simulationDelay = 0.0;

	SDFRenderSystem m_sdfRenderSystem;
	RenderSystem m_renderSystem;
	InstancedRenderSystem m_instancedRenderSystem;
	RBPhysicsSystem m_rbPhysicsSystem;

	size_t m_shapes = 0;
	size_t m_meshes = 0;
	bool m_useMeshInstancing = true;
	vector<Light> m_lights;

	void loadScene();
};

