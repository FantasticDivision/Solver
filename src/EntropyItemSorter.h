#pragma once

#include "ItemSorter.h"

namespace fitsolver {

// A refinement of DefaultItemSorter that accounts for shape as well as
// size. Keeps DefaultItemSorter's box-group clustering (a correctness
// requirement for LinkedItem, not a heuristic choice -- see
// LinkedItem.h) and its largest-volume-first primary key (the
// proven-good Decreasing-size ordering from classic bin-packing
// literature -- see PackingEntropy.h), but breaks volume ties by
// *ascending* shape entropy: a more elongated/awkward-shaped item (low
// ShapeEntropy) sorts before a cube-like item of the same volume, so
// the packer commits the harder-to-place item first, while more space
// is still available to work with -- the same "resolve the most
// constrained thing first" principle used for variable ordering in
// constraint search, applied here via PackingEntropy.h's
// information-theoretic shape measure.
//
// An alternative to DefaultItemSorter, not a replacement for it --
// nothing in this codebase uses this by default; pass an
// EntropyItemSorter to ItemList's constructor to opt in.
class EntropyItemSorter : public ItemSorter {
 public:
  int Compare(const Item& a, const Item& b) const override;
};

}  // namespace fitsolver
