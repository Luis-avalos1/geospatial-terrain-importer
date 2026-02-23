#include "TerrainRenderer.hpp"

#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

TerrainRenderer::TerrainRenderer(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    m_fpsTimer.start();
}

TerrainRenderer::~TerrainRenderer()
{
    makeCurrent();
    if (m_atlasTex) glDeleteTextures(1, &m_atlasTex);
    m_gpuMeshes.clear();
    doneCurrent();
}

// ── Public API ────────────────────────────────────────────────────────────────

void TerrainRenderer::loadMesh(LodManager *lodMgr)
{
    m_lodMgr = lodMgr;
    m_pendingUpload = true;
    update();
}

void TerrainRenderer::loadAtlasTexture(const TextureAtlas &atlas)
{
    if (atlas.empty()) return;
    makeCurrent();

    if (m_atlasTex) glDeleteTextures(1, &m_atlasTex);
    glGenTextures(1, &m_atlasTex);
    glBindTexture(GL_TEXTURE_2D, m_atlasTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 atlas.width(), atlas.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, atlas.rgba().data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (!atlas.rects().empty()) {
        const auto &r = atlas.rects()[0];
        m_atlasU0 = r.u0; m_atlasV0 = r.v0;
        m_atlasU1 = r.u1; m_atlasV1 = r.v1;
    }

    m_hasAtlas = true;
    doneCurrent();
    update();
}

void TerrainRenderer::clearTerrain()
{
    makeCurrent();
    m_gpuMeshes.clear();
    if (m_atlasTex) {
        glDeleteTextures(1, &m_atlasTex);
        m_atlasTex = 0;
    }
    m_hasAtlas   = false;
    m_lodMgr     = nullptr;
    doneCurrent();
    update();
}

void TerrainRenderer::setWireframe(bool on)  { m_wireframe   = on;  update(); }
void TerrainRenderer::setShowNormals(bool on) { m_showNormals = on;  update(); }
void TerrainRenderer::setHeightScale(float s) { m_heightScale = s;   update(); }

// ── GL lifecycle ──────────────────────────────────────────────────────────────

void TerrainRenderer::initializeGL()
{
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.12f, 0.13f, 0.15f, 1.0f);

    m_shader.load(":/shaders/terrain.vert", ":/shaders/terrain.frag");

    m_camera.reset({0.f, 0.f, 0.f}, 2000.f);
}

void TerrainRenderer::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void TerrainRenderer::paintGL()
{
    // Lazy GPU upload when triggered from loadMesh()
    if (m_pendingUpload && m_lodMgr) {
        uploadLodLevels();
        m_pendingUpload = false;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_gpuMeshes.empty() || !m_lodMgr) {
        // Nothing to draw yet
        ++m_frameCount;
        return;
    }

    m_shader.bind();

    // Matrices
    const glm::mat4 model(1.0f);
    const glm::mat4 view  = m_camera.viewMatrix();
    const glm::mat4 proj  = m_camera.projectionMatrix(width(), height());

    m_shader.setUniform("uModel",      model);
    m_shader.setUniform("uView",       view);
    m_shader.setUniform("uProjection", proj);
    m_shader.setUniform("uHeightMin",  m_heightMin);
    m_shader.setUniform("uHeightMax",  m_heightMax);
    m_shader.setUniform("uCameraPos",  m_camera.position());
    m_shader.setUniform("uLightDir",   glm::normalize(glm::vec3(0.6f, 1.0f, 0.4f)));
    m_shader.setUniform("uWireframe",  m_wireframe   ? 1.0f : 0.0f);
    m_shader.setUniform("uShowNormals",m_showNormals ? 1.0f : 0.0f);

    // Texture
    if (m_hasAtlas) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_atlasTex);
        m_shader.setUniform("uAtlasTex",  0);
        m_shader.setUniform("uHasAtlas",  1);
        m_shader.setUniform("uAtlasRect",
            glm::vec4(m_atlasU0, m_atlasV0, m_atlasU1, m_atlasV1));
    } else {
        m_shader.setUniform("uHasAtlas", 0);
        m_shader.setUniform("uAtlasRect", glm::vec4(0,0,1,1));
    }

    // LOD selection
    const float dist = m_camera.distanceTo(glm::vec3(0.f));
    const LodLevel &lod = m_lodMgr->selectLod(dist);

    // Find the GpuMesh matching the selected LOD level
    const auto &lodLevels = m_lodMgr->levels();
    for (size_t i = 0; i < lodLevels.size(); ++i) {
        if (&lodLevels[i] == &lod && i < m_gpuMeshes.size()) {
            if (m_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            m_gpuMeshes[i]->draw();
            if (m_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            break;
        }
    }

    m_shader.release();

    // FPS tracking
    ++m_frameCount;
    const qint64 elapsed = m_fpsTimer.elapsed();
    if (elapsed >= 500) {
        const double fps = static_cast<double>(m_frameCount) / (elapsed * 0.001);
        emit fpsUpdated(fps);
        m_frameCount = 0;
        m_fpsTimer.restart();
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void TerrainRenderer::uploadLodLevels()
{
    m_gpuMeshes.clear();
    if (!m_lodMgr) return;

    m_heightMin = std::numeric_limits<float>::max();
    m_heightMax = std::numeric_limits<float>::lowest();

    for (const LodLevel &lod : m_lodMgr->levels()) {
        for (const Vertex &v : lod.mesh.vertices) {
            m_heightMin = std::min(m_heightMin, v.y);
            m_heightMax = std::max(m_heightMax, v.y);
        }

        auto gm = std::make_unique<GpuMesh>();
        gm->upload(lod.mesh);
        m_gpuMeshes.push_back(std::move(gm));
    }

    // Recentre camera on terrain
    const float cx = 0.f;
    const float cy = (m_heightMin + m_heightMax) * 0.5f;
    const float cz = 0.f;
    m_camera.reset({cx, cy, cz}, m_heightMax * 3.0f + 1000.f);
}

// ── Mouse input ───────────────────────────────────────────────────────────────

void TerrainRenderer::mousePressEvent(QMouseEvent *e)
{
    m_lastMousePos = e->pos();
    m_leftDrag  = (e->button() == Qt::LeftButton);
    m_rightDrag = (e->button() == Qt::RightButton);
}

void TerrainRenderer::mouseMoveEvent(QMouseEvent *e)
{
    const QPoint delta = e->pos() - m_lastMousePos;
    m_lastMousePos = e->pos();

    if (m_leftDrag)
        m_camera.orbit(static_cast<float>(delta.x()), static_cast<float>(-delta.y()));
    else if (m_rightDrag)
        m_camera.pan(static_cast<float>(-delta.x()), static_cast<float>(delta.y()));

    update();
}

void TerrainRenderer::mouseReleaseEvent(QMouseEvent *)
{
    m_leftDrag  = false;
    m_rightDrag = false;
}

void TerrainRenderer::wheelEvent(QWheelEvent *e)
{
    const float delta = static_cast<float>(e->angleDelta().y()) / 120.0f;
    m_camera.zoom(delta);
    update();
}
