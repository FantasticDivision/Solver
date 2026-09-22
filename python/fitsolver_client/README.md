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
fitsolver-client>=0.6.1
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

## Releasing a new version to PyPI

PyPI versions are immutable, so a release is always a deliberate version
bump + tag. Publishing is automated (`.github/workflows/wheels.yml`) and
uses **Trusted Publishing** (OIDC) -- no API token. Do it whenever the
native `fitsolver_lib` behaviour changes (the wheels vendor the compiled
library) or the client code itself changes.

1. **Bump the version.** Edit `version` in
   `python/fitsolver_client/pyproject.toml` (semver: patch for a
   bug-fix-only lib rebuild, minor for new/changed packing behaviour,
   major for a breaking request/response change). Note the same version
   in this README's install snippet.

2. **Commit it on `main`.**

   ```bash
   git add python/fitsolver_client/pyproject.toml python/fitsolver_client/README.md
   git commit -m "fitsolver-client: release vX.Y.Z"
   git push origin main
   ```

   The push runs the wheels workflow in build-and-smoke-test-only mode
   (no upload) -- check it is green before tagging.

3. **Tag and push the tag -- this is what publishes to PyPI.**

   ```bash
   git tag vX.Y.Z          # tag name must match the pyproject version, with a leading v
   git push origin vX.Y.Z
   ```

   The tag push triggers the full matrix: per-OS wheels (Linux
   `manylinux_2_28`, macOS `universal2`, Windows x64) + an sdist, each
   smoke-tested in a clean venv, then `publish-pypi` uploads them.

4. **Verify.** `pip install fitsolver-client==X.Y.Z` in a fresh venv;
   check https://pypi.org/project/fitsolver-client/ shows the new files.

5. **Bump consumers.** Update `fitsolver-client>=X.Y.Z` in
   `fantastic-portal/backend/pyproject.toml` (and anywhere else it is
   pinned).

### First-time setup (already done for this project)

On PyPI: *Account -> Publishing -> add a pending publisher* -- project
`fitsolver-client`, owner `FantasticDivision`, repo `Solver`, workflow
`wheels.yml`, environment `pypi`. Create a matching `pypi` environment
under the GitHub repo's *Settings -> Environments*. No secrets are stored.

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
