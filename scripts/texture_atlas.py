#!/usr/bin/env python3
"""
texture_atlas.py — pack a directory of image tiles into a single texture atlas.

Shelf-packing algorithm: tiles sorted by height descending, placed left-to-right
on shelves of equal height. Outputs a PNG/TIF atlas and a JSON UV sidecar.

Usage:
    python texture_atlas.py \\
        --input-dir /data/tiles \\
        --output atlas.png \\
        --size 4096 \\
        --tile-size 512
"""

from __future__ import annotations

import argparse
import json
import logging
import sys
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import List, Optional, Tuple

from terrain_utils import setup_logging

log = setup_logging()


# ── PIL vs GDAL image backend ─────────────────────────────────────────────────

try:
    from PIL import Image as _PIL_Image
    _HAS_PIL = True
except ImportError:
    _HAS_PIL = False

try:
    from osgeo import gdal as _gdal
    _gdal.UseExceptions()
    _HAS_GDAL = True
except ImportError:
    _HAS_GDAL = False

if not _HAS_PIL and not _HAS_GDAL:
    raise ImportError(
        "Either Pillow or GDAL Python bindings are required. "
        "Install with: pip install pillow  or  pip install gdal"
    )


# ── Data structures ───────────────────────────────────────────────────────────

@dataclass
class TileMeta:
    path: str
    width: int
    height: int
    # UV normalised coords in the atlas (filled after packing)
    u0: float = 0.0
    v0: float = 0.0
    u1: float = 0.0
    v1: float = 0.0
    packed: bool = False


@dataclass
class Shelf:
    x: int = 0   # x cursor
    y: int = 0   # top of shelf
    h: int = 0   # current height of this shelf


# ── Image I/O ─────────────────────────────────────────────────────────────────

def _load_rgba_pil(path: Path, target_size: Optional[int]) -> Tuple[bytes, int, int]:
    img = _PIL_Image.open(str(path)).convert("RGBA")
    if target_size:
        img = img.resize((target_size, target_size), _PIL_Image.LANCZOS)
    data = img.tobytes("raw", "RGBA")
    return data, img.width, img.height


def _load_rgba_gdal(path: Path, target_size: Optional[int]) -> Tuple[bytes, int, int]:
    ds = _gdal.Open(str(path), _gdal.GA_ReadOnly)
    if ds is None:
        raise FileNotFoundError(f"GDAL cannot open: {path}")
    w = target_size or ds.RasterXSize
    h = target_size or ds.RasterYSize

    buf = bytearray(w * h * 4)
    n_bands = min(ds.RasterCount, 4)
    for ci in range(n_bands):
        band = ds.GetRasterBand(ci + 1)
        # ReadRaster returns a bytes object resampled to buf_xsize x buf_ysize.
        raw = band.ReadRaster(
            0, 0, ds.RasterXSize, ds.RasterYSize,
            buf_xsize=w, buf_ysize=h, buf_type=_gdal.GDT_Byte,
        )
        for pi in range(w * h):
            buf[pi * 4 + ci] = raw[pi]
    if n_bands < 4:
        # Fill alpha (and any missing channels) opaque.
        for pi in range(w * h):
            buf[pi * 4 + 3] = 255
    ds = None
    return bytes(buf), w, h


def load_rgba(path: Path, target_size: Optional[int]) -> Tuple[bytes, int, int]:
    if _HAS_PIL:
        return _load_rgba_pil(path, target_size)
    return _load_rgba_gdal(path, target_size)


# ── Shelf-packing ─────────────────────────────────────────────────────────────

def pack_tiles(
    tile_metas: List[TileMeta],
    atlas_size: int,
) -> Tuple[bytearray, List[TileMeta]]:
    """
    Shelf-pack tiles into an RGBA atlas buffer.

    Returns (atlas_bytes, updated_tile_metas) with UV coords filled in.
    """
    atlas = bytearray(atlas_size * atlas_size * 4)  # all zeros = transparent

    # Sort by height descending
    order = sorted(range(len(tile_metas)), key=lambda i: -tile_metas[i].height)

    shelves: List[Shelf] = [Shelf(x=0, y=0, h=0)]

    for idx in order:
        tm = tile_metas[idx]
        tw, th = tm.width, tm.height

        placed = False
        for shelf in shelves:
            # Does this tile fit on the current shelf?
            room_x = atlas_size - shelf.x >= tw
            room_y = atlas_size - shelf.y >= th
            fits_height = shelf.h == 0 or th <= shelf.h
            if room_x and room_y and fits_height:
                _blit(atlas, tm, shelf.x, shelf.y, atlas_size)
                _fill_uv(tm, shelf.x, shelf.y, tw, th, atlas_size)
                shelf.x += tw
                if th > shelf.h:
                    shelf.h = th
                tm.packed = True
                placed = True
                break

        if not placed:
            # Open a new shelf
            last = shelves[-1]
            new_y = last.y + last.h
            if new_y + th > atlas_size:
                log.warning("Atlas full — skipping tile %s", tm.path)
                continue
            new_shelf = Shelf(x=0, y=new_y, h=0)
            _blit(atlas, tm, 0, new_y, atlas_size)
            _fill_uv(tm, 0, new_y, tw, th, atlas_size)
            new_shelf.x = tw
            new_shelf.h = th
            shelves.append(new_shelf)
            tm.packed = True

    return atlas, tile_metas


def _fill_uv(tm: TileMeta, x: int, y: int, w: int, h: int, size: int) -> None:
    inv = 1.0 / size
    tm.u0, tm.v0 = x * inv, y * inv
    tm.u1, tm.v1 = (x + w) * inv, (y + h) * inv


