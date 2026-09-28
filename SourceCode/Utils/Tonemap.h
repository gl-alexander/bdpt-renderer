#pragma once
#include <vector>
#include "Vector.h"

using Image = std::vector<std::vector<Vector>>;

// Middle-grey target for log-average auto-exposure. Lower key = darker
// exposure = richer saturation before the Reinhard rolloff.
constexpr float TONEMAP_KEY = 0.12f;
constexpr float TONEMAP_GAMMA = 2.2f;

// Display transform for a linear HDR image: log-average auto-exposure →
// per-channel Reinhard rolloff → gamma, applied in place. `exposure` is an
// optional manual multiplier layered on top of the auto scale (1.0 = none).
struct Tonemap
{
	static float logAverageLuminance(const Image& image);
	static void apply(Image& image, float exposure = 1.0f);
};
