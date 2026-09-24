#include "BucketArray.h"

BucketArray::BucketArray(Scene* scene) : index(0)
{
    unsigned width = scene->getSettings().imageSettings.width;
    unsigned height = scene->getSettings().imageSettings.height;
    unsigned bucketSize = scene->getSettings().imageSettings.bucketSize;

    unsigned bucketsPerWidth = width / bucketSize;
    unsigned bucketsPerHeight = height / bucketSize;

    unsigned lastBucketWidth = bucketSize + (width % bucketSize);
    unsigned lastBucketHeight = bucketSize + (height % bucketSize); // to avoid leaving out pixels at the edges
    regions.reserve(bucketsPerWidth * bucketsPerHeight);
    for (unsigned i = 0; i < bucketsPerHeight; i++) {
        for (unsigned j = 0; j < bucketsPerWidth; j++) {
            Region reg{ int(j * bucketSize), int(i * bucketSize), int(bucketSize), int(bucketSize) };
            if (i == bucketsPerHeight - 1) {
                reg.height = lastBucketHeight;
            }
            if (j == bucketsPerWidth - 1) {
                reg.width = lastBucketWidth;
            }
            regions.push_back(reg);
        }
    }
}

Region BucketArray::getRegion(int idx) const
{
    return regions[idx];
}

bool BucketArray::nextRegion(Region& region)
{
    int localIndex = index.fetch_add(1);
    if (localIndex >= regions.size())
        return false;
    region = regions[localIndex];
    return true;
}

int BucketArray::size() const
{
    return regions.size();
}
