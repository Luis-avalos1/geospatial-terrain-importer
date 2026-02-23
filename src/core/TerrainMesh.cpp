#include "TerrainMesh.hpp"
#include "GeoTiffReader.hpp"

#include <cmath>
#include <stdexcept>

MeshData TerrainMesh::build(const ElevationData &data,
                             int   sampleStep,
                             float heightScale)
{
    if (data.empty())
        throw std::invalid_argument("TerrainMesh::build: empty elevation data");
    if (sampleStep < 1) sampleStep = 1;

    // Sampled grid dimensions
    const int cols = (data.width  - 1) / sampleStep + 1;
    const int rows = (data.height - 1) / sampleStep + 1;

    MeshData mesh;
    mesh.cols = cols;
    mesh.rows = rows;
    mesh.vertices.reserve(static_cast<size_t>(cols) * rows);
    mesh.indices.reserve(static_cast<size_t>(cols - 1) * (rows - 1) * 6);

    // Geo-transform origin and pixel size (we use simple planar coords here)
    const double originX  = data.geoTransform[0];
    const double pixelW   = data.geoTransform[1];
    const double originY  = data.geoTransform[3];
    const double pixelH   = data.geoTransform[5];  // usually negative

    // Normalise UV so (0,0) is top-left, (1,1) is bottom-right
    const float uvScaleU = 1.0f / static_cast<float>(std::max(cols - 1, 1));
    const float uvScaleV = 1.0f / static_cast<float>(std::max(rows - 1, 1));

    // ── Vertices ─────────────────────────────────────────────────────────────
    for (int row = 0; row < rows; ++row) {
        const int srcRow = std::min(row * sampleStep, data.height - 1);
        for (int col = 0; col < cols; ++col) {
            const int srcCol = std::min(col * sampleStep, data.width - 1);

            const float h = data.heights[static_cast<size_t>(srcRow) * data.width + srcCol]
                            * heightScale;

            Vertex v{};
            v.x = static_cast<float>(originX + srcCol * pixelW);
            v.y = h;
            v.z = static_cast<float>(originY + srcRow * pixelH);
            // Normals are computed after indices are set
            v.nx = 0.0f; v.ny = 1.0f; v.nz = 0.0f;
            v.u  = static_cast<float>(col) * uvScaleU;
            v.v  = static_cast<float>(row) * uvScaleV;

            mesh.vertices.push_back(v);
        }
    }

    // ── Indices (two CCW triangles per quad) ──────────────────────────────────
    for (int row = 0; row < rows - 1; ++row) {
        for (int col = 0; col < cols - 1; ++col) {
            const uint32_t tl = static_cast<uint32_t>(row * cols + col);
            const uint32_t tr = tl + 1;
            const uint32_t bl = tl + static_cast<uint32_t>(cols);
            const uint32_t br = bl + 1;

            // Wind both triangles CCW as seen from +Y so the surface gets
            // upward-facing normals for north-up DEMs (the common case, where
            // the geo-transform row step gt[5] is negative). The original
            // winding produced downward normals, which made the lit surface
            // face away from the light and get back-face culled from above.
            // Upper-left triangle
            mesh.indices.push_back(tl);
            mesh.indices.push_back(tr);
            mesh.indices.push_back(bl);

            // Lower-right triangle
            mesh.indices.push_back(tr);
            mesh.indices.push_back(br);
            mesh.indices.push_back(bl);
        }
    }

    recomputeNormals(mesh);
    return mesh;
}

void TerrainMesh::recomputeNormals(MeshData &mesh)
{
    // Zero out existing normals
    for (auto &v : mesh.vertices) {
        v.nx = v.ny = v.nz = 0.0f;
    }

    // Accumulate face normals into each vertex
    const size_t triCount = mesh.indices.size() / 3;
    for (size_t i = 0; i < triCount; ++i) {
        const uint32_t i0 = mesh.indices[i * 3 + 0];
        const uint32_t i1 = mesh.indices[i * 3 + 1];
        const uint32_t i2 = mesh.indices[i * 3 + 2];

        const Vertex &v0 = mesh.vertices[i0];
        const Vertex &v1 = mesh.vertices[i1];
        const Vertex &v2 = mesh.vertices[i2];

        // Edge vectors
        const float e1x = v1.x - v0.x, e1y = v1.y - v0.y, e1z = v1.z - v0.z;
        const float e2x = v2.x - v0.x, e2y = v2.y - v0.y, e2z = v2.z - v0.z;

        // Cross product
        const float cx = e1y * e2z - e1z * e2y;
        const float cy = e1z * e2x - e1x * e2z;
        const float cz = e1x * e2y - e1y * e2x;

        // Skip degenerate faces
        const float len2 = cx * cx + cy * cy + cz * cz;
        if (len2 < 1e-16f) continue;

        // Accumulate (area-weighted via cross-product magnitude)
        for (uint32_t idx : {i0, i1, i2}) {
            mesh.vertices[idx].nx += cx;
            mesh.vertices[idx].ny += cy;
            mesh.vertices[idx].nz += cz;
        }
    }

    // Normalise
    for (auto &v : mesh.vertices) {
        const float len = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
        if (len > 1e-8f) {
            v.nx /= len;
            v.ny /= len;
            v.nz /= len;
        } else {
            // Fallback: flat upward normal
            v.nx = 0.0f; v.ny = 1.0f; v.nz = 0.0f;
        }
    }
}
