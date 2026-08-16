#pragma once

#include "PackedBoxSorter.h"

namespace fitsolver {

// Most items packed wins; ties broken by higher volume utilisation, then
// by higher used volume. Mirrors BoxPacker's `DefaultPackedBoxSorter`.
class DefaultPackedBoxSorter : public PackedBoxSorter {
 public:
  int Compare(const PackedBox& a, const PackedBox& b) const override;
};

}  // namespace fitsolver
