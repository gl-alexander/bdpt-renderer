#include "Matrix.h"
#include <cmath>

Matrix::Matrix(const float matrix[MATRIX_ROWS][MATRIX_COLUMNS]) {
	for (int i = 0; i < MATRIX_ROWS; i++) {
		for (int j = 0; j < MATRIX_COLUMNS; j++) {
			this->matrix[i][j] = matrix[i][j];
		}
	}
}

Matrix::Matrix(Vector e[MATRIX_ROWS])
{
	for (int i = 0; i < MATRIX_ROWS; i++) {
		matrix[i][0] = e[i].x;
		matrix[i][1] = e[i].y;
		matrix[i][2] = e[i].z;
	}
}

Matrix::Matrix(const Vector& e1, const Vector& e2, const Vector& e3)
{
	matrix[0][0] = e1.x; matrix[0][1] = e2.x; matrix[0][2] = e3.x;
	matrix[1][0] = e1.y; matrix[1][1] = e2.y; matrix[1][2] = e3.y;
	matrix[2][0] = e1.z; matrix[2][1] = e2.z; matrix[2][2] = e3.z;
}

Matrix::Matrix() {
	for (int i = 0; i < MATRIX_ROWS; i++) {
		for (int j = 0; j < MATRIX_COLUMNS; j++) {
			this->matrix[i][j] = 0;
		}
	}
}

Matrix& Matrix::operator*=(const Matrix& other) {
	*this = (*this) * other;
	return *this;
}
Matrix& Matrix::operator+=(const Matrix& other) {
	for (int i = 0; i < MATRIX_ROWS; ++i) {
		for (int j = 0; j < MATRIX_COLUMNS; ++j) {
			matrix[i][j] += other.matrix[i][j];
		}
	}
	return *this;
}
Matrix& Matrix::operator-=(const Matrix& other) {
	for (int i = 0; i < MATRIX_ROWS; ++i) {
		for (int j = 0; j < MATRIX_COLUMNS; ++j) {
			matrix[i][j] -= other.matrix[i][j];
		}
	}
	return *this;
}

Matrix Matrix::transpose() const
{
	Matrix transposed;
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			transposed.matrix[j][i] = this->matrix[i][j];
		}
	}
	return transposed;
}

Matrix operator*(const Matrix& lhs, const Matrix& rhs) {
	Matrix result;
	for (int i = 0; i < MATRIX_ROWS; ++i) {
		for (int j = 0; j < MATRIX_ROWS; ++j) {
			result.matrix[i][j] = 0.0f;
			for (int k = 0; k < MATRIX_ROWS; ++k) {
				result.matrix[i][j] += lhs.matrix[i][k] * rhs.matrix[k][j];
			}
		}
	}
	return result;
}
Matrix operator+(const Matrix& lhs, const Matrix& rhs) {
	Matrix result = lhs;
	result += rhs;
	return result;
}
Matrix operator-(const Matrix& lhs, const Matrix& rhs) {
	Matrix result = lhs;
	result += rhs;
	return result;
}

Vector operator*(const Vector& lhs, const Matrix& rhs) {
	// row-major multiplication
	Vector result;
	result.x += lhs.x * rhs.matrix[0][0];
	result.x += lhs.y * rhs.matrix[1][0];
	result.x += lhs.z * rhs.matrix[2][0];

	result.y += lhs.x * rhs.matrix[0][1];
	result.y += lhs.y * rhs.matrix[1][1];
	result.y += lhs.z * rhs.matrix[2][1];

	result.z += lhs.x * rhs.matrix[0][2];
	result.z += lhs.y * rhs.matrix[1][2];
	result.z += lhs.z * rhs.matrix[2][2];
	return result;
}


Matrix xRotationMatrix(float degrees) {
	// counterclockwise on the X axis
	float rad = degrees * PI / 180.0f;
	float cosA = cos(rad);
	float sinA = sin(rad);

	float rotX[3][3] = {
			{1.0f, 0.0f, 0.0f},
			{0.0f, cosA, -sinA},
			{0.0f, sinA, cosA}
	};

	return Matrix(rotX);
}

Matrix yRotationMatrix(float degrees) {
	// rotate counterclockwise on the Y axis
	float rad = (degrees * PI) / 180.0f;
	float cosA = cos(rad);
	float sinA = sin(rad);

	float rotY[3][3] = {
	{cosA, 0.0f, sinA},
	{0.0f, 1.0f, 0.0f},
	{-sinA, 0.0f, cosA}
	};

	return Matrix(rotY);
}

Matrix zRotationMatrix(float degrees) {
	// counterclockwise on the Z axis
	float rad = degrees * PI / 180.0f;
	float cosA = cos(rad);
	float sinA = sin(rad);

	float rotZ[3][3] = {
	{cosA, -sinA, 0.0f},
	{sinA, cosA, 0.0f},
	{0.0f, 0.0f, 1.0f}
	};

	return Matrix(rotZ);
}
