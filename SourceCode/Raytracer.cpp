#include "Raytracer.h"
#include <algorithm>
#include <iostream>
#include <climits>
#include <assert.h>
#include <thread>

Raytracer::Raytracer(Scene* scene) : scene(scene)
{}

Raytracer::Raytracer(Animation* animation) : animation(animation)
{
}

void Raytracer::renderScene(const char* outputname) const
{
    ImageSaver::saveImage(outputname, renderAccelerated());
}

void Raytracer::renderSceneBarycentic(const char* outputname) const
{
    ImageSaver::saveImage(outputname, renderSceneBarycentic());
}

Image Raytracer::renderAccelerated() const
{
    const unsigned imageHeight = scene->getSettings().imageSettings.height;
    const unsigned imageWidth = scene->getSettings().imageSettings.width;

    Image image(imageHeight, std::vector<Vector>(imageWidth, scene->getSettings().bgColor));

    BucketArray buckets(scene); // the constructor generates the regions

    int threadCount = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;

    for (int i = 0; i < threadCount; i++) {
        threads.push_back(std::thread([this, &buckets, &image]() {
            Region region;
            while (buckets.nextRegion(region)) {
                if (scene->getSettings().renderedAA) {
                    renderRegionPixelGrid(region.startX, region.startY, region.width, region.height, image);
                    //renderRegionMonteCarloAA(region.startX, region.startY, region.width, region.height, image);
                    // the Monte Carlo AA chooses spots on the pixel at random, while the Pixel Grid uses a more uniform distribution
                }
                else {
                    if (scene->getSettings().stereoscopy) {
                        if (scene->getSettings().dof) {
                            renderRegionDOFStereoscopy(region.startX, region.startY, region.width, region.height, image);
                        }
                        else {
                            renderRegionStereoscopy(region.startX, region.startY, region.width, region.height, image);
                        }
                    }
                    else {
                        if (scene->getSettings().dof) {
                            renderRegionFocalBlur(region.startX, region.startY, region.width, region.height, image);
                        }
                        else {
                            renderRegion(region.startX, region.startY, region.width, region.height, image);
                        }
                    }
                }
            }
            }));
    }

    for (auto& thread : threads) {
        thread.join();
    }
    if (scene->getSettings().FXAA) {
        FXAA::applyFXAA(image);
    }
    return image;
}

Image Raytracer::renderAcceleratedSinglethreaded() const
{
    const unsigned imageHeight = scene->getSettings().imageSettings.height;
    const unsigned imageWidth = scene->getSettings().imageSettings.width;

    Image image(imageHeight, std::vector<Vector>(imageWidth, scene->getSettings().bgColor));
    if (scene->getSettings().renderedAA) {
        renderRegionPixelGrid(0, 0, imageWidth, imageHeight, image);
        //renderRegionMonteCarlo(0, 0, imageWidth, imageHeight, image);
    }
    else {
        if (scene->getSettings().stereoscopy) {
            if (scene->getSettings().dof) {
                renderRegionDOFStereoscopy(0, 0, imageWidth, imageHeight, image);
            }
            else {
                renderRegionStereoscopy(0, 0, imageWidth, imageHeight, image);
            }
        }
        else {
            if (scene->getSettings().dof) {
                renderRegionFocalBlur(0, 0, imageWidth, imageHeight, image);
            }
            else {
                renderRegion(0, 0, imageWidth, imageHeight, image);
            }
        }
    }
    if (scene->getSettings().FXAA) {
        FXAA::applyFXAA(image);
    }
    return image;
}

void Raytracer::renderAnimation(const char* outputname)
{
    if (animation == nullptr) return;

    while (animation->hasNextKeyframe()) {
        scene = animation->getNextKeyframe();
        char framename[128];
        snprintf(framename, sizeof(framename), "%s_frame_%03d.ppm", outputname, animation->getCurrentFrameIndex());
        renderScene(framename);
        std::cout << "Rendered " << framename << std::endl;
    }
}

