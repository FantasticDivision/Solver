#pragma once

#include "BoxSorter.h"

namespace fitsolver {

// Smallest inner volume first, then lightest empty weight, then largest
// usable weight capacity as a fallback. Mirrors BoxPacker's
// `DefaultBoxSorter`.
class DefaultBoxSorter : public BoxSorter {
 public:
  int Compare(const Box& a, const Box& b) const override;
};

}  // namespace fitsolver
