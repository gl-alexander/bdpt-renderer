#pragma once
#include "Vector.h"
#include "Ray.h"
#include <limits>

static constexpr float FLOAT_MAX = std::numeric_limits<float>::max();
static constexpr float FLOAT_MIN = -FLOAT_MAX;

struct Box {
	Vector min;
	Vector max;

	bool intersects(const Ray& ray) const;
	void split(Box& first, Box& second, AxisLabel label) const;
};

