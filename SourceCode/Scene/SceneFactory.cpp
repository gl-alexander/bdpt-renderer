#include "SceneFactory.h"

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
using namespace rapidjson;

static Document getJsonDocument(const char* filename) {
	std::ifstream ifs(filename);
	if (!ifs.is_open())
	{
		throw std::invalid_argument("couldn't open file");
	}

	IStreamWrapper isw(ifs);

	Document doc;
	doc.ParseStream(isw);
	return doc;
}



Vector SceneFactory::loadVector(const Value::ConstArray& arr) {
	assert(arr.Size() == 3);
	Vector vec{
		static_cast<float>(arr[0].GetFloat()),
		static_cast<float>(arr[1].GetFloat()),
		static_cast<float>(arr[2].GetFloat())
	};
	return vec;
}

Matrix SceneFactory::loadMatrix(const Value::ConstArray& arr) {
	assert(arr.Size() == 9);
	float matrix[3][3] = {
		{
			arr[0].GetFloat(),
			arr[1].GetFloat(),
			arr[2].GetFloat()
		},
		{
			static_cast<float>(arr[3].GetFloat()),
			static_cast<float>(arr[4].GetFloat()),
			static_cast<float>(arr[5].GetFloat())
		},
		{
			static_cast<float>(arr[6].GetFloat()),
			static_cast<float>(arr[7].GetFloat()),
			static_cast<float>(arr[8].GetFloat())
		}
	};
	return Matrix(matrix);
}

Light SceneFactory::loadLight(const Value::ConstObject& lightVal) {
	const Value& intensityVal = lightVal.FindMember(sceneLightIntensity)->value;
	assert(!intensityVal.IsNull() && intensityVal.IsNumber());
	float intensityResult = intensityVal.GetFloat();
	const Value& positionVal = lightVal.FindMember(sceneLightPosition)->value;
	assert(!positionVal.IsNull() && positionVal.IsArray());
	return Light(loadVector(positionVal.GetArray()), intensityResult);
}

std::vector<Light> SceneFactory::parseLights(const rapidjson::Document& doc) {
	std::vector<Light> lights;
	const Value& lightsVal = doc.FindMember(sceneLights)->value;
	if (!lightsVal.IsNull() && lightsVal.IsArray()) {
		int numLights = lightsVal.GetArray().Size();
		assert(numLights > 0);
		for (int i = 0; i < numLights; i++) {
			assert(!lightsVal.GetArray()[i].IsNull() && lightsVal.GetArray()[i].IsObject());
			lights.push_back(loadLight(lightsVal.GetArray()[i].GetObject()));
		}
	}
	return lights;
}

void SceneFactory::parseSettings(const Document& doc, Settings& settings, Camera& camera) {
	const Value& settingsVal = doc.FindMember(sceneSettings)->value;
	if (!settingsVal.IsNull() && settingsVal.IsObject()) {
		//const Value& sceneVersion = settingsVal.FindMember(sceneVersion)->value;
		//if (!sceneVersion.IsNull() && sceneVersion.IsInt()) {
		//	settings.version = sceneVersion.GetInt();
		//}

		const Value& bgColorValue = settingsVal.FindMember(sceneBGColor)->value;
		assert(!bgColorValue.IsNull() && bgColorValue.IsArray());
		settings.bgColor = loadVector(bgColorValue.GetArray());

		const Value& imageSettingsVal = settingsVal.FindMember(sceneImageSettings)->value;
		if (!imageSettingsVal.IsNull() && imageSettingsVal.IsObject()) {
			const Value& imageWidthVal = imageSettingsVal.FindMember(sceneImageWidth)->value;
			const Value& imageHeightVal = imageSettingsVal.FindMember(sceneImageHeight)->value;
			assert(!imageWidthVal.IsNull() && !imageHeightVal.IsNull()
				&& imageWidthVal.IsInt() && imageHeightVal.IsInt());
			settings.imageSettings.width = imageWidthVal.GetInt();
			settings.imageSettings.height = imageHeightVal.GetInt();
			camera.setImageSettings(settings.imageSettings.width, settings.imageSettings.height);

			const Value& imageBucketSize = imageSettingsVal.FindMember(sceneImageBucketSize)->value;
			if (!imageBucketSize.IsNull() && imageBucketSize.IsInt()) {
				settings.imageSettings.bucketSize = imageBucketSize.GetInt();
			}

			const Value& reflectionsOnVal = settingsVal.FindMember(sceneReflectionsOn)->value;
			if (!reflectionsOnVal.IsNull() && reflectionsOnVal.IsBool()) {
				settings.reflections = reflectionsOnVal.GetBool();
			}
			const Value& refractionsOnVal = settingsVal.FindMember(sceneRefractionsOn)->value;
			if (!refractionsOnVal.IsNull() && refractionsOnVal.IsBool()) {
				settings.refractions = refractionsOnVal.GetBool();
			}
			const Value& globalIlluminationVal = settingsVal.FindMember(sceneGIOn)->value;
			if (!globalIlluminationVal.IsNull() && globalIlluminationVal.IsBool()) {
				settings.globalIllumination = globalIlluminationVal.GetBool();
			}
			const Value& fxaaVal = settingsVal.FindMember(sceneFXAAOn)->value;
			if (!fxaaVal.IsNull() && fxaaVal.IsBool()) {
				settings.FXAA = fxaaVal.GetBool();
			}
		}

		const Value& cameraVal = doc.FindMember(sceneCamera)->value;
		if (!cameraVal.IsNull() && cameraVal.IsObject()) {
			const Value& matrixVal = cameraVal.FindMember(sceneCameraMatrix)->value;
			assert(!matrixVal.IsNull() && matrixVal.IsArray());
			camera.setRotation(loadMatrix(matrixVal.GetArray()));

			const Value& positionVal = cameraVal.FindMember(sceneCameraPosition)->value;
			assert(!positionVal.IsNull() && positionVal.IsArray());
			camera.setPosition(loadVector(positionVal.GetArray()));
		}
	}
}

