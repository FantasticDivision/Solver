#include "ConcreteBox.h"

#include <stdexcept>

#include "JsonUtil.h"

namespace fitsolver {

using json_detail::GetOptionalBool;
using json_detail::GetOptionalNumber;

ConcreteBox::ConcreteBox(std::string reference, double width, double length, double depth,
                          double max_weight, double empty_weight, bool active)
    : reference_(std::move(reference)),
      width_(width),
      length_(length),
      depth_(depth),
      max_weight_(max_weight),
      empty_weight_(empty_weight),
      active_(active) {}

ConcreteLimitedSupplyBox::ConcreteLimitedSupplyBox(ConcreteBox base, int quantity_available)
    : base_(std::move(base)), quantity_available_(quantity_available) {}

BoxPtr MakeBoxFromJson(const nlohmann::json& j) {
  if (!j.contains("Reference")) {
    throw std::invalid_argument("box is missing required field 'Reference'");
  }

  ConcreteBox base(j.at("Reference").get<std::string>(), j.at("Width").get<double>(),
                    j.at("Length").get<double>(), j.at("Depth").get<double>(),
                    GetOptionalNumber(j, "MaxWeight", kNoWeightLimit),
                    GetOptionalNumber(j, "BoxWeight", 0.0), GetOptionalBool(j, "Active", true));

  if (j.contains("MaximumBoxes") && !j.at("MaximumBoxes").is_null()) {
    return std::make_shared<ConcreteLimitedSupplyBox>(std::move(base),
                                                        j.at("MaximumBoxes").get<int>());
  }
  return std::make_shared<ConcreteBox>(std::move(base));
}

}  // namespace fitsolver
