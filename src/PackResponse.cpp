#include "PackResponse.h"

namespace fitsolver {

double PackResponse::GetOverallVolumeUtilisation() const {
  double item_volume = 0.0;
  double box_volume = 0.0;
  for (const auto& box : boxes) {
    item_volume += box.GetUsedVolume();
    box_volume += box.GetInnerVolume();
  }
  if (box_volume <= 0.0) return 0.0;
  return (item_volume / box_volume) * 100.0;
}

int PackResponse::GetTotalPlacedCount() const {
  int total = 0;
  for (const auto& box : boxes) total += static_cast<int>(box.GetItems().Count());
  return total;
}

nlohmann::json PackResponse::ToJson() const {
  nlohmann::json j;
  j["success"] = success;

  nlohmann::json boxes_json = nlohmann::json::array();
  nlohmann::json placements_json = nlohmann::json::array();
  std::size_t box_index = 0;
  for (const auto& box : boxes) {
    nlohmann::json box_json = box.ToJson();
    box_json["box_index"] = box_index;
    boxes_json.push_back(box_json);

    for (const auto& item : box.GetItems()) {
      nlohmann::json placement = item.ToJson();
      placement["box_index"] = box_index;
      placement["box_reference"] = box.GetBox()->GetReference();
      placements_json.push_back(std::move(placement));
    }
    ++box_index;
  }
  j["boxes"] = std::move(boxes_json);
  j["placements"] = std::move(placements_json);

  nlohmann::json unplaced_json = nlohmann::json::array();
  nlohmann::json unplaced_codes_json = nlohmann::json::array();
  for (const auto& item : unplaced_items) {
    unplaced_json.push_back(item->ToJson());
    unplaced_codes_json.push_back(item->GetItemCode());
  }
  j["unplaced_items"] = std::move(unplaced_json);
  j["unplaced_item_codes"] = std::move(unplaced_codes_json);

  nlohmann::json summary;
  summary["box_count"] = boxes.Count();
  summary["total_placed"] = GetTotalPlacedCount();
  summary["total_unplaced"] = GetTotalUnplacedCount();
  summary["overall_volume_utilisation"] = GetOverallVolumeUtilisation();
  j["summary"] = std::move(summary);

  return j;
}

std::string PackResponse::Dump(int indent) const { return ToJson().dump(indent < 0 ? -1 : indent); }

}  // namespace fitsolver