void Raytracer::renderRegionNoAABB(int x, int y, int width, int height, Image& output) const
{
    for (int rowId = y; rowId < y + height; rowId++) {
        for (int colId = x; colId < x + width; colId++) {
            Ray ray = scene->getCamera().getRayForPixel(rowId, colId);
            output[rowId][colId] = shade(ray, rayTrace(ray));
        }
    }
}

void Raytracer::renderRegionSimple(int x, int y, int width, int height, Image& output) const
{
    for (int rowId = y; rowId < y + height; rowId++) {
        for (int colId = x; colId < x + width; colId++) {
            Ray ray = scene->getCamera().getRayForPixel(rowId, colId); 
            if (scene->getAABB().intersects(ray)) {
                output[rowId][colId] = shade(ray, rayTrace(ray));
            }
        }
    }
}

void Raytracer::renderRegion(int x, int y, int width, int height, Image& output) const
{
    for (int rowId = y; rowId < y + height; rowId++) {
        for (int colId = x; colId < x + width; colId++) {
            Ray ray = scene->getCamera().getRayForPixel(rowId, colId);
            output[rowId][colId] = shade(ray, rayTraceAccelerated(ray));
        }
    }
}

void Raytracer::renderRegionMonteCarloAA(int x, int y, int width, int height, Image& output) const
{
    float mult = 1.0f / scene->getSettings().raysPerPixel;
    for (int rowId = y; rowId < y + height; rowId++) {
        for (int colId = x; colId < x + width; colId++) {
            Vector finalColor(0, 0, 0);
            for (int i = 0; i < scene->getSettings().raysPerPixel; i++) {
                Ray ray = scene->getCamera().getRayForSubpixel(rowId + randFloat(), colId + randFloat());
                finalColor += shade(ray, rayTraceAccelerated(ray));
            }
            output[rowId][colId] = finalColor * mult;
        }
    }
}

void Raytracer::renderRegionPixelGrid(int x, int y, int width, int height, Image& output) const
{
    float mult = 1.0f / scene->getSettings().raysPerPixel;
    int squareCount = scene->getSettings().raysPerPixel - 1;
    int squaresPerSide = sqrt(squareCount);
    float squareCenterX, squareCenterY, side, halfside;
    side = 1.0f / squaresPerSide;
    halfside = side / 2.0f;
    for (int rowId = y; rowId < y + height; rowId++) {
        for (int colId = x; colId < x + width; colId++) {
            Vector finalColor(0, 0, 0);
            for (int x = 0; x < squaresPerSide; x++) {
                for (int y = 0; y < squaresPerSide; y++) {
                    squareCenterX = x * side + halfside;
                    squareCenterY = y * side + halfside;
                    Ray ray = scene->getCamera().getRayForSubpixel(rowId + squareCenterY, colId + squareCenterX);
                    finalColor += shade(ray, rayTraceAccelerated(ray));
                }
            }
            for (int i = squareCount; i < scene->getSettings().raysPerPixel; i++) {
                // generate the rest using montecarlo
                Ray ray = scene->getCamera().getRayForSubpixel(rowId + randFloat(), colId + randFloat());
                finalColor += shade(ray, rayTraceAccelerated(ray));
            }
            output[rowId][colId] = finalColor * mult;
        }
    }
}

void Raytracer::renderRegionFocalBlur(int x, int y, int width, int height, Image& output) const
{
    const Camera& camera = scene->getCamera();
    float u, v, mult, aperature, focalDistance;
    if (scene->getSettings().autoFocus) {
        // if we autofocus, we'll focus at the center of the scene
        focalDistance = ((scene->getAABB().max - scene->getAABB().min) / 2.0 - camera.getPosition()).length();
    }
    else {
        focalDistance = scene->getSettings().focalPlaneDist;
    }

    mult = 1.0f / scene->getSettings().dofSamples;
    aperature = 1.0f / scene->getSettings().fNum;
    for (int rowId = y; rowId < y + height; rowId++) {
        for (int colId = x; colId < x + width; colId++) {
            Ray ray = camera.getRayForPixel(rowId, colId);
            Vector finalColor(0, 0, 0);
            for (int i = 0; i < scene->getSettings().dofSamples; i++) {
                float M = focalDistance / dot(camera.getFrontDirection(), ray.direction);
                Vector T = camera.getPosition() + ray.direction * M;

                randomCircleSample(u, v);
                u *= aperature;
                v *= aperature;
                ray.origin = scene->getCamera().getPosition() + u * camera.getRightDirection() + v * camera.getUpDirection();
                ray.direction = (T - ray.origin).normalize();
                finalColor += shade(ray, rayTraceAccelerated(ray));
            }
            output[rowId][colId] = finalColor * mult;
        }
    }
}

