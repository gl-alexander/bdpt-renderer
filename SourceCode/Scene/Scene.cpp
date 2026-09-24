#include "Scene.h"
#include <stdexcept>

Scene::Scene(const Camera& camera, const Settings& settings,
	const std::vector<Mesh>& geometryObjects, const std::vector<Material>& materials,
	const std::unordered_map<std::string, std::shared_ptr<Texture>>& textures, 
	const std::vector<Light>& lights, const Box& AABB)
	: camera(camera), settings(settings), geometryObjects(geometryObjects), materials(materials), lights(lights), textures(textures), AABB(AABB),
	accelerationTree(this->geometryObjects, this->materials, this->AABB)
{}

int Scene::getObjectsCount() const
{
	return geometryObjects.size();
}

const Mesh& Scene::getGeometryObject(int index) const
{
	return geometryObjects[index];
}

const Material& Scene::getMaterial(int index) const {
	return materials[index];
}

const KDTree& Scene::getAccelerationStructure() const
{
	return accelerationTree;
}

std::shared_ptr<Texture> Scene::getTexture(const std::string& textureName) const
{
	return textures.at(textureName);
}

const Camera& Scene::getCamera() const
{
	return camera;
}

void Scene::setCamera(const Camera& camera)
{
	this->camera = camera;
}

const Settings& Scene::getSettings() const
{
	return settings;
}

const std::vector<Light>& Scene::getLights() const {
	return lights;
}

const Light& Scene::getRandomLight() const
{
	return lights[randomInt(0, lights.size())];
}

const Box& Scene::getAABB() const
{
	return AABB;
}
