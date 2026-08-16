"""
_bootstrap.py

Locates the compiled fitsolver extension module (built by CMake into
build/, build/Release/, or build/Debug/) and adds its directory to
sys.path, so `import fitsolver` works from any of the example scripts no
matter the current working directory -- without needing to manually copy
the .pyd/.so next to the script or set PYTHONPATH by hand.

Usage: `import _bootstrap` (for its side effect) before `import fitsolver`.
"""

import glob
import os
import sys


def _add_build_dir_to_path() -> None:
    here = os.path.dirname(os.path.abspath(__file__))
    build_dir = os.path.join(here, "..", "build")
    candidates = [build_dir, os.path.join(build_dir, "Release"), os.path.join(build_dir, "Debug")]

    for candidate in candidates:
        has_module = glob.glob(os.path.join(candidate, "fitsolver*.pyd")) or glob.glob(
            os.path.join(candidate, "fitsolver*.so")
        )
        if has_module:
            if candidate not in sys.path:
                sys.path.insert(0, candidate)
            return

    raise ImportError(
        "Could not find the compiled fitsolver module under build/, build/Release/, "
        "or build/Debug/. Build it first:\n"
        "    cmake -B build\n"
        "    cmake --build build --config Release"
    )


_add_build_dir_to_path()
