#include "Item.h"

namespace fitsolver {

double Item::GetVolume() const { return GetWidth() * GetLength() * GetDepth(); }

nlohmann::json Item::ToJson() const {
  nlohmann::json j;
  j["ItemCode"] = GetItemCode();
  j["ItemReference"] = GetItemReference();
  j["Width"] = GetWidth();
  j["Length"] = GetLength();
  j["Depth"] = GetDepth();
  j["Weight"] = GetWeight();
  // Plain items have no box group; LinkedItem::ToJson() overwrites this
  // with the real group id, so the key is always present either way.
  j["BoxGroup"] = nullptr;
  j["AllowedRotation"] = RotationToString(GetAllowedRotation());
  j["Volume"] = GetVolume();
  return j;
}

}  // namespace fitsolver
