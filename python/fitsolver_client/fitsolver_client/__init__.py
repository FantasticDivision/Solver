"""Minimal ctypes client for the FitSolver C ABI (fitsolver_lib).

Deliberately thin: pure Python, no build step for the caller. A wheel
built for distribution vendors the native fitsolver_lib shared library
inside the package (fitsolver_client/_libs/), so `pip install
fitsolver-client` is self-contained -- no CMake, no env vars.

Library location is resolved in this order:
  1. FITSOLVER_LIB_PATH env var, if set -- the exact file to load.
  2. FITSOLVER_REPO_ROOT env var, if set -- look under <root>/build/.
  3. A build/ directory found by walking up from this file's own
     location -- covers `pip install -e` from inside a FitSolver
     checkout (multi-repo dev: the live build/ wins over any vendored
     copy so a rebuild is picked up immediately).
  4. The _libs/ directory vendored into this package by the wheel build
     -- the normal path for an installed (non-editable) release.

See example/python_example.py in the FitSolver repo for the original,
single-file version of this same pattern.
"""

import ctypes
import json
import os
import platform
from pathlib import Path

__all__ = ["solve", "SolverUnavailableError"]


class SolverUnavailableError(RuntimeError):
    """Raised when the fitsolver_lib shared library can't be found or loaded."""


def _library_filename() -> str:
    return {
        "Windows": "fitsolver_lib.dll",
        "Darwin": "fitsolver_lib.dylib",
    }.get(platform.system(), "fitsolver_lib.so")


def _candidate_build_dirs() -> list[Path]:
    candidates = []

    repo_root = os.environ.get("FITSOLVER_REPO_ROOT")
    if repo_root:
        candidates.append(Path(repo_root) / "build")

    # Walk up from this file looking for a sibling build/ directory.
    here = Path(__file__).resolve()
    candidates.extend(parent / "build" for parent in here.parents)

    return candidates


def _bundled_library(filename: str) -> Path | None:
    """The copy a distribution wheel vendors next to this module. Absent
    in an editable/source checkout -- populated only by the wheel build
    (see tools/vendor_lib.py + pyproject.toml package-data)."""
    candidate = Path(__file__).resolve().parent / "_libs" / filename
    return candidate if candidate.is_file() else None


def _find_library() -> Path:
    explicit = os.environ.get("FITSOLVER_LIB_PATH")
    if explicit:
        path = Path(explicit)
        if path.is_file():
            return path
        raise SolverUnavailableError(f"FITSOLVER_LIB_PATH is set but not a file: {explicit}")

    filename = _library_filename()

    # A live build/ from a dev checkout wins over the vendored copy, so a
    # local `cmake --build` is picked up without reinstalling.
    for build_dir in _candidate_build_dirs():
        for sub in ("Release", "Debug", "."):
            candidate = build_dir / sub / filename
            if candidate.is_file():
                return candidate

    bundled = _bundled_library(filename)
    if bundled is not None:
        return bundled

    raise SolverUnavailableError(
        f"Could not find {filename}. Install a release wheel of "
        "fitsolver-client (which vendors it), or build it in the FitSolver "
        "repo (cmake -B build && cmake --build build --config Release "
        "--target fitsolver_lib), or set FITSOLVER_LIB_PATH to its exact "
        "location."
    )


_lib: ctypes.CDLL | None = None


def _get_lib() -> ctypes.CDLL:
    # Loaded lazily (on first solve() call, not at import time) so
    # importing this package never fails just because the DLL hasn't
    # been built yet -- only actually calling solve() does.
    global _lib
    if _lib is None:
        lib = ctypes.CDLL(str(_find_library()))
        lib.fitsolver_solve.argtypes = [ctypes.c_char_p]
        lib.fitsolver_solve.restype = ctypes.c_void_p
        lib.fitsolver_free.argtypes = [ctypes.c_void_p]
        _lib = lib
    return _lib


def solve(request: dict) -> dict:
    """Sends a {"items": [...], "boxes": [...]} request through the
    fitsolver_lib C ABI and returns the parsed JSON response as a dict.

    See the FitSolver README's "Request / response JSON schema" section
    for the exact shape of `request` and the returned dict.
    """
    lib = _get_lib()
    request_bytes = json.dumps(request).encode("utf-8")
    response_ptr = lib.fitsolver_solve(request_bytes)
    try:
        # c_void_p (not c_char_p) on the ctypes signature so we keep the
        # real pointer and can correctly free it afterwards.
        response_bytes = ctypes.cast(response_ptr, ctypes.c_char_p).value
        return json.loads(response_bytes.decode("utf-8"))
    finally:
        lib.fitsolver_free(response_ptr)
