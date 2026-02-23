"""Tests for scripts/lod_generator.py."""

from __future__ import annotations

from pathlib import Path

import pytest

import lod_generator as lg


def test_switch_distance_matches_cpp_heuristic():
    assert lg._switch_distance(0) == 0.0
    assert lg._switch_distance(1) == 500.0
    assert lg._switch_distance(2) == 1000.0
    assert lg._switch_distance(3) == 2000.0


def test_parse_args_defaults():
    ns = lg.parse_args(["--input", "dem.tif", "--output-dir", "out"])
    assert str(ns.input) == "dem.tif"
    assert str(ns.output_dir) == "out"
    assert ns.levels == 4
    assert ns.base_resolution == 0


def test_generate_lod_native(synthetic_dem, tmp_path):
    pytest.importorskip("numpy")
    out_dir = tmp_path / "lods"
    out_dir.mkdir()

    entries = lg.generate_lod(synthetic_dem, out_dir, levels=3,
                              base_resolution=0)

    assert len(entries) == 3
    # Native 8x6 halves each level.
    assert (entries[0]["width"], entries[0]["height"]) == (8, 6)
    assert (entries[1]["width"], entries[1]["height"]) == (4, 3)
    assert entries[0]["sample_step"] == 1
    assert entries[1]["sample_step"] == 2
    for e in entries:
        assert Path(e["bin_file"]).is_file()