void Raytracer::renderRegionStereoscopy(int x, int y, int width, int height, Image& output) const
{
    float eyeDistance = scene->getSettings().eyeDistance;
    if (eyeDistance <= EPSILON) return; 
    const Vector& leftEyeColor = scene->getSettings().leftEyeColor;
    const Vector& rightEyeColor = scene->getSettings().rightEyeColor;
    for (int rowId = y; rowId < y + height; rowId++) {
        for (int colId = x; colId < x + width; colId++) {
            auto rayPair = scene->getCamera().getEyeRays(rowId, colId, eyeDistance);
            Vector colorLeft = shade(rayPair.first, rayTraceAccelerated(rayPair.first)) * DESATURIZATION * leftEyeColor;
            Vector colorRight = shade(rayPair.second, rayTraceAccelerated(rayPair.second)) * DESATURIZATION * rightEyeColor;
            output[rowId][colId] = colorLeft + colorRight;
        }
    }
}

void Raytracer::renderRegionDOFStereoscopy(int x, int y, int width, int height, Image& output) const
{
    float eyeDistance = scene->getSettings().eyeDistance;
    if (eyeDistance <= EPSILON) return;
    const Camera& camera = scene->getCamera();
    float u, v, mult, aperature, focalDistance;
    if (scene->getSettings().autoFocus) {
        // if we autofocus, we'll focus at the center of the scene
        focalDistance = ((scene->getAABB().max - scene->getAABB().min) / 2.0 - camera.getPosition()).length();
    }
    else {
        focalDistance = scene->getSettings().focalPlaneDist;
    }

    mult = 1.0f / scene->getSettings().dofSamples;
    aperature = 1.0f / scene->getSettings().fNum;
    const Vector& leftEyeColor = scene->getSettings().leftEyeColor;
    const Vector& rightEyeColor = scene->getSettings().rightEyeColor;
    for (int rowId = y; rowId < y + height; rowId++) {
        for (int colId = x; colId < x + width; colId++) {
            Vector finalColor(0, 0, 0);
            for (int i = 0; i < scene->getSettings().dofSamples; i++) {
                auto rayPair = scene->getCamera().getEyeRays(rowId, colId, eyeDistance);

                float M = focalDistance / dot(camera.getFrontDirection(), rayPair.first.direction);
                // both M values are the same, so we reuse them
                Vector T_left = rayPair.first.origin + rayPair.first.direction * M;
                Vector T_right = rayPair.second.origin + rayPair.second.direction * M;

                randomCircleSample(u, v);
                u *= aperature;
                v *= aperature;
                rayPair.first.origin = rayPair.first.origin + u * camera.getRightDirection() + v * camera.getUpDirection();
                rayPair.second.origin = rayPair.second.origin + u * camera.getRightDirection() + v * camera.getUpDirection();

                rayPair.first.direction = (T_left - rayPair.first.origin).normalize();
                rayPair.second.direction = (T_right - rayPair.second.origin).normalize();

                Vector colorLeft = shade(rayPair.first, rayTraceAccelerated(rayPair.first)) * DESATURIZATION * leftEyeColor;
                Vector colorRight = shade(rayPair.second, rayTraceAccelerated(rayPair.second)) * DESATURIZATION * rightEyeColor;

                finalColor += colorLeft + colorRight;
            }
            output[rowId][colId] = finalColor * mult;
        }
    }
}


