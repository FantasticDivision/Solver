#pragma once

#include <vector>

#include "PackedItem.h"
#include "VoidSpace.h"

namespace fitsolver {

// Finds empty rectangular regions inside a box given already-placed
// items. Starts from the full working volume, then for each packed item
// replaces every free region that intersects it with up to six leftover
// rectangular pieces (left, right, front, back, below, above). Mirrors
// BoxPacker's `VoidFinder`.
class VoidFinder {
 public:
  // Free rectangular spaces in a `width` x `length` x `depth` region,
  // given the items already placed inside it.
  std::vector<VoidSpace> Find(double width, double length, double depth,
                               const std::vector<PackedItem>& packed_items) const;

  // Same subtraction, applied on top of an existing set of spaces --
  // used to shrink the remaining voids after each void-fill pass places
  // more items.
  std::vector<VoidSpace> SubtractItems(std::vector<VoidSpace> spaces,
                                        const std::vector<PackedItem>& packed_items) const;
};

}  // namespace fitsolver
