#include "ConcreteItem.h"

#include <stdexcept>

#include "JsonUtil.h"

namespace fitsolver {

using json_detail::GetOptionalNumber;
using json_detail::GetOptionalString;

ConcreteItem::ConcreteItem(std::string item_code, std::string item_reference, double width,
                            double length, double depth, double weight,
                            Rotation allowed_rotation)
    : item_code_(std::move(item_code)),
      item_reference_(std::move(item_reference)),
      width_(width),
      length_(length),
      depth_(depth),
      weight_(weight),
      allowed_rotation_(allowed_rotation) {}

ConcreteLinkedItem::ConcreteLinkedItem(ConcreteItem base, std::string box_group)
    : base_(std::move(base)), box_group_(std::move(box_group)) {
  if (box_group_.empty()) {
    throw std::invalid_argument("ConcreteLinkedItem requires a non-empty box group");
  }
}

ItemPtr MakeItemFromJson(const nlohmann::json& j) {
  if (!j.contains("ItemCode")) {
    throw std::invalid_argument("item is missing required field 'ItemCode'");
  }

  const std::string item_code = j.at("ItemCode").get<std::string>();
  ConcreteItem base(item_code, GetOptionalString(j, "ItemReference", item_code),
                     j.at("Width").get<double>(), j.at("Length").get<double>(),
                     j.at("Depth").get<double>(), GetOptionalNumber(j, "Weight", 0.0),
                     RotationFromString(GetOptionalString(j, "AllowedRotation", "BestFit")));

  const std::string box_group = GetOptionalString(j, "BoxGroup", "");
  if (box_group.empty()) {
    return std::make_shared<ConcreteItem>(std::move(base));
  }
  return std::make_shared<ConcreteLinkedItem>(std::move(base), box_group);
}

}  // namespace fitsolver
