#!/usr/bin/env python3
"""Generates a deterministic set of random packing scenarios shared by both
sides of the comparison (fitsolver's C++ engine and dvdoug/boxpacker's PHP
engine), so both are timed and scored against the exact same inputs.

Each scenario uses only the feature subset both engines implement and have
already been verified to agree on (see FantasticSolver/README.md's
"Architecture" section, and tests/ThommeaxMinimumTests/): item/box
dimensions, item weight, AllowedRotation, box MaxWeight/BoxWeight/Active/
MaximumBoxes. BoxGroup is deliberately left out -- it exists on both engines
but is a segregation *policy* rather than a packing-density concern, and
leaving it out keeps this a clean comparison of the core bin-packing result.

Usage:
    python generate_scenarios.py [--count 1000] [--seed 1337] [--out scenarios.json]
"""

import argparse
import json
import random
from pathlib import Path

ROTATIONS = ["BestFit", "BestFit", "BestFit", "BestFit", "KeepFlat", "Never"]


def make_item_type(rng: random.Random, index: int) -> dict:
    width = rng.randint(20, 600)
    length = rng.randint(20, 600)
    depth = rng.randint(20, 600)
    weight = round(rng.uniform(0.01, 15.0), 2)
    return {
        "ItemCode": f"ITEM-{index}",
        "ItemReference": f"Random item {index}",
        "Width": width,
        "Length": length,
        "Depth": depth,
        "Weight": weight,
        "AllowedRotation": rng.choice(ROTATIONS),
    }


def partition(rng: random.Random, total: int, parts: int) -> list[int]:
    """`total` split into `parts` positive integers, order randomised."""
    if parts == 1:
        return [total]
    cuts = sorted(rng.sample(range(1, total), parts - 1))
    bounds = [0, *cuts, total]
    return [bounds[i + 1] - bounds[i] for i in range(parts)]


def make_box_type(rng: random.Random, index: int, max_item_dim: int, total_units: int) -> dict:
    # Boxes skew larger than the items in this scenario, like a real
    # warehouse's carton range, so most items have somewhere to go.
    lo = max(150, max_item_dim)
    hi = max(lo + 50, min(1300, max_item_dim * 4))
    width = rng.randint(lo, hi)
    length = rng.randint(lo, hi)
    depth = rng.randint(lo, hi)

    contents_limit = round(rng.uniform(3.0, 60.0), 2)
    box_weight = round(contents_limit * rng.uniform(0.02, 0.08), 2)

    box: dict = {
        "Reference": f"BOX-{index}",
        "Width": width,
        "Length": length,
        "Depth": depth,
        # FitSolver's MaxWeight is GROSS (box + contents), matching
        # dvdoug/boxpacker's Box::getMaxWeight() -- see README.md.
        "MaxWeight": round(contents_limit + box_weight, 2),
        "BoxWeight": box_weight,
        "Active": rng.random() < 0.9,
    }
    if rng.random() < 0.3:
        # Loose enough that most scenarios stay solvable; still exercises
        # LimitedSupplyBox / Box_Types.quantity on both engines.
        box["MaximumBoxes"] = max(1, total_units // 3 + rng.randint(0, 5))
    return box


def make_scenario(rng: random.Random, scenario_id: int) -> dict:
    total_units = rng.randint(5, 120)
    item_type_count = rng.randint(1, min(12, total_units))
    quantities = partition(rng, total_units, item_type_count)

    item_types = [make_item_type(rng, i) for i in range(item_type_count)]
    for item, qty in zip(item_types, quantities):
        item["Quantity"] = qty

    max_item_dim = max(max(it["Width"], it["Length"], it["Depth"]) for it in item_types)
    box_type_count = rng.randint(2, 6)
    box_types = [
        make_box_type(rng, i, max_item_dim, total_units) for i in range(box_type_count)
    ]
    # At least one unlimited, active box guarantees the scenario is not
    # trivially unsolvable by construction (still may leave odd items
    # unplaced on weight/shape grounds -- that is a legitimate outcome).
    box_types[0]["Active"] = True
    box_types[0].pop("MaximumBoxes", None)

    return {
        "id": scenario_id,
        "total_units": total_units,
        "items": item_types,
        "boxes": box_types,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--count", type=int, default=1000)
    parser.add_argument("--seed", type=int, default=1337)
    parser.add_argument("--out", type=Path, default=Path(__file__).parent / "scenarios.json")
    args = parser.parse_args()

    rng = random.Random(args.seed)
    scenarios = [make_scenario(rng, i) for i in range(args.count)]

    args.out.write_text(json.dumps(scenarios, indent=1), encoding="utf-8")
    total_units = sum(s["total_units"] for s in scenarios)
    print(f"Wrote {len(scenarios)} scenarios ({total_units} item units total) to {args.out}")


if __name__ == "__main__":
    main()
