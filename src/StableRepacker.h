#pragma once

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "Box.h"
#include "PackedItem.h"

namespace fitsolver {

// The third step of GravitySettler, for a box that ItemReseater could not
// leave with every item standing firmly (see StandsFirmly in Support.h).
// Moving one item at a time can't help when a whole row of items above a
// gap needs what is under it laid out differently: cartons bridging a
// block and a row of slightly shorter rods, say, with no other seat left
// in the box for any of them.
//
// This lays the same items out again in the same box from the floor up,
// putting each one only where it stands firmly: at the lowest such spot,
// tucked back and to the left, turned if its Rotation allows (the same
// turns as ItemReseater, OrientationsForMoving in Support.h). It tries
// the items in a few fixed orders (as they were packed, biggest footprint
// first, tallest first, and so on) and then in seeded shuffles, and gives
// back the first layout in which every item found a place. If none does
// within the work budget, it gives back nothing and the box stays as it
// was.
//
// Every item it places stands firmly when placed, and nothing placed
// later goes under it, so every item in a layout it returns stands
// firmly. It never moves an item to another box.
class StableRepacker {
 public:
  explicit StableRepacker(BoxPtr box) : box_(std::move(box)) {}

  std::optional<std::vector<PackedItem>> Repack(const std::vector<PackedItem>& items) const;

 private:
  // Seeded shuffles of the packed order, tried after the fixed orders.
  static constexpr int kShuffles = 64;
  // Item comparisons allowed per box across all tries. Bounds the work by
  // what is done rather than by the clock, so a given order always packs
  // the same way.
  static constexpr std::size_t kMaxWork = 4000000;

  BoxPtr box_;
};

}  // namespace fitsolver
