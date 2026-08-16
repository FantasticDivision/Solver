#include "Packer.h"

#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "LimitedSupplyBox.h"
#include "LinkedItem.h"
#include "VolumePacker.h"
#include "WeightRedistributor.h"

namespace fitsolver {

namespace {

// Strips any BoxGroup from `candidate` that isn't present in full --
// items whose group is only partially represented go back to being
// unplaced (for this box) rather than splitting the group across boxes.
// `group_totals` is the authoritative remaining count per group across
// the whole still-unplaced pool, not just this candidate.
void EnforceLinkedGroups(PackedBox& candidate,
                          const std::unordered_map<std::string, int>& group_totals) {
  if (group_totals.empty()) return;

  const auto candidate_counts = candidate.GetItems().GetLinkedGroupCounts();
  std::unordered_set<std::string> incomplete_groups;
  for (const auto& [group, total] : group_totals) {
    const auto it = candidate_counts.find(group);
    const int have = it == candidate_counts.end() ? 0 : it->second;
    if (have > 0 && have < total) incomplete_groups.insert(group);
  }
  if (incomplete_groups.empty()) return;

  PackedBox rebuilt(candidate.GetBox());
  for (const auto& packed_item : candidate.GetItems()) {
    if (const auto* linked = dynamic_cast<const LinkedItem*>(packed_item.GetItem().get())) {
      if (incomplete_groups.count(linked->GetBoxGroup()) > 0) continue;
    }
    rebuilt.AddItem(packed_item);
  }
  candidate = std::move(rebuilt);
}

bool IsBetterCandidate(const PackedBox& candidate, const PackedBox& current_best) {
  if (candidate.GetItems().Count() != current_best.GetItems().Count()) {
    return candidate.GetItems().Count() > current_best.GetItems().Count();
  }
  return candidate.GetVolumeUtilisation() > current_best.GetVolumeUtilisation();
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
    const auto group_totals = remaining.GetLinkedGroupCounts();

    std::optional<PackedBox> best;
    for (const auto& box : active_boxes) {
      if (const auto* limited = dynamic_cast<const LimitedSupplyBox*>(box.get())) {
        if (boxes_used_count[box->GetReference()] >= limited->GetQuantityAvailable()) continue;
      }

      VolumePacker volume_packer(box);
      PackedBox candidate = volume_packer.Pack(remaining);
      EnforceLinkedGroups(candidate, group_totals);
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
