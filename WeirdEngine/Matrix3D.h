#pragma once

template<class S> class Matrix3Dx;

typedef Matrix3Dx<float> Matrix3D;

typedef Matrix3Dx<float> Matrix3Dxf;
typedef Matrix3Dx<double> Matrix3Dxd;
typedef Matrix3Dx<int> Matrix3Dxi;


template<class S> class Matrix3Dx
{
private:
	S m_values[3][3] = { {1,0,0}, {0,1,0}, {0,0,1} };
public:

	Matrix3Dx() {}

	Matrix3Dx(S(&array)[3][3]) {
		for (size_t i = 0; i < 3; i++)
		{
			for (size_t j = 0; j < 3; j++)
			{
				m_values[i][j] = array[i][j];
			}
		}
	}

	//S operator [] (int i) const { return m_values[i]; }
	S* operator [] (int i) { return m_values[i]; }

	template<class S>
	Matrix3Dx<S> operator* (S scalar) {
		float newValues[3][3];

		for (size_t i = 0; i < 3; i++)
		{
			for (size_t j = 0; j < 3; j++)
			{
				newValues[i][j] = scalar * m_values[i][j];
			}
		}

		return Matrix3Dx<S>(newValues);
	}

	template<class S>
	Vector3Dx<S> operator* (Vector3Dx<S> vector) {

		S x = vector.x * m_values[0][0] + vector.y * m_values[1][0] + vector.z * m_values[2][0];
		S y = vector.x * m_values[0][1] + vector.y * m_values[1][1] + vector.z * m_values[2][1];
		S z = vector.x * m_values[0][2] + vector.y * m_values[1][2] + vector.z * m_values[2][2];

		return Vector3Dx<S>(x, y, z);
	};

	template<class S>
	Matrix3Dx<S> operator* (Matrix3Dx<S> matrix) {

		float newValues[3][3]{ 0 };

		for (size_t i = 0; i < 3; i++)
		{
			for (size_t j = 0; j < 3; j++)
			{
				// WHY DOES THIS NOT WORK?!
				/*values[i][j] =
					(m_values[i][1] * matrix[1][j]) +
					(m_values[i][2] * matrix[2][j]) +
					(m_values[i][3] * matrix[3][j]);*/

					// AND THIS DOES!
				for (int k = 0; k < 3; k++) {
					newValues[i][j] += m_values[i][k] * matrix[k][j];
				}
			}
		}

		return Matrix3Dx<S>(newValues);
	};



	float Determinant() {

		float result =
			m_values[0][0] * m_values[1][1] * m_values[2][2] +
			m_values[0][1] * m_values[1][2] * m_values[2][0] +
			m_values[0][2] * m_values[1][0] * m_values[2][1] -
			m_values[0][2] * m_values[1][1] * m_values[2][0] -
			m_values[0][1] * m_values[1][0] * m_values[2][2] -
			m_values[0][0] * m_values[1][2] * m_values[2][1];

		return result;
	};

	float Determinant(float a00, float a01, float a10, float a11) {
		float result = a00 * a11 - a01 * a10;
		return result;
	}

	Matrix3D Adjugate() {

		float minors[3][3];
		minors[0][0] = Determinant(m_values[1][1], m_values[1][2], m_values[2][1], m_values[2][2]);
		minors[0][1] = -Determinant(m_values[1][0], m_values[1][2], m_values[2][0], m_values[2][2]);
		minors[0][2] = Determinant(m_values[1][0], m_values[1][1], m_values[2][0], m_values[2][1]);
		minors[1][0] = -Determinant(m_values[0][1], m_values[0][2], m_values[2][1], m_values[2][2]);
		minors[1][1] = Determinant(m_values[0][0], m_values[0][2], m_values[2][0], m_values[2][2]);
		minors[1][2] = -Determinant(m_values[0][0], m_values[0][1], m_values[2][0], m_values[2][1]);
		minors[2][0] = Determinant(m_values[0][1], m_values[0][2], m_values[1][1], m_values[1][2]);
		minors[2][1] = -Determinant(m_values[0][0], m_values[0][2], m_values[1][0], m_values[1][2]);
		minors[2][2] = Determinant(m_values[0][0], m_values[0][1], m_values[1][0], m_values[1][1]);

		// Transpose the matrix of minors to obtain the adjugate matrix
		float transpose[3][3];
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				transpose[i][j] = minors[j][i];
			}
		}

		return Matrix3D(transpose);
	};

	Matrix3D Inverse() {
		return   Adjugate() * (1 / Determinant());
	};

};


