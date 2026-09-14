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
// A candidate box's fill commonly ends up holding items from more than
// one group, since VolumePacker just works down the sorted queue and has
// no group concept of its own. When that happens, keep the group the
// item sorter itself ranks highest (the first one reached walking
// `remaining` in priority order) and REPACK the box with every other
// group's items excluded -- mirroring BoxPacker's own
// LinkedItemGroupEnforcer, which likewise excludes the offending
// group(s) and repacks "so the freed space can be used by other eligible
// items".
//
// Repacking, rather than just deleting the offending items in place,
// is what makes this correct rather than merely legal: deleting leaves
// the box with holes where they were, and -- because the deleted items
// are whichever group happened to lose -- it also silently overrides the
// sorter's priority. A lower-priority group that had grabbed more slots
// would evict a higher-priority one that the sorter had deliberately put
// first, stranding the latter in boxes of its own later on.
PackedBox EnforceGroupSegregation(PackedBox candidate, const ItemList& remaining) {
  if (!remaining.HasLinkedItems()) return candidate;

  const auto counts = candidate.GetItems().GetLinkedGroupCounts();
  if (counts.size() <= 1) return candidate;  // 0 or 1 group present: nothing to resolve

  // Highest-priority group present, per the sorter's own ordering --
  // never a raw count, and never a hardcoded rule about group names.
  const std::string* keep_group = nullptr;
  for (const auto& item : remaining.SortedItems()) {
    const auto* linked = dynamic_cast<const LinkedItem*>(item.get());
    if (linked != nullptr && counts.count(linked->GetBoxGroup()) > 0) {
      keep_group = &linked->GetBoxGroup();
      break;
    }
  }
  if (keep_group == nullptr) return candidate;

  // Ungrouped items are compatible with anything, so they stay eligible.
  ItemList eligible(remaining.GetSorter());
  for (const auto& item : remaining) {
    const auto* linked = dynamic_cast<const LinkedItem*>(item.get());
    if (linked != nullptr && linked->GetBoxGroup() != *keep_group) continue;
    eligible.Insert(item);
  }
  if (eligible.Empty()) return PackedBox(candidate.GetBox());

  // `eligible` holds at most one group, so the repack can't reintroduce
  // a second one -- no need for BoxPacker's iterative exclusion loop.
  return VolumePacker(candidate.GetBox()).Pack(eligible);
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
      PackedBox candidate = EnforceGroupSegregation(volume_packer.Pack(remaining), remaining);
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
