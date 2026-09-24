#pragma once
#include <vector>
#include <memory>
#include <string>
#include "Camera.h"
#include "Mesh.h"
#include "Settings.h"
#include "Light.h"
#include "Textures/Texture.h"
#include "../Utils/Box.h"
#include "../Utils/KDTree.h"

class Scene
{
	Box AABB;
	std::vector<Mesh> geometryObjects;
	std::vector<Material> materials;
	std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
	KDTree accelerationTree;

	Camera camera;
	Settings settings;
	std::vector<Light> lights;

	std::shared_ptr<Texture> getTexture(const std::string& textureName) const;

public:
	Scene(const Camera& camera, const Settings& settings, 
		const std::vector<Mesh>& geometryObjects, const std::vector<Material>& materials, 
		const std::unordered_map<std::string, std::shared_ptr<Texture>>& textures, const std::vector<Light>& lights,
		const Box& AABB);
	
	const Mesh& getGeometryObject(int index) const;
	const Material& getMaterial(int index) const;
	const KDTree& getAccelerationStructure() const;

	int getObjectsCount() const;

	const Camera& getCamera() const;
	void setCamera(const Camera& camera);
	const Settings& getSettings() const;
	const std::vector<Light>& getLights() const;
	const Light& getRandomLight() const;
	const Box& getAABB() const;
};
