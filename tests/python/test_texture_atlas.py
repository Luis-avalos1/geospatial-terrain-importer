"""Tests for the pure shelf-packing logic in scripts/texture_atlas.py.

Importing the module requires at least one image backend (Pillow or GDAL);
the test is skipped automatically if neither is installed.
"""

from __future__ import annotations

import pytest

texture_atlas = pytest.importorskip("texture_atlas")


def _tile(width, height, fill):
    tm = texture_atlas.TileMeta(path=f"tile_{width}x{height}", width=width,
                                height=height)
    tm._rgba = bytes([fill, fill, fill, 255]) * (width * height)
    return tm


def test_fill_uv_math():
    tm = texture_atlas.TileMeta(path="t", width=10, height=20)
    texture_atlas._fill_uv(tm, 30, 40, 10, 20, 100)
    assert tm.u0 == pytest.approx(0.30)
    assert tm.v0 == pytest.approx(0.40)
    assert tm.u1 == pytest.approx(0.40)
    assert tm.v1 == pytest.approx(0.60)


def test_pack_single_tile_origin_and_blit():
    tm = _tile(2, 2, 200)
    atlas, metas = texture_atlas.pack_tiles([tm], 4)

    m = metas[0]
    assert m.packed
    assert (m.u0, m.v0) == (0.0, 0.0)
    assert m.u1 == pytest.approx(0.5)
    assert m.v1 == pytest.approx(0.5)

    assert len(atlas) == 4 * 4 * 4
    assert atlas[0] == 200                 # (0,0) red channel, blitted
    assert atlas[(3 * 4 + 3) * 4] == 0     # (3,3) untouched -> transparent


def test_pack_multiple_tiles_uv_in_bounds():
    metas = [_tile(16, 16, 50), _tile(32, 32, 100), _tile(8, 24, 150)]
    atlas, out = texture_atlas.pack_tiles(metas, 64)
    assert len(atlas) == 64 * 64 * 4
    for m in out:
        assert m.packed
        assert 0.0 <= m.u0 < m.u1 <= 1.0
        assert 0.0 <= m.v0 < m.v1 <= 1.0


def test_pack_tile_too_large_is_skipped():
    tm = _tile(16, 16, 1)
    _atlas, out = texture_atlas.pack_tiles([tm], 8)  # atlas smaller than tile
    assert out[0].packed is False
