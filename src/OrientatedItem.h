#pragma once

#include <string>
#include <vector>

#include "Item.h"

namespace fitsolver {

// One of the (up to 6) ways an item's Width/Length/Depth can be assigned
// to a box's X/Y/Z axes, subject to the item's allowed Rotation. Mirrors
// BoxPacker's `OrientatedItem`.
class OrientatedItem {
 public:
  OrientatedItem(ItemPtr item, double width, double length, double depth, std::string code);

  const ItemPtr& GetItem() const { return item_; }
  double GetWidth() const { return width_; }
  double GetLength() const { return length_; }
  double GetDepth() const { return depth_; }
  double GetVolume() const { return width_ * length_ * depth_; }
  // "WLD" = original order, "LWD" = width/length swapped, etc.
  const std::string& GetOrientationCode() const { return code_; }
  bool IsRotated() const { return code_ != "WLD"; }

 private:
  ItemPtr item_;
  double width_;
  double length_;
  double depth_;
  std::string code_;
};

// All orientations of `item` that its allowed Rotation permits (1 for
// Rotation::kNever, 2 for kKeepFlat, up to 6 for kBestFit).
std::vector<OrientatedItem> GenerateOrientations(const ItemPtr& item);

}  // namespace fitsolver
