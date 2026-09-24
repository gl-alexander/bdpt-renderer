#pragma once
#include "Raytracer.h"

constexpr float MIN_INTENSITY = 0.001f;

struct PathVertex {
	Vector point;
	Vector normal;
	Vector w_i;
	Vector w_o;
	Vector color;
	float pdf;
};

constexpr int CAM_PATH_LENGHT = 5;
constexpr int LIGHT_PATH_LENGHT = 5;

class Pathtracer : public Raytracer
{
	std::vector<PathVertex> tracePath(const Ray& initialRay, int maxLen) const;

	void spawnRay(const Intersection& data, const Vector& vec_in,
		Ray& ray_out, Vector& color_out, float& probability) const;
	void spawnDiffuseRay(const Intersection& data, const Vector& vec_in,
		Ray& ray_out, Vector& color_out, float& probability) const;
	void spawnReflectRay(const Intersection& data, const Vector& vec_in,
		Ray& ray_out, Vector& color_out, float& probability) const;
	void spawnRefractRay(const Intersection& data, const Vector& vec_in,
		Ray& ray_out, Vector& color_out, float& probability) const;

	bool connected(const Vector& a, const Vector& b) const;

	std::vector<PathVertex> getLigthPath(const Light& light) const;
	std::vector<PathVertex> getCameraPath(const Ray& ray) const;

	void renderRegion(int x, int y, int width, int height, Image& output) const;

	Vector computeColor(const Ray& cameraRay, Image& image) const;
	Vector directIllumination(const PathVertex& data, const Light& light) const;
	void castToImagePlane(const std::vector<PathVertex>& lightPath, int lightNode, const Light& light, Image& image, float mult) const;
	Vector connectVertices(const std::vector<PathVertex>& cameraPath, int cameraNode, const std::vector<PathVertex>& lightPath, int lightNode) const;
public:
	Pathtracer(Scene* scene);

	void renderScene(const char* outputname) const;
	Image renderScene() const;
};

