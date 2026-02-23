#!/usr/bin/env python3
"""
lod_generator.py — generate a LOD pyramid from a single GeoTIFF.

Each level halves the resolution of the previous one. Output files are named
<stem>_lod0.bin, <stem>_lod1.bin, … with a lod_manifest.json index.

Usage:
    python lod_generator.py \\
        --input dem.tif \\
        --output-dir /out/lods \\
        --levels 4 \\
        --base-resolution 1024
"""

from __future__ import annotations

import argparse
import json
import logging
import sys
from pathlib import Path

from terrain_utils import (
    inspect_geotiff,
    read_elevation_band,
    setup_logging,
    write_tmsh,
)

log = setup_logging()

# Switch distances mirror the C++ LodManager heuristic.
_BASE_SWITCH_DIST = 500.0


def _switch_distance(level: int) -> float:
    if level == 0:
        return 0.0
    return _BASE_SWITCH_DIST * float(1 << (level - 1))


# ── Per-level processor ───────────────────────────────────────────────────────

def generate_lod(
    src: Path,
    out_dir: Path,
    levels: int,
    base_resolution: int,
) -> list[dict]:
    """
    Generate LOD pyramid. Returns list of per-level metadata dicts.
    """
    stem = src.stem
    manifest_entries: list[dict] = []

    for level in range(levels):
        # Each level halves resolution
        if base_resolution > 0:
            res = max(base_resolution >> level, 1)
        else:
            # Use native size and halve by sample step
            info = inspect_geotiff(src)
            native_w = info["width"]
            native_h = info["height"]
            res_w = max(native_w >> level, 1)
            res_h = max(native_h >> level, 1)
            heights, meta = read_elevation_band(src, target_width=res_w, target_height=res_h)
            out_path = out_dir / f"{stem}_lod{level}.bin"
            write_tmsh(out_path, heights, res_w, res_h)
            entry = {
                "level":           level,
                "width":           res_w,
                "height":          res_h,
                "sample_step":     1 << level,
                "switch_distance": _switch_distance(level),
                "bin_file":        str(out_path),
                "geo_transform":   list(meta["geo_transform"]),
                "crs_wkt":         meta["crs_wkt"],
            }
            log.info("LOD %d: %dx%d → %s", level, res_w, res_h, out_path.name)
            manifest_entries.append(entry)
            continue

        # Square resolution path
        heights, meta = read_elevation_band(src, target_width=res, target_height=res)
        out_path = out_dir / f"{stem}_lod{level}.bin"
        write_tmsh(out_path, heights, res, res)

        entry = {
            "level":           level,
            "width":           res,
            "height":          res,
            "sample_step":     1 << level,
            "switch_distance": _switch_distance(level),
            "bin_file":        str(out_path),
            "geo_transform":   list(meta["geo_transform"]),
            "crs_wkt":         meta["crs_wkt"],
        }
        log.info("LOD %d: %dx%d → %s", level, res, res, out_path.name)
        manifest_entries.append(entry)

    return manifest_entries


# ── CLI ───────────────────────────────────────────────────────────────────────

def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Generate LOD pyramid from a single GeoTIFF",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("--input",          required=True, type=Path,
                   help="Source GeoTIFF file")
    p.add_argument("--output-dir",     required=True, type=Path,
                   help="Directory for output LOD files")
    p.add_argument("--levels",         default=4, type=int,
                   help="Number of LOD levels")
    p.add_argument("--base-resolution",default=0, type=int, metavar="N",
                   help="Level-0 grid size; 0 = native (non-square inputs keep aspect ratio)")
    p.add_argument("--verbose", "-v",  action="store_true")
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.verbose:
        log.setLevel(logging.DEBUG)

    src: Path = args.input
    out_dir: Path = args.output_dir

    if not src.is_file():
        log.error("Input file not found: %s", src)
        return 1

    out_dir.mkdir(parents=True, exist_ok=True)

    info = inspect_geotiff(src)
    log.info("Source: %s  (%dx%d, %d band(s))",
             src.name, info["width"], info["height"], info["bands"])

    entries = generate_lod(src, out_dir, args.levels, args.base_resolution)

    manifest = {
        "source":          str(src),
        "levels":          args.levels,
        "base_resolution": args.base_resolution or None,
        "lod_levels":      entries,
    }
    manifest_path = out_dir / "lod_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2))
    log.info("Manifest written to %s", manifest_path)

    return 0


if __name__ == "__main__":
    sys.exit(main())
