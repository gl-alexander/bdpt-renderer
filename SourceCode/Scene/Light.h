#pragma once
#include "../Utils/Vector.h"

class Light {
	Vector position;
	float intensity;
public:
	Light(const Vector& position, float intensity);

	const Vector& getPosition() const;
	float getIntensity() const;
};