#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "BoxList.h"
#include "PackedBox.h"
#include "PackedBoxList.h"

namespace fitsolver {

// Given a completed packing, tries to move items between boxes to
// reduce weight variance across them without changing how many boxes
// were used (or, if moving a box's last item empties it, eliminating
// that box entirely). Mirrors BoxPacker's `WeightRedistributor`.
//
// A move repacks both boxes involved into the smallest box type that
// still fits, which can be a different type from the one it replaces --
// so box quantity limits (LimitedSupplyBox) are tracked the way
// BoxPacker's doVolumeRepack() does: each type's remaining supply is its
// limit minus how many of it `original` already uses, and a repack may
// only pick a type with a unit to spare (the box it replaces counting as
// handed back).
class WeightRedistributor {
 public:
  explicit WeightRedistributor(BoxList boxes);

  PackedBoxList Redistribute(const PackedBoxList& original) const;

 private:
  // Remaining supply per box type, keyed by the same Box instances boxes_
  // holds (and that PackedBox::GetBox() returns).
  using Supply = std::unordered_map<const Box*, long long>;

  // Tries one move between boxes[i] and boxes[j]; returns true (and
  // mutates `boxes` and `supply`) on the first move found that helps.
  // Stops looking, returning false, once `expired()` says so.
  bool TryEqualiseWeight(std::vector<PackedBox>& boxes, std::size_t i, std::size_t j,
                          double target_weight, Supply& supply,
                          const std::function<bool()>& expired) const;

  // Packs `items` into the smallest active box type that fits all of them
  // and has a unit of supply to spare -- `replacing`, the type of the box
  // these items are coming out of, counts as having one extra -- or
  // std::nullopt if none does.
  std::optional<PackedBox> RepackSingleBox(const std::vector<ItemPtr>& items,
                                            const Box* replacing, const Supply& supply) const;

  // Whether VolumePacker fits every one of `items` into one `box`.
  std::optional<PackedBox> FitAll(const std::vector<ItemPtr>& items, const BoxPtr& box) const;

  BoxList boxes_;

  // Redistribute() rescans every box pair from the start after each
  // successful move, so the same "would these items fit this box?"
  // question (same items, same order, same box) is asked over and over for
  // boxes a move never touched. VolumePacker is deterministic for a given
  // ordered input, so remembering the answer is exact, not approximate.
  // Keyed per box type rather than per search, since which types a search
  // may use depends on the supply left at the time.
  mutable std::map<std::pair<std::vector<const Item*>, const Box*>, std::optional<PackedBox>>
      fit_cache_;
};

}  // namespace fitsolver
