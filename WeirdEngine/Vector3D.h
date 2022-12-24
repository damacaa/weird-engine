#pragma once

#include <cmath>

template<class S> class Vector3Dx;

typedef Vector3Dx<float> Vector3D;

typedef Vector3Dx<float> Vector3Dxf;
typedef Vector3Dx<double> Vector3Dxd;
typedef Vector3Dx<int> Vector3Dxi;

template<class S> class Vector3Dx
{
private:
public:
	S x, y, z;

	Vector3Dx(S x = 0, S y = 0, S z = 0) :x(x), y(y), z(z) {}

	inline S X() const { return x; }
	inline S Y() const { return y; }
	inline S Z() const { return z; }
	inline void SetX(S d) { x = d; }
	inline void SetY(S d) { y = d; }
	inline void SetZ(S d) { z = d; }

	Vector3Dx<S> Add(Vector3Dx<S> v);
	Vector3Dx<S> Substract(Vector3Dx<S> v);
	Vector3Dx<S> Product(S s);
	Vector3Dx<S> Division(S s);
	S DotProduct(Vector3Dx<S> v);
	Vector3Dx<S> CrossProduct(Vector3Dx<S> v);
	S Norm();
	void Normalize();
	Vector3D Normalized();

	Vector3Dx<S> operator+(Vector3Dx<S> v);
	Vector3Dx<S> operator-(Vector3Dx<S> v);
	Vector3Dx<S> operator*(S s);
	Vector3Dx<S> operator/(S s);
	Vector3Dx<S> operator-() const;
	S operator*(Vector3Dx<S> v);

	static Vector3Dx<S> Clamp(Vector3Dx<S> v, Vector3Dx<S> min, Vector3Dx<S> max);
};

template <class S> Vector3Dx<S> Vector3Dx<S>::Add(Vector3Dx<S> v)
{
	return Vector3Dx<S>(x + v.X(), y + v.Y(), z + v.Z());
}

template <class S> Vector3Dx<S> Vector3Dx<S>::operator+(Vector3Dx<S> v)
{
	return Vector3Dx<S>(x + v.X(), y + v.Y(), z + v.Z());
}

template <class S> Vector3Dx<S> Vector3Dx<S>::Substract(Vector3Dx<S> v)
{
	return Vector3Dx<S>(x - v.X(), y - v.Y(), z - v.Z());
}


template <class S> Vector3Dx<S> Vector3Dx<S>::operator-(Vector3Dx<S> v)
{
	return Vector3Dx<S>(x - v.X(), y - v.Y(), z - v.Z());
}

template <class S> Vector3Dx<S> Vector3Dx<S>::Product(S s)
{
	return Vector3Dx<S>(x * s, y * s, z * s);
}

template <class S> Vector3Dx<S> Vector3Dx<S>::operator*(S s)
{
	return Vector3Dx<S>(x * s, y * s, z * s);
}

template <class S> Vector3Dx<S> Vector3Dx<S>::Division(S s)
{
	return Vector3Dx<S>(x / s, y / s, z / s);
}

template <class S> Vector3Dx<S> Vector3Dx<S>::operator/(S s)
{
	return Vector3Dx<S>(x / s, y / s, z / s);
}

template <class S> Vector3Dx<S> Vector3Dx<S>::operator-() const {
	return Vector3Dx<S>(-x, -y, -z);
}

template <class S> S Vector3Dx<S>::DotProduct(Vector3Dx<S> v)
{
	return  x * v.x + y * v.y + z * v.z;
}

template <class S> Vector3Dx<S> Vector3Dx<S>::CrossProduct(Vector3Dx<S> v)
{
	return Vector3Dx<S>(
		(y * v.z) - (z * v.y),
		(z * v.x) - (x * v.z),
		(x * v.y) - (y * v.x));
}

template<class S> S Vector3Dx<S>::Norm()
{
	return sqrt(x * x + y * y + z * z);
}

template<class S>
inline void Vector3Dx<S>::Normalize()
{
	S norm = Norm();
	if (norm == 0)
		return;
	x = x / norm;
	y = y / norm;
	z = z / norm;
}

template<class S>
inline Vector3D Vector3Dx<S>::Normalized()
{
	S norm = Norm();
	if (norm == 0)
		return Vector3D();

	return Vector3D(x / norm, y / norm, z / norm);
}

template <class S> S Vector3Dx<S>::operator*(Vector3Dx<S> v)
{
	return x * v.X + this->Y * v.Y + this->Z * v.Z;
}

template<class S>
inline Vector3Dx<S> Vector3Dx<S>::Clamp(Vector3Dx<S> v, Vector3Dx<S> min, Vector3Dx<S> max)
{
	S x = v.x < min.x ? min.x : v.x;
	x = x > max.x ? max.x : x;

	S y = v.y < min.y ? min.y : v.y;
	y = y > max.y ? max.y : y;

	S z = v.z < min.z ? min.z : v.z;
	z = z > max.z ? max.z : z;

	return Vector3Dx<S>(x, y, z);
}

template <class S> Vector3Dx<S> operator *(S scale, Vector3Dx<S> v)
{
	return Vector3Dx<S>(scale * v.x, scale * v.y, scale * v.z);
}