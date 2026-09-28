#include "Tonemap.h"
#include <cmath>
#include <algorithm>

// Geometric mean of Rec.709 luminance — the scene "key" for auto-exposure.
// The epsilon keeps log(0) finite for fully black pixels.
float Tonemap::logAverageLuminance(const Image& image) {
	const unsigned height = image.size();
	const unsigned width = image[0].size();
	double logSum = 0.0;
	for (unsigned r = 0; r < height; r++)
		for (unsigned c = 0; c < width; c++) {
			const Vector& p = image[r][c];
			float lum = 0.2126f * p.x + 0.7152f * p.y + 0.0722f * p.z;
			logSum += std::log(std::max(lum, 0.0f) + 1e-4f);
		}
	return (float)std::exp(logSum / (double)(height * width));
}

void Tonemap::apply(Image& image, float exposure) {
	const unsigned height = image.size();
	const unsigned width = image[0].size();

	// Auto-exposure: scale the scene key to middle-grey so dark scenes don't
	// crush and bright ones don't blow out, without hand-tuning light intensity.
	float logAvg = logAverageLuminance(image);
	float scale = (logAvg > 0.0f ? TONEMAP_KEY / logAvg : 1.0f) * exposure;

	// Per-channel Reinhard x/(1+x) → smooth [0,inf)->[0,1) rolloff (no hard clip),
	// then gamma for display. Final clamp guards FP slop only.
	const float invGamma = 1.0f / TONEMAP_GAMMA;
	for (unsigned r = 0; r < height; r++)
		for (unsigned c = 0; c < width; c++) {
			Vector p = image[r][c] * scale;
			p.x = std::pow(p.x / (1.0f + p.x), invGamma);
			p.y = std::pow(p.y / (1.0f + p.y), invGamma);
			p.z = std::pow(p.z / (1.0f + p.z), invGamma);
			image[r][c] = p.clamp(0, 1);
		}
}
