#pragma once

#include "TerrainMesh.hpp"
#include "MeshSimplifier.hpp"

#include <vector>
#include <optional>

struct ElevationData;

struct LodLevel {
    MeshData mesh;
    int      sampleStep     = 1;
    float    switchDistance = 0.0f;  // camera distance at which to switch *to* this level
};

class LodManager {
public:
    struct Options {
        int   levels         = 4;
        float heightScale    = 1.0f;
        bool  runSimplifier  = false;
        float simplifyRatio  = 0.5f;
    };

    // Build all LOD levels from elevation data.
    // Level 0 is highest resolution (sampleStep=1), each subsequent level doubles the step.
    void build(const ElevationData &data, Options opts = {});

    // Select the appropriate LOD level for a given camera distance.
    // Returns a reference to the coarsest level whose switchDistance <= cameraDistance.
    const LodLevel &selectLod(float cameraDistance) const;

    const std::vector<LodLevel> &levels() const { return m_levels; }
    bool empty() const { return m_levels.empty(); }
    void clear() { m_levels.clear(); }

private:
    std::vector<LodLevel> m_levels;
};
