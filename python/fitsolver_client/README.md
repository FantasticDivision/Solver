# fitsolver-client

A minimal, dependency-free `ctypes` client for the FitSolver C ABI
(`fitsolver_lib`). JSON in, JSON out -- the same contract as
`fitsolver.solve()`, but without needing a compiled CPython extension.

## Install

### As a normal dependency (portal team, deployments)

Released wheels **vendor the native `fitsolver_lib`** inside the package,
so there is nothing else to build or configure:

```bash
pip install fitsolver-client
```

```
# requirements.txt / pyproject.toml
fitsolver-client==0.1.0
```

One wheel is published per OS (Linux `manylinux_2_28`, macOS x86_64 +
arm64, Windows x64); each is valid for every Python >= 3.9. See
`.github/workflows/wheels.yml` for how they are built and published.

### From a local checkout (multi-repo development)

```bash
pip install -e path/to/FantasticSolver/python/fitsolver_client \
  --config-settings editable_mode=compat
```

`editable_mode=compat` writes a plain-path `.pth` instead of an import
hook, so editors / Pylance resolve `import fitsolver_client` statically.
An editable install has no vendored library; it uses your local `build/`
(see below), so a `cmake --build` is picked up with no reinstall.

## The shared library

Resolution order at the first `solve()` call:

1. `FITSOLVER_LIB_PATH` -- exact path to the `.dll` / `.so` / `.dylib`.
2. `FITSOLVER_REPO_ROOT` -- looked up under `<root>/build/`.
3. A `build/` directory found by walking up from this package's location
   (covers `pip install -e` from inside a FitSolver checkout).
4. `fitsolver_client/_libs/` -- the copy vendored into a release wheel.

Importing the package never fails on its own; only `solve()` does, with
`SolverUnavailableError`, if none of the above resolve.

To build the library for a local checkout:

```bash
cmake -B build
cmake --build build --config Release --target fitsolver_lib
```

## Building a wheel by hand

```bash
cmake --build build --config Release --target fitsolver_lib
python python/fitsolver_client/tools/vendor_lib.py --build-dir build
python -m build --wheel python/fitsolver_client
# optional: normalise the tag to py3-none-<platform>
python -m wheel tags --python-tag py3 --abi-tag none \
  --platform-tag win_amd64 --remove python/fitsolver_client/dist/*.whl
```

## Usage

```python
import fitsolver_client

response = fitsolver_client.solve({
    "items": [
        {"ItemCode": "ITM-001", "ItemReference": "Widget A",
         "Width": 100, "Length": 200, "Depth": 50, "Weight": 1.0},
    ],
    "boxes": [
        {"Reference": "SML", "Width": 150, "Length": 150, "Depth": 150,
         "MaxWeight": 8.5},
    ],
})
```

See the FitSolver README's "Request / response JSON schema" section for the
exact shape of the request and response dicts.
