"""Counts, in real engine output, the items that nothing holds up.

Solves every order in a scenario file (benchmarks/boxpacker_comparison/
scenarios.json by default) or the three Thommeax scenarios through the
engine's C ABI, then checks every placed item against the items around it.
Gravity acts along -Z: z = 0 is a box's floor and Depth is vertical.

  floating     off the floor with no other item's top meeting its underside
  overhanging  touching something, but with the centre of its base outside
               the area held up from beneath, so it would tip

The checks are written out here in Python, separately from the engine's own
src/Support.cpp, so that one can't hide a mistake in the other.

  python check_support.py --lib ../../build/Release/fitsolver_lib.dll
  python check_support.py --lib ../../build/fitsolver_lib.so --thommeax
"""
import argparse
import ctypes
import json
import pathlib
import sys

EPS = 1e-6
HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent.parent


def load_engine(path):
    lib = ctypes.CDLL(str(path))
    lib.fitsolver_solve.argtypes = [ctypes.c_char_p]
    lib.fitsolver_solve.restype = ctypes.c_void_p
    lib.fitsolver_free.argtypes = [ctypes.c_void_p]
    lib.fitsolver_free.restype = None

    def solve(request):
        pointer = lib.fitsolver_solve(json.dumps(request).encode())
        try:
            return json.loads(ctypes.string_at(pointer).decode())
        finally:
            lib.fitsolver_free(pointer)

    return solve


def expand(item_types):
    """One entry with a Quantity becomes that many individually coded units."""
    units = []
    for item in item_types:
        for unit in range(item.get("Quantity", 1)):
            copy = {key: value for key, value in item.items() if key != "Quantity"}
            copy["ItemCode"] = f"{item['ItemCode']}::{unit}"
            units.append(copy)
    return units


def convex_hull(points):
    points = sorted(set(points))
    if len(points) < 3:
        return points

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    lower, upper = [], []
    for point in points:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], point) <= 0:
            lower.pop()
        lower.append(point)
    for point in reversed(points):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], point) <= 0:
            upper.pop()
        upper.append(point)
    return lower[:-1] + upper[:-1]


def inside(hull, point):
    if len(hull) < 3:
        return False
    for index, start in enumerate(hull):
        end = hull[(index + 1) % len(hull)]
        scale = max(1.0, abs(end[0] - start[0]) + abs(end[1] - start[1]))
        turn = (end[0] - start[0]) * (point[1] - start[1]) - (end[1] - start[1]) * (point[0] - start[0])
        if turn < -EPS * scale:
            return False
    return True


def classify(item, others):
    """'floating', 'overhanging' or 'firm' for one placed item."""
    if item["z"] <= EPS:
        return "firm"
    contact = []
    for other in others:
        if other is item or abs(other["z"] + other["depth"] - item["z"]) > EPS:
            continue
        x0 = max(item["x"], other["x"])
        x1 = min(item["x"] + item["width"], other["x"] + other["width"])
        y0 = max(item["y"], other["y"])
        y1 = min(item["y"] + item["length"], other["y"] + other["length"])
        if x1 - x0 > EPS and y1 - y0 > EPS:
            contact += [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    if not contact:
        return "floating"
    centre = (item["x"] + item["width"] / 2, item["y"] + item["length"] / 2)
    return "firm" if inside(convex_hull(contact), centre) else "overhanging"


def check(response, totals):
    clean_order = True
    for box in response["boxes"]:
        states = [classify(item, box["items"]) for item in box["items"]]
        floating = states.count("floating")
        overhanging = states.count("overhanging")
        totals["boxes"] += 1
        totals["items"] += len(states)
        totals["floating"] += floating
        totals["overhanging"] += overhanging
        totals["boxes_with_floating"] += 1 if floating else 0
        totals["boxes_all_firm"] += 0 if (floating or overhanging) else 1
        clean_order = clean_order and not (floating or overhanging)
    totals["orders"] += 1
    totals["orders_all_firm"] += 1 if clean_order else 0
    totals["unplaced"] += len(response["unplaced_item_codes"])


def report(title, totals):
    def share(part, whole):
        return f"{part} of {whole} ({100.0 * part / whole:.1f}%)" if whole else "0"

    print(title)
    print(f"  orders {totals['orders']}, boxes {totals['boxes']}, items placed {totals['items']}, "
          f"unplaced {totals['unplaced']}")
    print(f"  floating items:      {share(totals['floating'], totals['items'])}")
    print(f"  overhanging items:   {share(totals['overhanging'], totals['items'])}")
    print(f"  boxes with a floating item: {share(totals['boxes_with_floating'], totals['boxes'])}")
    print(f"  boxes with every item firm: {share(totals['boxes_all_firm'], totals['boxes'])}")
    print(f"  orders with every item firm: {share(totals['orders_all_firm'], totals['orders'])}")


def new_totals():
    return dict(orders=0, boxes=0, items=0, unplaced=0, floating=0, overhanging=0,
                boxes_with_floating=0, boxes_all_firm=0, orders_all_firm=0)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--lib", required=True, help="path to the built fitsolver_lib (.dll/.so/.dylib)")
    parser.add_argument("--scenarios", default=str(REPO / "benchmarks" / "boxpacker_comparison" / "scenarios.json"))
    parser.add_argument("--thommeax", action="store_true", help="check the three Thommeax scenarios instead")
    parser.add_argument("--json", help="also write the totals to this file")
    args = parser.parse_args()

    solve = load_engine(args.lib)
    results = {}
    if args.thommeax:
        for name in ("Simple", "SemiRealistic", "Chaotic"):
            folder = REPO / "tests" / "ThommeaxMinimumTests" / name
            boxes = [box for box in json.loads((folder / "boxes.json").read_text())
                     if box.get("MaxWeight") is not None]
            items = expand(json.loads((folder / "items.json").read_text()))
            totals = new_totals()
            check(solve({"items": items, "boxes": boxes}), totals)
            report(f"Thommeax {name}", totals)
            results[name] = totals
    else:
        totals = new_totals()
        for scenario in json.loads(pathlib.Path(args.scenarios).read_text()):
            check(solve({"items": expand(scenario["items"]), "boxes": scenario["boxes"]}), totals)
        report(pathlib.Path(args.scenarios).name, totals)
        results["scenarios"] = totals

    if args.json:
        pathlib.Path(args.json).write_text(json.dumps(results, indent=2))
    # Floating items are a defect; overhanging ones are reported, not failed.
    return 1 if any(totals["floating"] for totals in results.values()) else 0


if __name__ == "__main__":
    sys.exit(main())