std::vector<Vector> SceneFactory::loadVertices(const Value::ConstArray& arr, Box& AABB) {
	size_t verticesCount = arr.Size() / Vector::MEMBERS_COUNT;
	std::vector<Vector> result;
	assert(arr.Size() % Vector::MEMBERS_COUNT == 0);
	for (int i = 0; i < verticesCount; i++) {
		size_t currentBatch = i * Vector::MEMBERS_COUNT;
		Vector vec{
		static_cast<float>(arr[currentBatch + 0].GetFloat()),
		static_cast<float>(arr[currentBatch + 1].GetFloat()),
		static_cast<float>(arr[currentBatch + 2].GetFloat())
		};
		AABB.max.x = std::max(vec.x, AABB.max.x);
		AABB.max.y = std::max(vec.y, AABB.max.y);
		AABB.max.z = std::max(vec.z, AABB.max.z);

		AABB.min.x = std::min(vec.x, AABB.min.x);
		AABB.min.y = std::min(vec.y, AABB.min.y);
		AABB.min.z = std::min(vec.z, AABB.min.z);
		result.push_back(vec);
	}
	return result;
}

std::vector<Vector> SceneFactory::loadUVVertices(const rapidjson::Value::ConstArray& arr)
{
	size_t verticesCount = arr.Size() / Vector::MEMBERS_COUNT;
	std::vector<Vector> result;
	assert(arr.Size() % Vector::MEMBERS_COUNT == 0);
	for (int i = 0; i < verticesCount; i++) {
		size_t currentBatch = i * Vector::MEMBERS_COUNT;
		Vector vec{
		static_cast<float>(arr[currentBatch + 0].GetFloat()),
		static_cast<float>(arr[currentBatch + 1].GetFloat()),
		static_cast<float>(arr[currentBatch + 2].GetFloat())
		};
		result.push_back(vec);
	}
	return result;
}

std::vector<int> SceneFactory::loadTriangleIndices(const Value::ConstArray& arr) {
	assert(arr.Size() % VERTICES == 0);

	std::vector<int> result;
	for (int i = 0; i < arr.Size(); i++) {
		result.push_back(arr[i].GetInt());
	}
	return result;
}

Mesh SceneFactory::loadMesh(const Value::ConstObject& meshVal, Box& AABB) {
	const Value& meshVertices = meshVal.FindMember(sceneVertices)->value;
	assert(!meshVertices.IsNull() && meshVertices.IsArray());

	const Value& triangleVal = meshVal.FindMember(sceneTriangles)->value;
	assert(!triangleVal.IsNull() && triangleVal.IsArray());

	const Value& materialIndexVal = meshVal.FindMember(sceneMeshMaterialIndex)->value;
	assert(!materialIndexVal.IsNull() && materialIndexVal.IsInt());

	const Value& meshUVs = meshVal.FindMember(sceneUVs)->value;
	if (!meshUVs.IsNull() && meshUVs.IsArray()) {
		return Mesh(loadVertices(meshVertices.GetArray(), AABB),
			loadUVVertices(meshUVs.GetArray()),
			loadTriangleIndices(triangleVal.GetArray()),
			materialIndexVal.GetInt());
	}
	return Mesh(loadVertices(meshVertices.GetArray(), AABB),
		loadTriangleIndices(triangleVal.GetArray()),
		materialIndexVal.GetInt());
}

