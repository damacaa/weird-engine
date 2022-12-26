#pragma once
#include <cmath>

const float DEG_TO_RAD = 0.01745329252;
const float RAD_TO_DEG = 57.29577951308;

class Quaternion
{
private:
	float m_x, m_y, m_z, m_w;
public:

	Quaternion() : m_x(0), m_y(0), m_z(0), m_w(1) {};

	Quaternion(float x, float y, float z, float w) :
		m_x(x), m_y(y), m_z(z), m_w(w) {};

	/// <summary>
	/// Axis-angle to quaternion
	/// </summary>
	/// <param name="v">Axis</param>
	/// <param name="angle">Angle in deg</param>
	Quaternion(Vector3D axis, float angle) {
		axis.Normalize();
		float rad = angle * DEG_TO_RAD;

		m_x = sin(rad / 2.0f) * axis.x;
		m_y = sin(rad / 2.0f) * axis.y;
		m_z = sin(rad / 2.0f) * axis.z;
		m_w = cos(rad / 2.0f);
	};

	Quaternion(float roll, float pitch, float yaw) // roll (x), pitch (Y), yaw (z)
	{
		// Abbreviations for the various angular functions

		float cr = cos(roll * DEG_TO_RAD * 0.5f);
		float sr = sin(roll * DEG_TO_RAD * 0.5f);
		float cp = cos(pitch * DEG_TO_RAD * 0.5f);
		float sp = sin(pitch * DEG_TO_RAD * 0.5f);
		float cy = cos(yaw * DEG_TO_RAD * 0.5f);
		float sy = sin(yaw * DEG_TO_RAD * 0.5f);

		m_w = cr * cp * cy + sr * sp * sy;
		m_x = sr * cp * cy - cr * sp * sy;
		m_y = cr * sp * cy + sr * cp * sy;
		m_z = cr * cp * sy - sr * sp * cy;
	}

	Quaternion operator+(Quaternion other);
	Quaternion operator*(Quaternion other);

	void Normalize();
	void Invert();

	float Norm();

	Quaternion Conjugate();

	Vector3D ToEuler();
	Matrix3D ToRotationMatrix();

	Vector3D Right();
	Vector3D Up();
	Vector3D Forward();

	Quaternion& operator = (const Quaternion& rhs);
};


inline Quaternion Quaternion::operator+(Quaternion other)
{
	return Quaternion(m_x + other.m_x, m_y + other.m_y, m_z + other.m_z, m_w + other.m_w);
}

inline Quaternion Quaternion::operator*(Quaternion other)
{
	float a = m_x * other.m_w + m_w * other.m_x + m_y * other.m_z - m_z * other.m_y;
	float b = m_y * other.m_w + m_w * other.m_y + m_z * other.m_x - m_x * other.m_z;
	float c = m_z * other.m_w + m_w * other.m_z + m_x * other.m_y - m_y * other.m_x;
	float d = m_w * other.m_w - m_x * other.m_x - m_y * other.m_y - m_z * other.m_z;

	return Quaternion(a, b, c, d);
}

inline void Quaternion::Normalize()
{
	float inverseNorm = 1 / Norm();
	m_x = m_x * inverseNorm;
	m_y = m_y * inverseNorm;
	m_z = m_z * inverseNorm;
	m_w = m_w * inverseNorm;
}

inline void Quaternion::Invert()
{
	m_x = -m_x;
	m_y = -m_y;
	m_z = -m_z;
}

inline float Quaternion::Norm()
{
	return sqrt(m_x * m_x + m_y * m_y + m_z * m_z + m_w + m_w);
}

inline Quaternion Quaternion::Conjugate()
{
	return Quaternion(-m_x, -m_y, -m_z, m_w);
}

inline Vector3D Quaternion::ToEuler()
{
	Vector3D angles;

	// roll (x-axis rotation)
	float sinr_cosp = 2 * (m_w * m_x + m_y * m_z);
	float cosr_cosp = 1 - 2 * (m_x * m_x + m_y * m_y);
	angles.x = RAD_TO_DEG * std::atan2(sinr_cosp, cosr_cosp);

	// pitch (y-axis rotation)
	float sinp = 2 * (m_w * m_y - m_z * m_x);
	if (std::abs(sinp) >= 1)
		angles.y = RAD_TO_DEG * std::copysign(3.1416 / 2.0, sinp); // use 90 degrees if out of range
	else
		angles.y = RAD_TO_DEG * std::asin(sinp);

	// yaw (z-axis rotation)
	float siny_cosp = 2 * (m_w * m_z + m_x * m_y);
	float cosy_cosp = 1 - 2 * (m_y * m_y + m_z * m_z);
	angles.z = RAD_TO_DEG * std::atan2(siny_cosp, cosy_cosp);

	return angles;
}

inline Matrix3D Quaternion::ToRotationMatrix()
{
	float values[3][3];

	float q0 = m_w;
	float q1 = m_x;
	float q2 = m_y;
	float q3 = m_z;


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

inline Vector3D Quaternion::Right()
{
	Vector3D right;
	right.x = 1 - 2 * (m_y * m_y + m_z * m_z);
	right.y = 2 * (m_x * m_y + m_w * m_z);
	right.z = -2 * (m_x * m_z - m_w * m_y);
	return right;
}

inline Vector3D Quaternion::Up()
{
	Vector3D up;
	up.x = 2 * (m_x * m_y - m_w * m_z);
	up.y = 1 - 2 * (m_x * m_x + m_z * m_z);
	up.z = -2 * (m_y * m_z + m_w * m_x);

	return up;
}

inline Vector3D Quaternion::Forward()
{
	Vector3D forward;
	forward.x = 2.0f * (-m_x * -m_z - m_w * -m_y);
	forward.y = 2.0f * (-m_y * -m_z + m_w * -m_x);
	forward.z = -1.0f + 2.0f * (-m_x * -m_x + -m_y * -m_y);

	return forward;
}

inline Quaternion& Quaternion::operator=(const Quaternion& rhs)
{
	if (this != &rhs) {
		m_x = rhs.m_x;
		m_y = rhs.m_y;
		m_z = rhs.m_z;
		m_w = rhs.m_w;
	}
	return *this;
}
