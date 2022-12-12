#include "Camera.h"
#include "ECS.h"

void Camera::Render()
{
	Vector3D eulerRotation = _entity->Transform_->eulerRotation;
	glRotatef(eulerRotation.x, 1.0, 0.0, 0.0);
	glRotatef(eulerRotation.y, 0.0, 1.0, 0.0);
	glRotatef(eulerRotation.z, 0.0, 0.0, 1.0);

	Vector3D position = _entity->Transform_->postition;
	glTranslatef(-position.x, -position.y, -position.z);
}
