#pragma once
#include "../../Utils/Vector.h"

class ConstantTexture
{
	Vector color;
public:
	ConstantTexture(const Vector& color);
	Vector sample(float u, float v, const Vector& bary) const;
};