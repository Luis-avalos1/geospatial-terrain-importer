"""
terrain_utils.py — shared GDAL helpers for the terrain pipeline.

No side effects on import; all functions are pure utility.
"""

from __future__ import annotations

import logging
import math
import struct
from pathlib import Path
from typing import Optional, Tuple


def _import_gdal():
    """Import the GDAL Python bindings lazily.

    Keeping the import out of module scope means the pure helpers in this module
    (UTM-zone math, the TMSH binary format) stay usable — and unit-testable —
    on machines without a GDAL install. Functions that actually touch rasters
    call this and get a clear error if GDAL is missing.
    """
    try:
        from osgeo import gdal, osr
    except ImportError as exc:  # pragma: no cover - environment dependent
        raise ImportError(
            "GDAL Python bindings not found. "
            "Install with: pip install gdal  or  conda install gdal"
        ) from exc
    gdal.UseExceptions()
    return gdal, osr


# ── Logging ───────────────────────────────────────────────────────────────────

def setup_logging(level: int = logging.INFO) -> logging.Logger:
    """Return a consistently-formatted root logger. Safe to call multiple times."""
    log = logging.getLogger("terrain")
    if not log.handlers:
        handler = logging.StreamHandler()
        handler.setFormatter(
            logging.Formatter("%(asctime)s %(levelname)-8s %(name)s — %(message)s",
                              datefmt="%H:%M:%S")
        )
        log.addHandler(handler)
    log.setLevel(level)
    return log


# ── GeoTIFF inspection ────────────────────────────────────────────────────────

def inspect_geotiff(path: str | Path) -> dict:
    """Return basic metadata for a GDAL-readable raster file."""
    gdal, osr = _import_gdal()
    ds = gdal.Open(str(path), gdal.GA_ReadOnly)
    if ds is None:
        raise FileNotFoundError(f"Cannot open: {path}")

    gt = ds.GetGeoTransform()
    srs = osr.SpatialReference(wkt=ds.GetProjection())

    info: dict = {
        "path":        str(path),
        "width":       ds.RasterXSize,
        "height":      ds.RasterYSize,
        "bands":       ds.RasterCount,
        "driver":      ds.GetDriver().ShortName,
        "geo_transform": gt,
        "crs_wkt":     ds.GetProjection(),
        "crs_proj4":   srs.ExportToProj4().strip(),
        "nodata":      {},
    }

    for i in range(1, ds.RasterCount + 1):
        band = ds.GetRasterBand(i)
        nd = band.GetNoDataValue()
        if nd is not None:
            info["nodata"][i] = nd

    # Approximate geographic extent (top-left + bottom-right corners)
    x0, y0 = gt[0], gt[3]
    x1 = gt[0] + ds.RasterXSize  * gt[1] + ds.RasterYSize  * gt[2]
    y1 = gt[3] + ds.RasterXSize  * gt[4] + ds.RasterYSize  * gt[5]
    info["extent"] = (x0, y0, x1, y1)

    ds = None
    return info


# ── Elevation band reader ─────────────────────────────────────────────────────

def read_elevation_band(
    path: str | Path,
    band_index: int = 1,
    target_width: int = 0,
    target_height: int = 0,
) -> Tuple["np.ndarray", dict]:  # type: ignore[name-defined]
    """
    Read a single raster band as a float32 numpy array.

    Returns (array, metadata) where array is shape (height, width).
    Nodata values are replaced with 0.
    """
    try:
        import numpy as np
    except ImportError as exc:
        raise ImportError("numpy is required for read_elevation_band") from exc

    gdal, _ = _import_gdal()

    ds = gdal.Open(str(path), gdal.GA_ReadOnly)
    if ds is None:
        raise FileNotFoundError(f"Cannot open: {path}")

    band = ds.GetRasterBand(band_index)
    src_w, src_h = ds.RasterXSize, ds.RasterYSize
    out_w = target_width  or src_w
    out_h = target_height or src_h

    # Read the full band, resampling to the requested output size. The output
    # dimensions are controlled by buf_xsize/buf_ysize; Band.ReadAsArray has no
    # xsize/ysize parameters (passing them raises TypeError).
    arr = band.ReadAsArray(
        buf_xsize=out_w, buf_ysize=out_h,
        buf_type=gdal.GDT_Float32,
        resample_alg=gdal.GRIORA_Bilinear,
    ).astype(np.float32)

    nd = band.GetNoDataValue()
    if nd is not None:
        arr[arr == float(nd)] = 0.0

    meta = {
        "geo_transform": ds.GetGeoTransform(),
        "crs_wkt":       ds.GetProjection(),
        "native_size":   (src_w, src_h),
        "out_size":      (out_w, out_h),
        "nodata":        nd,
    }
    ds = None
    return arr, meta


# ── Coordinate conversion ─────────────────────────────────────────────────────

def utm_zone_from_lon(lon: float) -> int:
    """Return the UTM zone number for a given longitude."""
    return int(math.floor((lon + 180.0) / 6.0)) % 60 + 1


def wgs84_to_utm(
    lon: float,
    lat: float,
    zone: Optional[int] = None,
    north: Optional[bool] = None,
) -> Tuple[float, float]:
    """
    Convert WGS-84 lon/lat to UTM easting/northing.

    Zone and hemisphere are auto-detected if not supplied.
    Returns (easting, northing) in metres.
    """
    if zone is None:
        zone = utm_zone_from_lon(lon)
    if north is None:
        north = lat >= 0.0

    _, osr = _import_gdal()

    src = osr.SpatialReference()
    src.SetWellKnownGeogCS("WGS84")
    src.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    dst = osr.SpatialReference()
    dst.SetUTM(zone, int(north))
    dst.SetWellKnownGeogCS("WGS84")
    dst.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    xform = osr.CoordinateTransformation(src, dst)
    easting, northing, _ = xform.TransformPoint(lon, lat)
    return easting, northing


# ── Binary mesh format helpers (TMSH) ─────────────────────────────────────────

TMSH_MAGIC   = b"TMSH"
TMSH_VERSION = 1


def write_tmsh(path: str | Path, heights: "np.ndarray", width: int, height: int) -> None:  # type: ignore[name-defined]
    """
    Write heights to the TMSH binary format:
        magic(4) version(u32) width(u32) height(u32) count(u32) heights(f32[])
    """
    with open(path, "wb") as f:
        count = width * height
        f.write(TMSH_MAGIC)
        f.write(struct.pack("<III", TMSH_VERSION, width, height))
        f.write(struct.pack("<I", count))
        f.write(heights.astype("<f4").tobytes())


def read_tmsh(path: str | Path) -> Tuple["np.ndarray", int, int]:  # type: ignore[name-defined]
    """
    Read a TMSH file. Returns (heights_array, width, height).
    heights_array is shape (height, width) float32.
    """
    try:
        import numpy as np
    except ImportError as exc:
        raise ImportError("numpy is required for read_tmsh") from exc

    with open(path, "rb") as f:
        magic = f.read(4)
        if magic != TMSH_MAGIC:
            raise ValueError(f"Not a TMSH file: {path}")
        version, width, height, count = struct.unpack("<IIII", f.read(16))
        if version != TMSH_VERSION:
            raise ValueError(f"Unsupported TMSH version {version}")
        raw = f.read(count * 4)

    arr = np.frombuffer(raw, dtype="<f4").reshape(height, width).copy()
    return arr, width, height
