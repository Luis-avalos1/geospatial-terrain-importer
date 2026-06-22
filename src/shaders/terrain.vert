#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uHeightMin;
uniform float uHeightMax;
uniform float uHeightScale;  // live vertical exaggeration (1.0 = as loaded)

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out float vHeightFactor;  // 0..1 for colormap fallback

void main()
{
    float hs = max(uHeightScale, 1e-4);

    // Exaggerate elevation about sea level (y = 0) so the relief can be amplified
    // interactively without re-importing the raster.
    vec3 pos = vec3(aPos.x, aPos.y * hs, aPos.z);
    vec4 worldPos = uModel * vec4(pos, 1.0);
    vWorldPos = worldPos.xyz;

    // Scaling height by hs scales the surface normal's vertical term by 1/hs
    // (inverse-transpose of the diag(1, hs, 1) scale) — keeps lighting correct
    // as the exaggeration changes. uModel is a pure translation, so mat3 is fine.
    vec3 n = normalize(vec3(aNormal.x, aNormal.y / hs, aNormal.z));
    vNormal = normalize(mat3(uModel) * n);

    vUV = aUV;

    // Colour from the *unscaled* elevation so the hypsometric ramp stays put as
    // the exaggeration slider moves.
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