Intersection Raytracer::rayTrace(const Ray& ray) const
{
    Intersection intersection;
    Intersection curr_intersection;
    float minDistanceToOrigin = FLOAT_MAX;
    for (int i = 0; i < scene->getObjectsCount(); i++) {
        curr_intersection = scene->getGeometryObject(i).intersectsRay(ray);
        if (curr_intersection.triangleIndex != NO_HIT_INDEX) {
            if (curr_intersection.t < minDistanceToOrigin) {
                minDistanceToOrigin = curr_intersection.t;
                intersection = std::move(curr_intersection);
                intersection.hitObjectIndex = i; // here we set the mesh it hits
                intersection.materialIndex = scene->getGeometryObject(i).getMaterialIndex();
            }
        }
    }
    return intersection;
}

Intersection Raytracer::rayTraceAccelerated(const Ray& ray, float maxDistance) const
{
    return scene->getAccelerationStructure().intersect(ray, maxDistance);
}


bool Raytracer::intersectsObject(const Ray& ray, float distanceToLight) const
{
    return rayTraceAccelerated(ray, distanceToLight).hitObjectIndex != NO_HIT_INDEX;
}

Image Raytracer::renderSceneBarycentic() const
{
    const unsigned imageHeight = scene->getSettings().imageSettings.height;
    const unsigned imageWidth = scene->getSettings().imageSettings.width;

    Image image(imageHeight, std::vector<Vector>(imageWidth, scene->getSettings().bgColor));

    Intersection intersection;
    for (int rowId = 0; rowId < imageHeight; rowId++) {
        for (int colId = 0; colId < imageWidth; colId++) {
            intersection = rayTraceAccelerated(scene->getCamera().getRayForPixel(rowId, colId));
            if (intersection.triangleIndex != NO_HIT_INDEX) {
                image[rowId][colId] = intersection.barycentricCoordinates;
            }
            else {
                image[rowId][colId] = scene->getSettings().bgColor;
            }
        }
    }
    return image;
}

Image Raytracer::renderSinglethreaded() const
{
    const unsigned imageHeight = scene->getSettings().imageSettings.height;
    const unsigned imageWidth = scene->getSettings().imageSettings.width;

    Image image(imageHeight, std::vector<Vector>(imageWidth, scene->getSettings().bgColor));
    renderRegionNoAABB(0, 0, imageWidth, imageHeight, image);
    return image;
}

Image Raytracer::renderByRegions() const
{
    const unsigned imageHeight = scene->getSettings().imageSettings.height;
    const unsigned imageWidth = scene->getSettings().imageSettings.width;

    Image image(imageHeight, std::vector<Vector>(imageWidth, scene->getSettings().bgColor));
    int threadCount = std::thread::hardware_concurrency();
    unsigned threadsPerSideY = sqrt(threadCount);
    unsigned threadsPerSideX = threadCount / threadsPerSideY;
    unsigned heightPerThread = imageHeight / threadsPerSideY;
    unsigned widthPerThread = imageWidth / threadsPerSideX; // written like so to avoid the issue of having a non-perfect square number of threads

    std::vector<std::thread> threads;
    for (int i = 0; i < threadCount; i++) {
        unsigned startX = (i * widthPerThread) % imageWidth;
        unsigned startY = (i / threadsPerSideX) * heightPerThread;
        threads.push_back(std::thread(&Raytracer::renderRegion, this,
           startX, startY, widthPerThread, heightPerThread, std::ref(image))
        );
    }
    for (auto& thread : threads) {
        thread.join();
    }
    return image;
}

Image Raytracer::renderByBuckets() const
{
    if (scene->getSettings().imageSettings.bucketSize <= 0) { // the size is invalid
        return renderByRegions();
    }

    const unsigned imageHeight = scene->getSettings().imageSettings.height;
    const unsigned imageWidth = scene->getSettings().imageSettings.width;

    Image image(imageHeight, std::vector<Vector>(imageWidth, scene->getSettings().bgColor));
     BucketArray buckets(scene); // the constructor generates the regions
    int bucketsCount = buckets.size();
    std::atomic_int bucketIndex = 0;

    int threadCount = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;

    for (int i = 0; i < threadCount; i++) {
        //threads.emplace_back(&Raytracer::threadLoop, this, std::ref(image), std::ref(queue));
        threads.push_back(std::thread([this, &buckets, &bucketsCount, &bucketIndex, &image]() {
            Region region;
            int idx = 0;
            while ((idx = bucketIndex.fetch_add(1)) < bucketsCount) {
                region = buckets.getRegion(idx);
                renderRegionNoAABB(region.startX, region.startY, region.width, region.height, image);
            }
            }));
    }

    for (auto& thread : threads) {
        thread.join();
    }
    return image;
}

