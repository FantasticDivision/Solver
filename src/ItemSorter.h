#pragma once

#include "Item.h"

namespace fitsolver {

// Determines packing priority order between two items. Implement this to
// change how ItemList orders its contents; see DefaultItemSorter for the
// built-in strategy. Mirrors BoxPacker's `ItemSorter` interface.
class ItemSorter {
 public:
  virtual ~ItemSorter() = default;

  // Return < 0 if `a` should be tried before `b`, > 0 if `b` first, or 0
  // if neither is preferred.
  virtual int Compare(const Item& a, const Item& b) const = 0;
};

}  // namespace fitsolver
