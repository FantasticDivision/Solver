#pragma once

#include <memory>
#include <string>

#include "nlohmann/json.hpp"

namespace fitsolver {

// Sentinel meaning "no weight limit configured" so callers don't have to
// thread std::optional through every arithmetic path. Every Box answers
// GetMaxWeight() (mirroring BoxPacker's Box interface, which always
// requires one); this project's wire format just allows omitting it.
inline constexpr double kNoWeightLimit = -1.0;

// Contract for a box type items can be packed into. Mirrors BoxPacker's
// `Box` interface: implement this directly for a custom box type, or use
// the ready-made ConcreteBox (ConcreteBox.h) if a plain data holder is
// enough.
//
// Extension point: see LimitedSupplyBox.h for how an *optional* extra
// capability (a caller-set quantity limit) is layered on without
// touching this interface -- the same pattern applies to any future
// per-box behaviour.
class Box {
 public:
  virtual ~Box() = default;

  virtual const std::string& GetReference() const = 0;
  virtual double GetWidth() const = 0;
  virtual double GetLength() const = 0;
  virtual double GetDepth() const = 0;
  virtual double GetEmptyWeight() const = 0;
  virtual double GetMaxWeight() const = 0;  // kNoWeightLimit if unlimited
  virtual bool GetActive() const = 0;

  virtual bool HasWeightLimit() const;
  virtual double GetInnerVolume() const;
  // Weight budget available for contents (MaxWeight - EmptyWeight), or
  // kNoWeightLimit if this box has no configured weight limit.
  virtual double GetWeightCapacityForItems() const;

  virtual nlohmann::json ToJson() const;
};

// See ItemPtr's comment in Item.h -- same reasoning applies here.
using BoxPtr = std::shared_ptr<Box>;

}  // namespace fitsolver
