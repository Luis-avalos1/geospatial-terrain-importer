#pragma once

#include <vector>
#include <array>
#include <cstdint>

struct ElevationData;

// Per-vertex layout — matches the VBO stride expected by GpuMesh (32 bytes).
struct Vertex {
    float x, y, z;       // position
    float nx, ny, nz;    // normal
    float u, v;          // texture coords
};
static_assert(sizeof(Vertex) == 32, "Vertex must be 32 bytes for GPU stride");

struct MeshData {
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
    int cols = 0;  // grid columns (original, before any simplification)
    int rows = 0;
};

class TerrainMesh {
public:
    // Build a grid mesh from elevation data.
    // sampleStep > 1 decimates the grid (useful for LOD without simplification).
    // heightScale multiplies raw elevation values.
    static MeshData build(const ElevationData &data,
                          int   sampleStep  = 1,
                          float heightScale = 1.0f);

    // Recompute per-vertex normals via cross-product accumulation.
    static void recomputeNormals(MeshData &mesh);

private:
    TerrainMesh() = delete;
};
