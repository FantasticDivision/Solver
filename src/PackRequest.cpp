#include "PackRequest.h"

#include "ConcreteBox.h"
#include "ConcreteItem.h"

namespace fitsolver {

PackRequest::PackRequest(ItemList items, BoxList boxes)
    : items_(std::move(items)), boxes_(std::move(boxes)) {}

PackRequest PackRequest::FromJson(const nlohmann::json& j) {
  ItemList items;
  if (j.contains("items")) {
    for (const auto& item_json : j.at("items")) items.Insert(MakeItemFromJson(item_json));
  }

  BoxList boxes;
  if (j.contains("boxes")) {
    for (const auto& box_json : j.at("boxes")) boxes.Insert(MakeBoxFromJson(box_json));
  }

  return PackRequest(std::move(items), std::move(boxes));
}

PackRequest PackRequest::FromJsonString(const std::string& json_text) {
  return FromJson(nlohmann::json::parse(json_text));
}

}  // namespace fitsolver
