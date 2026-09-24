#pragma once
#include <unordered_map>
#include <memory>
#include "Texture.h"
#include "../../Dependencies/rapidjson/document.h"

class TextureFactory
{
	static void loadTexture(const rapidjson::Value::ConstObject& textureObj, 
		std::unordered_map<std::string, std::shared_ptr<Texture>>& textures);
	static std::shared_ptr<Texture> loadAlbedo(const rapidjson::Value::ConstObject& textureObj);
	static std::shared_ptr<Texture> loadEdges(const rapidjson::Value::ConstObject& textureObj);
	static std::shared_ptr<Texture> loadChecker(const rapidjson::Value::ConstObject& textureObj);
	static std::shared_ptr<Texture> loadBitmap(const rapidjson::Value::ConstObject& textureObj);
public:
	static std::unordered_map<std::string, std::shared_ptr<Texture>> parseTextures(const rapidjson::Document& doc);
};

static const char* sceneTextures = "textures";
static const char* sceneTextureName = "name";
static const char* sceneTextureType = "type";
static const char* sceneTextureAlbedo = "albedo";
static const char* sceneTextureAlbedoVector = "albedo";
static const char* sceneTextureEdges = "edges";
static const char* sceneTextureEdgeColorEdge = "edge_color";
static const char* sceneTextureEdgeColorInner = "inner_color";
static const char* sceneTextureEdgeWidth = "edge_width";
static const char* sceneTextureChecker = "checker";
static const char* sceneTextureCheckerColorA = "color_A";
static const char* sceneTextureCheckerColorB = "color_B";
static const char* sceneTextureCheckerSquareSize = "square_size";
static const char* sceneTextureBitmap = "bitmap";
static const char* sceneTextureBitmapPath = "file_path";
