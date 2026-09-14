#include "Packer.h"

#include <optional>
#include <unordered_map>

#include "LimitedSupplyBox.h"
#include "LinkedItem.h"
#include "VolumePacker.h"
#include "WeightRedistributor.h"

namespace fitsolver {

namespace {

// A BoxGroup means "don't mix with a DIFFERENT group", not "every unit of
// this group must end up in one box together" -- a group's units are free
// to spread across as many boxes as it takes, same as ungrouped items,
// as long as no single box ends up holding two or more distinct groups.
// (Verified against the Thommeax Chaotic benchmark's reference packing,
// which spreads a 14-unit and a 34-unit group across dozens of boxes
// each mixed with ungrouped items -- see
// tests/ThommeaxMinimumTests/Chaotic/RESULTS.md.)
//
// If a candidate box's shelf-fill happened to place items from more than
// one group (DefaultItemSorter tries grouped items first, so this is
// common on a mixed load), keep only the best-represented group's items
// -- plus every ungrouped item, which are always compatible -- and
// return the rest to unplaced for another box to pick up.
void EnforceGroupSegregation(PackedBox& candidate) {
  const auto counts = candidate.GetItems().GetLinkedGroupCounts();
  if (counts.size() <= 1) return;  // 0 or 1 distinct group present: nothing to resolve

  const std::string* keep_group = nullptr;
  int best_count = -1;
  for (const auto& [group, count] : counts) {
    if (count > best_count) {
      best_count = count;
      keep_group = &group;
    }
  }

  PackedBox rebuilt(candidate.GetBox());
  for (const auto& packed_item : candidate.GetItems()) {
    if (const auto* linked = dynamic_cast<const LinkedItem*>(packed_item.GetItem().get())) {
      if (linked->GetBoxGroup() != *keep_group) continue;
    }
    rebuilt.AddItem(packed_item);
  }
  candidate = std::move(rebuilt);
}

bool IsBetterCandidate(const PackedBox& candidate, const PackedBox& current_best) {
  if (candidate.GetItems().Count() != current_best.GetItems().Count()) {
    return candidate.GetItems().Count() > current_best.GetItems().Count();
  }
  if (candidate.GetVolumeUtilisation() != current_best.GetVolumeUtilisation()) {
    return candidate.GetVolumeUtilisation() > current_best.GetVolumeUtilisation();
  }
  // Mirrors BoxPacker's DefaultPackedBoxSorter's third tiebreak -- same
  // item count and utilisation can still mean different used volume when
  // the candidates are different box types.
  return candidate.GetUsedVolume() > current_best.GetUsedVolume();
}

}  // namespace

PackResponse Packer::Pack(const PackRequest& request) const {
  return Pack(request.GetItems(), request.GetBoxes());
}

PackResponse Packer::Pack(const ItemList& items, const BoxList& boxes) const {
  ItemList remaining = items;

  std::vector<BoxPtr> active_boxes;
  for (const auto& box : boxes) {
    if (box->GetActive()) active_boxes.push_back(box);
  }

  std::unordered_map<std::string, int> boxes_used_count;
  PackedBoxList packed_boxes;

  while (!remaining.Empty()) {
    std::optional<PackedBox> best;
    for (const auto& box : active_boxes) {
      if (const auto* limited = dynamic_cast<const LimitedSupplyBox*>(box.get())) {
        if (boxes_used_count[box->GetReference()] >= limited->GetQuantityAvailable()) continue;
      }

      VolumePacker volume_packer(box);
      PackedBox candidate = volume_packer.Pack(remaining);
      EnforceGroupSegregation(candidate);
      if (candidate.GetItems().Count() == 0) continue;

      if (!best.has_value() || IsBetterCandidate(candidate, *best)) {
        best = std::move(candidate);
      }
      if (best->GetItems().Count() == remaining.Count()) break;  // can't do better than everything
    }

    if (!best.has_value()) break;  // nothing left fits in any available box

    remaining.RemovePacked(best->GetItems());
    ++boxes_used_count[best->GetBox()->GetReference()];
    packed_boxes.Insert(std::move(*best));
  }

  if (packed_boxes.Count() > 1 && packed_boxes.Count() <= kMaxBoxesToBalanceWeight) {
    WeightRedistributor redistributor(boxes);
    packed_boxes = redistributor.Redistribute(packed_boxes);
  }

  PackResponse response;
  response.boxes = std::move(packed_boxes);
  response.unplaced_items = std::move(remaining);
  response.success = response.unplaced_items.Empty();
  return response;
}

}  // namespace fitsolver
