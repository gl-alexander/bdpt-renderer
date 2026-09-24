#pragma once
#include "../../Utils/Vector.h"

class EdgeTexture {
    Vector edgeColor;
    Vector innerColor;
    float edgeWidth;

public:
    EdgeTexture( const Vector& edgeColor, const Vector& innerColor, float edgeWidth);

    Vector sample(float u, float v, const Vector& bary) const;
};
