#include "Triangle.h"
#include "GL/glut.h"

void Triangle::Render()
{
	glBegin(GL_TRIANGLES);
	
	//vértice 1
	glColor3f(this->GetColor1().GetRedComponent(), this->GetColor1().GetGreenComponent(), this->GetColor1().GetBlueComponent());
	glNormal3f(this->GetNormal1().X(), this->GetNormal1().Y(), this->GetNormal1().Z());
	glVertex3f(this->GetVertex1().X(), this->GetVertex1().Y(), this->GetVertex1().Z());
	
	//vértice 2
	glColor3f(this->GetColor2().GetRedComponent(), this->GetColor2().GetGreenComponent(), this->GetColor2().GetBlueComponent());
	glNormal3f(this->GetNormal2().X(), this->GetNormal2().Y(), this->GetNormal2().Z());
	glVertex3f(GetVertex2().X(), this->GetVertex2().Y(), this->GetVertex2().Z());
	
	//vértice 3
	glColor3f(this->GetColor3().GetRedComponent(), this->GetColor3().GetGreenComponent(), this->GetColor3().GetBlueComponent());
	glNormal3f(this->GetNormal3().X(), this->GetNormal3().Y(), this->GetNormal3().Z());
	glVertex3f(this->GetVertex3().X(), this->GetVertex3().Y(), this->GetVertex3().Z());

	glEnd();

}
