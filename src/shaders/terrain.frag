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

// Simple 4-stop height colormap: water → grass → rock → snow
vec3 heightColormap(float t)
{
    const vec3 water = vec3(0.10, 0.28, 0.50);
    const vec3 grass = vec3(0.25, 0.52, 0.20);
    const vec3 rock  = vec3(0.50, 0.43, 0.35);
    const vec3 snow  = vec3(0.95, 0.97, 1.00);

    if (t < 0.25) return mix(water, grass, t / 0.25);
    if (t < 0.55) return mix(grass, rock,  (t - 0.25) / 0.30);
    if (t < 0.80) return mix(rock,  snow,  (t - 0.55) / 0.25);
    return snow;
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
