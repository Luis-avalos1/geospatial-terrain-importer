// test_helpers.hpp — small fixtures shared across the C++ unit tests.
#pragma once

#include "core/GeoTiffReader.hpp"       // ElevationData
#include "core/SatelliteImageLoader.hpp"  // ImageTile

#include <cstdint>

// Build a width×height elevation grid. When `ramp` is true the height at
// (row, col) is (row + col), producing a perfectly planar tilted surface;
// otherwise every cell is `base`. The geo-transform is a unit pixel grid with
// the origin at (0, 0) and the conventional negative north-up Y pixel size.
inline ElevationData makeElevation(int width, int height, bool ramp = true,
                                   float base = 0.0f)
{
    ElevationData d;
    d.width = width;
    d.height = height;
    d.heights.resize(static_cast<size_t>(width) * height);
    for (int r = 0; r < height; ++r)
        for (int c = 0; c < width; ++c)
            d.heights[static_cast<size_t>(r) * width + c] =
                ramp ? static_cast<float>(r + c) : base;
    d.geoTransform = {0.0, 1.0, 0.0, 0.0, 0.0, -1.0};
    d.projectionWkt = "";
    return d;
}

// Build a solid-colour RGBA tile.
inline ImageTile makeTile(int width, int height, uint8_t fill)
{
    ImageTile t;
    t.width = width;
    t.height = height;
    t.rgba.assign(static_cast<size_t>(width) * height * 4, fill);
    return t;
}
