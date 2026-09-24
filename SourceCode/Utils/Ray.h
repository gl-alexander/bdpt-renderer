#pragma once
#include "Vector.h"

enum class RayType {
	CAMERA, 
	SHADOW, 
	REFLECTIVE,
	REFRACTIVE,
	LIGHT
};

struct Ray
{
	Vector origin;
	Vector direction;
	RayType type;
	int depth;
};

