#include "OrientatedItem.h"

namespace fitsolver {

OrientatedItem::OrientatedItem(ItemPtr item, double width, double length, double depth,
                                std::string code)
    : item_(std::move(item)), width_(width), length_(length), depth_(depth),
      code_(std::move(code)) {}

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
