#include "LodManager.hpp"
#include "GeoTiffReader.hpp"
#include "MeshSimplifier.hpp"

#include <stdexcept>
#include <cmath>

void LodManager::build(const ElevationData &data, Options opts)
{
    if (data.empty()) throw std::invalid_argument("LodManager: empty elevation data");

    m_levels.clear();
    if (opts.levels < 1) opts.levels = 1;

    // Switch distances are heuristic: finer detail up close, coarser far away.
    // Level 0 is used closest; higher indices are used at greater distances.
    // switchDistance[i] is the camera distance at which we switch *from* level i
    // to level i+1. We store it on the level itself as the lower bound.
    const float baseDist = 500.0f;

    for (int i = 0; i < opts.levels; ++i) {
        const int step = 1 << i;   // 1, 2, 4, 8, ...

        LodLevel lod;
        lod.sampleStep     = step;
        lod.switchDistance = (i == 0) ? 0.0f : baseDist * static_cast<float>(1 << (i - 1));

        lod.mesh = TerrainMesh::build(data, step, opts.heightScale);

        if (opts.runSimplifier && i > 0) {
            MeshSimplifier::Options simOpts;
            simOpts.targetRatio = opts.simplifyRatio;
            MeshSimplifier::simplify(lod.mesh, simOpts);
        }

        m_levels.push_back(std::move(lod));
    }
}

const LodLevel &LodManager::selectLod(float cameraDistance) const
{
    if (m_levels.empty())
        throw std::runtime_error("LodManager::selectLod: no LOD levels built");

    // Walk from coarsest to finest; return the first level whose
    // switchDistance is <= cameraDistance.
    for (int i = static_cast<int>(m_levels.size()) - 1; i >= 0; --i) {
        if (cameraDistance >= m_levels[static_cast<size_t>(i)].switchDistance)
            return m_levels[static_cast<size_t>(i)];
    }
    return m_levels.front();
}
