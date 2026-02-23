// Tests for TerrainMesh::build / recomputeNormals.
#include "test_framework.hpp"
#include "test_helpers.hpp"

#include "core/TerrainMesh.hpp"

#include <cmath>

TEST_CASE("[mesh] grid dimensions and buffer sizes")
{
    const ElevationData d = makeElevation(4, 3);
    const MeshData m = TerrainMesh::build(d);

    CHECK(m.cols == 4);
    CHECK(m.rows == 3);
    CHECK(m.vertices.size() == 12u);          // cols * rows
    CHECK(m.indices.size() == 36u);           // (cols-1)*(rows-1)*6
    CHECK(m.indices.size() % 3 == 0u);
}

TEST_CASE("[mesh] every index is in range")
{
    const ElevationData d = makeElevation(8, 5);
    const MeshData m = TerrainMesh::build(d);
    for (uint32_t i : m.indices)
        CHECK(i < m.vertices.size());
}

TEST_CASE("[mesh] UV coordinates span the unit square")
{
    const ElevationData d = makeElevation(4, 3);
    const MeshData m = TerrainMesh::build(d);

    // First vertex is top-left (0,0), last is bottom-right (1,1).
    CHECK_APPROX(m.vertices.front().u, 0.0f, 1e-6);
    CHECK_APPROX(m.vertices.front().v, 0.0f, 1e-6);
    CHECK_APPROX(m.vertices.back().u, 1.0f, 1e-6);
    CHECK_APPROX(m.vertices.back().v, 1.0f, 1e-6);
}

TEST_CASE("[mesh] positions follow the geo-transform")
{
    const ElevationData d = makeElevation(4, 3);  // gt = {0,1,0, 0,0,-1}
    const MeshData m = TerrainMesh::build(d);

    // Vertex (row=0,col=0): x = 0, z = 0
    CHECK_APPROX(m.vertices[0].x, 0.0f, 1e-6);
    CHECK_APPROX(m.vertices[0].z, 0.0f, 1e-6);
    // Vertex (row=1,col=2) is index 1*4 + 2 = 6: x = 2, z = -1
    CHECK_APPROX(m.vertices[6].x, 2.0f, 1e-6);
    CHECK_APPROX(m.vertices[6].z, -1.0f, 1e-6);
}

TEST_CASE("[mesh] heightScale multiplies elevation")
{
    const ElevationData d = makeElevation(4, 4);
    const MeshData a = TerrainMesh::build(d, 1, 1.0f);
    const MeshData b = TerrainMesh::build(d, 1, 3.0f);
    for (size_t i = 0; i < a.vertices.size(); ++i)
        CHECK_APPROX(b.vertices[i].y, a.vertices[i].y * 3.0f, 1e-4);
}

TEST_CASE("[mesh] sampleStep decimates the grid")
{
    const ElevationData d = makeElevation(5, 5);
    const MeshData m = TerrainMesh::build(d, 2);
    // cols = (5-1)/2 + 1 = 3
    CHECK(m.cols == 3);
    CHECK(m.rows == 3);
    CHECK(m.vertices.size() == 9u);
}

TEST_CASE("[mesh] normals are unit length")
{
    const ElevationData d = makeElevation(6, 6);
    const MeshData m = TerrainMesh::build(d);
    for (const Vertex &v : m.vertices) {
        const float len = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
        CHECK_APPROX(len, 1.0f, 1e-3);
    }
}

TEST_CASE("[mesh] flat terrain has upward normals")
{
    const ElevationData d = makeElevation(5, 5, /*ramp=*/false, /*base=*/7.0f);
    const MeshData m = TerrainMesh::build(d);
    for (const Vertex &v : m.vertices) {
        CHECK_APPROX(v.nx, 0.0f, 1e-4);
        CHECK_APPROX(v.ny, 1.0f, 1e-4);
        CHECK_APPROX(v.nz, 0.0f, 1e-4);
    }
}

TEST_CASE("[mesh] empty elevation data throws")
{
    ElevationData empty;
    CHECK_THROWS_AS(TerrainMesh::build(empty), std::invalid_argument);
}
