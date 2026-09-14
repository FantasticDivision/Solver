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
  double GetSurfaceFootprint() const { return width_ * length_; }
  // "WLD" = original order, "LWD" = width/length swapped, etc.
  const std::string& GetOrientationCode() const { return code_; }
  bool IsRotated() const { return code_ != "WLD"; }

  // Low centre of gravity, tipping point modelled as >15 degrees (mirrors
  // BoxPacker's OrientatedItem::isStable(); assumes uniform weight
  // distribution across the item).
  bool IsStable() const;

  // True if `item`'s own (unrotated) dimensions match this orientation's,
  // as a multiset -- i.e. some rotation of `item` would occupy the same
  // footprint. Used to special-case "just place it the same way as the
  // last, identical item" and to detect runs of identical items.
  bool IsSameDimensions(const ItemPtr& item) const;

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
