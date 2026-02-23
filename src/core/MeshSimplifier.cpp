#include "MeshSimplifier.hpp"

#include <array>
#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <cmath>
#include <limits>
#include <functional>

// ── Quadric (symmetric 4×4 matrix stored as 10 unique values) ────────────────
struct Quadric {
    double q[10]{};   // row-major upper triangle: q00,q01,q02,q03,q11,q12,q13,q22,q23,q33

    Quadric &operator+=(const Quadric &o) {
        for (int i = 0; i < 10; ++i) q[i] += o.q[i];
        return *this;
    }

    // Plane error quadric from ax+by+cz+d=0
    static Quadric fromPlane(float a, float b, float c, float d) {
        Quadric Q;
        const double da = a, db = b, dc = c, dd = d;
        Q.q[0] = da*da; Q.q[1] = da*db; Q.q[2] = da*dc; Q.q[3] = da*dd;
        Q.q[4] = db*db; Q.q[5] = db*dc; Q.q[6] = db*dd;
        Q.q[7] = dc*dc; Q.q[8] = dc*dd;
        Q.q[9] = dd*dd;
        return Q;
    }

    // Error of point (x,y,z) wrt this quadric: v^T Q v
    double error(float x, float y, float z) const {
        const double vx = x, vy = y, vz = z;
        return q[0]*vx*vx + 2*q[1]*vx*vy + 2*q[2]*vx*vz + 2*q[3]*vx
             + q[4]*vy*vy + 2*q[5]*vy*vz + 2*q[6]*vy
             + q[7]*vz*vz + 2*q[8]*vz
             + q[9];
    }

    // Find optimal collapse point by solving the 3×3 linear system.
    // Returns false if the system is near-singular.
    bool optimalPoint(float &ox, float &oy, float &oz) const {
        // System: [ q00 q01 q02 ] [x]   [-q03]
        //         [ q01 q11 q12 ] [y] = [-q13]
        //         [ q02 q12 q22 ] [z]   [-q23]
        const double a00 = q[0], a01 = q[1], a02 = q[2];
        const double a11 = q[4], a12 = q[5];
        const double a22 = q[7];

        // Determinant via cofactor expansion
        const double det =
            a00 * (a11 * a22 - a12 * a12) -
            a01 * (a01 * a22 - a12 * a02) +
            a02 * (a01 * a12 - a11 * a02);

        if (std::abs(det) < 1e-10) return false;

        const double inv = 1.0 / det;
        const double b0 = -q[3], b1 = -q[6], b2 = -q[8];

        // Cramer's rule
        ox = static_cast<float>(inv * (
            b0  * (a11 * a22 - a12 * a12) -
            a01 * (b1  * a22 - a12 * b2 ) +
            a02 * (b1  * a12 - a11 * b2 )));
        oy = static_cast<float>(inv * (
            a00 * (b1  * a22 - a12 * b2 ) -
            b0  * (a01 * a22 - a12 * a02) +
            a02 * (a01 * b2  - b1  * a02)));
        oz = static_cast<float>(inv * (
            a00 * (a11 * b2  - b1  * a12) -
            a01 * (a01 * b2  - b1  * a02) +
            b0  * (a01 * a12 - a11 * a02)));
        return true;
    }
};

struct Edge {
    uint32_t v0, v1;  // v0 < v1 always
    bool operator==(const Edge &o) const { return v0 == o.v0 && v1 == o.v1; }
};

struct EdgeHash {
    size_t operator()(const Edge &e) const {
        return std::hash<uint64_t>{}((uint64_t)e.v0 << 32 | e.v1);
    }
};

// Per-edge bookkeeping: how many faces reference the edge, plus one incident
// face normal. For a boundary edge (count == 1) the stored normal is, by
// definition, the normal of its single adjacent face.
struct EdgeData {
    int   count = 0;
    float nx = 0.f, ny = 0.f, nz = 0.f;
};

// Collapse candidate in the priority queue
struct CollapseCandidate {
    double cost;
    uint32_t v0, v1;
    float px, py, pz;  // optimal collapse position
    uint32_t generation; // stale-detection token

    bool operator>(const CollapseCandidate &o) const { return cost > o.cost; }
};

