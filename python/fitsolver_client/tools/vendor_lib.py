"""Copy a freshly built fitsolver_lib into the package so `python -m build`
picks it up as package data.

Run this *after* `cmake --build build --target fitsolver_lib` and *before*
building the wheel:

    python tools/vendor_lib.py [--build-dir ../../build]

It looks for fitsolver_lib.{dll,so,dylib} under <build-dir> (and its
Release/ and Debug/ subdirs), copies the first match to
fitsolver_client/_libs/, and leaves a note so the file isn't mistaken for
something that belongs in version control.
"""

from __future__ import annotations

import argparse
import platform
import shutil
import sys
from pathlib import Path

_PKG_ROOT = Path(__file__).resolve().parent.parent
_LIBS_DIR = _PKG_ROOT / "fitsolver_client" / "_libs"

_DEFAULT_BUILD_DIR = _PKG_ROOT.parent.parent / "build"  # FantasticSolver/build

_LIB_NAMES = {
    "Windows": "fitsolver_lib.dll",
    "Darwin": "fitsolver_lib.dylib",
}


def _lib_name() -> str:
    return _LIB_NAMES.get(platform.system(), "fitsolver_lib.so")


def _find_built_lib(build_dir: Path, name: str) -> Path | None:
    for sub in (".", "Release", "Debug"):
        candidate = build_dir / sub / name
        if candidate.is_file():
            return candidate
    return None


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=_DEFAULT_BUILD_DIR,
        help=f"CMake build directory (default: {_DEFAULT_BUILD_DIR})",
    )
    args = parser.parse_args(argv)

    name = _lib_name()
    src = _find_built_lib(args.build_dir.resolve(), name)
    if src is None:
        print(
            f"error: {name} not found under {args.build_dir} (or its "
            "Release/ Debug/ subdirs). Build it first:\n"
            "    cmake -B build && cmake --build build --config Release "
            "--target fitsolver_lib",
            file=sys.stderr,
        )
        return 1

    _LIBS_DIR.mkdir(parents=True, exist_ok=True)
    (_LIBS_DIR / ".gitignore").write_text("*\n!.gitignore\n")
    dst = _LIBS_DIR / name
    shutil.copy2(src, dst)
    print(f"vendored {src} -> {dst.relative_to(_PKG_ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
