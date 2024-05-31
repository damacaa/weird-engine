#pragma once
#include "../weird-renderer/Shape.h"
#include "../weird-renderer/Model.h"
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

	size_t m_size = 10;
	ResourceManager m_resourceManager;

	Camera* m_camera;
	vec3 m_lightPosition;


private:

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

