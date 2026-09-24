#pragma once
#include "Utilities.hpp"

constexpr int AXIS_COUNT = 3;

enum class AxisLabel {
	X = 0,
	Y = 1,
	Z = 2
};

struct Vector {
	static const unsigned MEMBERS_COUNT = 3;

	float x;
	float y;
	float z;

	Vector();
	Vector(float x, float y, float z);
	Vector(const float pos[3]);
	
	float length() const;
	float length2() const;
	Vector& normalize();
	Vector& clamp(float a, float b);

	Vector& operator+=(const Vector& rhs);
	Vector& operator-=(const Vector& rhs);

	Vector& operator*=(float k);
	Vector& operator/=(float k);
	Vector& operator*=(const Vector& other);

	bool operator==(const Vector& other) const;
};

Vector operator+(const Vector& lhs, const Vector& rhs);
Vector operator-(const Vector& lhs, const Vector& rhs);

Vector operator-(const Vector& unaryVec);

Vector operator*(const Vector& lhs, float k);
Vector operator*(float k, const Vector& rhs);
Vector operator/(const Vector& lhs, float k);
Vector operator/(float k, const Vector& rhs);
Vector operator*(const Vector& lhs, const Vector& rhs);


Vector cross(const Vector& lhs, const Vector& rhs);
float dot(const Vector& lhs, const Vector& rhs);
// reflects the incomming ray by the surfaceNormal
Vector reflect(const Vector& incomming, const Vector& surfaceNormal);
// generates a random normal vector in a hemisphere with the passed normal
// param normal: the normal, by which the hemisphere is defined
Vector randomHemisphereSample(const Vector& normal);
Vector randomSphereSample();