Image Raytracer::renderWithAABB() const
{
    const unsigned imageHeight = scene->getSettings().imageSettings.height;
    const unsigned imageWidth = scene->getSettings().imageSettings.width;

    Image image(imageHeight, std::vector<Vector>(imageWidth, scene->getSettings().bgColor));

    BucketArray buckets(scene); // the constructor generates the regions
    int bucketsCount = buckets.size();
    std::atomic_int bucketIndex = 0;

    int threadCount = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;

    for (int i = 0; i < threadCount; i++) {
        //threads.emplace_back(&Raytracer::threadLoop, this, std::ref(image), std::ref(queue));
        threads.push_back(std::thread([this, &buckets, &bucketsCount, &bucketIndex, &image]() {
            Region region;
            int idx = 0;
            while ((idx = bucketIndex.fetch_add(1)) < bucketsCount) {
                region = buckets.getRegion(idx);
                renderRegion(region.startX, region.startY, region.width, region.height, image);
            }
            }));
    }

    for (auto& thread : threads) {
        thread.join();
    }
    return image;
}

Vector Raytracer::shade(const Ray& ray, const Intersection& data) const
{
    if (data.hitObjectIndex == NO_HIT_INDEX || ray.depth >= MAX_RAY_DEPTH) {
        return scene->getSettings().bgColor;
    }
    const Material& material = scene->getMaterial(data.materialIndex);
    if (material.type == MaterialType::DIFFUSE) {
        return shadeDiffuse(ray, data);
    }
    else if (material.type == MaterialType::REFLECTIVE) {
        if (scene->getSettings().reflections) 
            return shadeReflective(ray, data);
    }
    else if (material.type == MaterialType::REFRACTIVE) {
        if (scene->getSettings().refractions) 
            return shadeRefractive(ray, data);
    }
    else {
        assert(false);
    }
    return scene->getSettings().bgColor;
}

Vector Raytracer::shadeDiffuse(const Ray& ray, const Intersection& data) const {
    
    if (scene->getSettings().globalIllumination) {
        return (shadeDirectIllumination(ray, data) + shadeGlobalIllumination(ray, data)) / (float)(GI_RAYS + 1);
    }
    return shadeDirectIllumination(ray, data);
}

Vector Raytracer::shadeDirectIllumination(const Ray& ray, const Intersection& data) const
{
    Vector finalColor{ 0.0, 0.0, 0.0 };
    Vector normalVector = data.faceNormal;
    const Material& material = scene->getMaterial(data.materialIndex);
    if (material.smoothShading) {
        normalVector = data.smoothNormal;
    }
    for (const Light& light : scene->getLights()) {
        Vector lightDirection = light.getPosition() - data.hitPoint;
        float sphereRadius = lightDirection.length();
        float sphereArea = 4 * PI * sphereRadius * sphereRadius;
        lightDirection.normalize();
        float cosLaw = std::max(0.0f, dot(lightDirection, normalVector));
        Ray shadowRay{ data.hitPoint + normalVector * SHADOW_BIAS, lightDirection, RayType::SHADOW, ray.depth + 1 };
        if (!intersectsObject(shadowRay, sphereRadius)) {
            float multValue = light.getIntensity() / sphereArea * cosLaw;
            finalColor += scene->getGeometryObject(data.hitObjectIndex).sampleMaterial(scene->getMaterial(data.materialIndex), data) * multValue;
        }
    }
    return finalColor.clamp(0, 1);
}

