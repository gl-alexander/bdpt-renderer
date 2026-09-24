#include "Pathtracer.h"
#include <thread>
Pathtracer::Pathtracer(Scene* scene) : Raytracer(scene)
{
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

	BucketArray buckets(scene); // the constructor generates the regions

	int threadCount = std::thread::hardware_concurrency();
	std::vector<std::thread> threads;
	//threadCount = 1;
	//renderRegion(435, 632, 100, 100, image);
	//return image;
	for (int i = 0; i < threadCount; i++) {
		threads.push_back(std::thread([this, &buckets, &image]() {
			Region region;
			while (buckets.nextRegion(region)) {
				renderRegion(region.startX, region.startY, region.width, region.height, image);
			}
		}));
	}
	for (auto& thread : threads) {
		thread.join();
	}
	return image;
}

void Pathtracer::renderRegion(int x, int y, int width, int height, Image& output) const
{
	// same as raytracer's renderRegionPixelGrid
	float mult = 1.0f / scene->getSettings().raysPerPixel;
	int squareCount = scene->getSettings().raysPerPixel - 1;
	int squaresPerSide = sqrt(squareCount);
	float squareCenterX, squareCenterY, side, halfside;
	side = 1.0f / squaresPerSide;
	halfside = side / 2.0f;
	for (int rowId = y; rowId < y + height; rowId++) {
		for (int colId = x; colId < x + width; colId++) {
			Vector finalColor(0, 0, 0);
			for (int x1 = 0; x1 < squaresPerSide; x1++) {
				for (int y1 = 0; y1 < squaresPerSide; y1++) {
					squareCenterX = x1 * side + halfside;
					squareCenterY = y1 * side + halfside;
					Ray ray = scene->getCamera().getRayForSubpixel(rowId + squareCenterY, colId + squareCenterX);
					finalColor += computeColor(ray, output);
				}
			}
			for (int i = squareCount; i < scene->getSettings().raysPerPixel; i++) {
				// generate the rest using montecarlo
				Ray ray = scene->getCamera().getRayForSubpixel(rowId + randFloat(), colId + randFloat());
				finalColor += computeColor(ray, output);
			}
			output[rowId][colId] = (finalColor * mult).clamp(0, 1);
		}
	}
}

static float intensity(const Vector& color) {
	return 0.21 * color.x + 0.72 * color.y + 0.07 * color.z;
}

std::vector<PathVertex> Pathtracer::tracePath(const Ray& initialRay, int maxLen) const
{
	std::vector<PathVertex> path;
	Intersection ix;
	PathVertex vert;
	int depth = 0;
	Ray currRay = initialRay;
	while (depth < maxLen) {
		ix = rayTraceAccelerated(currRay);
		if (ix.hitObjectIndex == INVALID_IND) return path;
		vert.normal = scene->getMaterial(ix.materialIndex).smoothShading ? ix.smoothNormal : ix.faceNormal;
		vert.w_i = currRay.direction;
		vert.point = ix.hitPoint;
		currRay.depth++;
		// computes the PDF and BSDF
		spawnRay(ix, currRay.direction, currRay, vert.color, vert.pdf);
		if (vert.pdf <= 0) return path;

		vert.w_o = currRay.direction;
		if (scene->getMaterial(ix.materialIndex).type == MaterialType::DIFFUSE)
		{
			path.push_back(vert);
		}
		depth++;
	}

	return path;
}

void Pathtracer::spawnRay(const Intersection& data, const Vector& vec_in, Ray& ray_out, Vector& color_out, float& probability) const
{
	switch (scene->getMaterial(data.materialIndex).type) {
	case MaterialType::DIFFUSE:
		spawnDiffuseRay(data, vec_in, ray_out, color_out, probability);
		break;
	case MaterialType::REFLECTIVE:
		spawnReflectRay(data, vec_in, ray_out, color_out, probability);
		break;
	case MaterialType::REFRACTIVE:
		spawnRefractRay(data, vec_in, ray_out, color_out, probability);
		break;
	default:
		color_out = Vector(0, 0, 0);
	}
}

