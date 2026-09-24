#pragma once
#include <vector>
#include "../../Utils/Vector.h"
#include "../../Utils/Color.h"

class BitmapTexture
{
	int width;
	int height;
	int channels;
	std::vector<Vector> pixels;
public:
	BitmapTexture(const std::string& imagePath);
	Vector sample(float u, float v, const Vector& bary) const ;
};

