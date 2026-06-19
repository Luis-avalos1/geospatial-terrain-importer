#include "Camera.hpp"

#include <algorithm>
#include <cmath>

static constexpr float kPi = 3.14159265358979323846f;

Camera::Camera()
{
    // Sensible defaults — caller should call reset() with actual terrain bounds
}

void Camera::reset(const glm::vec3 &target, float distance)
{
    m_target   = target;
    m_distance = distance;
    m_yaw      = 45.f;
    m_pitch    = 30.f;
}

void Camera::orbit(float dx, float dy)
{
    m_yaw   += dx * m_orbitSens;
    m_pitch += dy * m_orbitSens;
    m_pitch  = std::clamp(m_pitch, -89.f, 89.f);
}

void Camera::pan(float dx, float dy)
{
    // Move the target in the view-right and view-up directions
    const glm::mat4 view  = viewMatrix();
    const glm::vec3 right = glm::vec3(view[0][0], view[1][0], view[2][0]);
    const glm::vec3 up    = glm::vec3(view[0][1], view[1][1], view[2][1]);

    const float scale = m_distance * m_panSens * 0.001f;
    m_target -= right * (dx * scale);
    m_target += up    * (dy * scale);
}

void Camera::zoom(float delta)
{
    m_distance -= delta * m_zoomSens;
    m_distance  = std::max(m_distance, 1.0f);
}

glm::vec3 Camera::position() const
{
    const float yRad = m_yaw   * kPi / 180.f;
    const float pRad = m_pitch * kPi / 180.f;

    const float x = m_distance * std::cos(pRad) * std::sin(yRad);
    const float y = m_distance * std::sin(pRad);
    const float z = m_distance * std::cos(pRad) * std::cos(yRad);

    return m_target + glm::vec3(x, y, z);
}

glm::mat4 Camera::viewMatrix() const
{
    const glm::vec3 pos = position();
    const glm::vec3 up  = glm::vec3(0.f, 1.f, 0.f);
    return glm::lookAt(pos, m_target, up);
}

glm::mat4 Camera::projectionMatrix(int w, int h) const
{
    if (h == 0) h = 1;
    const float aspect = static_cast<float>(w) / static_cast<float>(h);
    // Scale near/far with the orbit distance so depth precision stays good at
    // any zoom and the scene (radius ~ a few × distance) is never clipped.
    const float nearZ = std::max(0.05f, m_distance * 0.01f);
    const float farZ  = m_distance * 50.0f;
    return glm::perspective(glm::radians(m_fovDeg), aspect, nearZ, farZ);
}

float Camera::distanceTo(const glm::vec3 &point) const
{
    return glm::length(position() - point);
}

void Camera::setSensitivity(float orbitSens, float panSens, float zoomSens)
{
    m_orbitSens = orbitSens;
    m_panSens   = panSens;
    m_zoomSens  = zoomSens;
}
