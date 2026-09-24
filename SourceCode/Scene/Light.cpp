#include "Light.h"

Light::Light(const Vector& position, float intensity) : position(position), intensity(intensity)
{}

const Vector& Light::getPosition() const
{
	return position;
}

float Light::getIntensity() const
{
	return intensity;
}
