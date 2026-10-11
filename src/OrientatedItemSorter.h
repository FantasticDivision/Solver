#pragma once

#include <optional>
#include <vector>

#include "Item.h"
#include "OrientatedItem.h"
#include "PackQueue.h"

namespace fitsolver {

class OrientatedItemFactory;

// Ranks candidate orientations for one item at one position: prefers an
// exact fit (width, then length, then depth), then -- if everything left
// to pack is the same size -- whichever orientation fits the most of
// them on a grid, then a bounded look-ahead ("does the *next* item still
// fit after this choice, or how many more fit within a few items"), then
// a couple of shape-based tie-breaks. Mirrors BoxPacker's
// `OrientatedItemSorter` (minus ConstrainedPlacementItem support).
class OrientatedItemSorter {
 public:
  OrientatedItemSorter(const OrientatedItemFactory& factory, bool single_pass_mode,
                        double width_left, double length_left, double depth_left,
                        const PackQueue& next_items, double row_length);

  // std::sort-compatible: true if `a` should be ordered before `b`.
  bool operator()(const OrientatedItem& a, const OrientatedItem& b) const {
    return Compare(a, b) < 0;
  }

 private:
  int Compare(const OrientatedItem& a, const OrientatedItem& b) const;
  static int ExactFitDecider(double a_left, double b_left);
  int SameSizeItemsDecider(const OrientatedItem& a, const OrientatedItem& b) const;
  int LookAheadDecider(const OrientatedItem& a, const OrientatedItem& b, double a_width_left,
                        double b_width_left) const;
  bool AllRemainingSameSize(const OrientatedItem& oriented) const;
  long long HowManyFitWithOrientation(double width, double length, double depth) const;
  int CalculateAdditionalItemsPacked(const OrientatedItem& prev_item) const;
  static bool IsSlabLike(const ItemPtr& item);

  const OrientatedItemFactory& factory_;
  bool single_pass_mode_;
  double width_left_;
  double length_left_;
  double depth_left_;
  const PackQueue& next_items_;
  double row_length_;
  mutable std::optional<bool> all_remaining_same_size_cache_;
};

}  // namespace fitsolver
