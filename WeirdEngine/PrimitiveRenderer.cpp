#include "PrimitiveRenderer.h"
#include "ECS.h"
#include <stack>

void PrimitiveRenderer::Render() {

	glPushMatrix();

	Transform* t = _entity->Transform_;
	Vector3D position = t->postition;
	glTranslatef(position.x, position.y, position.z);

	Vector3D eulerRotation = t->Rotation.ToEuler();
	glRotatef(eulerRotation.x, 1.0, 0.0, 0.0);
	glRotatef(eulerRotation.y, 0.0, 1.0, 0.0);
	glRotatef(eulerRotation.z, 0.0, 0.0, 1.0);

	Vector3D scale = t->scale;
	glScalef(scale.x, scale.y, scale.z);

	glColor3f(_color.GetRedComponent(), _color.GetGreenComponent(), _color.GetBlueComponent());

	switch (_primitive)
	{
	case PrimitiveRenderer::Primitive::Sphere:
		glutSolidSphere(.5, 12, 12);
		break;
	case PrimitiveRenderer::Primitive::Cube:
		glutSolidCube(1);
		break;
	case PrimitiveRenderer::Primitive::Cone:
		glutSolidCone(.5, 1, 10, 0);
		break;
	case PrimitiveRenderer::Primitive::Torus:
		glutSolidTorus(.2, .4, 10, 10);
		break;
	case PrimitiveRenderer::Primitive::Dodecahedron:
		glutSolidDodecahedron();
		break;
	case PrimitiveRenderer::Primitive::Octahedron:
		glutSolidOctahedron();
		break;
	case PrimitiveRenderer::Primitive::Tetrahedron:
		glutSolidTetrahedron();
		break;
	case PrimitiveRenderer::Primitive::Icosahedron:
		glutSolidIcosahedron();
		break;
	case PrimitiveRenderer::Primitive::Teapot:
		glutSolidTeapot(1);
		break;
	default:
		break;
	}
	glPopMatrix();

	// Bounding box
	/*glPopMatrix();

	glPushMatrix();
	glTranslatef(position.x, position.y, position.z);
	glScalef(scale.x, scale.y, scale.z);

	glColor3f(1, 0, 0);
	glutWireCube(1);

	glPopMatrix();*/
}