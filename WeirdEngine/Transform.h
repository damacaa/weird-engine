#pragma once
#include "Math.h"
class Transform
{
public:
	Vector3D position;
	Quaternion rotation;
	Vector3D scale = Vector3D(1, 1, 1);
	Transform* parent = nullptr;

	Transform() {};
	void SetParent(Transform* parent) {
		this->parent = parent;

		if (!parent)
			return;

		position.x = position.x / parent->scale.x;
		position.y = position.y / parent->scale.y;
		position.z = position.z / parent->scale.z;

		scale.x = scale.x / parent->scale.x;
		scale.y = scale.y / parent->scale.y;
		scale.z = scale.z / parent->scale.z;
	}

	void Rotate(Vector3D axis, float angle);
};

inline void Transform::Rotate(Vector3D axis, float angle)
{
	Quaternion newRotation = rotation * Quaternion(axis, angle);
	rotation = newRotation;
}
