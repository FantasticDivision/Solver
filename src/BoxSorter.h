#pragma once

#include "Box.h"

namespace fitsolver {

// Determines try-order between two box types. Implement this to change
// how BoxList orders its contents; see DefaultBoxSorter for the built-in
// strategy. Mirrors BoxPacker's `BoxSorter` interface.
class BoxSorter {
 public:
  virtual ~BoxSorter() = default;

  // Return < 0 if `a` should be tried before `b`, > 0 if `b` first, or 0
  // if neither is preferred.
  virtual int Compare(const Box& a, const Box& b) const = 0;
};

}  // namespace fitsolver
