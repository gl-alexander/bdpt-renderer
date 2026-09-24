#pragma once
#include <vector>
#include "../Utils/Triangle.h"
#include "../Utils/Ray.h"
#include "Material.h"
#include "Intersection.h"
#include <unordered_map>

class Mesh
{
	std::vector<Vector> vertices;
	std::vector<Vector> uvs;
	std::vector<int> triangleVertIndices;
	std::vector<Vector> vertexNormals;
	std::vector<Vector> faceNormals;
	std::unordered_map<int, std::vector<int>> vertexTriangleParticipation; // for each vertex stores the triangles it's a part of
	int materialIndex;

	void calculateFaceNormals();
	void calculateVertexNormals();

public:
	Mesh() = default;
	Mesh(const std::vector<Vector>& vertices, const std::vector<int>& triangleVertIndices, int materialIndex);
	Mesh(const std::vector<Vector>& vertices, const std::vector<Vector>& uvs, 
		const std::vector<int>& triangleVertIndices, int materialIndex);

	Intersection intersectsRay(const Ray& ray) const;

	Vector getUV(const Intersection& data) const;
	Vector sampleMaterial(const Material& material, const Intersection& data) const;
	Vector calculateSmoothNormal(int triangleIndex, const Vector& barycentic, const Vector& point) const;

	int getMaterialIndex() const;

	Triangle getTriangleByIndex(int index) const;
	std::vector<Triangle> getAllTriangles() const;
	unsigned trianglesCount() const;
};

