#include "Solid.h"

void Solid::Update(const float& time)
{
	if (!this->inactive) 
	{
		Vector3D orientationInit = this->orientation;
		this->orientation = orientationInit + this->orientationSpeed * time;

		Vector3D positionInit = this->position;
		this->position = positionInit + this->speed * time;
	}
}
