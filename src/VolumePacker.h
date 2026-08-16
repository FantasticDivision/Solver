#pragma once

#include <optional>
#include <unordered_set>
#include <vector>

#include "Box.h"
#include "ItemList.h"
#include "OrientatedItem.h"
#include "PackedBox.h"
#include "VoidFinder.h"

namespace fitsolver {

// Packs as many items as possible into ONE instance of a single box
// type. Mirrors BoxPacker's `VolumePacker` (the per-box packer that
// Packer calls once per candidate box type).
//
// Strategy: a greedy shelf pass (largest item first, left-to-right,
// wrapping to a new row then a new vertical layer, trying every
// orientation the item's Rotation allows), followed by a void-filling
// pass that uses VoidFinder to locate leftover rectangular gaps and
// WorkingVolume + a nested VolumePacker to pack more items into each one
// (bounded by kMaxVoidFillDepth so it can't recurse forever).
class VolumePacker {
 public:
  explicit VolumePacker(BoxPtr box);

  // Items that don't fit are simply left out of the result -- they
  // remain in `items` and are still candidates for the next box tried.
  PackedBox Pack(const ItemList& items) const;

 private:
  struct ShelfCursor {
    double x, y, z;
    double row_length = 0.0;
    double layer_depth = 0.0;
  };

  // Recursion is only ever entered via Pack() -> FillVoids(), which
  // caps depth at kMaxVoidFillDepth; `weight_capacity` is passed through
  // explicitly rather than re-derived from `box_` at each level, because
  // a WorkingVolume sub-region reports no weight limit of its own (it's
  // not a real box, just a leftover chunk of the real one).
  PackedBox PackInternal(const std::vector<ItemPtr>& sorted_items, double weight_capacity,
                          int depth) const;

  void PackRegion(double origin_x, double origin_y, double origin_z, double region_width,
                   double region_length, double region_depth,
                   const std::vector<ItemPtr>& sorted_items, double weight_capacity,
                   double& weight_used, std::unordered_set<const Item*>& placed, PackedBox& out) const;

  void FillVoids(PackedBox& out, const std::vector<ItemPtr>& sorted_items,
                  std::unordered_set<const Item*>& placed, double weight_capacity, double& weight_used,
                  int depth) const;

  std::optional<OrientatedItem> BestFittingOrientation(const ItemPtr& item, double width_left,
                                                        double length_left, double depth_left) const;
  std::optional<PackedItem> TryPlaceItem(const ItemPtr& item, ShelfCursor& cursor, double region_x,
                                          double region_y, double region_z, double region_width,
                                          double region_length, double region_depth) const;

  BoxPtr box_;
  VoidFinder void_finder_;

  static constexpr int kMaxVoidFillDepth = 4;
};

}  // namespace fitsolver
