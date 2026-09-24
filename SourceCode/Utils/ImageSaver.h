#pragma once
#include <iostream>
#include <vector>
#include <fstream>
#include "Color.h"
#include "Vector.h"

using Image = std::vector<std::vector<Vector>>;

constexpr float EDGE_THRESHOLD_MIN = 0.0312;
constexpr float EDGE_THRESHOLD_MAX = 0.125;

static const char LINE_SEP = '\n';
static const char PIXEL_SEP = '\t';

class ImageSaver
{
public:
	static void saveImage(const char* imgName, const Image& image, bool FXAA = false);
	static void saveImage(std::ofstream& ppm, const Image& image, bool FXAA = false);
};

std::ofstream& operator<<(std::ofstream& ppm, const Color& pixel);