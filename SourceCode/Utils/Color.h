#pragma once
#include <iostream>
#include "Vector.h"
const short MAX_COLOR_COMPONENT = 255;

struct Color {
	short r;
	short g;
	short b;
	Color(short r, short g, short b);
	Color(const Vector& vec);
	Color();

	Color& operator+=(const Color& other);

	Color& operator*=(float m);
private:
	void cap();
};

Color operator+(const Color& lhs, const Color& rhs);

Color operator*(const Color& color, float m);

Color operator*(float m, const Color& color);

