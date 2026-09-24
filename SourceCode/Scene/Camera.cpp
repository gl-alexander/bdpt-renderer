#include "Camera.h"
#include <cmath>
#include <vector>

static const Vector FRONT(0, 0, -1);
static const Vector UP(0, 1, 0);
static const Vector RIGHT(1, 0, 0);
static const float DEFAULT_ROTATION[MATRIX_ROWS][MATRIX_COLUMNS] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
static const float DEFAULT_POSITION[MATRIX_ROWS] = { 0.0f, 0.0f, 0.0f };


Camera::Camera() : Camera(DEFAULT_POSITION, DEFAULT_ROTATION)
{}

Camera::Camera(const Vector& position, float FOV) : Camera(position, DEFAULT_ROTATION, FOV)
{}

Camera::Camera(const Vector& position, const Matrix& rotation, float FOV) : position(position), rotation(rotation), FOV(FOV)
{
	tanFOV = tan((FOV / 2) * PI / 180.0);
}

const Vector& Camera::getPosition() const {
	return position;
}

const Vector& Camera::getFrontDirection() const
{
	return frontDirection;
}

const Vector& Camera::getUpDirection() const
{
	return upDirection;
}

const Vector& Camera::getRightDirection() const
{
	return rightDirection;
}

const Matrix& Camera::getRotation() const {
	return rotation;
}

void Camera::setPosition(const Vector& position)
{
	this->position = position;
}

void Camera::setRotation(const Matrix& matrix)
{
	this->rotation = matrix;
}

void Camera::setImageSettings(unsigned width, unsigned height)
{
	imageWidth = width;
	imageHeight = height;
}

Ray Camera::getRayForSubpixel(float rowId, float colId, const Vector& position) const
{
	float x_coordinate = colId;
	float y_coordinate = rowId;
	float z_coordinate = 0;		// relative to the screen, its z is 0

	// Convert to NDC space
	x_coordinate /= imageWidth;
	y_coordinate /= imageHeight;

	// Convert NDC coordiantes to Screen space 
	x_coordinate = (2.0 * x_coordinate) - 1.0;
	y_coordinate = 1.0 - (2.0 * y_coordinate);

	// Consider the aspect ratio
	x_coordinate *= (float)imageWidth / (float)imageHeight;

	// Account for FOV
	x_coordinate *= tanFOV;
	y_coordinate *= tanFOV;

	// Convert to World space coordinates based on screen position
	Vector direction(x_coordinate, y_coordinate, z_coordinate);
	direction += FRONT; // since vec.-matrix multiplication is distributive, so we can add the offset to the pixel direction
	// and rotate it afterwards
	direction = direction * this->rotation;
	direction.normalize();

	return {position, direction, RayType::CAMERA, 0 };
}

void Camera::updateDirections()
{
	frontDirection = FRONT * rotation;
	upDirection = UP * rotation;
	rightDirection = RIGHT * rotation;

	Vector lowerLeftCorner = getRayForPixel(imageHeight - 1, 0).direction + position;
	Vector lowerRightCorner = getRayForPixel(imageHeight - 1, imageWidth - 1).direction + position;
	Vector upperRightCorner = getRayForPixel(0, imageWidth - 1).direction + position;
	Vector upperLeftCorner = getRayForPixel(0, 0).direction + position;

	imagePlane = Mesh(
		std::vector<Vector>({ lowerLeftCorner, lowerRightCorner, upperLeftCorner, upperRightCorner }),
		std::vector<Vector>({ Vector(0, 0, 0), Vector(1, 0, 0), Vector(0, 1, 0), Vector(1, 1, 0) }),
		std::vector<int>({ 0,1,2,1,3,2 }),
		0);
}

Ray Camera::getRayForPixel(unsigned rowId, unsigned colId) const {
	return getRayForSubpixel(rowId, colId); // the default ray per pixel gives us a pixel shooting at the upper left corner of the pixel
}

Ray Camera::getRayForSubpixel(float rowId, float colId) const
{
	return getRayForSubpixel(rowId, colId, this->position);
}

std::pair<int, int> Camera::getRayHitpoint(const Ray& ray) const
{
	Intersection data = imagePlane.intersectsRay(ray);
	if (data.triangleIndex == NO_HIT_INDEX) {
		return { -1, -1 };
	}
	Vector uvCoords = imagePlane.getUV(data);
	int pixelX = uvCoords.x * imageWidth;
	int pixelY = (1 - uvCoords.y) * imageHeight;
	return { pixelX, pixelY };
}

std::pair<Ray, Ray> Camera::getEyeRays(float rowId, float colId, float eyeDistance) const
{
	float halfdistance = eyeDistance / 2;
	Vector leftEyePos = this->position - rightDirection * halfdistance;
	Vector rightEyePos = this->position + rightDirection * halfdistance;
	return { getRayForSubpixel(rowId, colId, leftEyePos), getRayForSubpixel(rowId, colId, rightEyePos) };
}


void Camera::moveCamera(const Vector& position) {
	Vector adjustedPosition = position * rotation;
	this->position += adjustedPosition;
}

void Camera::dolly(float distance) {
	// on the Z axis
	Vector toMove(0, 0, -distance);
	moveCamera(toMove);
}

void Camera::truck(float distance) {
	// on the X axis
	Vector toMove(distance, 0, 0);
	moveCamera(toMove);
}

void Camera::pedestal(float distance) {
	// on the Y axis
	Vector toMove(0, distance, 0);
	moveCamera(toMove);
}

void Camera::pan(float degrees) {
	// rotate counterclockwise on the Y axis
	Matrix rotMatrix = yRotationMatrix(degrees);
	rotation = rotMatrix * rotation;
	updateDirections();
}

void Camera::tilt(float degrees) {
	// counterclockwise on the X axis
	Matrix rotMatrix = xRotationMatrix(degrees);
	rotation = rotMatrix * rotation;
	updateDirections();
}

void Camera::roll(float degrees) {
	// counterclockwise on the Z axis
	Matrix rotMatrix = zRotationMatrix(degrees);
	rotation = rotMatrix * rotation;
	updateDirections();
}

void Camera::setFOV(float FOV)
{
	this->FOV = FOV;
	this->tanFOV = tan((FOV / 2) * PI / 180.0);
}

float Camera::getFOV() const
{
	return FOV;
}

float Camera::getFOVtan() const
{
	return tanFOV;
}

unsigned Camera::getWidth() const
{
	return imageWidth;
}

unsigned Camera::getHeight() const
{
	return imageHeight;
}


