#include "PrimitiveRenderer.h"
#include "ECS.h"
#include <stack>

void PrimitiveRenderer::Render() {

	glPushMatrix();

	Transform* t = _entity->Transform_;
	std::stack<Transform*> parentTransforms;
	while (t != nullptr)
	{
		parentTransforms.push(t);
		t = t->Parent;
	}

	while (!parentTransforms.empty()) {

		t = parentTransforms.top();
		parentTransforms.pop();

		Vector3D position = t->postition;
		glTranslatef(position.x, position.y, position.z);


		Vector3D eulerRotation = t->Rotation.ToEuler();
		glRotatef(eulerRotation.x, 1.0, 0.0, 0.0);
		glRotatef(eulerRotation.y, 0.0, 1.0, 0.0);
		glRotatef(eulerRotation.z, 0.0, 0.0, 1.0);

		Vector3D scale = t->scale;
		glScalef(scale.x, scale.y, scale.z);
	}


	glColor3f(_color.GetRedComponent(), _color.GetGreenComponent(), _color.GetBlueComponent());

	switch (_primitive)
	{
	case PrimitiveRenderer::Primitive::Sphere:
		glutSolidSphere(.5, 10, 10);
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

}