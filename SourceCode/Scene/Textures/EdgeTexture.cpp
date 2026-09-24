#include "EdgeTexture.h"
#include <iostream>

EdgeTexture::EdgeTexture(const Vector& edgeColor, const Vector& innerColor, float edgeWidth)
	: edgeColor(edgeColor), innerColor(innerColor), edgeWidth(edgeWidth)
{}

Vector EdgeTexture::sample(float u, float v, const Vector& bary) const
{
	if (bary.x < edgeWidth || bary.y < edgeWidth || abs(bary.z) < edgeWidth || 1 - bary.x < edgeWidth || 1 - bary.y < edgeWidth) {
		return edgeColor;
	}
	return innerColor;
}
