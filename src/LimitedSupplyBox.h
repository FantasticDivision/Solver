#pragma once

#include "Box.h"

namespace fitsolver {

// A Box with a caller-imposed limit on how many physical instances of it
// are available -- only implement/use this (instead of a plain Box) if
// that limit is actually needed. Mirrors BoxPacker's `LimitedSupplyBox`
// interface exactly.
//
// ConcreteLimitedSupplyBox (ConcreteBox.h) is the ready-made
// implementation; Packer detects it via `dynamic_cast<const
// LimitedSupplyBox*>`, the same way BoxPacker uses `instanceof` (falling
// back to "unlimited" for a plain Box, exactly like BoxPacker falls back
// to PHP_INT_MAX).
class LimitedSupplyBox : public Box {
 public:
  // Always a real, finite count for a box actually wrapped in this
  // interface -- there's no "unlimited" value here, just don't use this
  // interface for a box with no quantity limit.
  virtual int GetQuantityAvailable() const = 0;

  nlohmann::json ToJson() const override;
};

}  // namespace fitsolver
