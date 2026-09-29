#include "Tonemap.h"
#include <cmath>
#include <algorithm>

float Tonemap::logAverageLuminance(const Image& image) {
	const unsigned height = image.size();
	const unsigned width = image[0].size();
	double logSum = 0.0;
	long count = 0;
	for (unsigned r = 0; r < height; r++)
		for (unsigned c = 0; c < width; c++) {
			const Vector& p = image[r][c];
			float lum = 0.2126f * p.x + 0.7152f * p.y + 0.0722f * p.z;
			// Exclude unrendered pixels (camera rays that miss all geometry stay
			// exactly (0,0,0)) so the empty margin around the scene doesn't drag
			// the key down and over-expose the lit content.
			if (lum <= 1e-6f) continue;
			logSum += std::log(lum + 1e-4f);
			count++;
		}
	return count > 0 ? (float)std::exp(logSum / (double)count) : 1.0f;
}

void Tonemap::apply(Image& image, float exposure, bool hdr) {
	const unsigned height = image.size();
	const unsigned width = image[0].size();

	// auto-exposure: scale the scene key to middle-grey (skipped when hdr=false)
	float scale = exposure;
	if (hdr) {
		float logAvg = logAverageLuminance(image);
		scale = (logAvg > 0.0f ? TONEMAP_KEY / logAvg : 1.0f) * exposure;
	}

	// per-channel Reinhard x/(1+x) to  smooth [0,inf)->[0,1) rolloff (no hard clip);
	// with hdr=false, skip Reinhard and clamp raw linear values instead.
	const float invGamma = 1.0f / TONEMAP_GAMMA;
	for (unsigned r = 0; r < height; r++)
		for (unsigned c = 0; c < width; c++) {
			Vector p = image[r][c] * scale;
			if (hdr) {
				p.x = p.x / (1.0f + p.x);
				p.y = p.y / (1.0f + p.y);
				p.z = p.z / (1.0f + p.z);
			}
			p.x = std::pow(std::max(p.x, 0.0f), invGamma);
			p.y = std::pow(std::max(p.y, 0.0f), invGamma);
			p.z = std::pow(std::max(p.z, 0.0f), invGamma);
			image[r][c] = p.clamp(0, 1);
		}
}
