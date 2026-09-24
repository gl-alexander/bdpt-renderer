#pragma once
#include "../Utils/Vector.h"
#include "../Utils/Ray.h"
#include "Intersection.h"
#include "Textures/Texture.h"
#include <memory>
#include <string>

enum class MaterialType {
	DIFFUSE,
	REFLECTIVE,
	REFRACTIVE
};

struct Material {
	MaterialType type;
	bool constantAlbedo;
	Vector albedo;
	std::shared_ptr<Texture> texture;
	bool smoothShading;
	float ior;
};

