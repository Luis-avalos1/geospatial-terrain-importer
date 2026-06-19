#include "GpuMesh.hpp"
#include "core/TerrainMesh.hpp"

#include <stdexcept>

GpuMesh::~GpuMesh() { destroy(); }

void GpuMesh::destroy()
{
    if (m_vao) {
        initializeOpenGLFunctions();
        glDeleteVertexArrays(1, &m_vao);
        glDeleteBuffers(1, &m_vbo);
        glDeleteBuffers(1, &m_ebo);
        m_vao = m_vbo = m_ebo = 0;
        m_indexCount = 0;
    }
}

void GpuMesh::upload(const MeshData &mesh)
{
    if (mesh.vertices.empty() || mesh.indices.empty())
        throw std::invalid_argument("GpuMesh::upload: empty mesh data");

    initializeOpenGLFunctions();
    destroy();  // release any previous buffers

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    // ── VBO ───────────────────────────────────────────────────────────────────
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex)),
                 mesh.vertices.data(),
                 GL_STATIC_DRAW);

    constexpr GLsizei stride = sizeof(Vertex);  // 32 bytes

    // location 0: position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void *>(offsetof(Vertex, x)));

    // location 1: normal (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void *>(offsetof(Vertex, nx)));

    // location 2: uv (vec2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void *>(offsetof(Vertex, u)));

    // ── EBO ───────────────────────────────────────────────────────────────────
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(uint32_t)),
                 mesh.indices.data(),
                 GL_STATIC_DRAW);

    glBindVertexArray(0);

    m_indexCount = static_cast<uint32_t>(mesh.indices.size());
}

void GpuMesh::draw()
{
    if (!m_vao) return;
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indexCount), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
