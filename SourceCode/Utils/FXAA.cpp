#include "FXAA.h"
#include <iostream>

float FXAA::FXAALuminance(const Vector& color)
{
    return 0.299 * color.x + 0.587 * color.y + 0.114 * color.z;
}

float FXAA::FXAAColorContrast(const Vector& a, const Vector& b)
{
    return fabs(a.x - b.x) + fabs(a.y - b.y) + fabs(a.z - b.z);
}

Vector FXAA::FXAAPixel(Image& image, unsigned x, unsigned y)
{
    Vector rgbN = image[y - 1][x];
    Vector rgbW = image[y][x - 1];
    Vector rgbM = image[y][x];
    Vector rgbE = image[y][x + 1];
    Vector rgbS = image[y + 1][x];

    float lumaN = FXAALuminance(rgbN);
    float lumaW = FXAALuminance(rgbW);
    float lumaM = FXAALuminance(rgbM);
    float lumaE = FXAALuminance(rgbE);
    float lumaS = FXAALuminance(rgbS);

    float rangeMin = std::min(lumaM, std::min(std::min(lumaN, lumaW), std::min(lumaS, lumaE)));
    float rangeMax = std::max(lumaM, std::max(std::max(lumaN, lumaW), std::max(lumaS, lumaE)));
    float range = rangeMax - rangeMin;

    if (range < std::max(FXAA_EDGE_THRESHOLD_MIN, rangeMax * FXAA_EDGE_THRESHOLD)) {
        return rgbM; // no edge detected, return the original pixel color
    }

    float lumaL = (lumaN + lumaW + lumaE + lumaS) * 0.25f;
    float rangeL = fabs(lumaL - lumaM);
    float blendL = std::max(0.0f, (rangeL / range) - FXAA_SUBPIX_TRIM) * FXAA_SUBPIX_CAP;
    blendL = std::min(FXAA_SUBPIX_CAP, blendL * 1.5f);

    Vector rgbNW = image[y - 1][x - 1];
    Vector rgbNE = image[y - 1][x + 1];
    Vector rgbSW = image[y + 1][x - 1];
    Vector rgbSE = image[y + 1][x + 1];

    float lumaNW = FXAALuminance(rgbNW);
    float lumaNE = FXAALuminance(rgbNE);
    float lumaSW = FXAALuminance(rgbSW);
    float lumaSE = FXAALuminance(rgbSE);

    float edgeVert = fabs((0.25 * lumaNW) + (-0.5 * lumaN) + (0.25 * lumaNE)) +
        fabs((0.50 * lumaW) + (-1.0 * lumaM) + (0.50 * lumaE)) +
        fabs((0.25 * lumaSW) + (-0.5 * lumaS) + (0.25 * lumaSE));

    float edgeHorz = fabs((0.25 * lumaNW) + (-0.5 * lumaW) + (0.25 * lumaSW)) +
        fabs((0.50 * lumaN) + (-1.0 * lumaM) + (0.50 * lumaS)) +
        fabs((0.25 * lumaNE) + (-0.5 * lumaE) + (0.25 * lumaSE));

    bool horzSpan = edgeHorz >= edgeVert;

    std::pair<int, int> posN = { x, y };
    std::pair<int, int> posP = { x, y };
    std::pair<int, int> offNP = horzSpan ? std::make_pair(1, 0) : std::make_pair(0, 1);

    float gradientN = horzSpan ? edgeHorz : edgeVert;
    float lumaEndN = lumaM;
    float lumaEndP = lumaM;

    bool doneN = false;
    bool doneP = false;

    float distN = 0.0f;
    float distP = 0.0f;

    for (unsigned i = 0; i < FXAA_SEARCH_STEPS; i++) {
        if (!doneN) {
            lumaEndN = FXAALuminance(image[posN.second - offNP.second][posN.first - offNP.first]);
            doneN = (fabs(lumaEndN - lumaM) >= gradientN);
            if (!doneN) {
                posN.first -= offNP.first;
                posN.second -= offNP.second;
                distN++;
            }
        }
        if (!doneP) {
            lumaEndP = FXAALuminance(image[posP.second + offNP.second][posP.first + offNP.first]);
            doneP = (fabs(lumaEndP - lumaM) >= gradientN);
            if (!doneP) {
                posP.first += offNP.first;
                posP.second += offNP.second;
                distP++;
            }
        }
        if (doneN && doneP) break;
    }

    float finalBlend = std::min(1.0f, distN / (distN + distP));
    finalBlend *= blendL;

    Vector rgbL = rgbN + rgbW + rgbM + rgbE + rgbS;
    rgbL += (rgbNW + rgbNE + rgbSW + rgbSE);
    rgbL *= (1.0 / 9.0);

    // detects contrast changes that have similar illumination
    float colorContrast = FXAAColorContrast(rgbM, rgbL);
    float colorBlendFactor = std::min(1.0f, colorContrast * FXAA_SUBPIX_CAP);

    float combinedBlend = std::max(finalBlend, colorBlendFactor);

    return rgbM * (1.0f - combinedBlend) + rgbL * combinedBlend;
}

void FXAA::applyFXAA(Image & image)
{
    unsigned height, width;
    height = image.size();
    width = image[0].size();
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            image[y][x] = FXAAPixel(image, x, y);
        }
    }
}