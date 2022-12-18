#pragma once
#include "Math.h"
class Transform
{
public:
	Vector3D postition;
	//Vector3D eulerRotation;
	Quaternion Rotation;
	Vector3D scale = Vector3D(1, 1, 1);
	Transform* Parent = nullptr;

	Transform() {};
	void SetParent(Transform* parent) {
		Parent = parent;
		postition.x = postition.x / parent->scale.x;
		postition.y = postition.y / parent->scale.y;
		postition.z = postition.z / parent->scale.z;

		scale.x = scale.x / parent->scale.x;
		scale.y = scale.y / parent->scale.y;
		scale.z = scale.z / parent->scale.z;
	}
};

