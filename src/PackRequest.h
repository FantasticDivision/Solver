#pragma once

#include <string>

#include "BoxList.h"
#include "ItemList.h"
#include "nlohmann/json.hpp"

namespace fitsolver {

// The items and available box types for one packing run.
class PackRequest {
 public:
  PackRequest() = default;
  PackRequest(ItemList items, BoxList boxes);

  const ItemList& GetItems() const { return items_; }
  const BoxList& GetBoxes() const { return boxes_; }

  // Parses {"items": [...], "boxes": [...]} (see README for the full
  // wire format) into a PackRequest, building ConcreteItem/ConcreteBox
  // (or their Linked/LimitedSupply variants) as appropriate.
  static PackRequest FromJson(const nlohmann::json& j);
  static PackRequest FromJsonString(const std::string& json_text);

 private:
  ItemList items_;
  BoxList boxes_;
};

}  // namespace fitsolver
