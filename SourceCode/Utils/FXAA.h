#pragma once
#include <vector>
#include "Vector.h"

using Image = std::vector<std::vector<Vector>>;

// aggressive
constexpr int FXAA_SEARCH_STEPS = 16;

constexpr float FXAA_EDGE_THRESHOLD = 0.0625;
constexpr float FXAA_EDGE_THRESHOLD_MIN = 0.012;
constexpr float FXAA_SUBPIX_TRIM = 1.0 / 8;
constexpr float FXAA_SUBPIX_CAP = 1.0f;


struct FXAA
{
	static float FXAALuminance(const Vector& rgb);
	static float FXAAColorContrast(const Vector& a, const Vector& b);
	static Vector FXAAPixel(Image& image, unsigned x, unsigned y);
	static void applyFXAA(Image& image);
};

