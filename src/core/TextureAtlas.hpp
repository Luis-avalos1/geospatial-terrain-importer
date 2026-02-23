#pragma once

#include <vector>
#include <cstdint>
#include <string>

struct ImageTile;

struct AtlasRect {
    float u0, v0, u1, v1;  // normalised UV coordinates in the atlas
};

class TextureAtlas {
public:
    // Pack tiles using shelf-packing (sorted by height descending).
    // atlasSize must be a power-of-two (e.g., 4096).
    void pack(const std::vector<const ImageTile *> &tiles, int atlasSize = 4096);

    const std::vector<uint8_t> &rgba()   const { return m_rgba; }
    const std::vector<AtlasRect> &rects() const { return m_rects; }
    int width()  const { return m_size; }
    int height() const { return m_size; }
    bool empty() const { return m_rgba.empty(); }

private:
    std::vector<uint8_t>   m_rgba;
    std::vector<AtlasRect> m_rects;
    int m_size = 0;

    struct Shelf {
        int x    = 0;   // current x cursor
        int y    = 0;   // top of shelf
        int h    = 0;   // height of shelf
    };

    // Blit one tile into the atlas buffer.
    void blit(const ImageTile &tile, int destX, int destY);
};
