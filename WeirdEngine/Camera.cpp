#include "Camera.h"
#include "ECS.h"

void Camera::Render()
{
	Vector3D eulerRotation = GetEntity().GetTransform().rotation.ToEuler();
	glRotatef(eulerRotation.x, 1.0, 0.0, 0.0);
	glRotatef(eulerRotation.y, 0.0, 1.0, 0.0);
	glRotatef(eulerRotation.z, 0.0, 0.0, 1.0);

	Vector3D position = GetEntity().GetTransform().position;
	glTranslatef(-position.x, -position.y, -position.z);
}
