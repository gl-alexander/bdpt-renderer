#include "Pathtracer.h"
#include <thread>
#include <mutex>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <atomic>

static std::atomic<long> g_splatCalls{0};     // castToImagePlane entries (any vertex j)
static std::atomic<long> g_splatDeltaGuard{0};// skipped: splatted vertex itself is delta
static std::atomic<long> g_splatVisFail{0};   // skipped: camera cannot see the vertex
static std::atomic<long> g_splatDone{0};      // actually accumulated to film
static std::atomic<long> g_causticCalls{0};   // of the above, predecessor is delta (glass)
static std::atomic<long> g_causticDone{0};    // caustic-carrying splats accumulated
static std::atomic<double> g_contribSum{0.0}; // sum of splatted contrib magnitudes
static std::atomic<double> g_contribMax{0.0}; // max single splatted contrib magnitude
static std::atomic<long> g_contribTiny{0};    // splats with magnitude < 1e-6

Pathtracer::Pathtracer(Scene* scene) : Raytracer(scene)
{
	// Optional debug-pass override via env var BDPT_DBG
	if (const char* m = std::getenv("BDPT_DBG")) {
		if      (!std::strcmp(m, "S1_T2")) debugMode = BDPTDebugMode::DBG_S1_T2;
		else if (!std::strcmp(m, "S1_T3")) debugMode = BDPTDebugMode::DBG_S1_T3;
		else if (!std::strcmp(m, "S2_T2")) debugMode = BDPTDebugMode::DBG_S2_T2;
		else if (!std::strcmp(m, "S4_T1")) debugMode = BDPTDebugMode::DBG_S4_T1;
		else if (!std::strcmp(m, "NEE"))   debugMode = BDPTDebugMode::NEE_ONLY;
		else if (!std::strcmp(m, "LT"))    debugMode = BDPTDebugMode::LIGHTTRACE;
	}
}

void Pathtracer::renderScene(const char* outputname) const
{
	ImageSaver::saveImage(outputname, renderScene());
}

Image Pathtracer::renderScene() const
{
	const unsigned imageHeight = scene->getSettings().imageSettings.height;
	const unsigned imageWidth = scene->getSettings().imageSettings.width;

	Image image(imageHeight, std::vector<Vector>(imageWidth, scene->getSettings().bgColor));
	// Separate accumulation buffer for light-tracing (t=1) splats.  Splats land in
	// arbitrary pixels, so they cannot share the camera-path buffer
	Image splat(imageHeight, std::vector<Vector>(imageWidth, Vector(0, 0, 0)));

	BucketArray buckets(scene);

	int threadCount = std::thread::hardware_concurrency();
	std::vector<std::thread> threads;
	for (int i = 0; i < threadCount; i++) {
		threads.push_back(std::thread([this, &buckets, &image, &splat]() {
			Region region;
			while (buckets.nextRegion(region)) {
				renderRegion(region.startX, region.startY, region.width, region.height, image, splat);
			}
		}));
	}
	for (auto& thread : threads) {
		thread.join();
	}

	const float mult = 1.0f / scene->getSettings().raysPerPixel;
	const char* expEnv = std::getenv("BDPT_EXPOSURE");
	const float exposure = expEnv ? (float)atof(expEnv) : 1.0f;
	for (unsigned r = 0; r < imageHeight; r++)
		for (unsigned c = 0; c < imageWidth; c++)
			image[r][c] = ((image[r][c] + splat[r][c] * mult) * exposure).clamp(0, 1);

	if (std::getenv("BDPT_CAUSTIC")) {
		fprintf(stderr,
			"[CAUSTIC] splatCalls=%ld deltaGuard=%ld visFail=%ld splatted=%ld | causticCalls=%ld causticSplatted=%ld\n",
			g_splatCalls.load(), g_splatDeltaGuard.load(), g_splatVisFail.load(),
			g_splatDone.load(), g_causticCalls.load(), g_causticDone.load());
		fprintf(stderr,
			"[CAUSTIC] contribSum=%.6g contribMax=%.6g tinySplats(<1e-6)=%ld avgContrib=%.6g\n",
			g_contribSum.load(), g_contribMax.load(), g_contribTiny.load(),
			g_splatDone.load() ? g_contribSum.load() / g_splatDone.load() : 0.0);
	}
	return image;
}

