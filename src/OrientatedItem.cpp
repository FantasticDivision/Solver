#include "OrientatedItem.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace fitsolver {

OrientatedItem::OrientatedItem(ItemPtr item, double width, double length, double depth,
                                std::string code)
    : item_(std::move(item)), width_(width), length_(length), depth_(depth),
      code_(std::move(code)) {}

bool OrientatedItem::IsStable() const {
  const double depth = depth_ != 0.0 ? depth_ : 1.0;
  return std::atan(std::min(width_, length_) / depth) > 0.261;
}

bool OrientatedItem::IsSameDimensions(const ItemPtr& item) const {
  if (item == item_) return true;
  std::array<double, 3> mine{width_, length_, depth_};
  std::array<double, 3> theirs{item->GetWidth(), item->GetLength(), item->GetDepth()};
  std::sort(mine.begin(), mine.end());
  std::sort(theirs.begin(), theirs.end());
  return mine == theirs;
}

std::vector<OrientatedItem> GenerateOrientations(const ItemPtr& item) {
  const double w = item->GetWidth();
  const double l = item->GetLength();
  const double d = item->GetDepth();

  switch (item->GetAllowedRotation()) {
    case Rotation::kNever:
      return {OrientatedItem(item, w, l, d, "WLD")};
    case Rotation::kKeepFlat:
      return {OrientatedItem(item, w, l, d, "WLD"), OrientatedItem(item, l, w, d, "LWD")};
    case Rotation::kBestFit:
    default:
      return {
          OrientatedItem(item, w, l, d, "WLD"), OrientatedItem(item, l, w, d, "LWD"),
          OrientatedItem(item, w, d, l, "WDL"), OrientatedItem(item, l, d, w, "LDW"),
          OrientatedItem(item, d, w, l, "DWL"), OrientatedItem(item, d, l, w, "DLW"),
      };
  }
}

}  // namespace fitsolver
