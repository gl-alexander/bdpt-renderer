#pragma once
#include "../../Utils/Vector.h"

class CheckerTexture
{
	Vector colorA;
	Vector colorB;
	float squareSize;
public:
	CheckerTexture(const Vector& colorA, const Vector& colorB, float squareSize);
	Vector sample(float u, float v, const Vector& bary) const;
};