void Pathtracer::renderRegion(int x, int y, int width, int height, Image& output, Image& splat) const
{
	float mult = 1.0f / scene->getSettings().raysPerPixel;
	int squareCount = scene->getSettings().raysPerPixel - 1;
	int squaresPerSide = (int)sqrt((float)squareCount);
	float side = 1.0f / squaresPerSide;
	float halfside = side / 2.0f;
	for (int rowId = y; rowId < y + height; rowId++) {
		for (int colId = x; colId < x + width; colId++) {
			Vector finalColor(0, 0, 0);
			for (int x1 = 0; x1 < squaresPerSide; x1++) {
				for (int y1 = 0; y1 < squaresPerSide; y1++) {
					float squareCenterX = x1 * side + halfside;
					float squareCenterY = y1 * side + halfside;
					Ray ray = scene->getCamera().getRayForSubpixel(rowId + squareCenterY, colId + squareCenterX);
					finalColor += computeColor(ray, splat);
				}
			}
			for (int i = squareCount; i < scene->getSettings().raysPerPixel; i++) {
				Ray ray = scene->getCamera().getRayForSubpixel(rowId + randFloat(), colId + randFloat());
				finalColor += computeColor(ray, splat);
			}
			output[rowId][colId] = finalColor * mult;
		}
	}
}

ScatterSample Pathtracer::spawnDiffuseRay(const Intersection& data, const Vector& vec_in) const
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

ScatterSample Pathtracer::spawnReflectRay(const Intersection& data, const Vector& vec_in) const
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

ScatterSample Pathtracer::spawnRefractRay(const Intersection& data, const Vector& vec_in) const
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

ScatterSample Pathtracer::spawnRay(const Intersection& data, const Vector& vec_in) const
{
	switch (scene->getMaterial(data.materialIndex).type) {
	case MaterialType::DIFFUSE:    return spawnDiffuseRay(data, vec_in);
	case MaterialType::REFLECTIVE: return spawnReflectRay(data, vec_in);
	case MaterialType::REFRACTIVE: return spawnRefractRay(data, vec_in);
	default:
		return ScatterSample{ data.hitPoint, Vector(0,0,0), Vector(0,0,0), 0.0f, 0.0f, false };
	}
}

