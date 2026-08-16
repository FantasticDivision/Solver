#include "Box.h"

namespace fitsolver {

bool Box::HasWeightLimit() const { return GetMaxWeight() >= 0.0; }

double Box::GetInnerVolume() const { return GetWidth() * GetLength() * GetDepth(); }

double Box::GetWeightCapacityForItems() const {
  if (!HasWeightLimit()) return kNoWeightLimit;
  const double capacity = GetMaxWeight() - GetEmptyWeight();
  return capacity > 0.0 ? capacity : 0.0;
}

nlohmann::json Box::ToJson() const {
  nlohmann::json j;
  j["Reference"] = GetReference();
  j["Width"] = GetWidth();
  j["Length"] = GetLength();
  j["Depth"] = GetDepth();
  j["MaxWeight"] = HasWeightLimit() ? nlohmann::json(GetMaxWeight()) : nlohmann::json(nullptr);
  j["BoxWeight"] = GetEmptyWeight();
  j["Active"] = GetActive();
  // Plain boxes have no quantity limit; LimitedSupplyBox::ToJson()
  // overwrites this with the real count, so the key is always present.
  j["MaximumBoxes"] = nullptr;
  j["InnerVolume"] = GetInnerVolume();
  return j;
}

}  // namespace fitsolver
