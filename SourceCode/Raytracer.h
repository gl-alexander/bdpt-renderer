#pragma once
#include "Utils/Vector.h"
#include "Scene/Scene.h"
#include "Scene/Animation.h"
#include "Utils/ImageSaver.h"
#include "Scene/BucketArray.h"
#include "Utils/FXAA.h"

//using Image = std::vector<std::vector<Vector>>;

constexpr int MAX_RAY_DEPTH = 4;
constexpr float SHADOW_BIAS = 0.001f;
constexpr float REFLECTION_BIAS = 0.001f;
constexpr float REFRACTION_BIAS = 0.001f;

static const float DESATURIZATION = 0.75;

constexpr int GI_RAYS = 1;
constexpr float LIGHT_IMPORTANCE = 0.6;

class Raytracer
{
protected: 
	Scene* scene;
	Animation* animation;

	Intersection rayTrace(const Ray& ray) const;
	Intersection rayTraceAccelerated(const Ray& ray, float maxDistance = FLOAT_MAX) const;

	Vector shade(const Ray& ray, const Intersection& data) const;
	Vector shadeDiffuse(const Ray& ray, const Intersection& data) const;
	Vector shadeDirectIllumination(const Ray& ray, const Intersection& data) const;
	Vector shadeGlobalIllumination(const Ray& ray, const Intersection& data) const;
	Vector shadeReflective(const Ray& ray, const Intersection& data) const;
	Vector shadeRefractive(const Ray& ray, const Intersection& data) const;

	bool intersectsObject(const Ray& ray, float distanceToLight) const;

	// old non-accelerated methods
	void renderRegionNoAABB(int x, int y, int width, int height, Image& output) const;
	void renderRegionSimple(int x, int y, int width, int height, Image& output) const;

	void renderRegion(int x, int y, int width, int height, Image& output) const;
	// Picks random points in pixel from which to shoot rays
	void renderRegionMonteCarloAA(int x, int y, int width, int height, Image& output) const;
	// Picks points in pixel in a grid-like patter
	void renderRegionPixelGrid(int x, int y, int width, int height, Image& output) const;
	// Renders the scene simulating a Depth of Field effect
	void renderRegionFocalBlur(int x, int y, int width, int height, Image& output) const;
	// Simulates two rays (resembling human eyes) and renders them 
	void renderRegionStereoscopy(int x, int y, int width, int height, Image& output) const;
	// Combines DOF and Stereoscopic rendering 
	void renderRegionDOFStereoscopy(int x, int y, int width, int height, Image& output) const;



	// for testing purposes
	Image renderSinglethreaded() const;
	Image renderByRegions() const;
	Image renderByBuckets() const;
	Image renderWithAABB() const;
public:
	Raytracer(Scene* scene);
	Raytracer(Animation* animation);

	void renderScene(const char* outputname) const;
	Image renderAccelerated() const;

	void renderAnimation(const char* outputname);

	// for testing purposes:
	void renderSceneBarycentic(const char* outputname) const;
	Image renderSceneBarycentic() const;
	Image renderAcceleratedSinglethreaded() const;

};