std::vector<PathVertex> Pathtracer::tracePath(const Ray& initialRay, int maxLen) const
{
	std::vector<PathVertex> path;
	path.reserve(maxLen);

	Vector beta_running;
	float  pendingFwdPdf;

	if (initialRay.type == RayType::LIGHT) {
		beta_running   = Vector(1, 1, 1);
		// pdf_fwd seed unused by MIS (misWeight recomputes light-side pdfs geometrically);
		// AREA emission pdf is handled there. beta stays (1,1,1); emission applied at connection.
		pendingFwdPdf  = (float)(1.0 / (4.0 * PI_HI));
	}
	else {
		// Camera path: seed with We / pdf
		std::pair<int,int> dummy;
		Vector dir = initialRay.direction; // already unit from getRayForSubpixel
		Vector we  = cameraWe(dir, dummy);
		float  pdf = cameraPdfW(dir);
		if (pdf <= 0.0f) return path;
		beta_running  = we * (1.0f / pdf); // = We / cameraPdfW
		pendingFwdPdf = pdf;
	}

	Ray currRay = initialRay;
	int depth = 0;

	while (depth < maxLen) {
		Intersection ix = rayTraceAccelerated(currRay);
		if (ix.hitObjectIndex == INVALID_IND) break;

		const Material& material = scene->getMaterial(ix.materialIndex);
		Vector normal = material.smoothShading ? ix.smoothNormal : ix.faceNormal;

		PathVertex vert;
		vert.position     = ix.hitPoint;
		vert.normal       = normal;
		vert.wo           = (-currRay.direction).normalize(); // unit dir toward previous vertex
		vert.beta         = beta_running;
		vert.pdf_fwd      = pendingFwdPdf;
		vert.materialIndex = ix.materialIndex;
		vert.is_delta     = (material.type != MaterialType::DIFFUSE);
		vert.is_light     = false;
		// Pre-sample albedo so evalBRDF works for textured materials at connection time
		vert.albedo       = scene->getGeometryObject(ix.hitObjectIndex).sampleMaterial(material, ix);

		ScatterSample sample = spawnRay(ix, currRay.direction);

		beta_running = beta_running * sample.betaFactor;
		pendingFwdPdf = sample.isDelta ? sample.discreteProb : sample.pdf_dir;

		path.push_back(vert);

		if (sample.isDelta && sample.discreteProb <= 0.0f) break;
		if (!sample.isDelta && sample.pdf_dir <= 0.0f) break;

		currRay.origin    = sample.origin;
		currRay.direction = sample.dir;
		currRay.depth++;
		depth++;
	}

	for (int i = 0; i < (int)path.size(); i++) {
		if (i == 0) {
			path[0].pdf_rev = 0.0f;
		}
		else {
			const Material& mi = scene->getMaterial(path[i].materialIndex);
			if (mi.type == MaterialType::DIFFUSE) {
				Vector revDir = path[i-1].position - path[i].position;
				revDir.normalize();
				float cosCheck = dot(revDir, path[i].normal);
				path[i].pdf_rev = (cosCheck > 0.0f) ? (float)(cosCheck / PI_HI) : 0.0f;
			}
			else {
				path[i].pdf_rev = path[i].pdf_fwd;
			}
		}
	}

	return path;
}


std::vector<PathVertex> Pathtracer::getLigthPath(const Light& light) const
{
	if (light.getType() == LightType::AREA) {
		float pdfA;
		Vector origin = light.samplePoint(pdfA);
		// Cosine-weighted hemisphere about the light normal (Malley's method),
		// matching the Lambertian emission profile and the cos/PI directional pdf
		// that misWeight assumes for the first light edge.
		Vector n = light.getNormal();
		float u1 = randFloat(), u2 = randFloat();
		float r = std::sqrt(u1), phi = (float)(2.0 * PI_HI) * u2;
		float x = r * std::cos(phi), y = r * std::sin(phi), z = std::sqrt(std::max(0.0f, 1.0f - u1));
		Vector up = std::abs(n.z) < 0.999f ? Vector(0, 0, 1) : Vector(1, 0, 0);
		Vector t = cross(up, n).normalize();
		Vector b = cross(n, t);
		Vector dir = (t * x + b * y + n * z).normalize();
		Ray areaRay{ origin, dir, RayType::LIGHT, 0 };
		return tracePath(areaRay, LIGHT_PATH_LENGHT);
	}
	Vector randomDir = randomSphereSample();
	Ray randomRay{ light.getPosition(), randomDir, RayType::LIGHT, 0 };
	return tracePath(randomRay, LIGHT_PATH_LENGHT);
}

std::vector<PathVertex> Pathtracer::getCameraPath(const Ray& ray) const
{
	return tracePath(ray, CAM_PATH_LENGHT);
}

float Pathtracer::cameraPdfW(const Vector& dir) const
{
	const Camera& cam = scene->getCamera();
	float cosTheta = dot(dir, cam.getFrontDirection()); // dir assumed unit
	if (cosTheta <= 0.0f) return 0.0f;
	float tanFOV = cam.getFOVtan();
	float aspect = (float)cam.getWidth() / (float)cam.getHeight();
	double A = (2.0 * tanFOV * aspect) * (2.0 * tanFOV); // full image-plane area
	return (float)(1.0 / (A * (double)cosTheta * cosTheta * cosTheta));
}

