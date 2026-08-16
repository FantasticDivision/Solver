#pragma once

#include "ItemSorter.h"

namespace fitsolver {

// Largest volume first, then heaviest, then item code as a stable
// tie-break. Mirrors BoxPacker's `DefaultItemSorter`.
class DefaultItemSorter : public ItemSorter {
 public:
  int Compare(const Item& a, const Item& b) const override;
};

}  // namespace fitsolver
