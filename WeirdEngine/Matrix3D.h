#pragma once

template<class S> class Matrix3Dx;

typedef Matrix3Dx<float> Matrix3D;

typedef Matrix3Dx<float> Matrix3Dxf;
typedef Matrix3Dx<double> Matrix3Dxd;
typedef Matrix3Dx<int> Matrix3Dxi;


template<class S> class Matrix3Dx
{
private:
	S _values[3][3] = { {1,0,0}, {0,1,0}, {0,0,1} };
public:

	Matrix3Dx() {}

	Matrix3Dx(S(&array)[3][3]) {
		for (size_t i = 0; i < 3; i++)
		{
			for (size_t j = 0; j < 3; j++)
			{
				_values[i][j] = array[i][j];
			}
		}
	}

	S operator [] (int i) const { return _values[i]; }
	S& operator [] (int i) { return _values[i]; }

	template<class S>
	Vector3Dx<S> operator* (Vector3Dx<S> vector) {

		S x = vector.x * _values[0][0] + vector.y * _values[1][0] + vector.z * _values[2][0];
		S y = vector.x * _values[0][1] + vector.y * _values[1][1] + vector.z * _values[2][1];
		S z = vector.x * _values[0][2] + vector.y * _values[1][2] + vector.z * _values[2][2];

		return Vector3Dx<S>(x, y, z);
	};
};


