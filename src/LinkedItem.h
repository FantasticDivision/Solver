#pragma once

#include "Item.h"

namespace fitsolver {

// An Item that must end up in the same box as every other Item sharing
// its box group -- only implement/use this (instead of a plain Item) if
// that constraint is actually needed, since it costs the packer extra
// work to enforce (see Packer.cpp's linked-group handling). Mirrors
// BoxPacker's `LinkedItem` interface exactly.
//
// ConcreteLinkedItem (ConcreteItem.h) is the ready-made implementation;
// ItemList::HasLinkedItems() / GetLinkedGroupCounts() detect membership
// via `dynamic_cast<const LinkedItem*>`, the same way BoxPacker uses
// `instanceof`.
class LinkedItem : public Item {
 public:
  // Must never be empty -- ConcreteLinkedItem enforces this at
  // construction time.
  virtual const std::string& GetBoxGroup() const = 0;

  nlohmann::json ToJson() const override;
};

}  // namespace fitsolver
