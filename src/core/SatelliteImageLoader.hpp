#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct ImageTile {
    std::vector<uint8_t> rgba;  // interleaved RGBA, width*height*4 bytes
    int width  = 0;
    int height = 0;

    bool empty() const { return rgba.empty(); }
};

class SatelliteImageLoader {
public:
    // Read a raster image as RGBA.
    // targetWidth/targetHeight == 0 → native resolution.
    static ImageTile load(const std::string &path,
                          int targetWidth  = 0,
                          int targetHeight = 0);

private:
    SatelliteImageLoader() = delete;
};
