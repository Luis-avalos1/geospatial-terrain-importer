// Integration tests for GeoTiffReader: round-trip a synthetic GeoTIFF written
// with GDAL, then read it back through the production code path.
#include "test_framework.hpp"

#include "core/GeoTiffReader.hpp"

#include <gdal_priv.h>
#include <cpl_conv.h>

#include <cmath>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace {

// Write a width×height Float32 GeoTIFF filled by `fill`. Returns the path.
std::string writeTiff(const std::string &name, int w, int h,
                      const std::vector<float> &data,
                      bool setNoData = false, double noData = 0.0)
{
    GDALAllRegister();
    const std::string path =
        (std::filesystem::temp_directory_path() / name).string();

    GDALDriver *drv = GetGDALDriverManager()->GetDriverByName("GTiff");
    if (!drv) throw std::runtime_error("no GTiff driver");

    GDALDataset *ds = drv->Create(path.c_str(), w, h, 1, GDT_Float32, nullptr);
    if (!ds) throw std::runtime_error("cannot create test raster");

    double gt[6] = {100.0, 2.0, 0.0, 200.0, 0.0, -2.0};
    ds->SetGeoTransform(gt);

    GDALRasterBand *band = ds->GetRasterBand(1);
    if (setNoData) band->SetNoDataValue(noData);

    band->RasterIO(GF_Write, 0, 0, w, h,
                   const_cast<float *>(data.data()), w, h, GDT_Float32, 0, 0);
    GDALClose(ds);
    return path;
}

}  // namespace

TEST_CASE("[geotiff] round-trips dimensions, values and geo-transform")
{
    const int W = 8, H = 6;
    std::vector<float> data(W * H);
    for (int i = 0; i < W * H; ++i) data[i] = static_cast<float>(i);

    const std::string path = writeTiff("tdi_basic.tif", W, H, data);
    const ElevationData d = GeoTiffReader::read(path);

    CHECK(d.width == W);
    CHECK(d.height == H);
    CHECK(d.heights.size() == static_cast<size_t>(W) * H);
    CHECK_APPROX(d.heights[1], 1.0f, 1e-5);
    CHECK_APPROX(d.heights[W * H - 1], static_cast<float>(W * H - 1), 1e-5);

    CHECK_APPROX(d.geoTransform[0], 100.0, 1e-9);
    CHECK_APPROX(d.geoTransform[1], 2.0, 1e-9);
    CHECK_APPROX(d.geoTransform[5], -2.0, 1e-9);

    std::filesystem::remove(path);
}

TEST_CASE("[geotiff] nodata is replaced with zero")
{
    const int W = 4, H = 4;
    std::vector<float> data(W * H, 5.0f);
    data[0] = -9999.0f;  // nodata sentinel

    const std::string path =
        writeTiff("tdi_nodata.tif", W, H, data, /*setNoData=*/true, -9999.0);
    const ElevationData d = GeoTiffReader::read(path);

    CHECK_APPROX(d.heights[0], 0.0f, 1e-6);
    CHECK_APPROX(d.heights[1], 5.0f, 1e-6);

    std::filesystem::remove(path);
}

TEST_CASE("[geotiff] non-finite samples are sanitized to zero")
{
    const int W = 4, H = 4;
    std::vector<float> data(W * H, 3.0f);
    data[2] = std::numeric_limits<float>::quiet_NaN();
    data[5] = std::numeric_limits<float>::infinity();

    const std::string path = writeTiff("tdi_nan.tif", W, H, data);
    const ElevationData d = GeoTiffReader::read(path);

    CHECK(std::isfinite(d.heights[2]));
    CHECK(std::isfinite(d.heights[5]));
    CHECK_APPROX(d.heights[2], 0.0f, 1e-6);
    CHECK_APPROX(d.heights[5], 0.0f, 1e-6);

    std::filesystem::remove(path);
}

TEST_CASE("[geotiff] downsampling honours the target size")
{
    const int W = 8, H = 8;
    std::vector<float> data(W * H, 1.0f);
    const std::string path = writeTiff("tdi_down.tif", W, H, data);

    const ElevationData d = GeoTiffReader::read(path, 4, 4);
    CHECK(d.width == 4);
    CHECK(d.height == 4);
    CHECK(d.heights.size() == 16u);

    std::filesystem::remove(path);
}

TEST_CASE("[geotiff] missing file throws")
{
    CHECK_THROWS(GeoTiffReader::read("/no/such/file/really_not_here.tif"));
}

TEST_CASE("[geotiff] out-of-range band throws")
{
    const int W = 4, H = 4;
    std::vector<float> data(W * H, 1.0f);
    const std::string path = writeTiff("tdi_band.tif", W, H, data);

    CHECK_THROWS(GeoTiffReader::read(path, 0, 0, 5));  // only 1 band exists

    std::filesystem::remove(path);
}