Vector Pathtracer::cameraWe(const Vector& dir, std::pair<int,int>& outPixel) const
{
	const Camera& cam = scene->getCamera();
	float cosTheta = dot(dir, cam.getFrontDirection());
	if (cosTheta <= 0.0f) { outPixel = { -1, -1 }; return Vector(0, 0, 0); }
	float tanFOV = cam.getFOVtan();
	float aspect = (float)cam.getWidth() / (float)cam.getHeight();
	double A = (2.0 * tanFOV * aspect) * (2.0 * tanFOV); // full image-plane area
	double cos4 = (double)cosTheta * cosTheta * cosTheta * cosTheta;
	// Use SHADOW type so getRayHitpoint skips backface culling of the image plane mesh
	Ray probeRay{ cam.getPosition(), dir, RayType::SHADOW, 0 };
	outPixel = cam.getRayHitpoint(probeRay);
	float we = (float)(1.0 / (A * cos4));
	return Vector(we, we, we);
}

bool Pathtracer::connected(const Vector& a, const Vector& b, const Vector& normalB) const
{
	Vector dir = a - b;
	float len  = dir.length();
	dir.normalize();
	Ray ray{ b + normalB * SHADOW_BIAS, dir, RayType::SHADOW, 0 };
	float maxDist = len - 2.0f * SHADOW_BIAS;
	if (maxDist <= 0.0f) return true; // trivially visible (points are essentially coincident)
	Intersection ix = rayTraceAccelerated(ray, maxDist);
	return ix.triangleIndex == NO_HIT_INDEX || ix.t >= len - EPSILON;
}

bool Pathtracer::cameraVisible(const Vector& p, const Vector& normalP) const
{
	Vector camPos = scene->getCamera().getPosition();
	Vector dir    = p - camPos;
	float  len     = dir.length();
	if (len <= EPSILON) return true;
	dir.normalize();
	float maxDist = len - 2.0f * SHADOW_BIAS;
	if (maxDist <= 0.0f) return true;
	Ray ray{ camPos, dir, RayType::CAMERA, 0 };
	Intersection ix = rayTraceAccelerated(ray, maxDist);
	return ix.triangleIndex == NO_HIT_INDEX || ix.t >= len - EPSILON;
}

float Pathtracer::geometryTerm(const PathVertex& a, const PathVertex& b,
	const Vector& dir, float dist2) const
{
	float cosA = std::abs(dot(a.normal, dir));
	float cosB = std::abs(dot(b.normal, dir)); // same dir, opposite incidence at b — abs handles it
	if (dist2 <= 0.0f) return 0.0f;
	return cosA * cosB / dist2;
}

float Pathtracer::pdfWtoA(float pdfW, float cosB, float dist2) const
{
	return pdfW * std::abs(cosB) / dist2;
}

// Lambertian BRDF: f_r = albedo / PI
Vector Pathtracer::evalBRDF(const PathVertex& vert, const Vector& wOut) const
{
	return vert.albedo * (float)(1.0 / PI_HI);
}

