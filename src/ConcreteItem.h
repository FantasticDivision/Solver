#pragma once

#include <string>

#include "Item.h"
#include "LinkedItem.h"

namespace fitsolver {

// Ready-made Item implementation -- a plain data holder. Construct
// directly, or via MakeItemFromJson() for the wire format documented in
// the README ({"ItemCode": ..., "Width": ..., ...}).
class ConcreteItem : public Item {
 public:
  ConcreteItem(std::string item_code, std::string item_reference, double width, double length,
               double depth, double weight, Rotation allowed_rotation = Rotation::kBestFit);

  const std::string& GetItemCode() const override { return item_code_; }
  const std::string& GetItemReference() const override { return item_reference_; }
  double GetWidth() const override { return width_; }
  double GetLength() const override { return length_; }
  double GetDepth() const override { return depth_; }
  double GetWeight() const override { return weight_; }
  Rotation GetAllowedRotation() const override { return allowed_rotation_; }

 private:
  std::string item_code_;
  std::string item_reference_;
  double width_;
  double length_;
  double depth_;
  double weight_;
  Rotation allowed_rotation_;
};

// Ready-made LinkedItem implementation: a ConcreteItem plus a box group.
// Implemented via composition (holds a ConcreteItem) rather than
// inheriting from it, so this stays a single, unambiguous
// is-a-LinkedItem type -- no diamond inheritance needed to also be an
// Item.
class ConcreteLinkedItem : public LinkedItem {
 public:
  ConcreteLinkedItem(ConcreteItem base, std::string box_group);

  const std::string& GetItemCode() const override { return base_.GetItemCode(); }
  const std::string& GetItemReference() const override { return base_.GetItemReference(); }
  double GetWidth() const override { return base_.GetWidth(); }
  double GetLength() const override { return base_.GetLength(); }
  double GetDepth() const override { return base_.GetDepth(); }
  double GetWeight() const override { return base_.GetWeight(); }
  Rotation GetAllowedRotation() const override { return base_.GetAllowedRotation(); }
  const std::string& GetBoxGroup() const override { return box_group_; }

 private:
  ConcreteItem base_;
  std::string box_group_;
};

// Parses one item from its wire format into a ConcreteItem, or a
// ConcreteLinkedItem if "BoxGroup" is present and non-empty.
ItemPtr MakeItemFromJson(const nlohmann::json& j);

}  // namespace fitsolver
