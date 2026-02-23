#include "SatelliteImageLoader.hpp"

#include <gdal.h>
#include <gdal_priv.h>
#include <cpl_conv.h>

#include <algorithm>
#include <stdexcept>

namespace {
    void ensureGdal() {
        // Thread-safe one-time registration (see GeoTiffReader for rationale).
        static const bool done = [] {
            GDALAllRegister();
            return true;
        }();
        (void) done;
    }

    struct DatasetGuard {
        GDALDataset *ds;
        explicit DatasetGuard(GDALDataset *d) : ds(d) {}
        ~DatasetGuard() { if (ds) GDALClose(ds); }
    };
}

ImageTile SatelliteImageLoader::load(const std::string &path,
                                     int targetWidth,
                                     int targetHeight)
{
    ensureGdal();

    GDALDataset *raw = static_cast<GDALDataset *>(
        GDALOpenEx(path.c_str(), GDAL_OF_RASTER | GDAL_OF_READONLY, nullptr, nullptr, nullptr)
    );
    if (!raw) throw std::runtime_error("SatelliteImageLoader: cannot open '" + path + "'");
    DatasetGuard guard(raw);

    const int nativeCols = raw->GetRasterXSize();
    const int nativeRows = raw->GetRasterYSize();
    const int bandCount  = raw->GetRasterCount();

    if (bandCount == 0)
        throw std::runtime_error("SatelliteImageLoader: no raster bands in '" + path + "'");

    const int outCols = (targetWidth  > 0) ? targetWidth  : nativeCols;
    const int outRows = (targetHeight > 0) ? targetHeight : nativeRows;
    const size_t pixels = static_cast<size_t>(outCols) * outRows;

    ImageTile tile;
    tile.width  = outCols;
    tile.height = outRows;
    tile.rgba.assign(pixels * 4, 255u);

    // Determine which band maps to which RGBA channel using colour interpretation.
    // Default: band 1 → R, 2 → G, 3 → B; alpha from band 4 if present.
    int rBand = -1, gBand = -1, bBand = -1, aBand = -1;

    for (int b = 1; b <= bandCount; ++b) {
        GDALRasterBand *band = raw->GetRasterBand(b);
        switch (band->GetColorInterpretation()) {
            case GCI_RedBand:   rBand = b; break;
            case GCI_GreenBand: gBand = b; break;
            case GCI_BlueBand:  bBand = b; break;
            case GCI_AlphaBand: aBand = b; break;
            default: break;
        }
    }

    // Fallback: positional assignment
    if (rBand < 0 && bandCount >= 1) rBand = 1;
    if (gBand < 0 && bandCount >= 2) gBand = 2;
    if (bBand < 0 && bandCount >= 3) bBand = 3;
    if (aBand < 0 && bandCount >= 4) aBand = 4;

    // For single-band data treat as greyscale (copy to R,G,B)
    const bool greyscale = (gBand < 0 && bBand < 0);

    auto readBand = [&](int index, uint8_t *dst, int channelOffset) {
        if (index < 1) return;
        std::vector<uint8_t> tmp(pixels);
        GDALRasterBand *band = raw->GetRasterBand(index);
        const CPLErr err = band->RasterIO(
            GF_Read, 0, 0, nativeCols, nativeRows,
            tmp.data(), outCols, outRows, GDT_Byte, 0, 0
        );
        if (err != CE_None) return;
        for (size_t i = 0; i < pixels; ++i)
            dst[i * 4 + channelOffset] = tmp[i];
    };

    readBand(rBand, tile.rgba.data(), 0);
    if (greyscale) {
        // Copy R → G, B
        for (size_t i = 0; i < pixels; ++i) {
            tile.rgba[i * 4 + 1] = tile.rgba[i * 4 + 0];
            tile.rgba[i * 4 + 2] = tile.rgba[i * 4 + 0];
        }
    } else {
        readBand(gBand, tile.rgba.data(), 1);
        readBand(bBand, tile.rgba.data(), 2);
    }
    readBand(aBand, tile.rgba.data(), 3);

    return tile;
}
