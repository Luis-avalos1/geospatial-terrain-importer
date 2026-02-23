#!/usr/bin/env python3
"""
batch_processor.py — bulk convert GeoTIFF/raster files to TMSH + JSON metadata.

Usage:
    python batch_processor.py \\
        --input-dir /data/tiles \\
        --output-dir /out/meshes \\
        --pattern "*.tif" \\
        --resolution 512 \\
        --jobs 4
"""

from __future__ import annotations

import argparse
import json
import logging
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

from terrain_utils import (
    inspect_geotiff,
    read_elevation_band,
    setup_logging,
    write_tmsh,
)

log = setup_logging()


# ── Worker ────────────────────────────────────────────────────────────────────

def process_one(
    src: Path,
    out_dir: Path,
    resolution: int,
) -> dict:
    """
    Read src, write <stem>.bin (TMSH) + <stem>.json (metadata).
    Returns a status dict with "file" and either "ok" or "error".
    """
    stem = src.stem
    bin_path  = out_dir / f"{stem}.bin"
    meta_path = out_dir / f"{stem}.json"

    try:
        info = inspect_geotiff(src)
        heights, meta = read_elevation_band(
            src,
            band_index=1,
            target_width=resolution,
            target_height=resolution,
        )

        write_tmsh(bin_path, heights, meta["out_size"][0], meta["out_size"][1])

        manifest = {
            "source":       str(src),
            "width":        meta["out_size"][0],
            "height":       meta["out_size"][1],
            "native_width": meta["native_size"][0],
            "native_height":meta["native_size"][1],
            "geo_transform":list(meta["geo_transform"]),
            "crs_wkt":      meta["crs_wkt"],
            "nodata":       meta["nodata"],
            "bin_file":     str(bin_path),
            "bands":        info["bands"],
            "driver":       info["driver"],
        }
        meta_path.write_text(json.dumps(manifest, indent=2))

        log.info("OK  %s → %s", src.name, bin_path.name)
        return {"file": str(src), "ok": True}

    except Exception as exc:  # noqa: BLE001
        log.error("ERR %s: %s", src.name, exc)
        return {"file": str(src), "ok": False, "error": str(exc)}


# ── CLI ───────────────────────────────────────────────────────────────────────

def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Bulk convert raster files to TMSH binary meshes",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("--input-dir",  required=True, type=Path, metavar="DIR",
                   help="Directory to scan for raster files")
    p.add_argument("--output-dir", required=True, type=Path, metavar="DIR",
                   help="Where to write .bin and .json files")
    p.add_argument("--pattern",    default="*.tif", metavar="GLOB",
                   help="Glob pattern for input files")
    p.add_argument("--resolution", default=0, type=int, metavar="N",
                   help="Output grid size (0 = native resolution)")
    p.add_argument("--jobs",       default=4, type=int, metavar="N",
                   help="Parallel worker threads")
    p.add_argument("--verbose", "-v", action="store_true",
                   help="Enable debug logging")
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.verbose:
        log.setLevel(logging.DEBUG)

    input_dir: Path  = args.input_dir
    output_dir: Path = args.output_dir

    if not input_dir.is_dir():
        log.error("Input directory does not exist: %s", input_dir)
        return 1

    output_dir.mkdir(parents=True, exist_ok=True)

    files = sorted(input_dir.glob(args.pattern))
    if not files:
        log.warning("No files matching '%s' in %s", args.pattern, input_dir)
        return 0

    log.info("Found %d file(s); processing with %d worker(s)", len(files), args.jobs)

    results = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {
            pool.submit(process_one, f, output_dir, args.resolution): f
            for f in files
        }
        for future in as_completed(futures):
            results.append(future.result())

    ok_count  = sum(1 for r in results if r.get("ok"))
    err_count = len(results) - ok_count
    log.info("Done: %d succeeded, %d failed", ok_count, err_count)

    # Write a summary manifest
    summary_path = output_dir / "batch_summary.json"
    summary_path.write_text(json.dumps({
        "total":   len(results),
        "success": ok_count,
        "failed":  err_count,
        "files":   results,
    }, indent=2))
    log.info("Summary written to %s", summary_path)

    return 0 if err_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
