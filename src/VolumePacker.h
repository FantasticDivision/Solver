#pragma once

#include <optional>
#include <unordered_set>
#include <vector>

#include "Box.h"
#include "Heightmap.h"
#include "ItemList.h"
#include "OrientatedItem.h"
#include "PackedBox.h"
#include "VoidFinder.h"

namespace fitsolver {

// Packs as many items as possible into ONE instance of a single box
// type. Mirrors BoxPacker's `VolumePacker` (the per-box packer that
// Packer calls once per candidate box type).
//
// Strategy: repeated shelf sweeps (largest item first, left-to-right,
// wrapping to a new row when the current one is full) over the region,
// where each item's Z is looked up from a HeightMap -- the real height
// of whatever's already stacked under that item's specific footprint,
// not a single value shared by everything in a "layer". This lets a
// later sweep stack correctly on top of a layer that mixed different
// item depths, instead of assuming a uniform floor. Sweeps repeat until
// one places nothing. Followed by a void-filling pass that uses
// VoidFinder to locate leftover rectangular gaps and WorkingVolume + a
// nested VolumePacker to pack more items into each one (bounded by
// kMaxVoidFillDepth so it can't recurse forever).
class VolumePacker {
 public:
  explicit VolumePacker(BoxPtr box);

  // Items that don't fit are simply left out of the result -- they
  // remain in `items` and are still candidates for the next box tried.
  PackedBox Pack(const ItemList& items) const;

 private:
  struct ShelfCursor {
    double x, y;
    double row_length = 0.0;
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

  // Best orientation for `item` at (x, y), given how much room is left
  // in X/Y and the region's Z ceiling -- Z itself is looked up per
  // candidate orientation from `height_map`, since different
  // orientations have different footprints and therefore potentially
  // different real heights beneath them.
  std::optional<OrientatedItem> BestFittingOrientation(const ItemPtr& item, double x, double y,
                                                        double width_left, double length_left,
                                                        double region_end_z,
                                                        const HeightMap& height_map) const;

  std::optional<PackedItem> TryPlaceItem(const ItemPtr& item, ShelfCursor& cursor, double region_x,
                                          double region_end_x, double region_end_y,
                                          double region_end_z, HeightMap& height_map) const;

  BoxPtr box_;
  VoidFinder void_finder_;

  static constexpr int kMaxVoidFillDepth = 4;
};

}  // namespace fitsolver