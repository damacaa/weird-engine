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

	Vector3Dx<S> operator+(Vector3Dx<S> v);
	Vector3Dx<S> operator-(Vector3Dx<S> v);
	Vector3Dx<S> operator*(S s);
	Vector3Dx<S> operator/(S s);
	S operator*(Vector3Dx<S> v);
};

template <class S> Vector3Dx<S> Vector3Dx<S>::Add(Vector3Dx<S> v)
{
	return Vector3Dx<S>(this->X() + v.X(), this->Y() + v.Y(), this->Z() + v.Z());
}

template <class S> Vector3Dx<S> Vector3Dx<S>::operator+(Vector3Dx<S> v)
{
	return Vector3Dx<S>(this->X() + v.X(), this->Y() + v.Y(), this->Z() + v.Z());
}

template <class S> Vector3Dx<S> Vector3Dx<S>::Substract(Vector3Dx<S> v)
{
	return Vector3Dx<S>(this->X() - v.X(), this->Y() - v.Y(), this->Z() - v.Z());
}


template <class S> Vector3Dx<S> Vector3Dx<S>::operator-(Vector3Dx<S> v)
{
	return Vector3Dx<S>(this->X() - v.X(), this->Y() - v.Y(), this->Z() - v.Z());
}

template <class S> Vector3Dx<S> Vector3Dx<S>::Product(S s)
{
	return Vector3Dx<S>(this->X() * s, this->Y() * s, this->Z() * s);
}

template <class S> Vector3Dx<S> Vector3Dx<S>::operator*(S s)
{
	return Vector3Dx<S>(this->X() * s, this->Y() * s, this->Z() * s);
}

template <class S> Vector3Dx<S> Vector3Dx<S>::Division(S s)
{
	return Vector3Dx<S>(this->X() / s, this->Y() / s, this->Z() / s);
}

template <class S> Vector3Dx<S> Vector3Dx<S>::operator/(S s)
{
	return Vector3Dx<S>(this->X() / s, this->Y() / s, this->Z() / s);
}


template <class S> S Vector3Dx<S>::DotProduct(Vector3Dx<S> v)
{
	return this->X() * v.X + this->Y * v.Y + this->Z * v.Z;
}

template <class S> Vector3Dx<S> Vector3Dx<S>::CrossProduct(Vector3Dx<S> v)
{
	return Vector3Dx<S>(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.z);
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

template <class S> S Vector3Dx<S>::operator*(Vector3Dx<S> v)
{
	return this->X() * v.X + this->Y * v.Y + this->Z * v.Z;
}

template <class S> Vector3Dx<S> operator *(S scale, Vector3Dx<S> v)
{
	return Vector3Dx<S>(scale * v.x, scale * v.y, scale * v.z);
}