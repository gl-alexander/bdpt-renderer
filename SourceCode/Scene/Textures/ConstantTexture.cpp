#include "ConstantTexture.h"

ConstantTexture::ConstantTexture(const Vector& color) : color(color)
{}

Vector ConstantTexture::sample(float u, float v, const Vector& bary) const
{
	return color;
}