void Pathtracer::spawnDiffuseRay(const Intersection& data, const Vector& vec_in, Ray& ray_out, Vector& color_out, float& probability) const
{
	// very similar to the GI algorithm in the Raytrace variant
	const Material& material = scene->getMaterial(data.materialIndex);
	Vector normal = data.faceNormal;
	if (material.smoothShading) normal = data.smoothNormal;

	ray_out.direction = randomHemisphereSample(normal);
	ray_out.origin = data.hitPoint + normal * REFLECTION_BIAS;

	Vector rawColor = scene->getGeometryObject(data.hitObjectIndex).sampleMaterial(material, data);
	color_out = rawColor * (1 / PI);
	probability = 1 / (2 * PI);
}

void Pathtracer::spawnReflectRay(const Intersection& data, const Vector& vec_in, Ray& ray_out, Vector& color_out, float& probability) const
{
	const Material& material = scene->getMaterial(data.materialIndex);
	Vector normal = data.faceNormal;
	if (material.smoothShading) normal = data.smoothNormal;

	ray_out.origin = data.hitPoint + normal * REFLECTION_BIAS;
	ray_out.direction = reflect(vec_in, normal);
	color_out = scene->getGeometryObject(data.hitObjectIndex).sampleMaterial(material, data) * 1.0f; // written * 1.0f to highlight that the probability could be anywhere up to infinity
	probability = 1.0f;
}

void Pathtracer::spawnRefractRay(const Intersection& data, const Vector& vec_in, Ray& ray_out, Vector& color_out, float& probability) const
{
	const Material& material = scene->getMaterial(data.materialIndex);
	Vector normal = material.smoothShading ? data.smoothNormal : data.faceNormal;

	float n1 = 1.0f; // Index of refraction of the air
	float n2 = material.ior;

	float dotPr = dot(vec_in, normal);
	if (dotPr > 0) { // Ray is leaving the refractive object
		normal *= -1;
		dotPr *= -1;
		std::swap(n1, n2);
	}

	float cosIncomming = -dotPr;
	float sinIncomming = 1.0f - cosIncomming * cosIncomming;

	if (sinIncomming > ((n2 * n2) / (n1 * n1))) { // total internal reflection
		spawnReflectRay(data, vec_in, ray_out, color_out, probability);
		return;
	}

	float sinOutcomming = (n1 / n2) * sqrt(std::max(0.0f, 1.0f - cosIncomming * cosIncomming));
	float cosOutcomming = sqrt(std::max(0.0f, 1.0f - sinOutcomming * sinOutcomming));

	Vector A = cosOutcomming * (-normal);
	Vector C = vec_in + (cosIncomming * normal);
	C.normalize();
	Vector refractedDirection = A + (C * sinOutcomming);
	refractedDirection.normalize();

	// Improved Fresnel calculation using Schlick's approximation
	float R0 = pow((n1 - n2) / (n1 + n2), 2);
	float fresnel = R0 + (1 - R0) * pow(1.0f - cosIncomming, 5);

	if (randFloat() < fresnel) {
		// the fresnel factors how much of the light is in reflection
		ray_out.origin = data.hitPoint + normal * REFLECTION_BIAS;
		ray_out.direction = reflect(vec_in, normal);
	}
	else {
		// 1 - fresnel determines how much of the light is refracted
		ray_out.origin = data.hitPoint - normal * REFRACTION_BIAS;
		ray_out.direction = refractedDirection;
	}
	probability = 1;
	color_out = scene->getGeometryObject(data.hitObjectIndex).sampleMaterial(material, data);
}

std::vector<PathVertex> Pathtracer::getLigthPath(const Light& light) const
{

	Vector randomDir = randomSphereSample();
	Ray randomRay{ light.getPosition(), randomDir, RayType::LIGHT, 0 };
	return tracePath(randomRay, LIGHT_PATH_LENGHT);
}

std::vector<PathVertex> Pathtracer::getCameraPath(const Ray& ray) const
{
	return tracePath(ray, CAM_PATH_LENGHT);
}

bool Pathtracer::connected(const Vector& a, const Vector& b) const
{
	Vector dir = (a - b);
	float len = dir.length();
	dir.normalize();
	Ray ray{ b, dir, RayType::SHADOW, 0 };
	Intersection ix = rayTraceAccelerated(ray, len);
	// either we hit the point we're looking for or there's no other intersection
	return (ix.hitPoint == a && ix.triangleIndex != NO_HIT_INDEX) || ix.triangleIndex == NO_HIT_INDEX;
}

