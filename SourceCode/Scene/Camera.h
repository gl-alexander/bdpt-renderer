#pragma once
#include <vector>
#include "Mesh.h"
#include "../Utils/Vector.h"
#include "../Utils/Matrix.h"
#include "../Utils/Ray.h"

static const float DEFAULT_FOV = 90;

class Camera
{
	Vector position;
	Vector frontDirection, rightDirection, upDirection;
	Matrix rotation;
	Mesh imagePlane;
	
	unsigned imageWidth;
	unsigned imageHeight;
	float FOV;
	float tanFOV;

	Ray getRayForSubpixel(float rowId, float colId, const Vector& position) const;
public:
	Camera();
	Camera(const Vector& position, float FOV = DEFAULT_FOV);
	Camera(const Vector& position, const Matrix& rotation, float FOV = DEFAULT_FOV);

	const Vector& getPosition() const;

	const Vector& getFrontDirection() const;
	const Vector& getUpDirection() const;
	const Vector& getRightDirection() const;

	const Matrix& getRotation() const;

	void setPosition(const Vector& position);
	void setRotation(const Matrix& matrix);
	void setImageSettings(unsigned width, unsigned height);

	Ray getRayForPixel(unsigned rowId, unsigned colId) const;
	Ray getRayForSubpixel(float rowId, float colId) const;
	std::pair<int, int> getRayHitpoint(const Ray& ray) const;

	std::pair<Ray, Ray> getEyeRays(float rowId, float colId, float eyeDistance) const;

	void moveCamera(const Vector& position);
	void dolly(float distance);
	void truck(float distance);
	void pedestal(float distance);

	void pan(float degrees);
	void tilt(float degrees);
	void roll(float degrees);

	void setFOV(float FOV);
	float getFOV() const;
	float getFOVtan() const;

	unsigned getWidth() const;
	unsigned getHeight() const;

	void updateDirections();
};

