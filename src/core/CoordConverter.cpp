#include "CoordConverter.hpp"

#include <ogr_spatialref.h>
#include <ogr_api.h>

#include <cmath>
#include <stdexcept>
#include <utility>

struct CoordConverter::Impl {
    OGRSpatialReference src;
    OGRSpatialReference dst;
    OGRCoordinateTransformation *xform = nullptr;

    ~Impl() {
        if (xform) OGRCoordinateTransformation::DestroyCT(xform);
    }
};

CoordConverter::CoordConverter(const std::string &srcWkt, const std::string &dstWkt)
    : m_impl(new Impl())
{
    // GDAL 3.x flips axis order for geographic CRSes by default. Force the
    // traditional (lon, lat) / (easting, northing) order throughout.
    m_impl->src.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    m_impl->dst.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    const char *srcPtr = srcWkt.c_str();
    const char *dstPtr = dstWkt.c_str();

    if (m_impl->src.importFromWkt(&srcPtr) != OGRERR_NONE)
        throw std::runtime_error("CoordConverter: invalid source WKT");
    if (m_impl->dst.importFromWkt(&dstPtr) != OGRERR_NONE)
        throw std::runtime_error("CoordConverter: invalid destination WKT");

    m_impl->xform = OGRCreateCoordinateTransformation(&m_impl->src, &m_impl->dst);
    if (!m_impl->xform)
        throw std::runtime_error("CoordConverter: cannot create OGR coordinate transform");
}

CoordConverter::~CoordConverter() { delete m_impl; }

CoordConverter::CoordConverter(CoordConverter &&o) noexcept : m_impl(o.m_impl) {
    o.m_impl = nullptr;
}

CoordConverter &CoordConverter::operator=(CoordConverter &&o) noexcept {
    if (this != &o) {
        delete m_impl;
        m_impl = o.m_impl;
        o.m_impl = nullptr;
    }
    return *this;
}

std::pair<double, double> CoordConverter::transform(double x, double y) const {
    double ox = x, oy = y;
    if (!m_impl->xform->Transform(1, &ox, &oy))
        throw std::runtime_error("CoordConverter: point transform failed");
    return {ox, oy};
}

// static helpers ──────────────────────────────────────────────────────────────

int CoordConverter::utmZoneFromLon(double lon) {
    return static_cast<int>(std::floor((lon + 180.0) / 6.0)) % 60 + 1;
}

std::string CoordConverter::utmWkt(int zone, bool north) {
    OGRSpatialReference srs;
    srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    srs.SetUTM(zone, north ? TRUE : FALSE);
    srs.SetWellKnownGeogCS("WGS84");

    char *wkt = nullptr;
    srs.exportToWkt(&wkt);
    std::string result(wkt ? wkt : "");
    CPLFree(wkt);
    return result;
}

CoordConverter CoordConverter::wgs84ToUtm(double refLon, int zone, bool north) {
    if (zone == 0) zone = utmZoneFromLon(refLon);

    OGRSpatialReference wgs84;
    wgs84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    wgs84.SetWellKnownGeogCS("WGS84");

    char *srcWkt = nullptr;
    wgs84.exportToWkt(&srcWkt);
    std::string src(srcWkt ? srcWkt : "");
    CPLFree(srcWkt);

    return CoordConverter(src, utmWkt(zone, north));
}