float Pathtracer::misWeight(const std::vector<PathVertex>& camPath, int t,
	const std::vector<PathVertex>& lightPath, int s,
	const Light& light) const
{
	const int k = s + t; // stored connected-path vertices
	if (k < 2) return 1.0f; // single stored vertex => only one realizable strategy

	// Combined chain x[0..k-1]: light-side first, then camera-side reversed.
	struct CV { Vector pos, nrm; bool delta; };
	std::vector<CV> x; x.reserve(k);
	for (int i = 0; i < s; ++i)
		x.push_back({ lightPath[i].position, lightPath[i].normal, lightPath[i].is_delta });
	for (int i = t - 1; i >= 0; --i)
		x.push_back({ camPath[i].position, camPath[i].normal, camPath[i].is_delta });

	const Vector lightPos = light.getPosition();
	const Vector camPos   = scene->getCamera().getPosition();

	auto scatterEdge = [&](const CV& from, const CV& to, const Vector& dir, double d2) -> double {
		if (from.delta) return 1.0; // deterministic specular bounce
		double cosFrom = dot(from.nrm, dir);
		double pdfW = (cosFrom > 0.0) ? cosFrom / PI_HI : 0.0;
		return pdfWtoA((float)pdfW, std::abs(dot(to.nrm, dir)), (float)d2);
	};

	auto pathAreaPdf = [&](int sp) -> double {
		const int tp = k - sp;
		if (sp < 0 || tp < 0) return 0.0;
		if (sp >= 1 && x[sp - 1].delta) return 0.0;
		if (tp >= 1 && x[sp].delta) return 0.0;
		double p = 1.0;
		Vector dir; double d2;
		auto edge = [&](const Vector& a, const Vector& b) { Vector d = b - a; d2 = d.length2(); dir = d; dir.normalize(); };

		if (sp >= 1) {
			edge(lightPos, x[0].pos);
			p *= pdfWtoA((float)(1.0 / (4.0 * PI_HI)), std::abs(dot(x[0].nrm, dir)), (float)d2);
			for (int i = 1; i < sp; ++i) {
				edge(x[i - 1].pos, x[i].pos);
				p *= scatterEdge(x[i - 1], x[i], dir, d2);
			}
		}

		if (tp >= 1) {
			edge(camPos, x[k - 1].pos);
			p *= pdfWtoA(cameraPdfW(dir), std::abs(dot(x[k - 1].nrm, dir)), (float)d2);
			for (int i = k - 2; i >= sp; --i) {
				edge(x[i + 1].pos, x[i].pos);
				p *= scatterEdge(x[i + 1], x[i], dir, d2);
			}
		}
		return p;
	};

	const double pUsed = pathAreaPdf(s);
	if (pUsed <= 0.0) return 0.0;

	double denom = 0.0;
	for (int sp = 0; sp <= k; ++sp) denom += pathAreaPdf(sp);
	if (denom <= 0.0) return 0.0;
	return (float)(pUsed / denom);
}



Vector Pathtracer::directIllumination(const PathVertex& data, const Light& light) const
{
	if (data.is_delta) return Vector(0, 0, 0);
	if (!connected(light.getPosition(), data.position, data.normal))
		return Vector(0, 0, 0);

	Vector lightDir  = light.getPosition() - data.position;
	float  dist2     = lightDir.length2();
	lightDir.normalize();
	float cosLaw    = std::max(0.0f, dot(lightDir, data.normal));
	float lightE     = light.getIntensity() / (float)(4.0 * PI_HI * dist2);
	Vector brdf      = evalBRDF(data, lightDir);
	// beta already contains throughput arriving at this vertex from the camera
	return data.beta * brdf * cosLaw * lightE;
}

Vector Pathtracer::connectVertices(const std::vector<PathVertex>& cameraPath, int cameraNode,
	const std::vector<PathVertex>& lightPath, int lightNode,
	const Light& light) const
{
	const PathVertex& cv = cameraPath[cameraNode];
	const PathVertex& lv = lightPath[lightNode];

	if (cv.is_delta || lv.is_delta) return Vector(0, 0, 0);
	if (!connected(cv.position, lv.position, lv.normal)) return Vector(0, 0, 0);

	Vector connDir = cv.position - lv.position;
	float  dist2   = connDir.length2();
	connDir.normalize();

	float  G    = geometryTerm(lv, cv, connDir, dist2);
	Vector brdfCam   = evalBRDF(cv, -connDir); // wOut at cam vert: toward light vert
	Vector brdfLight = evalBRDF(lv,  connDir); // wOut at light vert: toward cam vert

	Vector lightToV0 = lightPath[0].position - light.getPosition();
	float  lightDist2 = lightToV0.length2();
	float  cosEmit    = std::abs(dot(lightPath[0].normal, lightPath[0].wo));
	float  intensityFactor = (lightDist2 > 0.0f) ? cosEmit * light.getIntensity() / (float)(4.0 * PI_HI * lightDist2) : 0.0f;

	float  w = (debugMode == BDPTDebugMode::DBG_S2_T2) ? 1.0f
	           : misWeight(cameraPath, cameraNode + 1, lightPath, lightNode + 1, light);

	return cv.beta * lv.beta * brdfCam * brdfLight * G * intensityFactor * w;
}

