// Tests for the orbit Camera (pure glm math, no OpenGL context required).
#include "test_framework.hpp"

#include "Camera.hpp"

#include <cmath>

TEST_CASE("[camera] reset sets distance and keeps it as the orbit radius")
{
    Camera cam;
    cam.reset({0.f, 0.f, 0.f}, 100.f);
    CHECK_APPROX(cam.distance(), 100.0f, 1e-4);
    // Position lies on a sphere of `distance` around the target.
    CHECK_APPROX(cam.distanceTo({0.f, 0.f, 0.f}), 100.0f, 1e-3);
}

TEST_CASE("[camera] position uses the default yaw/pitch spherical mapping")
{
    Camera cam;
    cam.reset({0.f, 0.f, 0.f}, 100.f);  // yaw=45, pitch=30
    const glm::vec3 p = cam.position();
    // x = d cos(p) sin(y), y = d sin(p), z = d cos(p) cos(y)
    CHECK_APPROX(p.x, 61.2372f, 1e-2);
    CHECK_APPROX(p.y, 50.0f, 1e-2);
    CHECK_APPROX(p.z, 61.2372f, 1e-2);
}

TEST_CASE("[camera] target follows reset")
{
    Camera cam;
    cam.reset({5.f, -3.f, 2.f}, 10.f);
    CHECK_APPROX(cam.target().x, 5.0f, 1e-5);
    CHECK_APPROX(cam.target().y, -3.0f, 1e-5);
    CHECK_APPROX(cam.target().z, 2.0f, 1e-5);
}

TEST_CASE("[camera] zoom reduces distance")
{
    Camera cam;
    cam.reset({0.f, 0.f, 0.f}, 2000.f);
    cam.setSensitivity(0.3f, 0.5f, 50.f);
    cam.zoom(1.0f);  // distance -= 1 * 50
    CHECK_APPROX(cam.distance(), 1950.0f, 1e-3);
}

TEST_CASE("[camera] distance never drops below 1")
{
    Camera cam;
    cam.reset({0.f, 0.f, 0.f}, 100.f);
    cam.zoom(1000.0f);  // would drive distance hugely negative
    CHECK(cam.distance() >= 1.0f);
    CHECK_APPROX(cam.distance(), 1.0f, 1e-5);
}

TEST_CASE("[camera] orbit keeps the orbit radius constant")
{
    Camera cam;
    cam.reset({0.f, 0.f, 0.f}, 500.f);
    cam.orbit(120.f, -40.f);  // rotate; distance must not change
    CHECK_APPROX(cam.distance(), 500.0f, 1e-4);
    CHECK_APPROX(cam.distanceTo({0.f, 0.f, 0.f}), 500.0f, 1e-2);
}

TEST_CASE("[camera] pitch is clamped to avoid gimbal flip")
{
    Camera cam;
    cam.reset({0.f, 0.f, 0.f}, 100.f);
    cam.orbit(0.f, 100000.f);  // huge upward drag
    // pitch clamps at +89deg, so y must stay just under the full radius.
    const glm::vec3 p = cam.position();
    CHECK(p.y < 100.0f);
    CHECK(p.y > 99.0f);  // sin(89deg) ~= 0.9998
}

TEST_CASE("[camera] projection encodes the field of view and aspect ratio")
{
    Camera cam;
    cam.setFov(45.f);
    const glm::mat4 proj = cam.projectionMatrix(200, 100);  // aspect 2.0
    const float f = 1.0f / std::tan(45.0f * 3.14159265f / 180.0f / 2.0f);
    CHECK_APPROX(proj[1][1], f, 1e-3);          // vertical scale
    CHECK_APPROX(proj[0][0], f / 2.0f, 1e-3);   // horizontal scale = f / aspect
}

TEST_CASE("[camera] projection tolerates a zero-height viewport")
{
    Camera cam;
    // Should not divide by zero (height treated as 1).
    const glm::mat4 proj = cam.projectionMatrix(800, 0);
    CHECK(std::isfinite(proj[0][0]));
    CHECK(std::isfinite(proj[1][1]));
}
