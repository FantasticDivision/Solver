#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "BoxList.h"
#include "PackedBox.h"
#include "PackedBoxList.h"

namespace fitsolver {

// Given a completed packing, tries to move items between boxes to
// reduce weight variance across them without changing how many boxes
// were used (or, if moving a box's last item empties it, eliminating
// that box entirely). Mirrors BoxPacker's `WeightRedistributor`.
//
// Simplification vs. BoxPacker: box quantity limits (LimitedSupplyBox)
// aren't re-checked during redistribution, since a move only ever
// reuses box types already present in the result or repacks into a
// smaller one -- it never *adds* a new box, so this can't violate a
// quantity limit that the initial Packer pass already respected.
class WeightRedistributor {
 public:
  explicit WeightRedistributor(BoxList boxes);

  PackedBoxList Redistribute(const PackedBoxList& original) const;

 private:
  // Tries one move between boxes[i] and boxes[j]; returns true (and
  // mutates `boxes`) on the first move found that helps.
  bool TryEqualiseWeight(std::vector<PackedBox>& boxes, std::size_t i, std::size_t j,
                          double target_weight) const;

  // Packs `items` into the smallest single active box type that fits
  // all of them, or std::nullopt if none does.
  std::optional<PackedBox> RepackSingleBox(const std::vector<ItemPtr>& items) const;

  BoxList boxes_;
};

}  // namespace fitsolver
