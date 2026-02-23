#pragma once

#include <string>
#include <utility>   // pair

// Thin wrapper around PROJ/OGR coordinate transformation.
// All methods are thread-safe if called with separate instances.
class CoordConverter {
public:
    // Build a converter from srcWkt → dstWkt.
    // Throws std::runtime_error if OGR fails to parse either SRS.
    CoordConverter(const std::string &srcWkt, const std::string &dstWkt);
    ~CoordConverter();

    // No copy — OGR transform objects are not cheaply copyable.
    CoordConverter(const CoordConverter &) = delete;
    CoordConverter &operator=(const CoordConverter &) = delete;
    CoordConverter(CoordConverter &&) noexcept;
    CoordConverter &operator=(CoordConverter &&) noexcept;

    // Transform a single (x, y) point. Returns the transformed (x, y).
    std::pair<double, double> transform(double x, double y) const;

    // Convenience: build a WGS-84 → UTM converter.
    // Zone is auto-detected from the reference longitude if zone == 0.
    static CoordConverter wgs84ToUtm(double refLon, int zone = 0, bool north = true);

    // Return the EPSG:32600+zone (or 32700+zone for south) UTM WKT.
    static std::string utmWkt(int zone, bool north);

    // Detect UTM zone from longitude.
    static int utmZoneFromLon(double lon);

private:
    struct Impl;
    Impl *m_impl = nullptr;
};
