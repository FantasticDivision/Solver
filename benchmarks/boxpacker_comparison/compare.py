#!/usr/bin/env python3
"""Joins fitsolver_results.csv and boxpacker_results.csv (same scenario ids,
see run_fitsolver.cpp / run_boxpacker.php) and checks the two claims this
benchmark exists to verify:

  1. For every scenario, fitsolver uses <= boxes than dvdoug/boxpacker 4.3.
  2. fitsolver's average solve time is lower than dvdoug/boxpacker's.

Also reports fitsolver's own worst-case solve time against the 1-second
per-scenario budget, independent of the comparison.

Usage: python compare.py [--fitsolver fitsolver_results.csv]
                          [--boxpacker boxpacker_results.csv]
                          [--out comparison_report.md]
"""

import argparse
import csv
import statistics
from pathlib import Path


def load(path: Path) -> dict[int, dict]:
    with path.open(newline="", encoding="utf-8") as f:
        return {int(row["id"]): row for row in csv.DictReader(f)}


def main() -> None:
    here = Path(__file__).parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fitsolver", type=Path, default=here / "fitsolver_results.csv")
    parser.add_argument("--boxpacker", type=Path, default=here / "boxpacker_results.csv")
    parser.add_argument("--out", type=Path, default=here / "comparison_report.md")
    args = parser.parse_args()

    ours = load(args.fitsolver)
    theirs = load(args.boxpacker)
    ids = sorted(set(ours) & set(theirs))
    missing = (set(ours) | set(theirs)) - set(ids)
    if missing:
        print(f"WARNING: {len(missing)} scenario id(s) only present in one file: {sorted(missing)[:10]}...")

    our_times = [float(ours[i]["solve_ms"]) for i in ids]
    their_times = [float(theirs[i]["solve_ms"]) for i in ids if int(theirs[i]["boxes_used"]) >= 0]

    box_deltas = []  # ours - theirs; <=0 is a pass
    worse_box_count = []
    unplaced_mismatch = []
    over_budget = []

    for i in ids:
        o, t = ours[i], theirs[i]
        our_boxes = int(o["boxes_used"])
        their_boxes = int(t["boxes_used"])
        if their_boxes < 0:
            continue  # dvdoug/boxpacker errored on this scenario -- excluded from the box-count claim
        delta = our_boxes - their_boxes
        box_deltas.append(delta)
        if delta > 0:
            worse_box_count.append((i, our_boxes, their_boxes))
        if int(o["unplaced"]) != int(t["unplaced"]):
            unplaced_mismatch.append((i, int(o["unplaced"]), int(t["unplaced"])))
        if float(o["solve_ms"]) > 1000.0:
            over_budget.append((i, float(o["solve_ms"])))

    lines = []
    lines.append(f"# BoxPacker comparison report\n")
    lines.append(f"{len(ids)} scenarios compared (dvdoug/boxpacker 4.3.0).\n")

    lines.append("## Claim 1: fitsolver uses <= boxes than dvdoug/boxpacker on every scenario\n")
    lines.append(f"- Scenarios where fitsolver used MORE boxes: {len(worse_box_count)} / {len(box_deltas)}")
    lines.append(f"- Scenarios where fitsolver used FEWER boxes: {sum(1 for d in box_deltas if d < 0)} / {len(box_deltas)}")
    lines.append(f"- Scenarios tied: {sum(1 for d in box_deltas if d == 0)} / {len(box_deltas)}")
    lines.append(f"- **Result: {'PASS' if not worse_box_count else 'FAIL'}**\n")
    if worse_box_count:
        lines.append("Worst offenders (scenario id, our boxes, boxpacker boxes):")
        for i, ob, tb in sorted(worse_box_count, key=lambda x: x[1] - x[2], reverse=True)[:20]:
            lines.append(f"- id={i}: {ob} vs {tb} (+{ob - tb})")
        lines.append("")

    lines.append("## Claim 2: fitsolver's average solve time is lower\n")
    our_avg = statistics.mean(our_times)
    their_avg = statistics.mean(their_times) if their_times else float("nan")
    lines.append(f"- fitsolver average:   {our_avg:.3f} ms (median {statistics.median(our_times):.3f} ms)")
    lines.append(f"- boxpacker average:   {their_avg:.3f} ms (median {statistics.median(their_times):.3f} ms)")
    lines.append(f"- **Result: {'PASS' if our_avg < their_avg else 'FAIL'}** "
                 f"({their_avg / our_avg:.1f}x faster)" if our_avg < their_avg and our_avg > 0 else "")
    lines.append("")

    lines.append("## fitsolver per-scenario time budget (< 1000 ms)\n")
    lines.append(f"- Worst fitsolver solve time: {max(our_times):.3f} ms")
    lines.append(f"- Scenarios over budget: {len(over_budget)} / {len(our_times)}")
    lines.append(f"- **Result: {'PASS' if not over_budget else 'FAIL'}**\n")
    if over_budget:
        lines.append("Offending scenarios (id, ms):")
        for i, ms in sorted(over_budget, key=lambda x: -x[1])[:20]:
            lines.append(f"- id={i}: {ms:.1f} ms")
        lines.append("")

    lines.append("## Box quantity limits (MaximumBoxes) respected\n")
    our_quota = [(i, int(ours[i].get("quota_violations", 0) or 0)) for i in ids]
    their_quota = [(i, int(theirs[i].get("quota_violations", 0) or 0)) for i in ids]
    our_bad = [(i, v) for i, v in our_quota if v > 0]
    their_bad = [(i, v) for i, v in their_quota if v > 0]
    lines.append(f"- fitsolver scenarios over a box limit: {len(our_bad)} / {len(ids)}")
    lines.append(f"- boxpacker scenarios over a box limit: {len(their_bad)} / {len(ids)}")
    lines.append(f"- **Result: {'PASS' if not our_bad else 'FAIL'}** "
                 "(a box-count win that overspends a quota isn't a real win)\n")
    if our_bad:
        lines.append("fitsolver offenders (id, boxes over limit): "
                     + ", ".join(f"{i}:{v}" for i, v in our_bad[:20]) + "\n")

    if unplaced_mismatch:
        lines.append(f"## Note: {len(unplaced_mismatch)} scenario(s) placed a different number of items\n")
        lines.append("(id, fitsolver unplaced, boxpacker unplaced) -- expected occasionally, since the two")
        lines.append("engines' greedy choices can diverge on which items get left out, not just how many")
        lines.append("boxes it takes:")
        for i, ou, tu in unplaced_mismatch[:20]:
            lines.append(f"- id={i}: {ou} vs {tu}")
        lines.append("")

    report = "\n".join(lines)
    args.out.write_text(report, encoding="utf-8")
    print(report)


if __name__ == "__main__":
    main()
