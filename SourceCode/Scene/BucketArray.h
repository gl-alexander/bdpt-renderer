#pragma once
#include <vector>
#include "Region.h"
#include "Scene.h"
#include <atomic>

class BucketArray
{
	std::vector<Region> regions;
	std::atomic_int index;
public:
	BucketArray(Scene* scene);
	Region getRegion(int idx) const;
	bool nextRegion(Region& region);

	int size() const;
};

