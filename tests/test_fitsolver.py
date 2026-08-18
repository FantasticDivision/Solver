"""
test_fitsolver.py

Manual smoke test of the compiled `fitsolver` extension module, using
the real items.json / boxes.json data.

    python test_fitsolver.py
"""

import glob
import ctypes
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


def _test_c_abi() -> None:
    here = os.path.dirname(os.path.abspath(__file__))
    build_dir = os.path.join(here, "..", "build")
    candidates = []
    for directory in (build_dir, os.path.join(build_dir, "Release"), os.path.join(build_dir, "Debug")):
        for name in ("fitsolver_lib.dll", "fitsolver_lib.so", "fitsolver_lib.dylib"):
            path = os.path.join(directory, name)
            if os.path.isfile(path):
                candidates.append(path)
    if not candidates:
        raise FileNotFoundError("Could not find the fitsolver C ABI shared library under build/")

    library = ctypes.CDLL(candidates[0])
    library.fitsolver_solve.argtypes = [ctypes.c_char_p]
    library.fitsolver_solve.restype = ctypes.c_void_p
    library.fitsolver_free.argtypes = [ctypes.c_void_p]
    library.fitsolver_free.restype = None

    def solve(payload):
        pointer = library.fitsolver_solve(payload)
        assert pointer, "the C ABI must return an allocated response"
        try:
            return json.loads(ctypes.string_at(pointer).decode("utf-8"))
        finally:
            library.fitsolver_free(pointer)

    assert solve(b'{"items":[],"boxes":[]}')["success"] is True
    malformed = solve(b"{")
    assert malformed["success"] is False
    assert isinstance(malformed["error"], str) and malformed["error"]
    # A null input is defined to behave like an empty request.
    assert solve(None)["success"] is True


def main():
    _test_c_abi()
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

    # Exact-fit boundary case through the JSON API.
    exact = json.loads(
        a.solve(
            json.dumps(
                {
                    "items": [
                        {
                            "ItemCode": "EXACT",
                            "Width": 10,
                            "Length": 20,
                            "Depth": 30,
                            "Weight": 8,
                            "AllowedRotation": "Never",
                        }
                    ],
                    "boxes": [
                        {
                            "Reference": "EXACT-BOX",
                            "Width": 10,
                            "Length": 20,
                            "Depth": 30,
                            "MaxWeight": 10,
                            "BoxWeight": 2,
                        }
                    ],
                }
            )
        )
    )
    assert exact["success"] is True
    assert exact["summary"] == {
        "box_count": 1,
        "overall_volume_utilisation": 100.0,
        "total_placed": 1,
        "total_unplaced": 0,
    }
    assert exact["placements"][0]["orientation"] == "WLD"

    # Exercise the native object API and every exposed result layer.
    native_item = a.Item(
        "ROTATE", "Rotated item", 10, 10, 100, 2.5, allowed_rotation=a.Rotation.BEST_FIT
    )
    native_box = a.Box("FLAT", 100, 10, 10, max_weight=5, empty_weight=0.5, max_quantity=1)
    assert native_item.volume == 10000
    assert native_item.has_box_group is False
    assert native_box.has_weight_limit is True
    assert native_box.has_limited_quantity is True
    assert native_box.max_quantity == 1

    native_result = a.pack([native_item], [native_box])
    assert native_result.success is True
    assert native_result.total_placed == 1
    assert native_result.total_unplaced == 0
    assert len(native_result.boxes) == 1
    packed_box = native_result.boxes[0]
    assert len(packed_box) == 1
    assert packed_box.box.reference == "FLAT"
    assert packed_box.weight == 3.0
    assert packed_box.remaining_weight == 2.0
    packed_item = packed_box.items[0]
    assert packed_item.item.item_code == "ROTATE"
    assert packed_item.rotated is True
    assert packed_item.x >= 0 and packed_item.y >= 0 and packed_item.z >= 0
    assert json.loads(native_result.to_json())["success"] is True

    # Dict API: inactive and undersized boxes must leave the item unplaced.
    dict_result = a.pack_dicts(
        [{"ItemCode": "A", "Width": 5, "Length": 5, "Depth": 5}],
        [{"Reference": "OFF", "Width": 100, "Length": 100, "Depth": 100, "Active": False}],
    )
    assert dict_result.success is False
    assert [item.item_code for item in dict_result.unplaced_items] == ["A"]

    # Invalid JSON must surface as a Python exception, rather than crashing
    # the extension process (the C ABI separately converts this to error JSON).
    try:
        a.solve("{")
    except Exception:
        pass
    else:
        raise AssertionError("malformed JSON should raise")

    print()
    print("OK — JSON, dict, and native-object APIs passed")


if __name__ == "__main__":
    main()
