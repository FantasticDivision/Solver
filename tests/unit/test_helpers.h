#pragma once

#include <memory>
#include <string>

#include "ConcreteBox.h"
#include "ConcreteItem.h"
#include "OrientatedItem.h"
#include "PackedItem.h"

namespace fitsolver::test {

inline ItemPtr MakeItem(const std::string& code, double width, double length, double depth,
                        double weight, Rotation rotation = Rotation::kBestFit) {
  return std::make_shared<ConcreteItem>(code, code, width, length, depth, weight, rotation);
}

inline BoxPtr MakeBox(const std::string& reference, double width, double length, double depth,
                      double max_weight = kNoWeightLimit, double empty_weight = 0.0,
                      bool active = true) {
  return std::make_shared<ConcreteBox>(reference, width, length, depth, max_weight, empty_weight,
                                       active);
}

inline PackedItem MakePackedItem(const ItemPtr& item, double x = 0.0, double y = 0.0,
                                 double z = 0.0) {
  return PackedItem(OrientatedItem(item, item->GetWidth(), item->GetLength(), item->GetDepth(),
                                   "WLD"),
                    x, y, z);
}

}  // namespace fitsolver::test
