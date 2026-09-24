#include "Mesh.h"
#include <assert.h>
#include <limits>

void Mesh::calculateFaceNormals() {
    for (int i = 0; i < triangleVertIndices.size(); i += VERTICES) {
        Vector v0 = vertices[triangleVertIndices[i + 0]];
        Vector v1 = vertices[triangleVertIndices[i + 1]];
        Vector v2 = vertices[triangleVertIndices[i + 2]];

        //store them while traversing the face normals
        vertexTriangleParticipation[triangleVertIndices[i + 0]].push_back(i / VERTICES);
        vertexTriangleParticipation[triangleVertIndices[i + 1]].push_back(i / VERTICES);
        vertexTriangleParticipation[triangleVertIndices[i + 2]].push_back(i / VERTICES);
        Vector V1 = v1 - v0;
        Vector V2 = v2 - v0;

        Vector normal = cross(V1, V2); // we set it to the counterclockwise cross-product
        normal.normalize();
        faceNormals[i / VERTICES] = std::move(normal);
    }
}

void Mesh::calculateVertexNormals() {
    for (int i = 0; i < vertices.size(); i++) {
        Vector vNormal;
        for (int triangleIndex : vertexTriangleParticipation[i]) {
            vNormal += faceNormals[triangleIndex];
        }
        vNormal *= (1.0f / vertexTriangleParticipation[i].size()); // since each face normal has len 1 we only need to scale it down by the number of vectors we've added
        vertexNormals[i] = std::move(vNormal);
    }
}


Mesh::Mesh(const std::vector<Vector>& vertices, const std::vector<int>& triangleVertIndices, int materialIndex) :
    Mesh(vertices, std::vector<Vector>(), triangleVertIndices, materialIndex) 
{}

Mesh::Mesh(const std::vector<Vector>& vertices, const std::vector<Vector>& uvs, const std::vector<int>& triangleVertIndices, int materialIndex) :
    vertices(vertices), triangleVertIndices(triangleVertIndices), materialIndex(materialIndex), uvs(uvs)
{
    faceNormals.resize(triangleVertIndices.size() / VERTICES);
    vertexNormals.resize(vertices.size());
    calculateFaceNormals();
    calculateVertexNormals();
}

Vector Mesh::calculateSmoothNormal(int triangleIndex, const Vector& barycentic, const Vector& point) const {
    Vector v0_VertexNormal = vertexNormals[triangleVertIndices[triangleIndex * VERTICES + 0]];
    Vector v1_VertexNormal = vertexNormals[triangleVertIndices[triangleIndex * VERTICES + 1]];
    Vector v2_VertexNormal = vertexNormals[triangleVertIndices[triangleIndex * VERTICES + 2]];

    // calculate the normal based on the barycentic coordinates {u, v, w} using the formula
    return (v0_VertexNormal * barycentic.z
        + v1_VertexNormal * barycentic.x
        + v2_VertexNormal * barycentic.y).normalize();
}

Intersection Mesh::intersectsRay(const Ray& ray) const
{
    Intersection intersection;

    float closestHitDitance = std::numeric_limits<float>::max();
    Intersection triangle_intersection;
    for (int i = 0; i < triangleVertIndices.size(); i += VERTICES) {
        Triangle triangle = getTriangleByIndex(i / VERTICES);
        triangle_intersection = std::move(triangle.intersectsRay(ray));
        if (triangle_intersection.triangleIndex != NO_HIT_INDEX) {
            if (triangle_intersection.t < closestHitDitance) {
                closestHitDitance = triangle_intersection.t;
                intersection = std::move(triangle_intersection); // this copies all the data we've already calculated
                intersection.triangleIndex = i / VERTICES;
            }
        }
    }
    if (intersection.triangleIndex == NO_HIT_INDEX) {
        intersection.hitObjectIndex = NO_HIT_INDEX;
        return intersection;
    }
    intersection.smoothNormal = calculateSmoothNormal(intersection.triangleIndex, intersection.barycentricCoordinates, intersection.hitPoint);
    intersection.materialIndex = materialIndex;
    intersection.hitObjectIndex = 0; // so we mark the hit;
    return intersection;
}

Vector Mesh::getUV(const Intersection& data) const
{
    unsigned v0_index = triangleVertIndices[data.triangleIndex * VERTICES + 0];
    unsigned v1_index = triangleVertIndices[data.triangleIndex * VERTICES + 1];
    unsigned v2_index = triangleVertIndices[data.triangleIndex * VERTICES + 2];
    
    return uvs[v1_index] * data.barycentricCoordinates.x
        + uvs[v2_index] * data.barycentricCoordinates.y
        + uvs[v0_index] * data.barycentricCoordinates.z;
}

Vector Mesh::sampleMaterial(const Material& material, const Intersection& data) const
{
    if (material.constantAlbedo) {
        return material.albedo;
    }
    else {
        assert(material.texture != nullptr);
        Vector uv = getUV(data);
        // std::variant allows the type safe std::visit
        return std::visit([&uv, &data](const auto& t) { return t.sample(uv.x, uv.y, data.barycentricCoordinates); }, *material.texture);
    }
}


int Mesh::getMaterialIndex() const {
    return materialIndex;
}

Triangle Mesh::getTriangleByIndex(int index) const
{
    int ind = index * VERTICES;
    if (ind >= triangleVertIndices.size() || ind < 0) {
        assert(false);
    }
    return Triangle (vertices[triangleVertIndices[ind + 0]],
        vertices[triangleVertIndices[ind + 1]],
        vertices[triangleVertIndices[ind + 2]],
        faceNormals[index]);

}

std::vector<Triangle> Mesh::getAllTriangles() const
{
    int count = trianglesCount();
    std::vector<Triangle> triangles;
    triangles.reserve(count);
    for (int i = 0; i < count; i++) {
        triangles.push_back(getTriangleByIndex(i));
    }
    return triangles;
}

unsigned Mesh::trianglesCount() const
{
    return faceNormals.size();
}
