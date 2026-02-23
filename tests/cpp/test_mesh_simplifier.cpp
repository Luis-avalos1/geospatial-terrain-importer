// Tests for the QEM mesh simplifier.
#include "test_framework.hpp"
#include "test_helpers.hpp"

#include "core/TerrainMesh.hpp"
#include "core/MeshSimplifier.hpp"

#include <cmath>

// Returns true if no triangle in the mesh is degenerate (repeated index or
// zero area).
static bool noDegenerateTriangles(const MeshData &m)
{
    const size_t tris = m.indices.size() / 3;
    for (size_t t = 0; t < tris; ++t) {
        const uint32_t i0 = m.indices[t * 3 + 0];
        const uint32_t i1 = m.indices[t * 3 + 1];
        const uint32_t i2 = m.indices[t * 3 + 2];
        if (i0 == i1 || i1 == i2 || i0 == i2) return false;
        const Vertex &a = m.vertices[i0];
        const Vertex &b = m.vertices[i1];
        const Vertex &c = m.vertices[i2];
        const float ex = b.x - a.x, ey = b.y - a.y, ez = b.z - a.z;
        const float fx = c.x - a.x, fy = c.y - a.y, fz = c.z - a.z;
        const float cx = ey * fz - ez * fy;
        const float cy = ez * fx - ex * fz;
        const float cz = ex * fy - ey * fx;
        if (cx * cx + cy * cy + cz * cz < 1e-16f) return false;
    }
    return true;
}

TEST_CASE("[simplifier] reduces the triangle count")
{
    MeshData m = TerrainMesh::build(makeElevation(16, 16));
    const size_t before = m.indices.size() / 3;

    MeshSimplifier::Options opts;
    opts.targetRatio = 0.5f;
    const float ratio = MeshSimplifier::simplify(m, opts);

    const size_t after = m.indices.size() / 3;
    CHECK(after < before);
    CHECK(after > 0u);
    CHECK(ratio > 0.0f);
    CHECK(ratio <= 1.0f);
}

TEST_CASE("[simplifier] returned ratio matches the resulting mesh")
{
    MeshData m = TerrainMesh::build(makeElevation(20, 12));
    const size_t before = m.indices.size() / 3;

    MeshSimplifier::Options opts;
    opts.targetRatio = 0.4f;
    const float ratio = MeshSimplifier::simplify(m, opts);

    const float actual = static_cast<float>(m.indices.size() / 3) /
                         static_cast<float>(before);
    CHECK_APPROX(ratio, actual, 1e-4);
}

TEST_CASE("[simplifier] output mesh stays valid")
{
    MeshData m = TerrainMesh::build(makeElevation(16, 16));
    MeshSimplifier::simplify(m);

    CHECK(!m.vertices.empty());
    CHECK(m.indices.size() % 3 == 0u);
    for (uint32_t i : m.indices)
        CHECK(i < m.vertices.size());
    CHECK(noDegenerateTriangles(m));
}

TEST_CASE("[simplifier] normals remain unit length after simplification")
{
    MeshData m = TerrainMesh::build(makeElevation(16, 16));
    MeshSimplifier::simplify(m);
    for (const Vertex &v : m.vertices) {
        const float len = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
        CHECK_APPROX(len, 1.0f, 1e-3);
    }
}

TEST_CASE("[simplifier] empty mesh is a no-op")
{
    MeshData empty;
    const float ratio = MeshSimplifier::simplify(empty);
    CHECK_APPROX(ratio, 1.0f, 1e-6);
    CHECK(empty.vertices.empty());
    CHECK(empty.indices.empty());
}
