"""Shared pytest fixtures for the Python terrain-pipeline tests.

Adds the sibling ``scripts/`` directory to ``sys.path`` so the modules under
test (``terrain_utils``, ``lod_generator``, ``batch_processor``,
``texture_atlas``) can be imported by name.
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

SCRIPTS_DIR = Path(__file__).resolve().parents[2] / "scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))


@pytest.fixture
def synthetic_dem(tmp_path):
    """Write a tiny 8x6 Float32 GeoTIFF DEM (with a nodata value and a known
    ramp of values) and return its path. Skips the test if the GDAL Python
    bindings or numpy are unavailable."""
    gdal = pytest.importorskip("osgeo.gdal")
    osr = pytest.importorskip("osgeo.osr")
    np = pytest.importorskip("numpy")
    gdal.UseExceptions()

    path = tmp_path / "dem.tif"
    w, h = 8, 6
    drv = gdal.GetDriverByName("GTiff")
    ds = drv.Create(str(path), w, h, 1, gdal.GDT_Float32)
    ds.SetGeoTransform((100.0, 2.0, 0.0, 200.0, 0.0, -2.0))

    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("WGS84")
    ds.SetProjection(srs.ExportToWkt())

    arr = np.arange(w * h, dtype=np.float32).reshape(h, w)
    band = ds.GetRasterBand(1)
    band.SetNoDataValue(-9999.0)
    band.WriteArray(arr)
    ds.FlushCache()
    ds = None  # noqa: F841 — closes the dataset

    return path
