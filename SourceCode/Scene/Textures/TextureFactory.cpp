#include "TextureFactory.h"
using namespace rapidjson;

static Vector loadVector(const Value::ConstArray& arr) {
	assert(arr.Size() == 3);
	Vector vec{
		static_cast<float>(arr[0].GetDouble()),
		static_cast<float>(arr[1].GetDouble()),
		static_cast<float>(arr[2].GetDouble())
	};
	return vec;
}

void TextureFactory::loadTexture(const rapidjson::Value::ConstObject& textureVal, std::unordered_map<std::string, std::shared_ptr<Texture>>& textures)
{
	const Value& nameVal = textureVal.FindMember(sceneTextureName)->value;
	assert(!nameVal.IsNull() && nameVal.IsString());

	const Value& typeVal = textureVal.FindMember(sceneTextureType)->value;
	assert(!typeVal.IsNull() && typeVal.IsString());

	std::string type = typeVal.GetString();
	std::shared_ptr<Texture> loadedTexture;
	if (type == sceneTextureAlbedo) {
		loadedTexture = loadAlbedo(textureVal);
	}
	else if (type == sceneTextureEdges) {
		loadedTexture = loadEdges(textureVal);
	}
	else if (type == sceneTextureChecker) {
		loadedTexture = loadChecker(textureVal);
	}
	else if (type == sceneTextureBitmap) {
		loadedTexture = loadBitmap(textureVal);
	}
	else {
		assert(false);
	}
	textures[nameVal.GetString()] = loadedTexture;
}

std::shared_ptr<Texture> TextureFactory::loadAlbedo(const rapidjson::Value::ConstObject& textureObj)
{
	const Value& albedoVal = textureObj.FindMember(sceneTextureAlbedoVector)->value;
	assert(!albedoVal.IsNull() && albedoVal.IsArray());
	return std::make_shared<Texture>(ConstantTexture(loadVector(albedoVal.GetArray())));
}

std::shared_ptr<Texture> TextureFactory::loadEdges(const rapidjson::Value::ConstObject& textureObj)
{
	const Value& nameVal = textureObj.FindMember(sceneTextureName)->value;
	assert(!nameVal.IsNull() && nameVal.IsString());

	const Value& colorEdge = textureObj.FindMember(sceneTextureEdgeColorEdge)->value;
	assert(!colorEdge.IsNull() && colorEdge.IsArray());

	const Value& colorInner = textureObj.FindMember(sceneTextureEdgeColorInner)->value;
	assert(!colorInner.IsNull() && colorInner.IsArray());

	const Value& edgeWidth = textureObj.FindMember(sceneTextureEdgeWidth)->value;
	assert(!edgeWidth.IsNull() && edgeWidth.IsFloat());
	return std::make_shared<Texture>(
		EdgeTexture(loadVector(colorEdge.GetArray()), 
		loadVector(colorInner.GetArray()), 
		edgeWidth.GetFloat()));
}

std::shared_ptr<Texture> TextureFactory::loadChecker(const rapidjson::Value::ConstObject& textureObj)
{
	const Value& nameVal = textureObj.FindMember(sceneTextureName)->value;
	assert(!nameVal.IsNull() && nameVal.IsString());

	const Value& colorA = textureObj.FindMember(sceneTextureCheckerColorA)->value;
	assert(!colorA.IsNull() && colorA.IsArray());

	const Value& colorB = textureObj.FindMember(sceneTextureCheckerColorB)->value;
	assert(!colorB.IsNull() && colorB.IsArray());

	const Value& squareSize = textureObj.FindMember(sceneTextureCheckerSquareSize)->value;
	assert(!squareSize.IsNull() && squareSize.IsFloat());
	return std::make_shared<Texture>(
		CheckerTexture(loadVector(colorA.GetArray()),
		loadVector(colorB.GetArray()),
		squareSize.GetFloat()));
}

std::shared_ptr<Texture> TextureFactory::loadBitmap(const rapidjson::Value::ConstObject& textureObj)
{
	const Value& nameVal = textureObj.FindMember(sceneTextureName)->value;
	assert(!nameVal.IsNull() && nameVal.IsString());

	const Value& pathToImage = textureObj.FindMember(sceneTextureBitmapPath)->value;
	assert(!pathToImage.IsNull() && pathToImage.IsString());

	return std::make_shared<Texture>(BitmapTexture(pathToImage.GetString()));
}

std::unordered_map<std::string, std::shared_ptr<Texture>> TextureFactory::parseTextures(const rapidjson::Document& doc)
{
	std::unordered_map<std::string, std::shared_ptr<Texture>> result;
	const Value& objectsVal = doc.FindMember(sceneTextures)->value;
	if (!objectsVal.IsNull() && objectsVal.IsArray()) {
		size_t objectsCount = objectsVal.GetArray().Size();
		for (int i = 0; i < objectsCount; i++) {
			assert(!objectsVal.GetArray()[i].IsNull() && objectsVal.GetArray()[i].IsObject());
			loadTexture(objectsVal.GetArray()[i].GetObject(), result);
		}
	}
	return result;
}
