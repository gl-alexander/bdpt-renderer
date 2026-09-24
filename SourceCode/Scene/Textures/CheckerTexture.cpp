#include "CheckerTexture.h"

CheckerTexture::CheckerTexture(const Vector& colorA, const Vector& colorB, float squareSize)
	: colorA(colorA), colorB(colorB), squareSize(squareSize)
{}

Vector CheckerTexture::sample(float u, float v, const Vector& bary) const
{
	unsigned squareX = u / squareSize;
	unsigned squareY = v / squareSize;
	if (squareX % 2 == squareY % 2) {
		return colorA;
	}
	return colorB;
}
