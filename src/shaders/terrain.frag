#version 410 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in float vHeightFactor;

uniform sampler2D uAtlasTex;
uniform int       uHasAtlas;    // 1 if atlas texture is bound
uniform vec4      uAtlasRect;   // (u0, v0, u1, v1) normalised rect for this tile
uniform vec3      uLightDir;    // world-space, normalised, pointing toward light
uniform vec3      uCameraPos;
uniform float     uWireframe;   // 0 or 1
uniform float     uShowNormals; // 0 or 1

out vec4 fragColor;

// Multi-stop hypsometric ramp: lowland green → dry highland → rock → snow.
// More stops than a basic 4-colour ramp so adjacent elevations stay distinct.
vec3 heightColormap(float t)
{
    const vec3 c0 = vec3(0.16, 0.40, 0.22);  // deep green lowland
    const vec3 c1 = vec3(0.45, 0.58, 0.26);  // green
    const vec3 c2 = vec3(0.72, 0.69, 0.36);  // khaki / dry grass
    const vec3 c3 = vec3(0.62, 0.47, 0.31);  // tan / soil
    const vec3 c4 = vec3(0.46, 0.39, 0.35);  // rock (grey-brown)
    const vec3 c5 = vec3(0.74, 0.74, 0.76);  // bare light rock
    const vec3 c6 = vec3(0.98, 0.99, 1.00);  // snow

    if (t < 0.16) return mix(c0, c1, t / 0.16);
    if (t < 0.36) return mix(c1, c2, (t - 0.16) / 0.20);
    if (t < 0.55) return mix(c2, c3, (t - 0.36) / 0.19);
    if (t < 0.72) return mix(c3, c4, (t - 0.55) / 0.17);
    if (t < 0.88) return mix(c4, c5, (t - 0.72) / 0.16);
    return mix(c5, c6, clamp((t - 0.88) / 0.12, 0.0, 1.0));
}

void main()
{
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);

    // ── Base colour ───────────────────────────────────────────────────────────
    vec3 baseColor;
    if (uHasAtlas == 1) {
        // Sample atlas using the per-tile UV sub-rect
        vec2 atlasUV = mix(uAtlasRect.xy, uAtlasRect.zw, vUV);
        baseColor = texture(uAtlasTex, atlasUV).rgb;
    } else {
        baseColor = heightColormap(vHeightFactor);
    }

    if (uShowNormals > 0.5) {
        // Visualise normals as RGB (debug mode)
        fragColor = vec4(N * 0.5 + 0.5, 1.0);
        return;
    }

    if (uWireframe > 0.5) {
        fragColor = vec4(0.8, 0.6, 0.1, 1.0);
        return;
    }

    // ── Diffuse + ambient lighting ────────────────────────────────────────────
    const float ambient = 0.18;
    float diff = max(dot(N, L), 0.0);

    vec3 lit = baseColor * (ambient + (1.0 - ambient) * diff);

    // ── Gamma correction ──────────────────────────────────────────────────────
    lit = pow(clamp(lit, 0.0, 1.0), vec3(1.0 / 2.2));

    fragColor = vec4(lit, 1.0);
}
