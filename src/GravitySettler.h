#pragma once

#include "PackedBox.h"

namespace fitsolver {

// Lets gravity act on a finished box.
//
// Gravity acts along -Z: a box's floor is its Width x Length face at z = 0,
// and Depth is the vertical axis that layers stack along.
//
// LayerPacker starts every layer at the previous layer's tallest point, so
// an item placed above a shorter neighbour is left hanging over a gap, and
// void-filling places items wherever free space is, supported or not.
// LayerStabiliser only reorders whole layers; it never closes those gaps.
//
// Settle() closes them in two steps:
//   1. every item drops straight down onto the floor or the item beneath
//      it (DropOntoSupport), so nothing is left floating;
//   2. an item that landed with the centre of its base over thin air is
//      moved to a seat where it stands firmly, if the box has one
//      (ItemReseater).
// The items are then relisted bottom-up.
//
// It only rearranges a box's own items within that box, so it can never
// cost a box, unplace an item, break a weight limit or mix box groups.
// Packer runs it once, on the boxes that are actually going out, after
// every packing decision has been made, so it can't change which box an
// item goes in either.
//
// Not part of BoxPacker, whose own results have the same gaps.
class GravitySettler {
 public:
  PackedBox Settle(const PackedBox& box) const;
};

}  // namespace fitsolver
