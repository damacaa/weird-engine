#pragma once
#include "../weird-renderer/Shape.h"
#include "../weird-renderer/Model.h"
#include "../weird-physics/Simulation.h"

using namespace std;
class Scene
{
public:
	Scene();
	~Scene();
	void Render(Camera& camera, Shader& shader);
	void Update(double delta, double time);

	size_t m_size = 10;
	Shape* m_data;
	vector<Model*> m_models;

	Camera* m_camera;
private:
	double simulationDelay = 0.0;
	//std::unique_ptr<Simulation> m_simulation;

	void LoadScene();
};

