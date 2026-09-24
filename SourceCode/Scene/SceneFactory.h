#pragma once
#include <fstream>
#include "Scene.h"
#include "Textures/TextureFactory.h"
#include "../Dependencies/rapidjson/document.h"
#include "../Dependencies/rapidjson/istreamwrapper.h"

using TextureMap = std::unordered_map<std::string, std::shared_ptr<Texture>>;

class SceneFactory {

	static Vector loadVector(const rapidjson::Value::ConstArray& arr);
	static Matrix loadMatrix(const rapidjson::Value::ConstArray& arr);
	static Light loadLight(const rapidjson::Value::ConstObject& lightVal);

	static std::vector<Vector> loadVertices(const rapidjson::Value::ConstArray& arr, Box& AABB);
	static std::vector<Vector> loadUVVertices(const rapidjson::Value::ConstArray& arr);
	static std::vector<int> loadTriangleIndices(const rapidjson::Value::ConstArray& arr);
	static Mesh loadMesh(const rapidjson::Value::ConstObject& meshVal, Box& AABB);
	static Material loadMaterial(const rapidjson::Value::ConstObject& matVal, const TextureMap& textures);

	static std::vector<Light> parseLights(const rapidjson::Document& doc);
	static void parseSettings(const rapidjson::Document& doc, Settings& settings, Camera& camera);
	static std::vector<Mesh> parseObjects(const rapidjson::Document& doc, Box& AABB);
	static std::vector<Material> parseMaterials(const rapidjson::Document& doc, const TextureMap& textures);

public: 
	static Scene* factory(const char* filename);
};


static const char* sceneSettings = "settings";
static const char* sceneVersion = "version";
static const char* sceneBGColor = "background_color";
static const char* sceneReflectionsOn = "reflections_on";
static const char* sceneRefractionsOn = "refractions_on";
static const char* sceneGIOn = "gi_on";
static const char* sceneFXAAOn = "fxaa_on";
static const char* sceneImageSettings = "image_settings";
static const char* sceneImageWidth = "width";
static const char* sceneImageHeight = "height";
static const char* sceneImageBucketSize = "bucket_size";
static const char* sceneCamera = "camera";
static const char* sceneCameraMatrix = "matrix";
static const char* sceneCameraPosition = "position";
static const char* sceneObjects = "objects";
static const char* sceneVertices = "vertices";
static const char* sceneUVs = "uvs";
static const char* sceneTriangles = "triangles";
static const char* sceneLights = "lights";
static const char* sceneLightPosition = "position";
static const char* sceneLightIntensity = "intensity";
static const char* sceneMeshMaterials = "materials";
static const char* sceneMeshMaterialType = "type";
static const char* sceneMeshTextureName = "albedo";
static const char* sceneMeshAlbedo = "albedo";
static const char* sceneMeshMaterialSmoothShading = "smooth_shading";
static const char* sceneMeshMaterialIndex = "material_index";
static const char* sceneMeshMaterialDiffuse = "diffuse";
static const char* sceneMeshMaterialConstant = "constant";
static const char* sceneMeshMaterialReflective = "reflective";
static const char* sceneMeshMaterialRefractive = "refractive";
static const char* sceneMeshMaterialIndexOfRefraction = "ior";

