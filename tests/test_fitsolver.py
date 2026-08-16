"""
test_fitsolver.py

Manual smoke test of the compiled `fitsolver` extension module, using
the real items.json / boxes.json data.

    python test_fitsolver.py
"""

import glob
import json
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

import fitsolver as a  # noqa: E402  (must come after the sys.path fix above)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    with open(os.path.join(here, "items.json")) as f:
        items = json.load(f)
    with open(os.path.join(here, "boxes.json")) as f:
        boxes = json.load(f)

    request = {"items": items, "boxes": boxes}
    request_str = json.dumps(request)

    print("--- sending ---")
    print(json.dumps(request, indent=2))

    result_str = a.solve(request_str)
    result = json.loads(result_str)

    print()
    print("--- received ---")
    print(json.dumps(result, indent=2))

    # Sanity checks on the response shape: every item should be
    # accounted for exactly once, either placed or unplaced.
    assert "success" in result
    assert "placements" in result
    assert "unplaced_item_codes" in result

    placed_codes = {p["ItemCode"] for p in result["placements"]}
    unplaced_codes = set(result["unplaced_item_codes"])
    all_item_codes = {item["ItemCode"] for item in items}

    assert placed_codes | unplaced_codes == all_item_codes, \
        "every item should appear in either placements or unplaced_item_codes"
    assert not (placed_codes & unplaced_codes), \
        "no item should appear in both"

    print()
    print("OK — response is well-formed and accounts for every item")


if __name__ == "__main__":
    main()
