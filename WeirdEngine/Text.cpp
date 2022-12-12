#include "Text.h"

void Text::Render()
{
	glPushMatrix();
	glColor3f(GetRedComponent(), GetGreenComponent(), GetBlueComponent());
	glTranslatef(GetCoordinateX(), GetCoordinateY(), GetCoordinateZ());
	glRasterPos3d(0, 0, 0);
	for (char c : text)
		glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, c);
	glPopMatrix();
}
