#pragma once

#include <string>

#include "Box.h"
#include "LimitedSupplyBox.h"

namespace fitsolver {

// Ready-made Box implementation -- a plain data holder. Construct
// directly, or via MakeBoxFromJson() for the wire format documented in
// the README ({"Reference": ..., "Width": ..., ...}).
class ConcreteBox : public Box {
 public:
  ConcreteBox(std::string reference, double width, double length, double depth,
              double max_weight = kNoWeightLimit, double empty_weight = 0.0, bool active = true);

  const std::string& GetReference() const override { return reference_; }
  double GetWidth() const override { return width_; }
  double GetLength() const override { return length_; }
  double GetDepth() const override { return depth_; }
  double GetEmptyWeight() const override { return empty_weight_; }
  double GetMaxWeight() const override { return max_weight_; }
  bool GetActive() const override { return active_; }

 private:
  std::string reference_;
  double width_;
  double length_;
  double depth_;
  double max_weight_;
  double empty_weight_;
  bool active_;
};

// Ready-made LimitedSupplyBox implementation: a ConcreteBox plus a
// quantity limit. Implemented via composition (holds a ConcreteBox)
// rather than inheriting from it, for the same reason as
// ConcreteLinkedItem (see ConcreteItem.h).
class ConcreteLimitedSupplyBox : public LimitedSupplyBox {
 public:
  ConcreteLimitedSupplyBox(ConcreteBox base, int quantity_available);
  const std::string& GetReference() const override { return base_.GetReference(); }
  double GetWidth() const override { return base_.GetWidth(); }
  double GetLength() const override { return base_.GetLength(); }
  double GetDepth() const override { return base_.GetDepth(); }
  double GetEmptyWeight() const override { return base_.GetEmptyWeight(); }
  double GetMaxWeight() const override { return base_.GetMaxWeight(); }
  bool GetActive() const override { return base_.GetActive(); }
  int GetQuantityAvailable() const override { return quantity_available_; }

 private:
  ConcreteBox base_;
  int quantity_available_;
};

// Parses one box from its wire format into a ConcreteBox, or a
// ConcreteLimitedSupplyBox if "MaximumBoxes" is present and non-null.
BoxPtr MakeBoxFromJson(const nlohmann::json& j);

}  // namespace fitsolver