Vector Pathtracer::directIllumination(const PathVertex& data, const Light& light) const
{
	if (!connected(data.point, light.getPosition()))
		return Vector(0, 0, 0);
	// slightly modified version of the shader for diffuse materials
	Vector finalColor{ 0.0, 0.0, 0.0 };

	Vector lightDirection = light.getPosition() - data.point;
	float sphereRadius2 = lightDirection.length2();
	lightDirection.normalize();
	float cosLaw = std::max(0.0f, dot(lightDirection, data.normal));
	float lightIntensity = light.getIntensity() / sphereRadius2;
	return data.color * lightIntensity * cosLaw;
}

void Pathtracer::castToImagePlane(const std::vector<PathVertex>& lightPath, int j, const Light& light, Image& image, float mult) const
{
	Vector camDirection = (lightPath[j].point - scene->getCamera().getPosition()); // shoot from camera to have backface culling
	float dist = camDirection.length();
	camDirection.normalize();

	Ray cameraRay{ scene->getCamera().getPosition(), camDirection, RayType::CAMERA, 0 };
	Intersection ix = rayTraceAccelerated(cameraRay, dist);

	if (ix.triangleIndex != NO_HIT_INDEX && ix.hitPoint == lightPath[j].point) {
		cameraRay.type = RayType::SHADOW; // to avoid backface culling with image plane
		auto pixelProjection = scene->getCamera().getRayHitpoint(cameraRay);

		float G = dot(lightPath[j].normal, camDirection) / (dist * dist);
		G = abs(G);
		Vector color = lightPath[j].color * G * light.getIntensity();
		for (int i = j - 1; j >= 0; j--) {
			color *= lightPath[j].color * abs(dot(lightPath[j].w_o, lightPath[j].normal)) / lightPath[j].pdf;
		}
		
		// if passing a refractive material we'd need to scale the color down by the inverse IOR squared
		// currently not doing so to get brighter caustics :D 
		image[pixelProjection.second][pixelProjection.first] += color * mult;
	}
}

Vector Pathtracer::connectVertices(const std::vector<PathVertex>& cameraPath, int cameraNode, const std::vector<PathVertex>& lightPath, int lightNode) const
{
	if (!connected(cameraPath[cameraNode].point, lightPath[lightNode].point))
		return Vector(0, 0, 0);
	// connect the paths
	Vector connection = (cameraPath[cameraNode].point - lightPath[lightNode].point);
	float distance2 = connection.length2();
	connection.normalize();

	float G = abs(dot(cameraPath[cameraNode].normal, connection) * dot(lightPath[lightNode].normal, connection) / distance2);

	Vector color(1, 1, 1);
	for (int i = 0; i < cameraNode - 1; i++) {
		// compute the previous vertices of the path so far
		color *= cameraPath[i].color * abs(dot(cameraPath[i].w_o, cameraPath[i].normal)) / cameraPath[i].pdf;
	}

	//connection 
	color *= cameraPath[cameraNode].color * lightPath[lightNode].color * G;

	for (int j = lightNode - 1; j >= 0; j--) {
		// compute the previous vertices of the light path
		color *= lightPath[j].color * abs(dot(lightPath[j].w_o, lightPath[j].normal)) / lightPath[j].pdf;
	}

	return color;
}


Vector Pathtracer::computeColor(const Ray& cameraRay, Image& image) const
{
	const Light& lightSample = scene->getRandomLight();
	std::vector<PathVertex> lightPath = getLigthPath(lightSample);
	std::vector<PathVertex> cameraPath = getCameraPath(cameraRay);

	Vector directWeight(1, 1, 1);
	float mult = 1.0f / (lightPath.size() + cameraPath.size() - 1);
	Vector color(0, 0, 0);
	for (int i = 0; i < cameraPath.size(); i++) {
		color += directWeight * directIllumination(cameraPath[i], lightSample) / float(i + 1);
		// "connecting" with the next vertex in the camera path
		directWeight *= cameraPath[i].color * abs(dot(cameraPath[i].w_o, cameraPath[i].normal)) / cameraPath[i].pdf;

		for (int j = 0; j < lightPath.size(); j++) {
			color += lightSample.getIntensity() * connectVertices(cameraPath, i, lightPath, j) / float(i + j + 2);
			castToImagePlane(lightPath, j, lightSample, image, mult);
		}
	}

	return color * mult;
}