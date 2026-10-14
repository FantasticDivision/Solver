#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "Box.h"
#include "PackedItem.h"

namespace fitsolver {

// The second step of GravitySettler. Once every item has dropped onto
// something, one that landed on a ledge narrower than itself can have the
// centre of its base over thin air (see StandsFirmly in Support.h): on a
// real packing bench it would tip. This moves such an item to a seat in
// the same box where it does stand firmly: on the floor or on top of other
// items, turned if its Rotation allows. Everything that stood on it comes
// down with gravity.
//
// A move is kept only if the box ends up with fewer items that don't
// stand firmly, so the pass can't make a box worse and always terminates.
// Where no such seat exists the item stays put.
//
// Expects items that have already been dropped onto their supports.
class ItemReseater {
 public:
  explicit ItemReseater(BoxPtr box) : box_(std::move(box)) {}

  std::vector<PackedItem> Reseat(std::vector<PackedItem> items) const;

 private:
  // Item comparisons allowed per box while looking for seats and trying
  // them. Bounds the work on a box of hundreds of small items by work done
  // rather than by the clock, so a given order always packs the same way.
  static constexpr std::size_t kMaxWork = 2000000;
  static constexpr int kMaxPasses = 3;

  BoxPtr box_;
};

}  // namespace fitsolver
