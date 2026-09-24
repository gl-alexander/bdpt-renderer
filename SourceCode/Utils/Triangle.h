#pragma once
#include "Vector.h"
#include "Box.h"
#include "Ray.h"
#include <cmath>
#include <vector>
#include <utility>
#include "../Scene/Intersection.h"

const int VERTICES = 3;

class Triangle
{
	Vector vertices[VERTICES];
	Vector normal;
	void calculateNormalVector();

public:
	Triangle() = default;
	Triangle(const Vector vertices[VERTICES]);
	Triangle(const Vector vertices[VERTICES], const Vector& normal); // to avoid calculating the normal in case we already have it
	Triangle(const Vector& v1, const Vector& v2, const Vector& v3);
	Triangle(const Vector& v1, const Vector& v2, const Vector& v3, const Vector& normal);


	const Vector& getNormal() const;
	std::vector<Vector> getVertices() const;

	float area() const;
	float distanceToPoint(const Vector& point) const;
	bool pointInTriangle(const Vector& point) const;

	Intersection intersectsRay(const Ray& ray) const;
	bool isInBox(const Box& box) const;

	Vector getBarycenticCoordinates(const Vector& point) const;
};
