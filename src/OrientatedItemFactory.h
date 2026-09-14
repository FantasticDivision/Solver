#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include "Box.h"
#include "Item.h"
#include "OrientatedItem.h"

namespace fitsolver {

// Figures out which orientation(s) of an item fit a given free space, and
// picks the best one for a specific packing position. Mirrors BoxPacker's
// `OrientatedItemFactory` (minus its ConstrainedPlacementItem support --
// this codebase has no such item type).
class OrientatedItemFactory {
 public:
  explicit OrientatedItemFactory(BoxPtr box) : box_(std::move(box)) {}

  // Single-pass mode is used for the OrientatedItemSorter's own
  // look-ahead sub-packs: it disables the look-ahead recursively firing
  // *inside* a look-ahead (which would be exponential) and skips the
  // stabilise/void-fill refinement passes, since a look-ahead only needs
  // a rough "how much of this fits" estimate, not a polished result.
  void SetSinglePassMode(bool single_pass_mode) { single_pass_mode_ = single_pass_mode; }
  bool IsSinglePassMode() const { return single_pass_mode_; }

  // Every orientation of `item` that fits within width_left x length_left
  // x depth_left. If `prev_item` is the same size as `item`, only its
  // exact orientation is considered (not every alternative) -- placing a
  // run of identical items the same way is never worse and much cheaper
  // than re-evaluating all 6 permutations each time.
  std::vector<OrientatedItem> GetPossibleOrientations(const ItemPtr& item,
                                                       const OrientatedItem* prev_item,
                                                       double width_left, double length_left,
                                                       double depth_left) const;

  // The single best orientation for `item` at this position, or nullopt
  // if none fit. `next_items` is whatever the pack queue has left after
  // this item (used for look-ahead scoring); `row_length` is how deep
  // (in Y) the current shelf row already is.
  std::optional<OrientatedItem> GetBestOrientation(const ItemPtr& item,
                                                    const OrientatedItem* prev_item,
                                                    const std::vector<ItemPtr>& next_items,
                                                    double width_left, double length_left,
                                                    double depth_left, double row_length,
                                                    bool consider_stability) const;

 private:
  std::vector<OrientatedItem> GetUsableOrientations(const ItemPtr& item,
                                                     std::vector<OrientatedItem> possible) const;
  bool HasStableOrientationsInEmptyBox(const ItemPtr& item) const;

  BoxPtr box_;
  bool single_pass_mode_ = false;
  mutable std::unordered_map<std::string, bool> empty_box_stable_cache_;
};

}  // namespace fitsolver
