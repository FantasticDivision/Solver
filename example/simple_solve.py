"""
simple_solve.py

The easiest possible way to call FitSolver from Python: build a
{"items": [...], "boxes": [...]} request, pass it as a JSON string to
fitsolver.solve(), get a JSON string back. No fitsolver types, no object
model to learn -- just JSON in, JSON out.

This is the right entry point if all you want is "give me a JSON file
with all the packing data in it".

Run (after building -- see README "Python bindings" section):
    python simple_solve.py
"""

import json
import os

import _bootstrap  # noqa: F401  (side effect: puts the built fitsolver module on sys.path)
import fitsolver


def main() -> None:
    here = os.path.dirname(os.path.abspath(__file__))
    with open(os.path.join(here, "..", "tests", "fixtures", "items.json")) as f:
        items = json.load(f)
    with open(os.path.join(here, "..", "tests", "fixtures", "boxes.json")) as f:
        boxes = json.load(f)

    request = {"items": items, "boxes": boxes}
    response_json = fitsolver.solve(json.dumps(request))
    response = json.loads(response_json)

    print(json.dumps(response, indent=2))

    print()
    print(f"success: {response['success']}")
    print(f"boxes used: {len(response['boxes'])}")
    print(f"items placed: {len(response['placements'])}")
    print(f"items unplaced: {response['unplaced_item_codes']}")

    # Nothing stops you writing the response straight to a file, which is
    # probably what another service consuming this will want to do:
    out_path = os.path.join(here, "result.json")
    with open(out_path, "w") as f:
        json.dump(response, f, indent=2)
    print(f"\nWrote full result to {out_path}")


if __name__ == "__main__":
    main()
