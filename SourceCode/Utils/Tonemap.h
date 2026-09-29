#pragma once
#include <vector>
#include "Vector.h"

using Image = std::vector<std::vector<Vector>>;

constexpr float TONEMAP_KEY = 0.12f; // lower key -- darker exposure and richer saturation before Reinhard rollof
constexpr float TONEMAP_GAMMA = 2.2f;

// linear hdr mapping with log-avg auto exposure, per-channel reinhard rolloff and gamma.
// hdr=false skips auto-exposure + Reinhard (raw linear radiance), keeping only gamma + clamp.
struct Tonemap
{
	static float logAverageLuminance(const Image& image);
	static void apply(Image& image, float exposure = 1.0f, bool hdr = true);
};