def _blit(atlas: bytearray, tm: TileMeta, dx: int, dy: int, size: int) -> None:
    """Copy tm.rgba_data (stored as attribute) into atlas."""
    data: bytes = getattr(tm, "_rgba", b"")
    if not data:
        return
    for row in range(tm.height):
        if dy + row >= size:
            break
        for col in range(tm.width):
            if dx + col >= size:
                break
            src = (row * tm.width + col) * 4
            dst = ((dy + row) * size + (dx + col)) * 4
            atlas[dst:dst + 4] = data[src:src + 4]


# ── Atlas save ────────────────────────────────────────────────────────────────

def save_atlas_pil(atlas: bytes, size: int, path: Path) -> None:
    img = _PIL_Image.frombytes("RGBA", (size, size), atlas)
    img.save(str(path))


def save_atlas_gdal(atlas: bytes, size: int, path: Path) -> None:
    ext = path.suffix.lower()
    driver_name = "GTiff" if ext in (".tif", ".tiff") else "PNG"

    # The PNG/JPEG GDAL drivers are CreateCopy-only (they have no Create
    # capability), so assemble the image in an in-memory dataset and copy it
    # out. This also works for GTiff.
    mem = _gdal.GetDriverByName("MEM").Create("", size, size, 4, _gdal.GDT_Byte)
    for b in range(4):
        row_data = bytearray(size * size)
        for i in range(size * size):
            row_data[i] = atlas[i * 4 + b]
        mem.GetRasterBand(b + 1).WriteRaster(0, 0, size, size, bytes(row_data))

    out_driver = _gdal.GetDriverByName(driver_name)
    out_driver.CreateCopy(str(path), mem)
    mem = None


def save_atlas(atlas: bytes, size: int, path: Path) -> None:
    if _HAS_PIL:
        save_atlas_pil(atlas, size, path)
    else:
        save_atlas_gdal(atlas, size, path)


# ── CLI ───────────────────────────────────────────────────────────────────────

def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Pack image tiles into a texture atlas",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("--input-dir",  required=True, type=Path, metavar="DIR")
    p.add_argument("--output",     required=True, type=Path, metavar="FILE",
                   help="Output atlas image (PNG or TIF)")
    p.add_argument("--size",       default=4096, type=int,
                   help="Atlas size in pixels (square)")
    p.add_argument("--tile-size",  default=0,    type=int, metavar="N",
                   help="Resample each tile to NxN; 0 = keep native size")
    p.add_argument("--pattern",    default="*.png,*.jpg,*.tif", metavar="GLOBS",
                   help="Comma-separated glob patterns for input tiles")
    p.add_argument("--verbose", "-v", action="store_true")
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.verbose:
        log.setLevel(logging.DEBUG)

    input_dir: Path  = args.input_dir
    out_path:  Path  = args.output
    atlas_size: int  = args.size
    tile_target: Optional[int] = args.tile_size or None

    if not input_dir.is_dir():
        log.error("Input directory not found: %s", input_dir)
        return 1

    out_path.parent.mkdir(parents=True, exist_ok=True)

    # Collect tiles matching any of the supplied glob patterns
    patterns = [g.strip() for g in args.pattern.split(",") if g.strip()]
    tile_paths: list[Path] = []
    for pat in patterns:
        tile_paths.extend(sorted(input_dir.glob(pat)))

    if not tile_paths:
        log.warning("No tiles found in %s matching %s", input_dir, args.pattern)
        return 0

    log.info("Packing %d tile(s) into a %dx%d atlas", len(tile_paths), atlas_size, atlas_size)

    # Load tiles
    tile_metas: List[TileMeta] = []
    total_input_bytes = 0
    for p in tile_paths:
        try:
            rgba, w, h = load_rgba(p, tile_target)
            tm = TileMeta(path=str(p), width=w, height=h)
            tm._rgba = rgba  # type: ignore[attr-defined]
            tile_metas.append(tm)
            total_input_bytes += w * h * 4
            log.debug("Loaded %s (%dx%d)", p.name, w, h)
        except Exception as exc:  # noqa: BLE001
            log.warning("Skipping %s: %s", p.name, exc)

    if not tile_metas:
        log.error("No tiles could be loaded")
        return 1

    # Pack
    atlas_buf, packed_metas = pack_tiles(tile_metas, atlas_size)

    # Save atlas image
    save_atlas(bytes(atlas_buf), atlas_size, out_path)
    log.info("Atlas saved to %s", out_path)

    # Memory savings estimate
    atlas_bytes = atlas_size * atlas_size * 4
    saving_pct = (1.0 - atlas_bytes / max(total_input_bytes, 1)) * 100.0
    log.info(
        "Memory: input %.1f MiB → atlas %.1f MiB (%.0f%% reduction)",
        total_input_bytes / 1e6,
        atlas_bytes / 1e6,
        saving_pct,
    )

    # Write UV sidecar JSON
    uv_path = out_path.with_suffix(".json")
    uv_data = {
        "atlas_size": atlas_size,
        "tiles": [
            {
                "path":   tm.path,
                "width":  tm.width,
                "height": tm.height,
                "uv":     [tm.u0, tm.v0, tm.u1, tm.v1],
                "packed": tm.packed,
            }
            for tm in packed_metas
        ],
    }
    uv_path.write_text(json.dumps(uv_data, indent=2))
    log.info("UV sidecar written to %s", uv_path)

    unpacked = sum(1 for tm in packed_metas if not tm.packed)
    if unpacked:
        log.warning("%d tile(s) did not fit in the atlas", unpacked)

    return 0


if __name__ == "__main__":
    sys.exit(main())
