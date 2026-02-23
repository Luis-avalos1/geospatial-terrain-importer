#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Orbit / arcball camera using spherical coordinates centred on a target point.
// Yaw rotates around the vertical (Y) axis; pitch tilts up/down.
class Camera {
public:
    Camera();

    // Orbit (left drag): delta in pixels → yaw/pitch
    void orbit(float dx, float dy);

    // Pan (right drag): translate the target in the view plane
    void pan(float dx, float dy);

    // Zoom (scroll): positive = zoom in
    void zoom(float delta);

    // Reset to defaults
    void reset(const glm::vec3 &target, float distance);

    glm::mat4 viewMatrix()       const;
    glm::mat4 projectionMatrix(int viewportW, int viewportH) const;

    glm::vec3 position()  const;
    glm::vec3 target()    const { return m_target; }
    float     distance()  const { return m_distance; }

    // For LOD distance queries
    float distanceTo(const glm::vec3 &point) const;

    void setFov(float deg) { m_fovDeg = deg; }
    void setSensitivity(float orbitSens, float panSens, float zoomSens);

private:
    glm::vec3 m_target{0.f, 0.f, 0.f};
    float m_yaw      = 45.f;   // degrees
    float m_pitch    = 30.f;   // degrees, clamped ±89°
    float m_distance = 2000.f;
    float m_fovDeg   = 45.f;

    float m_orbitSens = 0.3f;
    float m_panSens   = 0.5f;
    float m_zoomSens  = 50.f;
};
