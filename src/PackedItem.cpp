#include "PackedItem.h"

#include "LinkedItem.h"

namespace fitsolver {

PackedItem::PackedItem(OrientatedItem orientation, double x, double y, double z)
    : orientation_(std::move(orientation)), x_(x), y_(y), z_(z) {}

PackedItem PackedItem::WithOffset(double dx, double dy, double dz) const {
  return PackedItem(orientation_, x_ + dx, y_ + dy, z_ + dz);
}

nlohmann::json PackedItem::ToJson() const {
  const Item& item = *GetItem();
  nlohmann::json j;
  j["ItemCode"] = item.GetItemCode();
  j["ItemReference"] = item.GetItemReference();
  j["x"] = x_;
  j["y"] = y_;
  j["z"] = z_;
  j["width"] = GetWidth();
  j["length"] = GetLength();
  j["depth"] = GetDepth();
  j["original_width"] = item.GetWidth();
  j["original_length"] = item.GetLength();
  j["original_depth"] = item.GetDepth();
  j["weight"] = GetWeight();
  j["rotated"] = IsRotated();
  j["orientation"] = GetOrientationCode();
  if (const auto* linked = dynamic_cast<const LinkedItem*>(&item)) {
    j["box_group"] = linked->GetBoxGroup();
  } else {
    j["box_group"] = nullptr;
  }
  return j;
}

}  // namespace fitsolver
