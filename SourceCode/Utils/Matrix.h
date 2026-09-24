#pragma once
#include "Vector.h"

const int MATRIX_ROWS = 3;
const int MATRIX_COLUMNS = 3;

class Matrix
{
	float matrix[3][3];
public:
	Matrix();
	Matrix(const float m[MATRIX_ROWS][MATRIX_COLUMNS]);
	Matrix(Vector e[MATRIX_ROWS]);
	Matrix(const Vector& e1, const Vector& e2, const Vector& e3);

	Matrix& operator*=(const Matrix& other);
	Matrix& operator+=(const Matrix& other);
	Matrix& operator-=(const Matrix& other);

	friend Matrix operator*(const Matrix& lhs, const Matrix& rhs);
	friend Matrix operator+(const Matrix& lhs, const Matrix& rhs);
	friend Matrix operator-(const Matrix& lhs, const Matrix& rhs);

	friend Vector operator*(const Vector& lhs, const Matrix& rhs);

	friend Matrix xRotationMatrix(float degrees);
	friend Matrix yRotationMatrix(float degrees);
	friend Matrix zRotationMatrix(float degrees);

	Matrix transpose() const;
};


Matrix xRotationMatrix(float degrees);
Matrix yRotationMatrix(float degrees);
Matrix zRotationMatrix(float degrees);
