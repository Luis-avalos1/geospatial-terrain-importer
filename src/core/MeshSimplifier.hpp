#pragma once

#include "TerrainMesh.hpp"

// Quadric Error Metrics (QEM) mesh simplifier — Garland & Heckbert 1997.
// Operates in-place on MeshData produced by TerrainMesh::build().
class MeshSimplifier {
public:
    struct Options {
        float targetRatio   = 0.5f;   // fraction of triangles to keep
        float boundaryWeight = 100.0f; // penalty multiplier for boundary edges
    };

    // Simplify mesh in-place. Returns the actual ratio achieved.
    static float simplify(MeshData &mesh, Options opts);

    // Convenience overload using default options. (A brace-defaulted argument
    // `Options opts = {}` can't be used here: Options is a nested type whose
    // default member initializers aren't available inside the enclosing class
    // definition, so both GCC and Clang reject it.)
    static float simplify(MeshData &mesh) { return simplify(mesh, Options{}); }

private:
    MeshSimplifier() = delete;
};
