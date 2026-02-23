#pragma once

#include <QOpenGLFunctions_4_1_Core>
#include <cstdint>

struct MeshData;

// One VAO/VBO/EBO triplet per LOD level.
// Upload once, draw many times.
class GpuMesh : protected QOpenGLFunctions_4_1_Core {
public:
    GpuMesh() = default;
    ~GpuMesh();

    GpuMesh(const GpuMesh &) = delete;
    GpuMesh &operator=(const GpuMesh &) = delete;

    // Upload vertex + index data to the GPU.
    // Vertex layout: pos(3f) normal(3f) uv(2f) → stride 32 bytes.
    void upload(const MeshData &mesh);

    // Issue the draw call (must have a bound shader program).
    void draw() const;

    bool isUploaded() const { return m_vao != 0; }
    uint32_t indexCount() const { return m_indexCount; }

    void destroy();

private:
    GLuint   m_vao        = 0;
    GLuint   m_vbo        = 0;
    GLuint   m_ebo        = 0;
    uint32_t m_indexCount = 0;
};
