#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uHeightMin;
uniform float uHeightMax;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out float vHeightFactor;  // 0..1 for colormap fallback

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;

    // Normal in world space (no non-uniform scale, so mat3 is fine here)
    vNormal = normalize(mat3(uModel) * aNormal);

    vUV = aUV;

    float range = uHeightMax - uHeightMin;
    float h = (range > 0.001)
        ? clamp((aPos.y - uHeightMin) / range, 0.0, 1.0)
        : 0.5;
    // Gentle gamma (<1) widens the low/mid band so terrain whose area is mostly
    // low (e.g. a peak rising from plains) shows colour variation instead of a
    // single flat band, while the summit still reaches the top of the ramp.
    vHeightFactor = pow(h, 0.72);

    gl_Position = uProjection * uView * worldPos;
}
