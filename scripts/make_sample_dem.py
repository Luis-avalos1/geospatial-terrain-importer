#!/usr/bin/env python3
"""
make_sample_dem.py — generate a synthetic DEM GeoTIFF.

Lets you try the terrain importer (or the Python pipeline) without sourcing real
elevation data. The output is a single-band Float32 GeoTIFF with a UTM
geo-transform, filled with a few Gaussian mountains plus multi-octave value
noise for natural-looking ridges and valleys.

Usage:
    python make_sample_dem.py --output sample_dem.tif --size 512
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from terrain_utils import _import_gdal, setup_logging

log = setup_logging()


def _bilinear_resize(arr, size: int):
    """Resize a 2D array to (size, size) with separable bilinear interpolation."""
    import numpy as np

    h, w = arr.shape
    xi = np.linspace(0, w - 1, size)
    yi = np.linspace(0, h - 1, size)

    rows = np.empty((h, size))
    for r in range(h):
        rows[r] = np.interp(xi, np.arange(w), arr[r])

    out = np.empty((size, size))
    cols = np.arange(h)
    for c in range(size):
        out[:, c] = np.interp(yi, cols, rows[:, c])
    return out


def synthetic_heights(size: int, seed: int = 1):
    """Return a (size, size) float32 height field in metres."""
    import numpy as np

    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float64)

    # A few Gaussian peaks (relative position, amplitude in m, sigma fraction).
    peaks = [
        (0.35, 0.40, 900.0, 0.18),
        (0.65, 0.62, 700.0, 0.14),
        (0.50, 0.50, 350.0, 0.32),
    ]
    h = np.zeros((size, size))
    for px, py, amp, sigma in peaks:
        dx = xs - px * size
        dy = ys - py * size
        h += amp * np.exp(-(dx * dx + dy * dy) / (2.0 * (sigma * size) ** 2))

    # Multi-octave value noise for ridges/valleys.
    freq, amp = 4, 220.0
    for _ in range(5):
        coarse = rng.standard_normal((freq, freq))
        h += amp * _bilinear_resize(coarse, size)
        freq *= 2
        amp *= 0.5

    h -= h.min()  # keep elevations non-negative
    return h.astype(np.float32)


def write_dem(path: Path, heights, pixel_size: float = 30.0) -> None:
    """Write a height field to a Float32 GeoTIFF with a UTM zone 10N transform."""
    gdal, osr = _import_gdal()

    rows, cols = heights.shape
    drv = gdal.GetDriverByName("GTiff")
    ds = drv.Create(str(path), cols, rows, 1, gdal.GDT_Float32,
                    options=["COMPRESS=DEFLATE"])

    # Arbitrary but valid UTM origin (easting/northing in metres).
    ds.SetGeoTransform((500000.0, pixel_size, 0.0,
                        5_000_000.0, 0.0, -pixel_size))

    srs = osr.SpatialReference()
    srs.SetUTM(10, 1)  # zone 10, northern hemisphere
    srs.SetWellKnownGeogCS("WGS84")
    ds.SetProjection(srs.ExportToWkt())

    band = ds.GetRasterBand(1)
    band.WriteArray(heights)
    band.SetNoDataValue(-9999.0)
    ds.FlushCache()
    ds = None  # noqa: F841 — closes the dataset


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Generate a synthetic DEM GeoTIFF",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("--output", required=True, type=Path, help="Output .tif path")
    p.add_argument("--size", default=512, type=int, help="Grid size (NxN)")
    p.add_argument("--seed", default=1, type=int, help="Noise seed")
    p.add_argument("--pixel-size", default=30.0, type=float,
                   help="Ground sample distance in metres")
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        import numpy  # noqa: F401
    except ImportError:
        log.error("numpy is required: pip install numpy")
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    log.info("Generating %dx%d synthetic DEM (seed=%d)…", args.size, args.size, args.seed)
    heights = synthetic_heights(args.size, args.seed)
    write_dem(args.output, heights, args.pixel_size)
    log.info("Wrote %s  (elevation range %.1f–%.1f m)",
             args.output, float(heights.min()), float(heights.max()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
