#include "PackedBox.h"

#include <algorithm>

namespace fitsolver {

PackedBox::PackedBox(BoxPtr box) : box_(std::move(box)) {}

void PackedBox::AddItem(PackedItem item) { items_.Insert(std::move(item)); }

double PackedBox::GetWeight() const { return box_->GetEmptyWeight() + GetItemWeight(); }

double PackedBox::GetRemainingWeight() const {
  if (!box_->HasWeightLimit()) return kNoWeightLimit;
  const double remaining = box_->GetMaxWeight() - GetWeight();
  return remaining > 0.0 ? remaining : 0.0;
}

double PackedBox::GetVolumeUtilisation() const {
  const double inner = GetInnerVolume();
  if (inner <= 0.0) return 0.0;
  return (GetUsedVolume() / inner) * 100.0;
}

double PackedBox::GetUsedWidth() const {
  double max_extent = 0.0;
  for (const auto& item : items_) max_extent = std::max(max_extent, item.GetX() + item.GetWidth());
  return max_extent;
}

double PackedBox::GetUsedLength() const {
  double max_extent = 0.0;
  for (const auto& item : items_) max_extent = std::max(max_extent, item.GetY() + item.GetLength());
  return max_extent;
}

double PackedBox::GetUsedDepth() const {
  double max_extent = 0.0;
  for (const auto& item : items_) max_extent = std::max(max_extent, item.GetZ() + item.GetDepth());
  return max_extent;
}

nlohmann::json PackedBox::ToJson() const {
  nlohmann::json j;
  j["box_reference"] = box_->GetReference();
  j["width"] = box_->GetWidth();
  j["length"] = box_->GetLength();
  j["depth"] = box_->GetDepth();
  j["max_weight"] = box_->HasWeightLimit() ? nlohmann::json(box_->GetMaxWeight()) : nlohmann::json(nullptr);
  j["empty_weight"] = box_->GetEmptyWeight();
  j["weight"] = GetWeight();
  j["item_weight"] = GetItemWeight();
  j["remaining_weight"] =
      box_->HasWeightLimit() ? nlohmann::json(GetRemainingWeight()) : nlohmann::json(nullptr);
  j["used_volume"] = GetUsedVolume();
  j["inner_volume"] = GetInnerVolume();
  j["unused_volume"] = GetUnusedVolume();
  j["volume_utilisation"] = GetVolumeUtilisation();
  j["used_width"] = GetUsedWidth();
  j["used_length"] = GetUsedLength();
  j["used_depth"] = GetUsedDepth();
  j["remaining_width"] = GetRemainingWidth();
  j["remaining_length"] = GetRemainingLength();
  j["remaining_depth"] = GetRemainingDepth();
  j["item_count"] = items_.Count();

  nlohmann::json items_json = nlohmann::json::array();
  for (const auto& item : items_) items_json.push_back(item.ToJson());
  j["items"] = std::move(items_json);
  return j;
}

}  // namespace fitsolver
