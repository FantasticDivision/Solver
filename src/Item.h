#pragma once

#include <memory>
#include <string>

#include "Rotation.h"
#include "nlohmann/json.hpp"

namespace fitsolver {

// Contract for anything that can be packed. Mirrors the BoxPacker PHP
// library's `Item` interface: implement this directly for a custom item
// type with its own behaviour, or use the ready-made ConcreteItem
// (ConcreteItem.h) if a plain data holder is enough.
//
// Extension point: to add new per-item behaviour without touching any
// existing Item implementation, add a new interface deriving from Item
// (see LinkedItem.h for the pattern) -- e.g. a future
// ConstrainedPlacementItem that hooks into where an item is allowed to
// be placed. Packer/VolumePacker can then `dynamic_cast<const
// NewInterface*>(item.get())` wherever that behaviour needs to kick in,
// exactly like LinkedItem is detected today.
class Item {
 public:
  virtual ~Item() = default;

  virtual const std::string& GetItemCode() const = 0;
  virtual const std::string& GetItemReference() const = 0;
  virtual double GetWidth() const = 0;
  virtual double GetLength() const = 0;
  virtual double GetDepth() const = 0;
  virtual double GetWeight() const = 0;
  virtual Rotation GetAllowedRotation() const = 0;

  // Provided in terms of the pure-virtual getters above; override if a
  // subclass needs different behaviour.
  virtual double GetVolume() const;
  virtual nlohmann::json ToJson() const;
};

// Items are handed around by shared reference everywhere (ItemList,
// PackedItem, ...) rather than copied, so a custom Item subclass only
// ever needs to be constructed once. Every Item method is const --
// nothing in this codebase ever mutates one through this pointer -- the
// alias is non-const shared_ptr purely so it lines up with pybind11's
// default holder type when exposed to Python.
using ItemPtr = std::shared_ptr<Item>;

}  // namespace fitsolver
