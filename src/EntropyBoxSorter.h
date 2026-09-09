#pragma once

#include "BoxSorter.h"

namespace fitsolver {

// A refinement of DefaultBoxSorter that accounts for shape as well as
// size. Keeps DefaultBoxSorter's smallest-inner-volume-first primary key
// (prefer the smallest box that could plausibly work -- the standard
// Best-Fit container-selection principle), but breaks inner-volume ties
// by *descending* shape entropy: a more cube-like box (high
// ShapeEntropy) sorts before an equally-sized but oddly-proportioned
// one, since a cube accommodates the widest range of item orientations
// without wasting space. See PackingEntropy.h for the literature
// grounding.
//
// An alternative to DefaultBoxSorter, not a replacement for it --
// nothing in this codebase uses this by default; pass an
// EntropyBoxSorter to BoxList's constructor to opt in.
class EntropyBoxSorter : public BoxSorter {
 public:
  int Compare(const Box& a, const Box& b) const override;
};

}  // namespace fitsolver
