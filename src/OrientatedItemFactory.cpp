#include "OrientatedItemFactory.h"

#include <algorithm>
#include <string>

#include "OrientatedItemSorter.h"

namespace fitsolver {

std::vector<OrientatedItem> OrientatedItemFactory::GetPossibleOrientations(
    const ItemPtr& item, const OrientatedItem* prev_item, double width_left, double length_left,
    double depth_left) const {
  // Special case: placing a run of identical items the same way every
  // time is never worse than re-evaluating all up-to-6 permutations, and
  // much cheaper.
  std::vector<OrientatedItem> candidates;
  if (prev_item != nullptr && prev_item->IsSameDimensions(item)) {
    candidates.push_back(OrientatedItem(item, prev_item->GetWidth(), prev_item->GetLength(),
                                         prev_item->GetDepth(), prev_item->GetOrientationCode()));
  } else {
    candidates = GenerateOrientations(item);
  }

  // Orientations with identical placed dimensions (e.g. every permutation
  // of a cube, or any pair of equal edges) are interchangeable for every
  // purpose downstream -- only keep one of each distinct
  // (width, length, depth), since evaluating the rest is pure duplicate
  // work with zero effect on the outcome.
  std::vector<OrientatedItem> fitting;
  fitting.reserve(candidates.size());
  for (auto& candidate : candidates) {
    if (candidate.GetWidth() <= width_left && candidate.GetLength() <= length_left &&
        candidate.GetDepth() <= depth_left) {
      const bool duplicate =
          std::any_of(fitting.begin(), fitting.end(), [&](const OrientatedItem& seen) {
            return seen.GetWidth() == candidate.GetWidth() &&
                   seen.GetLength() == candidate.GetLength() && seen.GetDepth() == candidate.GetDepth();
          });
      if (!duplicate) fitting.push_back(std::move(candidate));
    }
  }
  return fitting;
}

std::optional<OrientatedItem> OrientatedItemFactory::GetBestOrientation(
    const ItemPtr& item, const OrientatedItem* prev_item, const std::vector<ItemPtr>& next_items,
    double width_left, double length_left, double depth_left, double row_length,
    bool consider_stability) const {
  auto possible = GetPossibleOrientations(item, prev_item, width_left, length_left, depth_left);
  auto usable = consider_stability ? GetUsableOrientations(item, std::move(possible))
                                    : std::move(possible);
  if (usable.empty()) return std::nullopt;

  const OrientatedItemSorter sorter(*this, single_pass_mode_, width_left, length_left, depth_left,
                                     next_items, row_length);
  std::sort(usable.begin(), usable.end(), sorter);
  return usable.front();
}

std::vector<OrientatedItem> OrientatedItemFactory::GetUsableOrientations(
    const ItemPtr& item, std::vector<OrientatedItem> possible) const {
  std::vector<OrientatedItem> stable;
  std::vector<OrientatedItem> unstable;

  // Divide into stable (low centre of gravity, or fills the box's full
  // depth so there's nothing to tip onto) and unstable orientations.
  for (auto& orientation : possible) {
    if (orientation.IsStable() || box_->GetDepth() == orientation.GetDepth()) {
      stable.push_back(std::move(orientation));
    } else {
      unstable.push_back(std::move(orientation));
    }
  }

  // Prefer stable orientations; fall back to unstable ones only if the
  // item could never be placed stably in this box at all (better an
  // unstable placement than none).
  if (!stable.empty()) return stable;
  if (!unstable.empty() && !HasStableOrientationsInEmptyBox(item)) return unstable;
  return {};
}

bool OrientatedItemFactory::HasStableOrientationsInEmptyBox(const ItemPtr& item) const {
  const std::string cache_key =
      std::to_string(item->GetWidth()) + "|" + std::to_string(item->GetLength()) + "|" +
      std::to_string(item->GetDepth()) + "|" +
      std::to_string(static_cast<int>(item->GetAllowedRotation())) + "|" +
      std::to_string(box_->GetWidth()) + "|" + std::to_string(box_->GetLength()) + "|" +
      std::to_string(box_->GetDepth());

  if (auto it = empty_box_stable_cache_.find(cache_key); it != empty_box_stable_cache_.end()) {
    return it->second;
  }

  const auto orientations =
      GetPossibleOrientations(item, nullptr, box_->GetWidth(), box_->GetLength(), box_->GetDepth());
  const bool any_stable = std::any_of(orientations.begin(), orientations.end(),
                                       [](const OrientatedItem& o) { return o.IsStable(); });
  empty_box_stable_cache_[cache_key] = any_stable;
  return any_stable;
}

}  // namespace fitsolver
