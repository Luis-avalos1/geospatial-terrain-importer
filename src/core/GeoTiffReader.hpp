#pragma once

#include <string>
#include <vector>
#include <array>
#include <stdexcept>

struct ElevationData {
    std::vector<float> heights;   // row-major, width * height elements
    int width  = 0;
    int height = 0;
    // Affine geo-transform: X = gt[0] + col*gt[1] + row*gt[2]
    //                       Y = gt[3] + col*gt[4] + row*gt[5]
    std::array<double, 6> geoTransform{};
    std::string projectionWkt;    // SRS as WKT string

    bool empty() const { return heights.empty(); }
};

class GeoTiffReader {
public:
    // Read elevation from a GeoTIFF (or any GDAL-supported raster).
    // targetWidth/targetHeight == 0 → use native resolution.
    // Band index is 1-based (GDAL convention).
    static ElevationData read(const std::string &path,
                              int targetWidth  = 0,
                              int targetHeight = 0,
                              int bandIndex    = 1);

private:
    GeoTiffReader() = delete;
};
