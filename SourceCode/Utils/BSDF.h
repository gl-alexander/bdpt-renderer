#pragma once
#include "Vector.h"
#include "../Scene/Scene.h"
#include "../Scene/Intersection.h"

// High-precision PI for all pdf / We math (global PI=3.1415f is too coarse)
constexpr double PI_HI = 3.14159265358979323846;

// Return value of all BSDF sampling helpers
struct ScatterSample {
	Vector origin;       // next ray origin (with surface bias already applied)
	Vector dir;          // next ray direction (unit)
	Vector betaFactor;   // multiply into running beta: (albedo/PI)*cos/pdf for diffuse; reflectance/discreteProb for specular
	float  pdf_dir;      // continuous solid-angle pdf (0.0 for delta)
	float  discreteProb; // discrete branch probability (1.0 for diffuse/pure-reflect; fresnel or 1-fresnel for refract)
	bool   isDelta;
};

// Surface scattering: given an intersection and the incoming direction, sample
// an outgoing direction and the throughput / pdf factors the path integrator needs.
// Dispatches on material type; specular branches return delta samples (pdf_dir=0).
struct BSDF
{
	static ScatterSample spawnRay(const Scene* scene, const Intersection& data, const Vector& vec_in);
	static ScatterSample spawnDiffuseRay(const Scene* scene, const Intersection& data, const Vector& vec_in);
	static ScatterSample spawnReflectRay(const Scene* scene, const Intersection& data, const Vector& vec_in);
	static ScatterSample spawnRefractRay(const Scene* scene, const Intersection& data, const Vector& vec_in);
};
