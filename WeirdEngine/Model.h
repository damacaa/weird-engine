#pragma once
#include "Solid.h"
#include "Triangle.h"
#include <vector>
using namespace std;

class Model : public Solid
{
private:

	vector<Triangle> triangles;

public:

	void AddTriangle(Triangle triangle);
	void Clear();
	void PaintColor(Color color);
	void Render();

	inline vector<Triangle> GetTriangles() { return this->triangles; }
	inline void SetModel(Model m) { this->triangles = m.GetTriangles(); }
};

