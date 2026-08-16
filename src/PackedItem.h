#pragma once

#include "OrientatedItem.h"
#include "nlohmann/json.hpp"

namespace fitsolver {

// An Item placed at a specific position inside a box, in a specific
// orientation. Mirrors BoxPacker's `PackedItem`.
class PackedItem {
 public:
  PackedItem(OrientatedItem orientation, double x, double y, double z);

  const ItemPtr& GetItem() const { return orientation_.GetItem(); }
  double GetX() const { return x_; }
  double GetY() const { return y_; }
  double GetZ() const { return z_; }
  double GetWidth() const { return orientation_.GetWidth(); }
  double GetLength() const { return orientation_.GetLength(); }
  double GetDepth() const { return orientation_.GetDepth(); }
  double GetWeight() const { return GetItem()->GetWeight(); }
  double GetVolume() const { return orientation_.GetVolume(); }
  const std::string& GetOrientationCode() const { return orientation_.GetOrientationCode(); }
  bool IsRotated() const { return orientation_.IsRotated(); }

  // The same item, translated by (dx, dy, dz) -- used when merging a
  // sub-region packing (see VolumePacker's void-filling) back into its
  // parent box's coordinate frame.
  PackedItem WithOffset(double dx, double dy, double dz) const;

  nlohmann::json ToJson() const;

 private:
  OrientatedItem orientation_;
  double x_;
  double y_;
  double z_;
};

}  // namespace fitsolver
