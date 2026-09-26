#include "Light.h"
#include "../Utils/Utilities.hpp"
#include <stdexcept>

Light::Light(const Vector& position, float intensity)
	: type(LightType::POINT), position(position), intensity(intensity),
	  area(0.0f), center(position) {}

Light::Light(const Vector& corner, const Vector& u, const Vector& v,
	const Vector& emission, float intensity, int materialIndex)
	: type(LightType::AREA), position(corner), intensity(intensity),
	  u(u), v(v), emission(emission), emissiveMaterialIndex(materialIndex) {
	Vector n = cross(u, v);
	area = n.length();
	if (area <= 1e-6f) throw std::logic_error("area light has degenerate (zero-area) quad");
	normal = n / area;
	center = corner + (u + v) * 0.5f;
}

LightType Light::getType() const { return type; }

const Vector& Light::getPosition() const {
	return (type == LightType::AREA) ? center : position;
}

float Light::getIntensity() const { return intensity; }

Vector Light::samplePoint(float& pdfArea) const {
	float a = randFloat();
	float b = randFloat();
	pdfArea = 1.0f / area;
	return position + u * a + v * b;
}

const Vector& Light::getNormal() const { return normal; }
float Light::getArea() const { return area; }

Vector Light::emittedRadiance() const {
	return emission * (intensity / (area * 3.14159265358979323846f));
}

int Light::getEmissiveMaterialIndex() const { return emissiveMaterialIndex; }
