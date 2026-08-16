#pragma once

namespace fitsolver {

class PackedBox;

// Determines "which packed box is better" -- used to rank candidate box
// choices during packing and to order PackedBoxList's iteration. Mirrors
// BoxPacker's `PackedBoxSorter` interface.
class PackedBoxSorter {
 public:
  virtual ~PackedBoxSorter() = default;

  // Return < 0 if `a` is "best", > 0 if `b` is "best", or 0 if neither
  // is preferred.
  virtual int Compare(const PackedBox& a, const PackedBox& b) const = 0;
};

}  // namespace fitsolver
