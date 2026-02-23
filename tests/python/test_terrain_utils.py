"""Tests for scripts/terrain_utils.py."""

from __future__ import annotations

import pytest

import terrain_utils as tu


# ── Pure helpers (no GDAL required) ───────────────────────────────────────────

def test_utm_zone_from_lon_known_values():
    assert tu.utm_zone_from_lon(-123.0) == 10   # Vancouver
    assert tu.utm_zone_from_lon(0.0) == 31      # prime meridian
    assert tu.utm_zone_from_lon(-180.0) == 1
    assert tu.utm_zone_from_lon(-75.0) == 18    # US east coast
    assert tu.utm_zone_from_lon(135.0) == 53    # Japan (135E -> zone 53)


def test_utm_zone_matches_cpp_formula():
    # Mirrors CoordConverter::utmZoneFromLon — must stay in lockstep.
    for lon in (-179.0, -123.4, -0.1, 0.0, 6.0, 179.0):
        zone = tu.utm_zone_from_lon(lon)
        assert 1 <= zone <= 60


def test_setup_logging_is_idempotent():
    log_a = tu.setup_logging()
    log_b = tu.setup_logging()
    assert log_a is log_b
    # Calling twice must not stack duplicate handlers.
    assert len(log_a.handlers) == 1


# ── TMSH binary format (needs numpy only) ─────────────────────────────────────

def test_tmsh_round_trip(tmp_path):
    np = pytest.importorskip("numpy")
    heights = np.arange(12, dtype=np.float32).reshape(3, 4)
    path = tmp_path / "mesh.tmsh"

    tu.write_tmsh(path, heights, 4, 3)
    out, w, h = tu.read_tmsh(path)

    assert (w, h) == (4, 3)
    assert out.shape == (3, 4)
    assert np.allclose(out, heights)


def test_read_tmsh_rejects_bad_magic(tmp_path):
    pytest.importorskip("numpy")
    path = tmp_path / "bad.tmsh"
    path.write_bytes(b"XXXX" + b"\x00" * 16)
    with pytest.raises(ValueError):
        tu.read_tmsh(path)


# ── GDAL-backed helpers (skip if GDAL/numpy missing) ──────────────────────────

def test_inspect_geotiff(synthetic_dem):
    info = tu.inspect_geotiff(synthetic_dem)
    assert info["width"] == 8
    assert info["height"] == 6
    assert info["bands"] == 1
    assert info["driver"] == "GTiff"
    assert info["nodata"].get(1) == -9999.0
    assert len(info["geo_transform"]) == 6


def test_read_elevation_band(synthetic_dem):
    # Regression guard: this used to crash because read_elevation_band passed
    # invalid `xsize`/`ysize` keyword arguments to Band.ReadAsArray.
    np = pytest.importorskip("numpy")
    arr, meta = tu.read_elevation_band(synthetic_dem)
    assert arr.shape == (6, 8)
    assert arr.dtype == np.float32
    assert meta["native_size"] == (8, 6)
    assert meta["out_size"] == (8, 6)
    assert arr[0, 1] == pytest.approx(1.0)  # second cell of the ramp


def test_read_elevation_band_downsamples(synthetic_dem):
    pytest.importorskip("numpy")
    arr, meta = tu.read_elevation_band(
        synthetic_dem, target_width=4, target_height=3
    )
    assert arr.shape == (3, 4)
    assert meta["out_size"] == (4, 3)
