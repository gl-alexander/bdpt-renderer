#pragma once
#include <iostream>
#include <random>
#include <thread>
#include <atomic>
#include <cstdint>

constexpr float EPSILON = 0.00001;
constexpr float PI = 3.1415;

inline std::mt19937& rng() {
	static std::atomic<uint32_t> counter{0};
	thread_local std::mt19937 gen([] {
		std::seed_seq seq{ std::random_device{}(), counter.fetch_add(1) };
		return std::mt19937(seq);
	}());
	return gen;
}

// generates a random float between 0 and 1
inline float randFloat() {
	return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng());
}

inline int randomInt(int start, int end) {
	return std::uniform_int_distribution<int>(start, end - 1)(rng());
}

// clams the value between start and end
inline float clampValue(float original, float start, float end) {
	if (original < start) return start;
	if (original > end) return end;
	return original;
}

inline void randomCircleSample(float& x, float& y) {
	do {
		x = randFloat();
		y = randFloat();
	} while (x * x + y * y > 1);
}
