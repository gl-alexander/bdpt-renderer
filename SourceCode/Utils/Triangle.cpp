#include "Triangle.h"
#include <limits>

Triangle::Triangle(const Vector vertices[VERTICES]) : Triangle(vertices[0], vertices[1], vertices[2]) 
{
}

Triangle::Triangle(const Vector vertices[VERTICES], const Vector& normal) : Triangle(vertices[0], vertices[1], vertices[2], normal) 
{
}


Triangle::Triangle(const Vector& v1, const Vector& v2, const Vector& v3) {
	this->vertices[0] = v1;
	this->vertices[1] = v2;
	this->vertices[2] = v3;
	calculateNormalVector();
}


Triangle::Triangle(const Vector& v1, const Vector& v2, const Vector& v3, const Vector& normal) {
	this->vertices[0] = v1;
	this->vertices[1] = v2;
	this->vertices[2] = v3;
	this->normal = normal;
}


/* the order of the vertices matters
		v2


	v0		v1
*/
void Triangle::calculateNormalVector(){
	Vector v1 = vertices[1] - vertices[0];
	Vector v2 = vertices[2] - vertices[0];

	normal = cross(v1, v2); // we set it to the counterclockwise cross-product
	normal.normalize();
}

const Vector& Triangle::getNormal() const {
	return this->normal;
}

std::vector<Vector> Triangle::getVertices() const
{
	std::vector<Vector> vert = { vertices[0], vertices[1], vertices[2] };
	return vert;
}


float Triangle::area() const {
	Vector v1 = vertices[1] - vertices[0];
	Vector v2 = vertices[2] - vertices[0];

	return cross(v1, v2).length() / 2;
}

float Triangle::distanceToPoint(const Vector& point) const {
	// it doesn't matter which vertex we pick to calculate from
	return dot(normal, vertices[0] - point);
}


bool Triangle::pointInTriangle(const Vector& point) const {
	Vector E0 = vertices[1] - vertices[0];
	Vector E1 = vertices[2] - vertices[1];
	Vector E2 = vertices[0] - vertices[2];

	Vector relativeE0 = cross(E0, point - vertices[0]);
	Vector relativeE1 = cross(E1, point - vertices[1]);
	Vector relativeE2 = cross(E2, point - vertices[2]);

	// all the cross products need to be oriented the same way as the normal vector

	return  dot(normal, relativeE0) >= -EPSILON && dot(normal, relativeE1) >= -EPSILON && dot(normal, relativeE2) >= -EPSILON;
}

Intersection Triangle::intersectsRay(const Ray& ray) const {
	Intersection intersection;
	intersection.triangleIndex = NO_HIT_INDEX;

	float dotPr = dot(normal, ray.direction);
	if (ray.type == RayType::CAMERA && dotPr >= 0) {
		// back face culling for camera rays only
		// Ray is parallel to the plane or facing away from the triangle
		return intersection;
	}

	// Calculate the signed distance from the ray origin to the plane
	float d = -dot(normal, vertices[0]); // the D in the ray equation

	float t = -(dot(normal, ray.origin) + d) / dotPr;

	if (t < 0) {
		// The intersection is behind the ray origin
		return intersection;
	}

	// Compute the hit point
	Vector hitPoint = ray.origin + (ray.direction * t);

	if (pointInTriangle(hitPoint)) {
		intersection.hitPoint = hitPoint;
		intersection.triangleIndex = 0;
		intersection.faceNormal = normal;
		intersection.barycentricCoordinates = getBarycenticCoordinates(hitPoint);
		intersection.t = t;
		return intersection;
	}

	return intersection;
}

bool Triangle::isInBox(const Box& box) const
{
	Vector tr_min{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
	Vector tr_max = -tr_min;
	for (auto& vertex : vertices) {
		tr_min.x = std::min(tr_min.x, vertex.x);
		tr_min.y = std::min(tr_min.y, vertex.y);
		tr_min.z = std::min(tr_min.z, vertex.z);

		tr_max.x = std::max(tr_max.x, vertex.x);
		tr_max.y = std::max(tr_max.y, vertex.y);
		tr_max.z = std::max(tr_max.z, vertex.z);
	}
	if (tr_min.x > box.max.x || tr_max.x < box.min.x)
		return false;
	if (tr_min.y > box.max.y || tr_max.y < box.min.y)
		return false;
	if (tr_min.z > box.max.z || tr_max.z < box.min.z)
		return false;
	return true;
}

Vector Triangle::getBarycenticCoordinates(const Vector& point) const
{
	// we could skip the isInTriangle check to optimize runtime
	Triangle M_triangle(vertices[0], point, vertices[2], normal);	// both subtriangles are from the same plane
	Triangle N_triangle(vertices[0], vertices[1], point, normal);	// so they can use the same normal

	float thisArea = area();

	float u = M_triangle.area() / thisArea;
	float v = N_triangle.area() / thisArea;

	return { u, v, 1 - u - v };
}
