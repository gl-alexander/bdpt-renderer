#pragma once
#include "Raytracer.h"

constexpr float MIN_INTENSITY = 0.001f;
constexpr int CAM_PATH_LENGHT = 5;
constexpr int LIGHT_PATH_LENGHT = 5;

// High-precision PI for all pdf / We math (global PI=3.1415f is too coarse)
constexpr double PI_HI = 3.14159265358979323846;

struct PathVertex {
	Vector position;      // surface hit point
	Vector normal;        // shading normal (smooth or face per material)
	Vector wo;            // unit dir FROM this vertex TOWARD the previous vertex (= -incoming)
	Vector beta;          // cached throughput ARRIVING at this vertex (before scattering here)
	Vector albedo;        // pre-sampled surface albedo (for evalBRDF without needing Intersection)
	float  pdf_fwd;       // solid-angle pdf of sampling THIS vertex from the previous one
	float  pdf_rev;       // solid-angle pdf of sampling the PREVIOUS vertex from THIS one
	int    materialIndex; // stable index into scene materials (NOT Material* — avoids realloc)
	bool   is_delta;      // == (material.type != DIFFUSE)
	bool   is_light;      // true only for the synthetic point-light vertex
};

// Return value of all spawnXxxRay helpers
struct ScatterSample {
	Vector origin;       // next ray origin (with surface bias already applied)
	Vector dir;          // next ray direction (unit)
	Vector betaFactor;   // multiply into running beta: (albedo/PI)*cos/pdf for diffuse; reflectance/discreteProb for specular
	float  pdf_dir;      // continuous solid-angle pdf (0.0 for delta)
	float  discreteProb; // discrete branch probability (1.0 for diffuse/pure-reflect; fresnel or 1-fresnel for refract)
	bool   isDelta;
};

enum class BDPTDebugMode {
	ALL,        // normal MIS-combined render
	NEE_ONLY,   // only s=1 directIllumination (clean direct light + hard shadows)
	LIGHTTRACE, // only t=1 castToImagePlane (noisy caustics)
	PATHTRACE,  // s=0: renders BLACK by design (no emission, point light has zero area)
	// --- isolated (s,t) diagnostic passes (Veach counting: light+camera counted). weight forced 1 ---
	DBG_S1_T2,  // Pass 1: NEE at primary eye hit (cameraPath[0])          — direct + hard shadows
	DBG_S1_T3,  // Pass 2: NEE at 2nd eye vertex (cameraPath[1])           — 1 indirect bounce via eye
	DBG_S2_T2,  // Pass 3: connectVertices(cameraPath[0], lightPath[0])    — 1 indirect bounce via BDPT connect
	DBG_S4_T1   // Pass 4: castToImagePlane(lightPath[2])                  — caustic splat
};

class Pathtracer : public Raytracer
{
	std::vector<PathVertex> tracePath(const Ray& initialRay, int maxLen) const;

	ScatterSample spawnRay(const Intersection& data, const Vector& vec_in) const;
	ScatterSample spawnDiffuseRay(const Intersection& data, const Vector& vec_in) const;
	ScatterSample spawnReflectRay(const Intersection& data, const Vector& vec_in) const;
	ScatterSample spawnRefractRay(const Intersection& data, const Vector& vec_in) const;

	float cameraPdfW(const Vector& dir) const;
	Vector cameraWe(const Vector& dir, std::pair<int, int>& outPixel) const;

	bool connected(const Vector& a, const Vector& b, const Vector& normalB) const;

	bool cameraVisible(const Vector& p, const Vector& normalP) const;

	float geometryTerm(const PathVertex& a, const PathVertex& b, const Vector& dir, float dist2) const;

	float pdfWtoA(float pdfW, float cosB, float dist2) const;

	Vector evalBRDF(const PathVertex& vert, const Vector& wOut) const;

	float misWeight(const std::vector<PathVertex>& camPath, int t,
		const std::vector<PathVertex>& lightPath, int s,
		const Light& light) const;

	float misWeightS0(const std::vector<PathVertex>& camPath, int t, const Light& light) const;

	Vector directIllumination(const PathVertex& data, const Light& light) const;
	void castToImagePlane(const std::vector<PathVertex>& lightPath, int j,
		const Light& light, Image& image) const;
	Vector connectVertices(const std::vector<PathVertex>& cameraPath, int cameraNode,
		const std::vector<PathVertex>& lightPath, int lightNode,
		const Light& light) const;

	std::vector<PathVertex> getLigthPath(const Light& light) const;
	std::vector<PathVertex> getCameraPath(const Ray& ray) const;

	void renderRegion(int x, int y, int width, int height, Image& output, Image& splat) const;
	Vector computeColor(const Ray& cameraRay, Image& splat) const;

public:
	BDPTDebugMode debugMode = BDPTDebugMode::ALL;
	void setDebugMode(BDPTDebugMode m) { debugMode = m; }

	Pathtracer(Scene* scene);
	void renderScene(const char* outputname) const;
	Image renderScene() const;
};
