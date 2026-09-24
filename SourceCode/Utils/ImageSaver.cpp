#include "ImageSaver.h"

std::ofstream& operator<<(std::ofstream& ppm, const Color& pixel) {
    // defined in an operator for ease of use
    // we save control over what to print before and after each pixel
    ppm << (int)pixel.r << " " << (int)pixel.g << " " << (int)pixel.b;
    return ppm;
}

void ImageSaver::saveImage(const char* imgName, const Image& image, bool FXAA) {
    std::ofstream ppmFileStream(imgName, std::ios::out | std::ios::trunc);
    ppmFileStream << "P3" << LINE_SEP;
    unsigned imageHeight = image.size();
    unsigned imageWidth = image[0].size();
    ppmFileStream << imageWidth << " " << imageHeight << LINE_SEP;
    ppmFileStream << MAX_COLOR_COMPONENT << LINE_SEP;

    saveImage(ppmFileStream, image, FXAA);

    ppmFileStream.close();
}

void ImageSaver::saveImage(std::ofstream& ppm, const Image& image, bool FXAA) {
    unsigned imageHeight = image.size();
    unsigned imageWidth = image[0].size();
    for (int rowId = 0; rowId < imageHeight; rowId++) {
        for (int colId = 0; colId < imageWidth; colId++) {
            Vector finalColor = image[rowId][colId] * MAX_COLOR_COMPONENT;
            ppm << finalColor;

            if (colId != imageWidth) {
                ppm << PIXEL_SEP;
            }
        }
        if (rowId < imageHeight) { // so we don't save extra lines after the end of the image
            ppm << LINE_SEP;
        }
    }
}