# fitsolver-client

A minimal, dependency-free `ctypes` client for the FitSolver C ABI
(`fitsolver_lib`). JSON in, JSON out -- the same contract as
`fitsolver.solve()`, but without needing a compiled CPython extension.

## Install

```bash
pip install "fitsolver-client @ git+https://github.com/FantasticDivision/Solver.git@main#subdirectory=python/fitsolver_client"
```

or, from a local checkout:

```bash
pip install -e path/to/FantasticSolver/python/fitsolver_client
```

## The shared library

This package does **not** vendor or build `fitsolver_lib` -- build it once
in the FitSolver repo:

```bash
cmake -B build
cmake --build build --config Release --target fitsolver_lib
```

At call time the library is resolved in this order:

1. `FITSOLVER_LIB_PATH` -- exact path to the `.dll` / `.so` / `.dylib`.
2. `FITSOLVER_REPO_ROOT` -- looked up under `<root>/build/`.
3. A `build/` directory found by walking up from this package's location
   (covers `pip install -e` from inside a FitSolver checkout).

If none resolve, the first `solve()` call raises `SolverUnavailableError`
(importing the package never fails on its own).

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