std::vector<Mesh> SceneFactory::parseObjects(const Document& doc, Box& AABB) {
	std::vector<Mesh> geometryObjects;
	const Value& objectsVal = doc.FindMember(sceneObjects)->value;
	if (!objectsVal.IsNull() && objectsVal.IsArray()) {
		size_t objectsCount = objectsVal.GetArray().Size();
		for (int i = 0; i < objectsCount; i++) {
			assert(!objectsVal.GetArray()[i].IsNull() && objectsVal.GetArray()[i].IsObject());
			geometryObjects.push_back(loadMesh(objectsVal.GetArray()[i].GetObject(), AABB));
		}
	}
	return geometryObjects;
}

Material SceneFactory::loadMaterial(const Value::ConstObject& matVal, const TextureMap& textures) {
	Material material;
	const Value& typeVal = matVal.FindMember(sceneMeshMaterialType)->value;
	assert(!typeVal.IsNull() && typeVal.IsString());
	const char* typeAsString = typeVal.GetString();
	if (strcmp(typeAsString, sceneMeshMaterialDiffuse) == 0
		|| strcmp(typeAsString, sceneMeshMaterialConstant) == 0) {
		// legacy scenes use "constant" for flat-albedo materials; render as diffuse
		material.type = MaterialType::DIFFUSE;
	}
	else if (strcmp(typeAsString, sceneMeshMaterialReflective) == 0) {
		material.type = MaterialType::REFLECTIVE;
	}
	else if (strcmp(typeAsString, sceneMeshMaterialRefractive) == 0) {
		material.type = MaterialType::REFRACTIVE;
	}
	else {
		throw std::logic_error("material type unknown");
	}
	const Value& shadingVal = matVal.FindMember(sceneMeshMaterialSmoothShading)->value;
	assert(!shadingVal.IsNull() && shadingVal.IsBool());
	material.smoothShading = shadingVal.GetBool();

	const Value& textureNameVal = matVal.FindMember(sceneMeshTextureName)->value;
	if (!textureNameVal.IsNull() && textureNameVal.IsArray()) {
		material.albedo = loadVector(textureNameVal.GetArray());
		material.constantAlbedo = true;
	}
	else if (!textureNameVal.IsNull() && textureNameVal.IsString()) {
		material.texture = textures.at(textureNameVal.GetString());
		material.constantAlbedo = false;
	}

	if (material.type == MaterialType::REFRACTIVE) {
		const Value& iorVal = matVal.FindMember(sceneMeshMaterialIndexOfRefraction)->value;
		assert(!iorVal.IsNull() && iorVal.IsDouble());
		material.ior = iorVal.GetFloat();
	}

	return material;
}

std::vector<Material> SceneFactory::parseMaterials(const rapidjson::Document& doc, const TextureMap& textures)
{
	std::vector<Material> materials;
	const Value& materialsVal = doc.FindMember(sceneMeshMaterials)->value;
	if (!materialsVal.IsNull() && materialsVal.IsArray()) {
		size_t materialsCount = materialsVal.GetArray().Size();
		for (int i = 0; i < materialsCount; i++) {
			assert(!materialsVal.GetArray()[i].IsNull() && materialsVal.GetArray()[i].IsObject());
			materials.push_back(loadMaterial(materialsVal.GetArray()[i].GetObject(), textures));
		}
	}
	return materials;
}


Scene* SceneFactory::factory(const char* filename)
{
	Document doc = getJsonDocument(filename);
	Camera camera;
	Settings settings;
	parseSettings(doc, settings, camera);

	Box AABB;
	AABB.max = { FLOAT_MIN, FLOAT_MIN, FLOAT_MIN };
	AABB.min = { FLOAT_MAX, FLOAT_MAX, FLOAT_MAX };
	std::vector<Light> lights = parseLights(doc);
	std::unordered_map<std::string, std::shared_ptr<Texture>> textures = TextureFactory::parseTextures(doc);
	std::vector<Material> materials = parseMaterials(doc, textures);
	std::vector<Mesh> geometryObjects = parseObjects(doc, AABB);

	camera.updateDirections();
	return new Scene(camera, settings, geometryObjects, materials, textures, lights, AABB);
}
