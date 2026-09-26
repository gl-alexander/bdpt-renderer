#pragma once
#include "../Utils/Vector.h"

enum class LightType { POINT, AREA };

class Light {
	LightType type;
	Vector position;   // POINT: point; AREA: quad corner
	float intensity;
	Vector u, v;       // AREA edge vectors
	Vector normal;     // AREA derived, normalize(u x v)
	Vector emission;   // AREA emission colour
	float area;        // AREA derived, |u x v|
	Vector center;     // AREA derived, position + (u+v)/2
	int emissiveMaterialIndex = -1; // AREA: source emissive material (for s=0 lookup)
public:
	Light(const Vector& position, float intensity);                 // POINT
	Light(const Vector& corner, const Vector& u, const Vector& v,
		const Vector& emission, float intensity, int materialIndex); // AREA

	LightType getType() const;
	const Vector& getPosition() const;
	float getIntensity() const;

	Vector samplePoint(float& pdfArea) const;
	const Vector& getNormal() const;
	float getArea() const;
	Vector emittedRadiance() const;
	int getEmissiveMaterialIndex() const;
};
