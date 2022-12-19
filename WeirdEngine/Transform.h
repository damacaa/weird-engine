#pragma once
#include "Math.h"
class Transform
{
public:
	Vector3D postition;
	Quaternion rotation;
	Vector3D scale = Vector3D(1, 1, 1);
	Transform* parent = nullptr;

	Transform() {};
	void SetParent(Transform* parent) {
		this->parent = parent;

		if (!parent)
			return;

		postition.x = postition.x / parent->scale.x;
		postition.y = postition.y / parent->scale.y;
		postition.z = postition.z / parent->scale.z;

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
