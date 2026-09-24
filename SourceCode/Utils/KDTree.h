#pragma once
#include <vector>
#include <unordered_map>
#include <stack>
#include "../Scene/Mesh.h"
#include "../Scene/Material.h"
#include "Box.h"
#include "Triangle.h"

static const int INVALID_IND = -1;

static const int MAX_DEPTH = 40;
static const int MAX_TRIANGLES = 16;

struct KDTreeNode {
	struct TriangleIndexPair {
		Triangle triangle;
		int index = INVALID_IND;
	};
	Box box;
	int parentInd = INVALID_IND;
	int leftInd = INVALID_IND;
	int rightInd = INVALID_IND;
	std::vector<TriangleIndexPair> triangles;
};

using NodeTriangles = std::vector<KDTreeNode::TriangleIndexPair>;

class KDTree {
	std::vector<KDTreeNode> nodes;
	int treeDepth;
	std::vector<int> objectTriangleCount;
	const std::vector<Mesh>& objects;
	const std::vector<Material>& materials;

	// calculates the index of the mesh and the relative index of the triangle in the mesh, by a given absolute triangle Index
	void getMeshAndRelativeIndex(int triangleIndex, int& meshIndex, int& relativeIndex) const;

	Intersection intersectLeaf(const Ray& ray, const NodeTriangles& triangles, float maxDist = FLOAT_MAX) const;

	void build(int parentInd, const NodeTriangles& remainingTriangles, int depth);
public:
	KDTree(const std::vector<Mesh>& objects, const std::vector<Material>& materials, const Box& sceneAABB);

	// returns the closest intersection to the ray origin
	Intersection intersect(const Ray& ray, float maxDist = FLOAT_MAX) const;
};