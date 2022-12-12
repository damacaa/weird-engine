#pragma once
#include "Solid.h"
class Triangle : public Solid
{
private:

	Vector3D vertex1, vertex2, vertex3, normal1, normal2, normal3;
	Color color1, color2, color3;

public:
	Triangle() : Solid(), 
		vertex1(0,0,0), vertex2(0, 0, 0), vertex3(0, 0, 0),
		normal1(0, 0, 0), normal2(0, 0, 0), normal3(0, 0, 0), 
		color1(0, 0, 0), color2(0, 0, 0), color3(0, 0, 0) {};

	Triangle(Vector3D v1, Vector3D v2, Vector3D v3, Vector3D n1, Vector3D n2, Vector3D n3) : Solid(),
		vertex1(v1), vertex2(v2), vertex3(v3),
		normal1(n1), normal2(n2), normal3(n3),
		color1(0, 0, 0), color2(0, 0, 0), color3(0, 0, 0) {};

	inline Vector3D GetVertex1() const { return this->vertex1; }
	inline Vector3D GetVertex2() const { return this->vertex2; }
	inline Vector3D GetVertex3() const { return this->vertex3; }
	inline Vector3D GetNormal1() const { return this->normal1; }
	inline Vector3D GetNormal2() const { return this->normal2; }
	inline Vector3D GetNormal3() const { return this->normal3; }
	inline Color GetColor1() const { return this->color1; }
	inline Color GetColor2() const { return this->color2; }
	inline Color GetColor3() const { return this->color3; }

	inline void SetVertex1(const Vector3D& vertex1ToSet) { this->vertex1 = vertex1ToSet; }
	inline void SetVertex2(const Vector3D& vertex2ToSet) { this->vertex2 = vertex2ToSet; }
	inline void SetVertex3(const Vector3D& vertex3ToSet) { this->vertex3 = vertex3ToSet; }
	inline void SetNormal1(const Vector3D& normal1ToSet) { this->normal1 = normal1ToSet; }
	inline void SetNormal2(const Vector3D& normal2ToSet) { this->normal2 = normal2ToSet; }
	inline void SetNormal3(const Vector3D& normal3ToSet) { this->normal3 = normal3ToSet; }
	inline void SetColor1(const Color& color1ToSet) { this->color1 = color1ToSet; }
	inline void SetColor2(const Color& color2ToSet) { this->color2 = color2ToSet; }
	inline void SetColor3(const Color& color3ToSet) { this->color3 = color3ToSet; }

	void Render();
};

