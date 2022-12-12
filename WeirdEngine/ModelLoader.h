#pragma once
#include "Math.h"
#include "Model.h"
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
using namespace std;

class ModelLoader
{
private:
	float s = 0;
	Model model;
	vector<Vector3D> vertexes;
	vector<Vector3D> normals;
	float minX, minY, minZ, maxX, maxY, maxZ;

	inline float getWidth()	{ return this->maxX - this->minX; };
	inline float getHeight()	{ return this->maxY - this->minY; };
	inline float getLength()	{ return this->maxZ - this->minZ; };

	void calcBoundaries(Vector3D lastVertex);
	Triangle center(Triangle t);

	Vector3D parseObjLineToVector3D(const string& line);
	Triangle parseObjTriangle(const string& line);

	ModelLoader(float s = 1) : s(s) {}
public:

	static ModelLoader* GetInstance() {
		return new ModelLoader();
	}

	inline float getScale() { return this->s; }
	inline void setScale(const float& sizeToSet) { this->s = sizeToSet; }

	Model getModel() { return this->model; }
	void LoadModel(const string& ruta);

	void Clear();


	

};

