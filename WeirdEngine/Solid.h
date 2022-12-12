#pragma once
#include "Math.h"
#include "Color.h"

class Solid
{
private:
	Vector3D position;
	Vector3D orientation;
	Color color;

	Vector3D orientationSpeed;
	Vector3D speed;

	bool inactive;

public:
	Solid() :
		position(0, 0, 0),
		color(0, 0, 0),
		orientation(0, 0, 0),
		orientationSpeed(0, 0, 0),
		speed(0, 0, 0),
		inactive(false) {}

	inline float GetCoordinateX() const { return this->position.x; }
	inline float GetCoordinateY() const { return this->position.y; }
	inline float GetCoordinateZ() const { return this->position.z; }
	inline float GetRedComponent() const { return this->color.GetRedComponent(); }
	inline float GetGreenComponent() const { return this->color.GetGreenComponent(); }
	inline float GetBlueComponent() const { return this->color.GetBlueComponent(); }
	inline float GetOrientationX() const { return this->orientation.x; }
	inline float GetOrientationY() const { return this->orientation.y; }
	inline float GetOrientationZ() const { return this->orientation.z; }
	inline float GetOrientationSpeedX() const { return this->orientationSpeed.x; }
	inline float GetOrientationSpeedY() const { return this->orientationSpeed.y; }
	inline float GetOrientationSpeedZ() const { return this->orientationSpeed.z; }
	inline float GetSpeedX() const { return this->speed.x; }
	inline float GetSpeedY() const { return this->speed.y; }
	inline float GetSpeedZ() const { return this->speed.z; }

	inline Vector3D GetCoordinate() const { return this->position; }
	inline Vector3D GetOrientation() const { return this->orientation; }
	inline Vector3D GetSpeed() const { return this->speed; }

	inline void SetCoordinate(const float& coordinateXToSet, const float& coordinateYToSet, const float& coordinateZToSet) {
		this->position.x = coordinateXToSet, this->position.y = coordinateYToSet, this->position.x = coordinateZToSet;
	}
	inline void SetCoordinateVector(const Vector3D& p) {
		this->position.x = p.x, this->position.y = p.y, this->position.x = p.z;
	}
	inline void SetComponent(const float& redComponentToSet, const float& greenComponentToSet, const float& blueComponentToSet) {
		this->color.SetRedComponent(redComponentToSet), this->color.SetGreenComponent(greenComponentToSet), this->color.SetBlueComponent(blueComponentToSet);
	}
	inline void SetOrientation(const float& coordinateXToSet, const float& coordinateYToSet, const float& coordinateZToSet) {
		this->orientation.x = coordinateXToSet, this->orientation.y = coordinateYToSet, this->orientation.x = coordinateZToSet;
	}
	inline void SetOrientationSpeed(const float& coordinateXToSet, const float& coordinateYToSet, const float& coordinateZToSet) {
		this->orientationSpeed.x = coordinateXToSet, this->orientationSpeed.y = coordinateYToSet, this->orientationSpeed.x = coordinateZToSet;
	}
	inline void SetSpeed(const float& coordinateXToSet, const float& coordinateYToSet, const float& coordinateZToSet) {
		this->speed.x = coordinateXToSet, this->speed.y = coordinateYToSet, this->speed.x = coordinateZToSet;
	}
	inline void SetSpeedVector(const Vector3D& s) {
		this->speed.x = s.x, this->speed.y = s.y, this->speed.z = s.z;
	}

	inline void SetCoordinateX(const float& coordinateXToSet) { this->position.x = coordinateXToSet; }
	inline void SetCoordinateY(const float& coordinateYToSet) { this->position.y = coordinateYToSet; }
	inline void SetCoordinateZ(const float& coordinateZToSet) { this->position.z = coordinateZToSet; }
	inline void SetRedComponent(const float& redComponentToSet) { this->color.SetRedComponent(redComponentToSet); }
	inline void SetGreenComponent(const float& greenComponentToSet) { this->color.SetGreenComponent(greenComponentToSet); }
	inline void SetBlueComponent(const float& blueComponentToSet) { this->color.SetBlueComponent(blueComponentToSet); }
	inline void SetOrientationX(const float& coordinateXToSet) { this->orientation.x = coordinateXToSet; }
	inline void SetOrientationY(const float& coordinateYToSet) { this->orientation.y = coordinateYToSet; }
	inline void SetOrientationZ(const float& coordinateZToSet) { this->orientation.z = coordinateZToSet; }
	inline void SetOrientationSpeedX(const float& coordinateXToSet) { this->orientationSpeed.x = coordinateXToSet; }
	inline void SetOrientationSpeedY(const float& coordinateYToSet) { this->orientationSpeed.y = coordinateYToSet; }
	inline void SetOrientationSpeedZ(const float& coordinateZToSet) { this->orientationSpeed.z = coordinateZToSet; }
	inline void SetSpeedX(const float& coordinateXToSet) { this->speed.x = coordinateXToSet; }
	inline void SetSpeedY(const float& coordinateYToSet) { this->speed.y = coordinateYToSet; }
	inline void SetSpeedZ(const float& coordinateZToSet) { this->speed.x = coordinateZToSet; }

	inline bool GetInactive() { return this->inactive; }
	inline void Deactivate() { this->inactive = true; }
	inline void Activate() { this->inactive = false; }


	virtual void Render() = 0;
	virtual void Update(const float& time);
};

