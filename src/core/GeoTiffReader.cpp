#include "GeoTiffReader.hpp"

#include <gdal.h>
#include <gdal_priv.h>
#include <cpl_conv.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
    struct GdalDatasetDeleter {
        void operator()(GDALDataset *ds) const { if (ds) GDALClose(ds); }
    };
    using DatasetPtr = std::unique_ptr<GDALDataset, GdalDatasetDeleter>;

    void ensureGdal() {
        // Function-local static initialisation is thread-safe in C++11+, so
        // GDALAllRegister() runs exactly once even when a raster is loaded from
        // a worker thread (the GUI imports on a QtConcurrent thread).
        static const bool registered = [] {
            GDALAllRegister();
            return true;
        }();
        (void) registered;
    }
}

ElevationData GeoTiffReader::read(const std::string &path,
                                  int targetWidth,
                                  int targetHeight,
                                  int bandIndex)
{
    ensureGdal();

    GDALDataset *raw = static_cast<GDALDataset *>(
        GDALOpenEx(path.c_str(), GDAL_OF_RASTER | GDAL_OF_READONLY, nullptr, nullptr, nullptr)
    );
    if (!raw) {
        throw std::runtime_error("GeoTiffReader: cannot open '" + path + "'");
    }
    DatasetPtr ds(raw);

    const int nativeCols = ds->GetRasterXSize();
    const int nativeRows = ds->GetRasterYSize();

    if (bandIndex < 1 || bandIndex > ds->GetRasterCount()) {
        throw std::runtime_error(
            "GeoTiffReader: band " + std::to_string(bandIndex) +
            " out of range (file has " + std::to_string(ds->GetRasterCount()) + " bands)");
    }

    const int outCols = (targetWidth  > 0) ? targetWidth  : nativeCols;
    const int outRows = (targetHeight > 0) ? targetHeight : nativeRows;

    GDALRasterBand *band = ds->GetRasterBand(bandIndex);

    int hasNodata = 0;
    const double nodataVal = band->GetNoDataValue(&hasNodata);

    ElevationData result;
    result.width  = outCols;
    result.height = outRows;
    result.heights.resize(static_cast<size_t>(outCols) * outRows, 0.0f);

    // RasterIO with bilinear resampling when the output size differs from native
    CPLErr err = band->RasterIO(
        GF_Read,
        0, 0, nativeCols, nativeRows,
        result.heights.data(),
        outCols, outRows,
        GDT_Float32,
        0, 0
    );
    if (err != CE_None) {
        throw std::runtime_error("GeoTiffReader: RasterIO failed for '" + path + "'");
    }

    // Replace nodata and any non-finite samples with 0 so the mesh builder
    // never sees NaN/Inf. A NaN nodata value can't be matched with ==, which is
    // exactly why the std::isfinite() guard is needed in addition to the
    // equality check.
    const float nd = static_cast<float>(nodataVal);
    const bool hasFiniteNodata = hasNodata && std::isfinite(nd);
    for (float &h : result.heights) {
        if (!std::isfinite(h) || (hasFiniteNodata && h == nd))
            h = 0.0f;
    }

    ds->GetGeoTransform(result.geoTransform.data());
    result.projectionWkt = ds->GetProjectionRef();

    return result;
}
