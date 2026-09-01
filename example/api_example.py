import json
import os

import _bootstrap  # noqa: F401  (side effect: puts the built fitsolver module on sys.path)
import fitsolver


def demo_pack_dicts() -> None:
    print("=" * 70)
    print("fitsolver.pack_dicts(items, boxes) -- plain dicts in, rich objects out")
    print("=" * 70)

    here = os.path.dirname(os.path.abspath(__file__))
    with open(os.path.join(here, "..", "tests", "items.json")) as f:
        items = json.load(f)
    with open(os.path.join(here, "..", "tests", "boxes.json")) as f:
        boxes = json.load(f)

    result = fitsolver.pack_dicts(items, boxes)

    print(f"result: {result!r}")
    print(f"success: {result.success}")
    print(f"overall volume utilisation: {result.overall_volume_utilisation:.1f}%")
    print(f"total placed / unplaced: {result.total_placed} / {result.total_unplaced}")

    for box in result.boxes:
        print(f"\n{box!r}")
        print(f"  reference:            {box.box.reference}")
        print(f"  dimensions (WxLxD):   {box.box.width} x {box.box.length} x {box.box.depth}")
        print(f"  weight (incl. box):   {box.weight}")
        print(f"  max weight:           {box.max_weight}")
        print(f"  remaining weight:     {box.remaining_weight}")
        print(f"  volume utilisation:   {box.volume_utilisation:.1f}%")
        print(f"  remaining W/L/D:      "
              f"{box.remaining_width} / {box.remaining_length} / {box.remaining_depth}")

        for packed_item in box.items:
            print(f"    - {packed_item!r}")
            print(f"        original item:   {packed_item.item.item_reference}")
            print(f"        position:        ({packed_item.x}, {packed_item.y}, {packed_item.z})")
            print(f"        placed W/L/D:     "
                  f"{packed_item.width} x {packed_item.length} x {packed_item.depth}")
            print(f"        orientation:      {packed_item.orientation} "
                  f"(rotated: {packed_item.rotated})")
            print(f"        weight:           {packed_item.weight}")

    if result.unplaced_items:
        print("\nunplaced items:")
        for item in result.unplaced_items:
            print(f"  - {item!r} ({item.item_reference})")

    # Every object also has .to_dict() if you decide later you *do* want
    # plain-dict/JSON access after all -- no need to pick one style only.
    print("\nresult.to_dict() keys:", list(result.to_dict().keys()))


def demo_pack_objects() -> None:
    print()
    print("=" * 70)
    print("fitsolver.pack(items, boxes) -- built from fitsolver.Item / fitsolver.Box")
    print("=" * 70)

    items = [
        fitsolver.Item("A1", "Small crate", width=100, length=100, depth=100, weight=2.0),
        fitsolver.Item("A2", "Long pole", width=20, length=20, depth=400, weight=1.5,
                        allowed_rotation=fitsolver.Rotation.BEST_FIT),
        fitsolver.Item("A3", "Fragile panel", width=300, length=200, depth=10, weight=0.9,
                        allowed_rotation=fitsolver.Rotation.KEEP_FLAT),
    ]
    boxes = [
        fitsolver.Box("STANDARD", width=500, length=500, depth=500, max_weight=20.0,
                       empty_weight=1.0),
    ]

    result = fitsolver.pack(items, boxes)
    print(f"packed {result.total_placed} of {len(items)} items into {len(result.boxes)} box(es)")
    for box in result.boxes:
        for packed_item in box.items:
            print(f"  {packed_item.item.item_code}: orientation={packed_item.orientation} "
                  f"placed_dims={packed_item.width}x{packed_item.length}x{packed_item.depth}")


if __name__ == "__main__":
    demo_pack_dicts()
    demo_pack_objects()