float MeshSimplifier::simplify(MeshData &mesh, Options opts)
{
    const size_t origTriangles = mesh.indices.size() / 3;
    const size_t targetTriangles = static_cast<size_t>(
        static_cast<float>(origTriangles) * std::clamp(opts.targetRatio, 0.01f, 0.99f));

    if (origTriangles == 0 || targetTriangles >= origTriangles)
        return 1.0f;

    const size_t n = mesh.vertices.size();

    // ── Build per-vertex quadrics from face planes ────────────────────────────
    std::vector<Quadric> Q(n);

    // Count how many triangles reference each edge (for boundary detection)
    // and remember one incident face normal per edge.
    std::unordered_map<Edge, EdgeData, EdgeHash> edges;
    edges.reserve(origTriangles * 3);

    const size_t triCount = origTriangles;
    for (size_t t = 0; t < triCount; ++t) {
        const uint32_t i0 = mesh.indices[t * 3 + 0];
        const uint32_t i1 = mesh.indices[t * 3 + 1];
        const uint32_t i2 = mesh.indices[t * 3 + 2];

        const auto &v0 = mesh.vertices[i0];
        const auto &v1 = mesh.vertices[i1];
        const auto &v2 = mesh.vertices[i2];

        // Face normal (unnormalised)
        const float e1x = v1.x - v0.x, e1y = v1.y - v0.y, e1z = v1.z - v0.z;
        const float e2x = v2.x - v0.x, e2y = v2.y - v0.y, e2z = v2.z - v0.z;
        float a = e1y * e2z - e1z * e2y;
        float b = e1z * e2x - e1x * e2z;
        float c = e1x * e2y - e1y * e2x;
        const float len = std::sqrt(a*a + b*b + c*c);
        if (len < 1e-8f) continue;
        a /= len; b /= len; c /= len;
        const float d = -(a * v0.x + b * v0.y + c * v0.z);

        const Quadric Qf = Quadric::fromPlane(a, b, c, d);
        Q[i0] += Qf;
        Q[i1] += Qf;
        Q[i2] += Qf;

        // Record edge usage and a face normal for boundary detection.
        for (auto [ea, eb] : std::array<std::pair<uint32_t,uint32_t>, 3>{
            {{i0, i1}, {i1, i2}, {i2, i0}}})
        {
            if (ea > eb) std::swap(ea, eb);
            EdgeData &ed = edges[{ea, eb}];
            ed.count++;
            ed.nx = a; ed.ny = b; ed.nz = c;   // for boundary edges, THE face
        }
    }

    // ── Boundary preservation ──────────────────────────────────────────────────
    // For every boundary edge (used by exactly one face) add a heavily-weighted
    // constraint-plane quadric. The plane contains the edge and is perpendicular
    // to the incident face, so error grows quickly if a boundary vertex is moved
    // off the border. This is Garland & Heckbert's standard treatment and is
    // what makes the boundaryWeight option meaningful.
    for (const auto &[edge, ed] : edges) {
        if (ed.count != 1) continue;

        const Vertex &va = mesh.vertices[edge.v0];
        const Vertex &vb = mesh.vertices[edge.v1];

        const float ex = vb.x - va.x, ey = vb.y - va.y, ez = vb.z - va.z;
        // Constraint normal = edgeDirection × faceNormal
        float cnx = ey * ed.nz - ez * ed.ny;
        float cny = ez * ed.nx - ex * ed.nz;
        float cnz = ex * ed.ny - ey * ed.nx;
        const float clen = std::sqrt(cnx * cnx + cny * cny + cnz * cnz);
        if (clen < 1e-8f) continue;
        cnx /= clen; cny /= clen; cnz /= clen;

        const float cd = -(cnx * va.x + cny * va.y + cnz * va.z);
        Quadric Qb = Quadric::fromPlane(cnx, cny, cnz, cd);
        for (double &qv : Qb.q) qv *= opts.boundaryWeight;

        Q[edge.v0] += Qb;
        Q[edge.v1] += Qb;
    }

    // ── Vertex merge union-find ───────────────────────────────────────────────
    std::vector<uint32_t> parent(n);
    std::iota(parent.begin(), parent.end(), 0u);

    std::function<uint32_t(uint32_t)> find = [&](uint32_t x) -> uint32_t {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    };
    auto unite = [&](uint32_t a, uint32_t b) {
        a = find(a); b = find(b);
        if (a != b) parent[b] = a;
    };

    std::vector<uint32_t> generation(n, 0u);

    // ── Build initial priority queue ──────────────────────────────────────────
    std::priority_queue<CollapseCandidate,
                        std::vector<CollapseCandidate>,
                        std::greater<CollapseCandidate>> pq;

    auto enqueueEdge = [&](uint32_t a, uint32_t b) {
        if (a > b) std::swap(a, b);
        const Quadric Qab = [&]{ Quadric q = Q[a]; q += Q[b]; return q; }();

        float px, py, pz;
        if (!Qab.optimalPoint(px, py, pz)) {
            // Fallback to midpoint
            px = (mesh.vertices[a].x + mesh.vertices[b].x) * 0.5f;
            py = (mesh.vertices[a].y + mesh.vertices[b].y) * 0.5f;
            pz = (mesh.vertices[a].z + mesh.vertices[b].z) * 0.5f;
        }
        const double cost = Qab.error(px, py, pz);
        pq.push({cost, a, b, px, py, pz, generation[a]});
    };

    for (auto &[edge, _] : edges)
        enqueueEdge(edge.v0, edge.v1);

    // ── Collapse loop ─────────────────────────────────────────────────────────
    std::vector<bool> removed(n, false);
    size_t removedTri = 0;

    // Build adjacency for re-enqueueing after collapse
    std::vector<std::unordered_set<uint32_t>> adj(n);
    for (auto &[edge, _] : edges) {
        adj[edge.v0].insert(edge.v1);
        adj[edge.v1].insert(edge.v0);
    }

    while (!pq.empty() && (origTriangles - removedTri) > targetTriangles) {
        const CollapseCandidate c = pq.top(); pq.pop();

        const uint32_t ra = find(c.v0);
        const uint32_t rb = find(c.v1);
        if (ra == rb) continue;                          // already merged
        if (generation[ra] != c.generation) continue;   // stale entry

        // Move ra to the optimal point, merge rb → ra
        mesh.vertices[ra].x = c.px;
        mesh.vertices[ra].y = c.py;
        mesh.vertices[ra].z = c.pz;
        Q[ra] += Q[rb];
        unite(ra, rb);
        removed[rb] = true;
        generation[ra]++;

        // Re-enqueue neighbours of the survivor
        for (uint32_t nb : adj[ra]) {
            const uint32_t rnb = find(nb);
            if (rnb != ra) enqueueEdge(ra, rnb);
        }

        // Rough tri removal estimate (2 collapsed per edge on average)
        removedTri += 2;
    }

    // ── Rebuild index buffer ──────────────────────────────────────────────────
    std::vector<uint32_t> newIndices;
    newIndices.reserve(mesh.indices.size());

    for (size_t t = 0; t < origTriangles; ++t) {
        const uint32_t i0 = find(mesh.indices[t * 3 + 0]);
        const uint32_t i1 = find(mesh.indices[t * 3 + 1]);
        const uint32_t i2 = find(mesh.indices[t * 3 + 2]);

        if (i0 == i1 || i1 == i2 || i0 == i2) continue;  // degenerate

        // Check for degenerate by area
        const auto &vA = mesh.vertices[i0];
        const auto &vB = mesh.vertices[i1];
        const auto &vC = mesh.vertices[i2];
        const float ex = vB.x - vA.x, ey = vB.y - vA.y, ez = vB.z - vA.z;
        const float fx = vC.x - vA.x, fy = vC.y - vA.y, fz = vC.z - vA.z;
        const float cx = ey*fz - ez*fy, cy = ez*fx - ex*fz, cz = ex*fy - ey*fx;
        if (cx*cx + cy*cy + cz*cz < 1e-16f) continue;

        newIndices.push_back(i0);
        newIndices.push_back(i1);
        newIndices.push_back(i2);
    }

    // Compact vertices — only those still referenced
    std::vector<uint32_t> remap(n, UINT32_MAX);
    std::vector<Vertex> newVerts;
    newVerts.reserve(n / 2);

    for (uint32_t &idx : newIndices) {
        if (remap[idx] == UINT32_MAX) {
            remap[idx] = static_cast<uint32_t>(newVerts.size());
            newVerts.push_back(mesh.vertices[idx]);
        }
        idx = remap[idx];
    }

    mesh.vertices = std::move(newVerts);
    mesh.indices  = std::move(newIndices);

    TerrainMesh::recomputeNormals(mesh);

    const size_t finalTri = mesh.indices.size() / 3;
    return static_cast<float>(finalTri) / static_cast<float>(origTriangles);
}
