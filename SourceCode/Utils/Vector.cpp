#include "Vector.h"
#include <cmath>

Vector::Vector() : Vector(0.0, 0.0, 0.0) {}
Vector::Vector(float x, float y, float z) : x(x), y(y), z(z) {}

Vector::Vector(const float pos[3])
{
	x = pos[0];
	y = pos[1];
	z = pos[2];
}

float Vector::length() const {
	return sqrt(x * x + y * y + z * z);
}

float Vector::length2() const
{
	return x * x + y * y + z * z;
}

Vector& Vector::normalize() {
	float originalLength = length();
	float multiplication = 1.0f / length();
	if (std::abs(originalLength - 1) <= EPSILON || originalLength <= EPSILON) {		
		// doesn't normalize already normalized vectors, as well as the 0 length vector
		return *this;
	}
	x *= multiplication;
	y *= multiplication;
	z *= multiplication;
	return *this;
}

Vector& Vector::clamp(float a, float b)
{
	x = clampValue(x, a, b);
	y = clampValue(y, a, b);
	z = clampValue(z, a, b);
	return *this;
}

Vector& Vector::operator+=(const Vector& rhs) {
	this->x += rhs.x;
	this->y += rhs.y;
	this->z += rhs.z;
	return *this;
}

Vector& Vector::operator-=(const Vector& rhs) {
	this->x -= rhs.x;
	this->y -= rhs.y;
	this->z -= rhs.z;
	return *this;
}

Vector& Vector::operator*=(float k) {
	x *= k;
	y *= k;
	z *= k;
	return *this;
}

Vector& Vector::operator/=(float k)
{
	return (*this) *= (1.0f / k);
}

// by-component multiplication
Vector& Vector::operator*=(const Vector& other) {
	x *= other.x;
	y *= other.y;
	z *= other.z;
	return *this;
}

bool Vector::operator==(const Vector& other) const {
	return abs(this->x - other.x) <= EPSILON
		&& abs(this->y - other.y) <= EPSILON
		&& abs(this->z - other.z) <= EPSILON;
}

Vector operator+(const Vector& lhs, const Vector& rhs) {
	Vector result = lhs;
	result += rhs;
	return result;
}

Vector operator-(const Vector& lhs, const Vector& rhs) {
	Vector result = lhs;
	result -= rhs;
	return result;
}

Vector operator-(const Vector& unaryVec)
{
	return unaryVec * -1.0f;
}

Vector operator*(const Vector& lhs, const float k) {
	Vector result = lhs;
	result *= k;
	return result;
}

Vector operator*(const float k, const Vector& rhs) {
	return rhs * k;
}

Vector operator/(const Vector& lhs, float k)
{
	Vector result = lhs;
	result /= k;
	return result;
}

Vector operator/(float k, const Vector& rhs)
{
	return rhs / k;
}

// by-component multiplication
Vector operator*(const Vector& lhs, const Vector& rhs) {
	Vector result = lhs;
	result *= rhs;
	return result;
}

/*
Cross product of A, B :
A (xA, yA, zA), B (xB, yB, zB)
is calculated as:
det (	e1, xA, xB
		e2, yA, yB
		e3, zA, zB	)

where e1, e2, e3 are the unit vectors of the space
when calculating it makes out:
*/

Vector cross(const Vector& A, const Vector& B) {
	return Vector(	A.y * B.z - A.z * B.y, 
						A.z * B.x - A.x * B.z, 
						A.x * B.y - A.y * B.x	);
}

float dot(const Vector& A, const Vector& B) {
	return (A.x * B.x) + (A.y * B.y) + (A.z * B.z);
}


Vector reflect(const Vector& incomming, const Vector& surfaceNormal)
{
	Vector Y = dot(incomming, surfaceNormal) * surfaceNormal;
	return (incomming - 2 * Y).normalize();
}

Vector randomHemisphereSample(const Vector& normal)
{
	Vector vec = randomSphereSample();

	/// restrict to the hemisphere: it is on the wrong side, just flip the result:
	if (dot(vec, normal) < 0)
		vec = -vec;

	return vec;
}

Vector randomSphereSample()
{
	double theta = 2 * PI * randFloat();
	double cosPhi = 2 * randFloat() - 1;
	double sinPhi = sqrt(1 - cosPhi * cosPhi);

	return Vector(
		cos(theta) * sinPhi,
		sin(theta) * sinPhi,
		cosPhi
	);
}


