#include "BSDF.h"
#include "Utilities.hpp"
#include <cmath>
#include <algorithm>

// Surface-offset biases (mirror Raytracer.h; kept local so BSDF doesn't depend
// on the whole raytracer header).
static constexpr float REFLECTION_BIAS = 0.001f;
static constexpr float REFRACTION_BIAS = 0.001f;

ScatterSample BSDF::spawnDiffuseRay(const Scene* scene, const Intersection& data, const Vector& vec_in)
{
	const Material& material = scene->getMaterial(data.materialIndex);
	Vector normal = material.smoothShading ? data.smoothNormal : data.faceNormal;

	// Cosine-weighted hemisphere sampling: pdf = cosTheta/PI.  The cosTheta then
	// cancels the BRDF's cosine in betaFactor, so betaFactor = albedo (no cosine,
	// no grazing-angle variance) — the main quality-per-sample win for diffuse GI.
	float u1 = randFloat(), u2 = randFloat();
	float r = std::sqrt(u1), phi = (float)(2.0 * PI_HI) * u2;
	float x = r * std::cos(phi), y = r * std::sin(phi), z = std::sqrt(std::max(0.0f, 1.0f - u1));
	// Orthonormal basis (t,b,normal); z aligns with the normal.
	Vector up = std::abs(normal.z) < 0.999f ? Vector(0, 0, 1) : Vector(1, 0, 0);
	Vector t = cross(up, normal).normalize();
	Vector b = cross(normal, t);
	Vector dir = (t * x + b * y + normal * z).normalize();
	float cosTheta = std::max(0.0f, dot(dir, normal));

	// betaFactor = (albedo/PI) * cosTheta / (cosTheta/PI) = albedo
	Vector albedo = scene->getGeometryObject(data.hitObjectIndex).sampleMaterial(material, data);
	float pdf = (float)(cosTheta / PI_HI);

	ScatterSample s;
	s.origin      = data.hitPoint + normal * REFLECTION_BIAS;
	s.dir         = dir;
	s.betaFactor  = albedo;
	s.pdf_dir     = pdf;
	s.discreteProb = 1.0f;
	s.isDelta     = false;
	return s;
}

ScatterSample BSDF::spawnReflectRay(const Scene* scene, const Intersection& data, const Vector& vec_in)
{
	const Material& material = scene->getMaterial(data.materialIndex);
	Vector normal = material.smoothShading ? data.smoothNormal : data.faceNormal;

	ScatterSample s;
	s.origin      = data.hitPoint + normal * REFLECTION_BIAS;
	s.dir         = reflect(vec_in, normal);
	// betaFactor = reflectance / discreteProb = albedo / 1.0 (pure mirror: no cosine, no continuous pdf)
	s.betaFactor  = scene->getGeometryObject(data.hitObjectIndex).sampleMaterial(material, data);
	s.pdf_dir     = 0.0f;
	s.discreteProb = 1.0f;
	s.isDelta     = true;
	return s;
}

ScatterSample BSDF::spawnRefractRay(const Scene* scene, const Intersection& data, const Vector& vec_in)
{
	const Material& material = scene->getMaterial(data.materialIndex);
	Vector normal = material.smoothShading ? data.smoothNormal : data.faceNormal;

	float n1 = 1.0f;
	float n2 = material.ior;

	float dotPr = dot(vec_in, normal);
	if (dotPr > 0) {
		normal *= -1;
		dotPr  *= -1;
		std::swap(n1, n2);
	}

	float cosIncoming  = -dotPr;
	float sinIncoming2 = 1.0f - cosIncoming * cosIncoming;

	// Schlick Fresnel
	float R0 = ((n1 - n2) / (n1 + n2)) * ((n1 - n2) / (n1 + n2));
	float fresnel = R0 + (1.0f - R0) * std::pow(1.0f - cosIncoming, 5.0f);

	Vector albedo = scene->getGeometryObject(data.hitObjectIndex).sampleMaterial(material, data);

	// Total internal reflection
	if (sinIncoming2 > (n2 * n2) / (n1 * n1)) {
		ScatterSample s;
		s.origin       = data.hitPoint + normal * REFLECTION_BIAS;
		s.dir          = reflect(vec_in, normal);
		s.betaFactor   = albedo;  // TIR: all energy reflects, discreteProb=1
		s.pdf_dir      = 0.0f;
		s.discreteProb = 1.0f;
		s.isDelta      = true;
		return s;
	}

	float sinOutcoming  = (n1 / n2) * std::sqrt(std::max(0.0f, 1.0f - cosIncoming * cosIncoming));
	float cosOutcoming  = std::sqrt(std::max(0.0f, 1.0f - sinOutcoming * sinOutcoming));

	Vector refractDir = cosOutcoming * (-normal) + (vec_in + cosIncoming * normal).normalize() * sinOutcoming;
	refractDir.normalize();

	ScatterSample s;
	s.isDelta  = true;
	s.pdf_dir  = 0.0f;

	if (randFloat() < fresnel) {
		// Reflect branch (probability = fresnel)
		s.origin       = data.hitPoint + normal * REFLECTION_BIAS;
		s.dir          = reflect(vec_in, normal);
		s.discreteProb = fresnel;
		// betaFactor = albedo / discreteProb (Dirac delta: cosine + pdf cancel)
		s.betaFactor   = albedo * (1.0f / fresnel);
	}
	else {
		// Refract branch (probability = 1-fresnel)
		s.origin       = data.hitPoint - normal * REFRACTION_BIAS;
		s.dir          = refractDir;
		s.discreteProb = 1.0f - fresnel;
		// (n1/n2)^2 scaling preserves radiance across interface — affects caustic brightness
		float etaScale = (n1 / n2) * (n1 / n2);
		s.betaFactor   = albedo * (etaScale / (1.0f - fresnel));
	}
	return s;
}

ScatterSample BSDF::spawnRay(const Scene* scene, const Intersection& data, const Vector& vec_in)
{
	switch (scene->getMaterial(data.materialIndex).type) {
	case MaterialType::DIFFUSE:    return spawnDiffuseRay(scene, data, vec_in);
	case MaterialType::REFLECTIVE: return spawnReflectRay(scene, data, vec_in);
	case MaterialType::REFRACTIVE: return spawnRefractRay(scene, data, vec_in);
	default:
		return ScatterSample{ data.hitPoint, Vector(0,0,0), Vector(0,0,0), 0.0f, 0.0f, false };
	}
}
