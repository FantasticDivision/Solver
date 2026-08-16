#include "LinkedItem.h"

namespace fitsolver {

nlohmann::json LinkedItem::ToJson() const {
  nlohmann::json j = Item::ToJson();
  j["BoxGroup"] = GetBoxGroup();
  return j;
}

}  // namespace fitsolver