void Pathtracer::castToImagePlane(const std::vector<PathVertex>& lightPath, int j,
	const Light& light, Image& image) const
{
	const PathVertex& lv = lightPath[j];
	const bool dbgCaustic = std::getenv("BDPT_CAUSTIC") != nullptr;
	const bool causticPath = (j > 0) && lightPath[j - 1].is_delta; // predecessor was glass/mirror
	// TEMP diagnostic: isolate splat classes. BDPT_SPLIT=direct|indirect|caustic
	if (const char* sp = std::getenv("BDPT_SPLIT")) {
		const bool isDirect   = (j == 0);
		const bool isCaustic  = causticPath;
		const bool isIndirect = (j > 0) && !causticPath;
		if      (!std::strcmp(sp, "direct")   && !isDirect)   return;
		else if (!std::strcmp(sp, "indirect") && !isIndirect) return;
		else if (!std::strcmp(sp, "caustic")  && !isCaustic)  return;
	}
	if (dbgCaustic) { g_splatCalls++; if (causticPath) g_causticCalls++; }
	if (lv.is_delta) { if (dbgCaustic) g_splatDeltaGuard++; return; }

	Vector camPos  = scene->getCamera().getPosition();
	// Direction FROM light vertex TOWARD camera
	Vector toCam   = camPos - lv.position;
	float  dist2   = toCam.length2();
	toCam.normalize();

	if (!cameraVisible(lv.position, lv.normal)) { if (dbgCaustic) g_splatVisFail++; return; }

	Vector toLight = -toCam;
	std::pair<int,int> pixel;
	Vector we = cameraWe(toLight, pixel); // We = 1/(A_film cos^4θ); also resolves the pixel
	const int W = (int)scene->getSettings().imageSettings.width;
	const int H = (int)scene->getSettings().imageSettings.height;
	if (pixel.first < 0 || pixel.second < 0 || pixel.first >= W || pixel.second >= H) return;

	float cosAtLv  = std::abs(dot(lv.normal, toCam));
	float cosAtCam = std::abs(dot(scene->getCamera().getFrontDirection(), toLight));
	float G = (dist2 > 0.0f) ? (cosAtLv * cosAtCam / dist2) : 0.0f;

	Vector lightToV0  = lightPath[0].position - light.getPosition();
	float  lightDist2 = lightToV0.length2();
	float  cosEmit    = std::abs(dot(lightPath[0].normal, lightPath[0].wo));
	float  intensityFactor = (lightDist2 > 0.0f) ? cosEmit * light.getIntensity() / (float)(4.0 * PI_HI * lightDist2) : 0.0f;

	std::vector<PathVertex> emptyCamPath;
	float w = (debugMode == BDPTDebugMode::DBG_S4_T1 || debugMode == BDPTDebugMode::LIGHTTRACE) ? 1.0f
	          : misWeight(emptyCamPath, 0, lightPath, j + 1, light);

	Vector brdfLight = evalBRDF(lv, toCam);

	Vector contrib = lv.beta * brdfLight * we * G * intensityFactor * w;
	static std::mutex splatMutex;
	std::lock_guard<std::mutex> lock(splatMutex);
	image[pixel.second][pixel.first] += contrib;
	if (dbgCaustic) {
		g_splatDone++; if (causticPath) g_causticDone++;
		static double s_sum = 0.0, s_max = 0.0; static long s_tiny = 0;
		double m = (double)contrib.x + contrib.y + contrib.z;
		s_sum += m; if (m < 1e-6) s_tiny++;
		if (m > s_max) {
			s_max = m;
			fprintf(stderr,"[FACTORS] contrib=%.4g | beta=%.4g brdfL=%.4g we=%.6g G=%.6g If=%.4g dist2=%.4g lightDist2=%.4g cosAtLv=%.3f cosAtCam=%.3f\n",
				m, lv.beta.x, brdfLight.x, we.x, G, intensityFactor, dist2, lightDist2, cosAtLv, cosAtCam);
		}
		g_contribSum = s_sum; g_contribMax = s_max; g_contribTiny = s_tiny;
	}
}

