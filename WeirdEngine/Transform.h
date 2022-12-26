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

	Vector3D GetRightVector();
	Vector3D GetUpVector();
	Vector3D GetForwardVector();
};

inline void Transform::Rotate(Vector3D axis, float angle)
{
	Quaternion newRotation = rotation * Quaternion(axis, angle);
	rotation = newRotation;
}

inline Vector3D Transform::GetRightVector()
{
	//Matrix3D rotMat = rotation.ToRotationMatrix();
	//return rotMat * Vector3D(1, 0, 0);
	return rotation.Right();
}

inline Vector3D Transform::GetUpVector()
{
	//Matrix3D rotMat = rotation.ToRotationMatrix();
	//return rotMat * Vector3D(0, 1, 0);
	return rotation.Up();
}

inline Vector3D Transform::GetForwardVector()
{
	//Matrix3D rotMat = rotation.ToRotationMatrix().Inverse();
	//Vector3D forward = rotMat * Vector3D(0, 0, 1);
	//forward.z = -forward.z;
	return rotation.Forward();
}