# Contributing

Thanks for your interest in improving the Geospatial Terrain Importer!

## Development setup

See the [README](README.md#building) for dependencies. For a fast,
GUI-free dev loop you only need a compiler, CMake, GDAL, and GLM:

```bash
cmake -B build -DBUILD_GUI=OFF -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

For the Python pipeline:

```bash
pip install -r scripts/requirements-dev.txt
pytest
ruff check scripts
```

## Conventions

- **C++**: C++17, 4-space indent, 100-column lines. Run
  `clang-format -i` (a `.clang-format` is provided) before committing.
- **Python**: formatted/linted with `ruff` (config in `pyproject.toml`).
- **Tests**: new logic in `terrain_core` or `scripts/` should come with a test.
  C++ tests live in `tests/cpp/` (tag each `TEST_CASE` with a suite name such as
  `"[mesh] …"`); Python tests live in `tests/python/`.

## Pull requests

- Keep the core library free of Qt/OpenGL dependencies — that separation is what
  keeps it testable in CI.
- CI must be green (`.github/workflows/ci.yml` runs both test suites).
