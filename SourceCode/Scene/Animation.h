#pragma once
#include "Scene.h"

static constexpr int FRAMERATE = 24;
static constexpr int DURATION = 120;

static constexpr float ORBIT_PER_FRAME = 5;
static constexpr float FOV_PER_UNIT = 5;

class Animation
{
	Scene* scene;
	bool meshMovement = false;
	int currentKeyframe = 0;
	int framesCount;

	std::vector<Camera> cameraKeyframes;
	std::vector<Scene*> meshKeyframes;

	float heightAtDistance(float distance) const;
	float FOVatHeightAndDist(float height, float distance) const;

	std::vector<Camera> generateOrbitKeyframes(const Vector& anchor,
		int durationFrames = DURATION, float degreesPerFrame = ORBIT_PER_FRAME, bool snapToAxis = false);
	std::vector<Camera> generateVertigoKeyframes(const float movement, const float focusDistance, int durationFrames = DURATION);
public:
	Animation(Scene* scene);
	const Scene* getOriginScene() const;

	static Animation vertigoAnimation(Scene* scene, const float movement, 
		const float focusDistance, int durationFrames = DURATION);
	static Animation orbitAnimation(Scene* scene, const Vector& anchor,
		int durationFrames = DURATION, float degreesPerFrame = ORBIT_PER_FRAME, bool snapToAxis = false);

	bool hasNextKeyframe() const;
	int getCurrentFrameIndex() const;
	Scene* getNextKeyframe();

};

