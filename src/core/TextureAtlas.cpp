#include "TextureAtlas.hpp"
#include "SatelliteImageLoader.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>

void TextureAtlas::pack(const std::vector<const ImageTile *> &tiles, int atlasSize)
{
    if (tiles.empty()) return;
    if (atlasSize <= 0) throw std::invalid_argument("TextureAtlas: atlasSize must be positive");

    m_size = atlasSize;
    m_rgba.assign(static_cast<size_t>(m_size) * m_size * 4, 0u);
    m_rects.resize(tiles.size());

    // Sort tile indices by height descending (shelf-packing heuristic)
    std::vector<size_t> order(tiles.size());
    std::iota(order.begin(), order.end(), 0u);
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return tiles[a]->height > tiles[b]->height;
    });

    std::vector<Shelf> shelves;
    shelves.push_back({0, 0, 0});

    for (size_t origIdx : order) {
        const ImageTile *tile = tiles[origIdx];
        if (!tile || tile->empty()) {
            m_rects[origIdx] = {0.f, 0.f, 0.f, 0.f};
            continue;
        }

        const int tw = tile->width;
        const int th = tile->height;

        // Find a shelf with enough width
        Shelf *target = nullptr;
        for (auto &shelf : shelves) {
            if (m_size - shelf.x >= tw && m_size - shelf.y >= th) {
                if (shelf.h == 0 || th <= shelf.h) {
                    target = &shelf;
                    break;
                }
            }
        }

        // Open a new shelf if needed
        if (!target) {
            const int newY = shelves.back().y + shelves.back().h;
            if (newY + th > m_size) {
                // Atlas full — skip tile
                m_rects[origIdx] = {0.f, 0.f, 0.f, 0.f};
                continue;
            }
            shelves.push_back({0, newY, 0});
            target = &shelves.back();
        }

        // Place tile
        const int px = target->x;
        const int py = target->y;
        blit(*tile, px, py);

        target->x += tw;
        if (target->h < th) target->h = th;

        const float invS = 1.0f / static_cast<float>(m_size);
        m_rects[origIdx] = {
            static_cast<float>(px)      * invS,
            static_cast<float>(py)      * invS,
            static_cast<float>(px + tw) * invS,
            static_cast<float>(py + th) * invS
        };
    }
}

void TextureAtlas::blit(const ImageTile &tile, int destX, int destY)
{
    for (int row = 0; row < tile.height; ++row) {
        const int dstRow = destY + row;
        if (dstRow >= m_size) break;
        for (int col = 0; col < tile.width; ++col) {
            const int dstCol = destX + col;
            if (dstCol >= m_size) break;

            const size_t src = (static_cast<size_t>(row) * tile.width + col) * 4;
            const size_t dst = (static_cast<size_t>(dstRow) * m_size + dstCol) * 4;

            m_rgba[dst + 0] = tile.rgba[src + 0];
            m_rgba[dst + 1] = tile.rgba[src + 1];
            m_rgba[dst + 2] = tile.rgba[src + 2];
            m_rgba[dst + 3] = tile.rgba[src + 3];
        }
    }
}
