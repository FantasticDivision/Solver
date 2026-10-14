#include "PackedItem.h"

#include <algorithm>
#include <cmath>

#include "LinkedItem.h"

namespace fitsolver {

namespace {
constexpr double kOrderEps = 1e-6;
}  // namespace

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

void SortBottomUp(std::vector<PackedItem>& items) {
  std::stable_sort(items.begin(), items.end(), [](const PackedItem& a, const PackedItem& b) {
    if (std::abs(a.GetZ() - b.GetZ()) > kOrderEps) return a.GetZ() < b.GetZ();
    if (std::abs(a.GetY() - b.GetY()) > kOrderEps) return a.GetY() < b.GetY();
    return a.GetX() < b.GetX();
  });
}

}  // namespace fitsolver
