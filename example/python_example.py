"""
python_example.py

Demonstrates the ctypes path -- calling the fitsolver_lib shared library
built from src/abi.cpp -- instead of the CPython extension module
(`import fitsolver`, built from src/pymodule.cpp). This is the option to
reach for if you ever need to call FitSolver from something other than
Python, or want to see the raw C ABI in action without a Python-specific
build step.

For the normal, easier Python path, see simple_solve.py / rich_api_example.py
instead -- both `import fitsolver` directly.

Requires the fitsolver_lib shared library to exist first:
    cmake -B build
    cmake --build build --config Release
    python python_example.py
"""

import ctypes
import json
import os
import platform


def _shared_lib_path() -> str:
    """Locates the fitsolver_lib shared library built by CMake.

    CMake was told (via set_target_properties PREFIX "") to name this
    library "fitsolver_lib" on every platform, so only the extension
    differs: .dll on Windows, .dylib on macOS, .so on Linux. Multi-config
    generators (Visual Studio, Xcode) put it under build/<Config>/ instead
    of build/ directly, so both locations are checked.
    """
    system = platform.system()
    filename = {
        "Windows": "fitsolver_lib.dll",
        "Darwin": "fitsolver_lib.dylib",
    }.get(system, "fitsolver_lib.so")

    here = os.path.dirname(os.path.abspath(__file__))
    build_dir = os.path.join(here, "..", "build")
    candidates = [
        os.path.join(build_dir, filename),
        os.path.join(build_dir, "Release", filename),
        os.path.join(build_dir, "Debug", filename),
    ]
    for candidate in candidates:
        if os.path.exists(candidate):
            return candidate

    raise FileNotFoundError(
        f"Could not find {filename} in build/, build/Release/, or build/Debug/. "
        "Build the project first: cmake -B build && cmake --build build --config Release"
    )


_lib = ctypes.CDLL(_shared_lib_path())
_lib.fitsolver_solve.argtypes = [ctypes.c_char_p]
_lib.fitsolver_solve.restype = ctypes.c_void_p
_lib.fitsolver_free.argtypes = [ctypes.c_void_p]


def solve(request: dict) -> dict:
    """Sends a {"items": [...], "boxes": [...]} request through the raw
    C ABI and returns the parsed JSON response as a dict."""
    request_bytes = json.dumps(request).encode("utf-8")
    response_ptr = _lib.fitsolver_solve(request_bytes)

    # c_void_p (not c_char_p) so we keep the real pointer and can
    # correctly free it afterwards.
    response_bytes = ctypes.cast(response_ptr, ctypes.c_char_p).value
    result = json.loads(response_bytes.decode("utf-8"))

    _lib.fitsolver_free(response_ptr)
    return result


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    with open(os.path.join(here, "..", "tests", "fixtures", "items.json")) as f:
        items = json.load(f)
    with open(os.path.join(here, "..", "tests", "fixtures", "boxes.json")) as f:
        boxes = json.load(f)

    result = solve({"items": items, "boxes": boxes})
    print(json.dumps(result, indent=2))
