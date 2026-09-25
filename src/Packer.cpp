#include "Packer.h"

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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

using Clock = std::chrono::steady_clock;

// Everything one packing strategy has committed so far. Box counts carry
// across the passes of a multi-pass strategy, so a quota-limited box type
// used up in one pass isn't handed out again in the next.
struct Packing {
  std::vector<PackedBox> boxes;
  std::vector<ItemPtr> unplaced;
  std::unordered_map<std::string, int> used;
};

// Fewer unplaced items first (placing everything matters more than box
// count), then fewer boxes.
bool IsBetterPacking(const Packing& a, const Packing& b) {
  if (a.unplaced.size() != b.unplaced.size()) return a.unplaced.size() < b.unplaced.size();
  return a.boxes.size() < b.boxes.size();
}

// The greedy Packer loop over one pass's items: each round, pack a fresh
// instance of every box type with supply left and commit whichever holds
// the most, until nothing more fits. `give_up` is checked before each
// round; returning true abandons the pass (and so the strategy).
template <typename GiveUp>
bool PackPass(ItemList remaining, const std::vector<BoxPtr>& active_boxes, Packing& packing,
              GiveUp give_up) {
  while (!remaining.Empty()) {
    if (give_up(packing)) return false;

    std::optional<PackedBox> best;
    for (const auto& box : active_boxes) {
      if (const auto* limited = dynamic_cast<const LimitedSupplyBox*>(box.get())) {
        if (packing.used[box->GetReference()] >= limited->GetQuantityAvailable()) continue;
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
    ++packing.used[best->GetBox()->GetReference()];
    packing.boxes.push_back(std::move(*best));
  }

  // An item a pass couldn't place can't be placed by a later pass either:
  // a pass only ends once no box type with supply left takes any of what
  // remains, and later passes only ever have less supply.
  for (const auto& item : remaining) packing.unplaced.push_back(item);
  return true;
}

// Distinct box groups in `items`, most units first (name breaks ties, so
// the strategy order -- and with it the result -- is deterministic).
std::vector<std::string> GroupsBySize(const ItemList& items) {
  const auto counts = items.GetLinkedGroupCounts();
  std::vector<std::pair<std::string, int>> ordered(counts.begin(), counts.end());
  std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
    return a.second != b.second ? a.second > b.second : a.first < b.first;
  });
  std::vector<std::string> groups;
  for (const auto& [group, count] : ordered) groups.push_back(group);
  return groups;
}

// Items of `group`, plus the ungrouped items when `with_ungrouped`.
ItemList Slice(const ItemList& items, const std::string* group, bool with_ungrouped) {
  ItemList slice(items.GetSorter());
  for (const auto& item : items) {
    const auto* linked = dynamic_cast<const LinkedItem*>(item.get());
    if (linked == nullptr ? with_ungrouped : (group != nullptr && linked->GetBoxGroup() == *group)) {
      slice.Insert(item);
    }
  }
  return slice;
}

}  // namespace

PackResponse Packer::Pack(const PackRequest& request) const {
  return Pack(request.GetItems(), request.GetBoxes());
}

PackResponse Packer::Pack(const ItemList& items, const BoxList& boxes) const {
  const auto started = Clock::now();

  std::vector<BoxPtr> active_boxes;
  for (const auto& box : boxes) {
    if (box->GetActive()) active_boxes.push_back(box);
  }

  // Baseline: every item in one greedy pass, group segregation enforced
  // box by box.
  Packing best;
  PackPass(items, active_boxes, best, [](const Packing&) { return false; });

  // With two or more box groups, which items share a pass matters a lot:
  // the single pass above lets ungrouped items drift between groups one
  // box at a time. Also try, for each group, packing the ungrouped items
  // together with that group and then every other group on its own --
  // how the Thommeax Chaotic reference was produced (see
  // tests/ThommeaxMinimumTests/Chaotic/RESULTS.md) -- and keep whichever
  // strategy does best. Ties keep the baseline, so requests these
  // strategies don't improve pack exactly as before.
  const std::vector<std::string> groups = GroupsBySize(items);
  if (groups.size() >= 2) {
    const auto deadline = started + kGroupStrategyBudget;
    for (std::size_t host = 0; host < groups.size() && host < kMaxGroupStrategies; ++host) {
      if (Clock::now() >= deadline) break;

      // Abandon a strategy once it can't beat the best so far, or once the
      // time budget is spent.
      auto give_up = [&](const Packing& so_far) {
        if (Clock::now() >= deadline) return true;
        return best.unplaced.empty() && so_far.boxes.size() >= best.boxes.size();
      };

      Packing candidate;
      bool finished = PackPass(Slice(items, &groups[host], true), active_boxes, candidate, give_up);
      for (std::size_t other = 0; finished && other < groups.size(); ++other) {
        if (other == host) continue;
        finished = PackPass(Slice(items, &groups[other], false), active_boxes, candidate, give_up);
      }
      if (finished && IsBetterPacking(candidate, best)) best = std::move(candidate);
    }
  }

  PackedBoxList packed_boxes;
  for (auto& box : best.boxes) packed_boxes.Insert(std::move(box));

  if (packed_boxes.Count() > 1 && packed_boxes.Count() <= kMaxBoxesToBalanceWeight) {
    WeightRedistributor redistributor(boxes);
    packed_boxes = redistributor.Redistribute(packed_boxes);
  }

  ItemList unplaced(items.GetSorter());
  for (const auto& item : best.unplaced) unplaced.Insert(item);

  PackResponse response;
  response.boxes = std::move(packed_boxes);
  response.unplaced_items = std::move(unplaced);
  response.success = response.unplaced_items.Empty();
  return response;
}

}  // namespace fitsolver
