#pragma once

#include <optional>
#include <vector>

#include "Box.h"
#include "ItemList.h"
#include "LayerPacker.h"
#include "OrientatedItem.h"
#include "PackedBox.h"
#include "PackedLayer.h"
#include "VoidFinder.h"

namespace fitsolver {

// Packs as many items as possible into ONE instance of a single box
// type. Mirrors BoxPacker's `VolumePacker` (the per-box packer that
// Packer calls once per candidate box type) -- ported faithfully from
// the real dvdoug/boxpacker algorithm rather than this project's earlier
// simpler shelf+void-fill approach, to close the density gap against it.
//
// Strategy: build up horizontal layers (LayerPacker) one at a time --
// each layer's true depth is discovered with a cheap preliminary pass,
// then (if needed) redone once that depth is known so the first item
// reserves the right stacking headroom -- stabilise the resulting layers
// (LayerStabiliser, reorders by footprint so nothing overhangs thin
// air), fill any leftover void spaces (VoidFinder) with whatever's left,
// and try both the box's natural orientation and width/length swapped
// (plus every valid orientation of the single highest-priority item as
// a special first placement), keeping whichever attempt is best.
//
// Minus BoxPacker's ConstrainedPlacementItem support and
// beStrictAboutItemOrdering path -- neither applies to this codebase's
// data model.
class VolumePacker {
 public:
  explicit VolumePacker(BoxPtr box);

  // Used for the OrientatedItemSorter's own look-ahead sub-packs: skips
  // the whole-box-rotation trial and the stabilise/void-fill refinement
  // passes, since a look-ahead only needs a rough "how much of this
  // fits" estimate, not a polished result. Mutates only this instance's
  // own local state -- calling it on a temporary look-ahead packer never
  // affects any other VolumePacker.
  void SetSinglePassMode(bool single_pass_mode);

  // Items that don't fit are simply left out of the result -- they
  // remain in `items` and are still candidates for the next box tried.
  PackedBox Pack(const ItemList& items) const;

 private:
  PackedBox PackRotation(double box_width, double box_length,
                          const OrientatedItem* first_item_orientation,
                          const std::vector<ItemPtr>& sorted_items) const;

  std::vector<PackedLayer> FillVoids(std::vector<PackedLayer> layers, PackQueue& queue,
                                      LayerPacker& layer_packer, double box_width,
                                      double box_length) const;

  // Swaps x/y (and each item's placed width/length) back to the box's
  // real, unrotated coordinate frame -- only needed if this attempt
  // tried the box with width/length swapped.
  std::vector<PackedLayer> CorrectLayerRotation(std::vector<PackedLayer> layers,
                                                 double box_width) const;

  // Reorders the finished box's items bottom layer first, then back to
  // front, then left to right (SortBottomUp), so the response reads as a
  // sequence a worker could physically follow.
  PackedBox FinaliseOrder(PackedBox box) const;

  static PackedItemList FlattenLayers(const std::vector<PackedLayer>& layers);
  static double SumLayerDepths(const std::vector<PackedLayer>& layers);
  static bool AllItemsNoRotation(const std::vector<ItemPtr>& items);

  BoxPtr box_;
  VoidFinder void_finder_;
  bool single_pass_mode_ = false;
};

}  // namespace fitsolver