Vector Raytracer::shadeGlobalIllumination(const Ray& ray, const Intersection& data) const
{
    Vector diffuseReflections{ 0.0, 0.0, 0.0 };
    Vector normalVector = data.faceNormal;
    const Material& material = scene->getMaterial(data.materialIndex);
    if (material.smoothShading) {
        normalVector = data.smoothNormal;
    }
    Vector e1, e3;
    e1 = cross(ray.direction, normalVector).normalize();
    e3 = cross(e1, normalVector);
    Matrix localHit(e1, normalVector, e3);
    for (int i = 0; i < GI_RAYS; i++) {
        Vector randomDirection = randomHemisphereSample(normalVector);
        // Importance sampling towards light sources
        Vector lightDirection = (scene->getRandomLight().getPosition() - data.hitPoint).normalize();
        if (randFloat() < LIGHT_IMPORTANCE) {
            randomDirection = ((1 - LIGHT_IMPORTANCE) * randomDirection + LIGHT_IMPORTANCE * lightDirection);
        }
        Ray diffuseReflectionRay{ data.hitPoint + (normalVector * REFLECTION_BIAS),
            randomDirection,
            RayType::REFLECTIVE,
            ray.depth + 1
        };
        diffuseReflections += shade(diffuseReflectionRay, rayTraceAccelerated(diffuseReflectionRay)).clamp(0, 1);
    }
    return diffuseReflections;
}

Vector Raytracer::shadeReflective(const Ray& ray, const Intersection& data) const {
    Vector normalVector = data.faceNormal;
    const Material& material = scene->getMaterial(data.materialIndex);
    if (material.smoothShading) {
        normalVector = data.smoothNormal;
    }
    Vector reflectedDirection = reflect(ray.direction, normalVector);
    Ray reflectedRay{ data.hitPoint + normalVector * REFLECTION_BIAS, reflectedDirection, RayType::REFLECTIVE, ray.depth + 1 };

    return shade(reflectedRay, rayTraceAccelerated(reflectedRay))
        * scene->getGeometryObject(data.hitObjectIndex).sampleMaterial(scene->getMaterial(data.materialIndex), data);
}

Vector Raytracer::shadeRefractive(const Ray& ray, const Intersection& data) const {
    const Material& material = scene->getMaterial(data.materialIndex);
    float n1 = 1.0f; // Index of refraction of the air
    float n2 = material.ior;
    Vector normalVector = material.smoothShading ? data.smoothNormal : data.faceNormal;

    float dotPr = dot(ray.direction, normalVector);
    if (dotPr > 0) { // Ray is leaving the refractive object
        normalVector *= -1;
        dotPr *= -1;
        std::swap(n1, n2);
    }

    float cosIncomming = -dotPr;
    float sinIncomming = 1.0f - cosIncomming * cosIncomming;

    if (sinIncomming > ((n2 * n2) / (n1 * n1))) { // total internal reflection
        Vector reflectedDirection = reflect(ray.direction, normalVector);
        Ray reflectedRay{ data.hitPoint + normalVector * REFLECTION_BIAS, reflectedDirection, RayType::REFLECTIVE, ray.depth + 1 };
        return shade(reflectedRay, rayTraceAccelerated(reflectedRay));
    }

    float sinOutcomming = (n1 / n2) * sqrt(std::max(0.0f, 1.0f - cosIncomming * cosIncomming));
    float cosOutcomming = sqrt(std::max(0.0f, 1.0f - sinOutcomming * sinOutcomming));

    Vector A = cosOutcomming * (-normalVector);
    Vector C = ray.direction + (cosIncomming * normalVector);
    C.normalize();
    Vector refractedDirection = A + (C * sinOutcomming);
    refractedDirection.normalize();

    Ray refractedRay{ data.hitPoint + normalVector * -REFRACTION_BIAS, refractedDirection, RayType::REFRACTIVE, ray.depth + 1 };

    Vector reflectedDirection = reflect(ray.direction, normalVector);
    Ray reflectedRay{ data.hitPoint + normalVector * REFLECTION_BIAS, reflectedDirection, RayType::REFLECTIVE, ray.depth + 1 };

    // Improved Fresnel calculation using Schlick's approximation
    float R0 = pow((n1 - n2) / (n1 + n2), 2);
    float fresnel = R0 + (1 - R0) * pow(1.0f - cosIncomming, 5);

    return fresnel * shade(reflectedRay, rayTraceAccelerated(reflectedRay)) 
        + (1 - fresnel) * shade(refractedRay, rayTraceAccelerated(refractedRay));
}
