// Tests for the shelf-packing TextureAtlas.
#include "test_framework.hpp"
#include "test_helpers.hpp"

#include "core/TextureAtlas.hpp"
#include "core/SatelliteImageLoader.hpp"

#include <stdexcept>
#include <vector>

TEST_CASE("[atlas] packs tiles and produces an RGBA buffer of the right size")
{
    const ImageTile a = makeTile(16, 16, 100);
    const ImageTile b = makeTile(32, 32, 150);
    const ImageTile c = makeTile(16, 16, 200);
    std::vector<const ImageTile *> tiles = {&a, &b, &c};

    TextureAtlas atlas;
    atlas.pack(tiles, 64);

    CHECK(!atlas.empty());
    CHECK(atlas.width() == 64);
    CHECK(atlas.height() == 64);
    CHECK(atlas.rgba().size() == static_cast<size_t>(64) * 64 * 4);
    CHECK(atlas.rects().size() == tiles.size());
}

TEST_CASE("[atlas] UV rects stay within the unit square and are non-empty")
{
    const ImageTile a = makeTile(16, 16, 100);
    const ImageTile b = makeTile(32, 24, 150);
    std::vector<const ImageTile *> tiles = {&a, &b};

    TextureAtlas atlas;
    atlas.pack(tiles, 128);

    for (const AtlasRect &r : atlas.rects()) {
        CHECK(r.u0 >= 0.0f);
        CHECK(r.v0 >= 0.0f);
        CHECK(r.u1 <= 1.0f);
        CHECK(r.v1 <= 1.0f);
        CHECK(r.u1 > r.u0);
        CHECK(r.v1 > r.v0);
    }
}

TEST_CASE("[atlas] single tile lands at the origin with correct UVs")
{
    const ImageTile t = makeTile(2, 2, 200);
    std::vector<const ImageTile *> tiles = {&t};

    TextureAtlas atlas;
    atlas.pack(tiles, 4);

    const AtlasRect &r = atlas.rects()[0];
    CHECK_APPROX(r.u0, 0.0f, 1e-6);
    CHECK_APPROX(r.v0, 0.0f, 1e-6);
    CHECK_APPROX(r.u1, 0.5f, 1e-6);   // 2 / 4
    CHECK_APPROX(r.v1, 0.5f, 1e-6);

    // The packed pixel (0,0) carries the tile colour; an unpacked pixel is 0.
    CHECK(atlas.rgba()[0] == 200);            // (0,0).r
    const size_t outside = (static_cast<size_t>(3) * 4 + 3) * 4;  // (3,3).r
    CHECK(atlas.rgba()[outside] == 0);
}

TEST_CASE("[atlas] empty tile list leaves the atlas empty")
{
    std::vector<const ImageTile *> tiles;
    TextureAtlas atlas;
    atlas.pack(tiles, 64);
    CHECK(atlas.empty());
}

TEST_CASE("[atlas] tile too large to fit is skipped with a zero rect")
{
    const ImageTile big = makeTile(16, 16, 255);
    std::vector<const ImageTile *> tiles = {&big};

    TextureAtlas atlas;
    atlas.pack(tiles, 8);   // atlas smaller than the tile

    const AtlasRect &r = atlas.rects()[0];
    CHECK_APPROX(r.u0, 0.0f, 1e-6);
    CHECK_APPROX(r.v0, 0.0f, 1e-6);
    CHECK_APPROX(r.u1, 0.0f, 1e-6);
    CHECK_APPROX(r.v1, 0.0f, 1e-6);
}

TEST_CASE("[atlas] non-positive atlas size throws")
{
    const ImageTile t = makeTile(4, 4, 1);
    std::vector<const ImageTile *> tiles = {&t};
    TextureAtlas atlas;
    CHECK_THROWS_AS(atlas.pack(tiles, 0), std::invalid_argument);
}
