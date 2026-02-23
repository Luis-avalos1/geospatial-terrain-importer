// Tests for the LOD pyramid builder + distance-based selection.
#include "test_framework.hpp"
#include "test_helpers.hpp"

#include "core/LodManager.hpp"

#include <stdexcept>

TEST_CASE("[lod] builds the requested number of levels")
{
    LodManager mgr;
    LodManager::Options opts;
    opts.levels = 4;
    mgr.build(makeElevation(32, 32), opts);

    CHECK(!mgr.empty());
    CHECK(mgr.levels().size() == 4u);
}

TEST_CASE("[lod] sample step doubles each level")
{
    LodManager mgr;
    LodManager::Options opts;
    opts.levels = 4;
    mgr.build(makeElevation(32, 32), opts);

    CHECK(mgr.levels()[0].sampleStep == 1);
    CHECK(mgr.levels()[1].sampleStep == 2);
    CHECK(mgr.levels()[2].sampleStep == 4);
    CHECK(mgr.levels()[3].sampleStep == 8);
}

TEST_CASE("[lod] switch distances follow the heuristic")
{
    LodManager mgr;
    LodManager::Options opts;
    opts.levels = 4;
    mgr.build(makeElevation(32, 32), opts);

    CHECK_APPROX(mgr.levels()[0].switchDistance, 0.0f, 1e-3);
    CHECK_APPROX(mgr.levels()[1].switchDistance, 500.0f, 1e-3);
    CHECK_APPROX(mgr.levels()[2].switchDistance, 1000.0f, 1e-3);
    CHECK_APPROX(mgr.levels()[3].switchDistance, 2000.0f, 1e-3);
}

TEST_CASE("[lod] selectLod picks finer detail up close, coarser far away")
{
    LodManager mgr;
    LodManager::Options opts;
    opts.levels = 4;
    mgr.build(makeElevation(32, 32), opts);

    CHECK(mgr.selectLod(0.0f).sampleStep == 1);      // closest -> finest
    CHECK(mgr.selectLod(100.0f).sampleStep == 1);
    CHECK(mgr.selectLod(600.0f).sampleStep == 2);    // crossed 500
    CHECK(mgr.selectLod(1500.0f).sampleStep == 4);   // crossed 1000
    CHECK(mgr.selectLod(9000.0f).sampleStep == 8);   // beyond 2000 -> coarsest
}

TEST_CASE("[lod] levels count is clamped to at least one")
{
    LodManager mgr;
    LodManager::Options opts;
    opts.levels = 0;
    mgr.build(makeElevation(8, 8), opts);
    CHECK(mgr.levels().size() == 1u);
}

TEST_CASE("[lod] empty elevation data throws")
{
    LodManager mgr;
    ElevationData empty;
    CHECK_THROWS_AS(mgr.build(empty), std::invalid_argument);
}

TEST_CASE("[lod] selecting on an unbuilt manager throws")
{
    LodManager mgr;
    CHECK_THROWS_AS(mgr.selectLod(10.0f), std::runtime_error);
}

TEST_CASE("[lod] clear empties the pyramid")
{
    LodManager mgr;
    mgr.build(makeElevation(8, 8));
    CHECK(!mgr.empty());
    mgr.clear();
    CHECK(mgr.empty());
}
