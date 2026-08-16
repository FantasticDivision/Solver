#include "LimitedSupplyBox.h"

namespace fitsolver {

nlohmann::json LimitedSupplyBox::ToJson() const {
  nlohmann::json j = Box::ToJson();
  j["MaximumBoxes"] = GetQuantityAvailable();
  return j;
}

}  // namespace fitsolver
