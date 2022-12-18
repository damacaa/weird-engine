#pragma once
#include <cmath>

const float DEG_TO_RAD = 0.01745329252;
const float RAD_TO_DEG = 57.29577951308;

class Quaternion
{
private:

public:
	float _x, _y, _z, _w;

	Quaternion() : _x(0), _y(0), _z(0), _w(1) {};

	Quaternion(float x, float y, float z, float w) :
		_x(x), _y(y), _z(z), _w(w) {};

	/// <summary>
	/// Axis-angle to quaternion
	/// </summary>
	/// <param name="v">Axis</param>
	/// <param name="angle">Angle in deg</param>
	Quaternion(Vector3D axis, float angle) {
		axis.Normalize();
		float rad = angle * DEG_TO_RAD;

		_x = sin(rad / 2.0f) * axis.x;
		_y = sin(rad / 2.0f) * axis.y;
		_z = sin(rad / 2.0f) * axis.z;
		_w = cos(rad / 2.0f);
	};

	Quaternion(float roll, float pitch, float yaw) // roll (x), pitch (Y), yaw (z)
	{
		// Abbreviations for the various angular functions

		float cr = cos(roll * DEG_TO_RAD * 0.5);
		float sr = sin(roll * DEG_TO_RAD * 0.5);
		float cp = cos(pitch * DEG_TO_RAD * 0.5);
		float sp = sin(pitch * DEG_TO_RAD * 0.5);
		float cy = cos(yaw * DEG_TO_RAD * 0.5);
		float sy = sin(yaw * DEG_TO_RAD * 0.5);

		_w = cr * cp * cy + sr * sp * sy;
		_x = sr * cp * cy - cr * sp * sy;
		_y = cr * sp * cy + sr * cp * sy;
		_z = cr * cp * sy - sr * sp * cy;
	}



	Quaternion operator+(Quaternion other);
	Quaternion operator*(Quaternion other);

	void Normalize();
	void Invert();

	float Norm();

	Quaternion Conjugate();

	Vector3D ToEuler();
	Matrix3D ToRotationMatrix();
};


inline Quaternion Quaternion::operator+(Quaternion other)
{
	return Quaternion(_x + other._x, _y + other._y, _z + other._z, _w + other._w);
}

inline Quaternion Quaternion::operator*(Quaternion other)
{
	float a = _x * other._w + _w * other._x + _y * other._z - _z * other._y;
	float b = _y * other._w + _w * other._y + _z * other._x - _x * other._z;
	float c = _z * other._w + _w * other._z + _x * other._y - _y * other._x;
	float d = _w * other._w - _x * other._x - _y * other._y - _z * other._z;

	return Quaternion(a, b, c, d);
}

inline void Quaternion::Normalize()
{
	float inverseNorm = 1 / Norm();
	_x = _x * inverseNorm;
	_y = _y * inverseNorm;
	_z = _z * inverseNorm;
	_w = _w * inverseNorm;
}

inline void Quaternion::Invert()
{
	_x = -_x;
	_y = -_y;
	_z = -_z;
}

inline float Quaternion::Norm()
{
	return sqrt(_x * _x + _y * _y + _z * _z + _w + _w);
}

inline Quaternion Quaternion::Conjugate()
{
	return Quaternion(-_x, -_y, -_z, _w);
}

inline Vector3D Quaternion::ToEuler()
{
	Vector3D angles;

	// roll (x-axis rotation)
	float sinr_cosp = 2 * (_w * _x + _y * _z);
	float cosr_cosp = 1 - 2 * (_x * _x + _y * _y);
	angles.x = RAD_TO_DEG * std::atan2(sinr_cosp, cosr_cosp);

	// pitch (y-axis rotation)
	float sinp = 2 * (_w * _y - _z * _x);
	if (std::abs(sinp) >= 1)
		angles.y = RAD_TO_DEG * std::copysign(3.1416 / 2.0, sinp); // use 90 degrees if out of range
	else
		angles.y = RAD_TO_DEG * std::asin(sinp);

	// yaw (z-axis rotation)
	float siny_cosp = 2 * (_w * _z + _x * _y);
	float cosy_cosp = 1 - 2 * (_y * _y + _z * _z);
	angles.z = RAD_TO_DEG * std::atan2(siny_cosp, cosy_cosp);

	return angles;
}

inline Matrix3D Quaternion::ToRotationMatrix()
{
	float values[3][3];

	float q0 = _w;
	float q1 = _x;
	float q2 = _y;
	float q3 = _z;


	values[0][0] = 2 * (q0 * q0 + q1 * q1) - 1;
	values[0][1] = 2 * (q1 * q2 - q0 * q3);
	values[0][2] = 2 * (q1 * q3 + q0 * q2);

	values[1][0] = 2 * (q1 * q2 + q0 * q3);
	values[1][1] = 2 * (q0 * q0 + q2 * q2) - 1;
	values[1][2] = 2 * (q2 * q3 - q0 * q1);


	values[2][0] = 2 * (q1 * q3 - q0 * q2);
	values[2][1] = 2 * (q2 * q3 + q0 * q1);
	values[2][2] = 2 * (q0 * q0 + q3 * q3) - 1;

	return Matrix3D(values);
}
