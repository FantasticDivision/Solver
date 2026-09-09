#pragma once

#include <vector>

namespace fitsolver {

// Information-theoretic measures used as secondary sort keys by
// EntropyItemSorter and EntropyBoxSorter.
//
// Background: classic decreasing-size bin-packing heuristics
// (First-Fit-Decreasing / Best-Fit-Decreasing) sort purely by size and
// carry a proven worst-case bound of FFD(I) <= 11/9 OPT(I) + 6/9
// [Dosa & Sgall, "First Fit bin packing: A tight analysis"; Coffman,
// Garey & Johnson, "Approximation algorithms for bin packing: a
// survey", 1997] -- that is exactly what DefaultItemSorter and
// DefaultBoxSorter already implement (largest-volume-first /
// smallest-box-first). The container-loading literature also documents
// that *shape*, not just size, affects how hard an item is to place
// well: aspect-ratio distribution measurably changes which heuristic
// performs best, and elongated/irregular items fragment leftover space
// more than cube-like items of the same volume [Bortfeldt & Wascher's
// container-loading survey; Crainic, Perboli & Tadei's extreme-point
// heuristics for 3D bin packing].
//
// Shannon entropy [Shannon, "A Mathematical Theory of Communication",
// 1948] is a standard way to turn "how unevenly is this thing spread
// out" into a single comparable number. Treating an item or box's three
// dimensions as a distribution and measuring how uniformly their extent
// is spread across axes is our own application of that tool to
// packing-difficulty scoring -- a practical heuristic proxy grounded in
// the general aspect-ratio-matters principle above, not a restatement
// of one specific paper that does exactly this. It is also unrelated to
// the "entropy method" from bin-packing *approximation theory* (Beck's
// entropy/discrepancy method; see e.g. Rothvoss's O(log OPT * log log
// OPT) result) -- that is an LP-rounding technique for proving
// approximation bounds, not a sort key for a greedy heuristic.

// Shannon entropy, in nats, of a distribution given as raw
// (non-negative, not-necessarily-normalized) weights: -sum(p_i *
// ln(p_i)) where p_i = weights[i] / sum(weights). Returns 0 for a
// degenerate input (all-zero, or all mass on a single entry) -- there
// is no disorder left to measure.
double ShannonEntropy(const std::vector<double>& weights);

// "Shape entropy" of a box or item's three dimensions: normalizes
// (width, length, depth) into a distribution and measures how evenly
// their extent is spread across axes. 0 for a fully degenerate
// flat/linear shape (all extent on one axis); maximal (ln 3) for a
// perfect cube -- the shape that can be oriented most freely without
// wasting space, regardless of which axis ends up where.
double ShapeEntropy(double width, double length, double depth);

}  // namespace fitsolver
