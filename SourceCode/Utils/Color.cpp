#include "Color.h"

Color::Color(short r, short g, short b) : r(r), g(g), b(b) {}
Color::Color(const Vector& vec) {
	r = (short)vec.x;
	g = (short)vec.y;
	b = (short)vec.z;
	cap();
}
Color::Color() : Color(0, 0, 0) {}

Color& Color::operator+=(const Color& other) {
	r += other.r;
	g += other.g;
	b += other.b;
	cap();
	return *this;
}

Color& Color::operator*=(float m) {
	r *= m;
	g *= m;
	b *= m;
	cap();
	return *this;
}

void Color::cap() {
	if (r > MAX_COLOR_COMPONENT) r = MAX_COLOR_COMPONENT;
	if (g > MAX_COLOR_COMPONENT) g = MAX_COLOR_COMPONENT;
	if (b > MAX_COLOR_COMPONENT) b = MAX_COLOR_COMPONENT;
}

Color operator+(const Color& lhs, const Color& rhs) {
	Color cpy = lhs;
	cpy += rhs;
	return cpy;
}

Color operator*(const Color& color, float m) {
	Color result = color;
	result *= m;
	return result;
}

Color operator*(float m, const Color& color) {
	return color * m;
}

