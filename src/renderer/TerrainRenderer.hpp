#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_4_1_Core>
#include <QElapsedTimer>
#include <QPoint>

#include <memory>
#include <vector>

#include "Camera.hpp"
#include "ShaderProgram.hpp"
#include "GpuMesh.hpp"
#include "core/LodManager.hpp"
#include "core/TextureAtlas.hpp"

class TerrainRenderer : public QOpenGLWidget, protected QOpenGLFunctions_4_1_Core {
    Q_OBJECT
public:
    explicit TerrainRenderer(QWidget *parent = nullptr);
    ~TerrainRenderer() override;

    // Called from the main thread after async load completes.
    void loadMesh(LodManager *lodMgr);
    void loadAtlasTexture(const TextureAtlas &atlas);
    void clearTerrain();

    // GUI toggles
    void setWireframe(bool on);
    void setShowNormals(bool on);
    void setHeightScale(float s);

    Camera &camera() { return m_camera; }

signals:
    void fpsUpdated(double fps);

protected:
    void initializeGL()              override;
    void resizeGL(int w, int h)      override;
    void paintGL()                   override;

    void mousePressEvent(QMouseEvent *e)   override;
    void mouseMoveEvent(QMouseEvent *e)    override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e)        override;

private:
    void uploadLodLevels();

    Camera        m_camera;
    ShaderProgram m_shader;

    // One GpuMesh per LOD level, managed here
    std::vector<std::unique_ptr<GpuMesh>> m_gpuMeshes;
    LodManager   *m_lodMgr      = nullptr;

    GLuint m_atlasTex   = 0;
    bool   m_hasAtlas   = false;
    bool   m_wireframe  = false;
    bool   m_showNormals = false;
    float  m_heightScale = 1.0f;

    // Height bounds for the colormap uniform
    float  m_heightMin = 0.f;
    float  m_heightMax = 1000.f;

    // Atlas UV rect for the first tile (single-tile common case)
    float m_atlasU0 = 0.f, m_atlasV0 = 0.f, m_atlasU1 = 1.f, m_atlasV1 = 1.f;

    QPoint m_lastMousePos;
    bool   m_leftDrag  = false;
    bool   m_rightDrag = false;

    QElapsedTimer m_fpsTimer;
    int    m_frameCount = 0;

    bool   m_pendingUpload = false;
};
