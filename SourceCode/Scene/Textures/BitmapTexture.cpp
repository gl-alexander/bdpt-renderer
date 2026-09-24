#include "BitmapTexture.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../../Dependencies/stb_image.h"
#include <algorithm>

BitmapTexture::BitmapTexture(const std::string& imagePath)
{
	uint8_t* image = stbi_load(imagePath.c_str(), &width, &height, &channels, 0);
	if (image) {
		pixels.resize(width * height);
		for (int i = 0; i < width * height; ++i) {
			// To save resources on deletion, we can parse the colors and free the stbi_image directly when constructing
			float multiValue = 1.0f / (float)MAX_COLOR_COMPONENT;
			pixels[i] = { image[channels * i] * multiValue,  
				image[channels * i + 1] * multiValue,
				image[channels * i + 2] * multiValue };
		}
		stbi_image_free(image);
	}
	else {
		throw std::invalid_argument("No such image found");
	}
}

Vector BitmapTexture::sample(float u, float v, const Vector& bary) const
{
	int x = std::clamp(static_cast<int>(u * width), 0, width - 1);
	int y = std::clamp(static_cast<int>((1 - v) * height), 0, height - 1);
	return pixels[y * width + x];
}