Vector Pathtracer::computeColor(const Ray& cameraRay, Image& splat) const
{
	const std::vector<Light>& lights = scene->getLights();
	if (lights.empty()) return Vector(0, 0, 0);
	float numLights = (float)lights.size();

	if (debugMode == BDPTDebugMode::PATHTRACE) {
		return Vector(0, 0, 0);
	}

	std::vector<PathVertex> cameraPath = getCameraPath(cameraRay);
	Vector color(0, 0, 0);

	if (debugMode == BDPTDebugMode::DBG_S1_T2 || debugMode == BDPTDebugMode::DBG_S1_T3 ||
	    debugMode == BDPTDebugMode::DBG_S2_T2 || debugMode == BDPTDebugMode::DBG_S4_T1) {
		for (const Light& light : lights) {
			if (debugMode == BDPTDebugMode::DBG_S1_T2) {            // s=1,t=2: NEE at cameraPath[0]
				if ((int)cameraPath.size() > 0 && !cameraPath[0].is_delta)
					color += directIllumination(cameraPath[0], light);
			}
			else if (debugMode == BDPTDebugMode::DBG_S1_T3) {      // s=1,t=3: NEE at cameraPath[1]
				if ((int)cameraPath.size() > 1 && !cameraPath[1].is_delta)
					color += directIllumination(cameraPath[1], light);
			}
			else if (debugMode == BDPTDebugMode::DBG_S2_T2) {      // s=2,t=2: connect cam[0]-light[0]
				std::vector<PathVertex> lightPath = getLigthPath(light);
				if ((int)cameraPath.size() > 0 && (int)lightPath.size() > 0)
					color += connectVertices(cameraPath, 0, lightPath, 0, light) /*w already inside*/;
			}
			else if (debugMode == BDPTDebugMode::DBG_S4_T1) {      // s=4,t=1: splat light[2] to film
				std::vector<PathVertex> lightPath = getLigthPath(light);
				if ((int)lightPath.size() > 2)
					castToImagePlane(lightPath, 2, light, splat);
			}
		}
		return color / numLights;
	}

	for (const Light& light : lights) {
		std::vector<PathVertex> lightPath = getLigthPath(light);

		// s=1, t>=1: NEE (direct illumination from point light)
		if (debugMode == BDPTDebugMode::ALL || debugMode == BDPTDebugMode::NEE_ONLY) {
			for (int t = 0; t < (int)cameraPath.size(); t++) {
				if (cameraPath[t].is_delta) continue;
				Vector contrib = directIllumination(cameraPath[t], light);
				float w = 1.0f;
				if (debugMode == BDPTDebugMode::ALL) {
					// Point light is a delta terminal (0 stored light verts).
					w = misWeight(cameraPath, t + 1, lightPath, 0, light);
				}
				// NEE_ONLY: weight=1 (raw estimator, no MIS)
				color += contrib * w;
			}
		}

		if (debugMode == BDPTDebugMode::ALL) {
			for (int t = 1; t <= (int)cameraPath.size(); t++) {
				for (int s = 1; s <= (int)lightPath.size(); s++) {
					color += connectVertices(cameraPath, t - 1, lightPath, s - 1, light);
				}
			}
		}

		// t=1, s>=1: light-subpath vertices splatted to image (caustics)
		if (debugMode == BDPTDebugMode::ALL || debugMode == BDPTDebugMode::LIGHTTRACE) {
			for (int s = 0; s < (int)lightPath.size(); s++) {
				castToImagePlane(lightPath, s, light, splat);
			}
		}
	}

	return color / numLights;
}
