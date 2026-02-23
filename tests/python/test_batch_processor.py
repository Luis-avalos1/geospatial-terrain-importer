"""Tests for scripts/batch_processor.py."""

from __future__ import annotations

import pytest

import batch_processor as bp


def test_parse_args_defaults():
    ns = bp.parse_args(["--input-dir", "in", "--output-dir", "out"])
    assert str(ns.input_dir) == "in"
    assert ns.pattern == "*.tif"
    assert ns.resolution == 0
    assert ns.jobs == 4


def test_process_one_reports_errors_for_missing_file(tmp_path):
    result = bp.process_one(tmp_path / "does_not_exist.tif", tmp_path, 0)
    assert result["ok"] is False
    assert "error" in result


def test_process_one_success_writes_outputs(synthetic_dem, tmp_path):
    pytest.importorskip("numpy")
    out_dir = tmp_path / "out"
    out_dir.mkdir()

    result = bp.process_one(synthetic_dem, out_dir, 0)

    assert result["ok"] is True
    assert (out_dir / "dem.bin").is_file()
    assert (out_dir / "dem.json").is_file()
