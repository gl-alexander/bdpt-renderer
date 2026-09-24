#pragma once
#include "../Utils/Vector.h"

const int NO_HIT_INDEX = -1;

struct Intersection {
	Vector hitPoint;
	Vector faceNormal;
	Vector smoothNormal;
	Vector barycentricCoordinates;
	int materialIndex;
	int hitObjectIndex = NO_HIT_INDEX;
	int triangleIndex = NO_HIT_INDEX;
	float t;
};