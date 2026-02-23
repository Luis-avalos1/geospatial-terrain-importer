// Tests for CoordConverter (OGR/PROJ-backed coordinate transforms).
#include "test_framework.hpp"

#include "core/CoordConverter.hpp"

#include <stdexcept>
#include <string>

TEST_CASE("[coord] UTM zone is derived correctly from longitude")
{
    CHECK(CoordConverter::utmZoneFromLon(-123.0) == 10);  // Vancouver
    CHECK(CoordConverter::utmZoneFromLon(0.0) == 31);     // prime meridian -> zone 31
    CHECK(CoordConverter::utmZoneFromLon(-180.0) == 1);
    CHECK(CoordConverter::utmZoneFromLon(3.0) == 31);
    CHECK(CoordConverter::utmZoneFromLon(-75.0) == 18);   // US east coast
    CHECK(CoordConverter::utmZoneFromLon(135.0) == 53);   // Japan (135E -> zone 53)
}

TEST_CASE("[coord] utmWkt produces a usable, non-empty projection string")
{
    const std::string wkt = CoordConverter::utmWkt(10, true);
    CHECK(!wkt.empty());
    // A converter can be built from WGS84 into it without throwing.
    CHECK(wkt.find("UTM") != std::string::npos ||
          wkt.find("Transverse_Mercator") != std::string::npos ||
          wkt.find("Transverse Mercator") != std::string::npos);
}

TEST_CASE("[coord] a point on the central meridian maps to false-easting 500000")
{
    // Zone 10's central meridian is 123 deg W; a point there at the equator
    // should land on easting = 500000, northing = 0.
    CoordConverter cc = CoordConverter::wgs84ToUtm(-123.0);
    const auto [easting, northing] = cc.transform(-123.0, 0.0);
    CHECK_APPROX(easting, 500000.0, 1.0);
    CHECK_APPROX(northing, 0.0, 1.0);
}

TEST_CASE("[coord] northern points have increasing northing")
{
    CoordConverter cc = CoordConverter::wgs84ToUtm(-123.0);
    const auto [e0, n0] = cc.transform(-123.0, 0.0);
    const auto [e1, n1] = cc.transform(-123.0, 45.0);
    CHECK(n1 > n0);
    CHECK(n1 > 4'000'000.0);  // ~45 deg N is roughly 5 million metres up
    (void)e0;
    (void)e1;
}

TEST_CASE("[coord] invalid WKT throws")
{
    CHECK_THROWS_AS(CoordConverter("not valid wkt", "also not valid"),
                    std::runtime_error);
}
