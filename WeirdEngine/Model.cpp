#include "Model.h"
#include "GL/glut.h"

void Model::AddTriangle(Triangle triangle)
{
	this->triangles.push_back(triangle);
}

void Model::Clear()
{
	this->triangles.clear();
}

void Model::PaintColor(Color color)
{
	for (Triangle& triangle : this->triangles)
	{
		triangle.SetColor1(color);
		triangle.SetColor2(color);
		triangle.SetColor3(color);
	}
}

void Model::Render()
{
	glPushMatrix();
	glTranslatef(this->GetCoordinateX(), this->GetCoordinateY(), this->GetCoordinateZ());
	glColor3f(this->GetRedComponent(), this->GetGreenComponent(), this->GetBlueComponent());
	glRotatef(this->GetOrientationX(), 1.0, 0.0, 0.0);
	glRotatef(this->GetOrientationY(), 0.0, 1.0, 0.0);
	glRotatef(this->GetOrientationZ(), 0.0, 0.0, 1.0);

	for (int i = 0; i < this->triangles.size(); i++) {
	 triangles[i].Render();
	}

	glPopMatrix();
}
