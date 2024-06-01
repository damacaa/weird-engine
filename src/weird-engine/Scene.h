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
	void RenderModels(Shader& shader, Shader& instancingShader);
	void RenderShapes(Shader& shader, RenderPlane& rp);
	void Update(double delta, double time);

	Camera* m_camera;
	vec3 m_lightPosition;

private:

	size_t m_shapes = 0;
	size_t m_meshes = 255;
	bool m_useMeshInstancing = true;

	ResourceManager m_resourceManager;

	ECS m_ecs;
	std::vector<Entity> m_entities;

	double m_simulationDelay = 0.0;
	Simulation* m_simulation;

	vector<Light> m_lights;

	SDFRenderSystem m_sdfRenderSystem;
	RenderSystem m_renderSystem;
	InstancedRenderSystem m_instancedRenderSystem;
	RBPhysicsSystem m_rbPhysicsSystem;

	void LoadScene();
};

