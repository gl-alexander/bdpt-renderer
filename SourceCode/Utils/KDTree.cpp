#include "KDTree.h"
#include <iostream>

static unsigned nearestPow2(unsigned n) {
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return ++n;
}

KDTree::KDTree(const std::vector<Mesh>& objects, const std::vector<Material>& materials, const Box& sceneAABB) 
	: objects(objects), materials(materials)
{
	NodeTriangles allTriangles;
	int currTrianglesCount = 0;
	objectTriangleCount.push_back(currTrianglesCount);
	for (auto& mesh : objects) {
		const std::vector<Triangle> meshTriangles = std::move(mesh.getAllTriangles());
		for (int i = 0; i < meshTriangles.size(); i++) {
			allTriangles.push_back({ meshTriangles[i], i + currTrianglesCount });
		}
		currTrianglesCount += meshTriangles.size();
		objectTriangleCount.push_back(currTrianglesCount);
	}
	// The worst case scenario is that each leaf node contains 1 triangle, so we'd have allTriangles.size() leaf nodes, regardless of MAX_TRIANGLES
	// this implementation of a KD tree is binary and the maximum number of nodes for a full k-tree with L leaf nodes is 
	// L + nearestPowK(L) - 1
	// if there are less triangles than MAX_TRIANGLES, then leafNodes is 0 and all triangles will be in the root box
	unsigned maxNodesCount = allTriangles.size() + nearestPow2(allTriangles.size()) - 1;
	nodes.reserve(maxNodesCount);
	nodes.push_back({ sceneAABB, INVALID_IND, INVALID_IND, INVALID_IND, NodeTriangles() });
	build(0, allTriangles, 0);
}

// parentInd is the node whose children we're creating, depth is the current recursion depth
void KDTree::build(int parentInd, const NodeTriangles& remainingTriangles, int depth)
{
	treeDepth = std::max(treeDepth, depth);
	if (remainingTriangles.size() <= MAX_TRIANGLES || depth > MAX_DEPTH) {
		nodes[parentInd].triangles = remainingTriangles;
		return;
	}

	Box first, second;
	nodes[parentInd].box.split(first, second, (AxisLabel)(depth % AXIS_COUNT));

	NodeTriangles firstTriangles;
	NodeTriangles secondTriangles;
	firstTriangles.reserve(remainingTriangles.size() / 2);
	secondTriangles.reserve(remainingTriangles.size() / 2);

	for (auto& tri : remainingTriangles) {
		if (tri.triangle.isInBox(first)) {
			firstTriangles.push_back(tri);
		}
		if (tri.triangle.isInBox(second)) {
			secondTriangles.push_back(tri);
		}
	}
	if (firstTriangles.size() > 0) {
		int firstInd = nodes.size();
		nodes.push_back({ first, parentInd, INVALID_IND, INVALID_IND, NodeTriangles() });
		nodes[parentInd].leftInd = firstInd;
		build(firstInd, firstTriangles, depth + 1);
	}
	if (secondTriangles.size() > 0) {
		int secondInd = nodes.size();
		nodes.push_back({ second, parentInd, INVALID_IND, INVALID_IND, NodeTriangles() });
		nodes[parentInd].rightInd = secondInd;
		build(secondInd, secondTriangles, depth + 1);
	}
	
}

// triangleIndex is the total index of the triangle in the scene, as stored in TriangleIndexPair
void KDTree::getMeshAndRelativeIndex(int triangleIndex, int& meshIndex, int& relativeIndex) const
{
	// in order to find the index of the mesh this triangle is part of, we can use its index
	// knowing that the meshes are in a fixed order, if for example meshes[0] has 30 triangles, meshes[1] has 12 and meshes[2] has 45
	// we can build the objectTriangleCount by adding the number of meshes in total in the scene for each mesh in order
	// for the above example the array would look like this: [0, 30, 42, 87]
	// so when searching for triangle with index 37, we can just do std::lower_bound and find it in log(#meshes) time

	auto it = std::lower_bound(objectTriangleCount.begin(), objectTriangleCount.end(), triangleIndex);
	meshIndex = std::distance(objectTriangleCount.begin(), it);
	if (objectTriangleCount[meshIndex] != triangleIndex) {
		meshIndex--;
		// this is because lower bound returns the first element that is not less than triangleIndex
		// so for triangleIndex 34 it would return an iterator to 42, or meshIndex 2, when in reality it should be 1 
	}
	relativeIndex = triangleIndex - objectTriangleCount[meshIndex];
}

Intersection KDTree::intersectLeaf(const Ray& ray, const NodeTriangles& triangles, float maxDistance) const
{
	Intersection intersection, triangle_intersection;
	int meshIndex = 0, relativeIndex = 0;
	float closestHitDitance = FLOAT_MAX;
	for (auto& tri : triangles) {
		triangle_intersection = std::move(tri.triangle.intersectsRay(ray));
		if (triangle_intersection.triangleIndex != NO_HIT_INDEX) {
			if (triangle_intersection.t < closestHitDitance && triangle_intersection.t <= maxDistance) {
				getMeshAndRelativeIndex(tri.index, meshIndex, relativeIndex);
				/*if (ray.type == RayType::SHADOW
					&& materials[objects[meshIndex].getMaterialIndex()].type == MaterialType::REFRACTIVE) {
					continue;
				}*/
				closestHitDitance = triangle_intersection.t;
				intersection = std::move(triangle_intersection); // this copies all the data we've already calculated
				intersection.triangleIndex = relativeIndex;
				intersection.hitObjectIndex = meshIndex;
			}
		}
	}
	if (intersection.triangleIndex != NO_HIT_INDEX) {
		intersection.smoothNormal = objects[meshIndex].calculateSmoothNormal(
			intersection.triangleIndex, intersection.barycentricCoordinates, intersection.hitPoint);
		intersection.materialIndex = objects[meshIndex].getMaterialIndex();
	}
	return intersection;
}

Intersection KDTree::intersect(const Ray& ray, float maxDistance) const
{
	Intersection result;
	result.t = FLOAT_MAX;
	Intersection currIntersection = {};
	std::stack<int> nodeStack;
	int currInd = 0;
	nodeStack.push(currInd);
	while (!nodeStack.empty()) {
		currInd = nodeStack.top();
		nodeStack.pop();
		if (nodes[currInd].box.intersects(ray)) {
			if (nodes[currInd].triangles.size() > 0) { // leaf node
				currIntersection = intersectLeaf(ray, nodes[currInd].triangles, maxDistance);
				if (currIntersection.triangleIndex != NO_HIT_INDEX) {
					if (currIntersection.t < result.t) {
						result = std::move(currIntersection);
					}
				}
			}
			else { // adds children nodes
				if (nodes[currInd].leftInd != INVALID_IND)
					nodeStack.push(nodes[currInd].leftInd);
				if (nodes[currInd].rightInd != INVALID_IND)
					nodeStack.push(nodes[currInd].rightInd);
			}
		}
	}
	return result;
}