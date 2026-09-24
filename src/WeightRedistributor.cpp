#include "WeightRedistributor.h"

#include <algorithm>
#include <chrono>

#include "LinkedItem.h"
#include "VolumePacker.h"

namespace fitsolver {

namespace {
double Variance(double a, double b) {
  const double mean = (a + b) / 2.0;
  return (a - mean) * (a - mean);
}

// Each accepted swap strictly reduces variance *for the pair it was chosen
// for* (Variance() above is relative to that pair's own mean, not the
// request's overall target_weight -- matching BoxPacker's own
// calculateVariance(), which has the same pair-relative shape). That is not
// a transitive, globally-monotone measure: two boxes can trade an item back
// and forth as the redistributor keeps re-pairing them against different
// neighbours in between, each individual move legitimately variance-
// reducing *for the pair it was made for* while the overall arrangement
// doesn't converge. Confirmed by construction on an adversarial fixture
// (see benchmarks/boxpacker_comparison/): the same item object moved
// box[4]->box[6] and was later moved box[6]->box[4], and neither this port
// nor BoxPacker's own PHP implementation has anything that rules that out.
// This is therefore a deadline, not an iteration cap -- capping iterations
// would just move the same non-termination risk to "how expensive is one
// iteration" instead of removing it. Weight balancing is a pure
// optimisation pass over an already-valid, already-complete packing, so
// timing out mid-pass and keeping whatever arrangement has been reached so
// far is always safe: it can only leave boxes somewhat less evenly loaded,
// never break correctness.
constexpr std::chrono::milliseconds kRedistributionDeadline{250};
}  // namespace

WeightRedistributor::WeightRedistributor(BoxList boxes) : boxes_(std::move(boxes)) {}

PackedBoxList WeightRedistributor::Redistribute(const PackedBoxList& original) const {
  const double target_weight = original.GetMeanItemWeight();

  std::vector<PackedBox> boxes(original.begin(), original.end());
  std::sort(boxes.begin(), boxes.end(),
            [](const PackedBox& a, const PackedBox& b) { return a.GetWeight() > b.GetWeight(); });

  const auto deadline = std::chrono::steady_clock::now() + kRedistributionDeadline;

  bool changed = true;
  while (changed && std::chrono::steady_clock::now() < deadline) {
    changed = false;
    for (std::size_t i = 0; i < boxes.size() && !changed; ++i) {
      for (std::size_t j = i + 1; j < boxes.size() && !changed; ++j) {
        if (boxes[i].GetWeight() == boxes[j].GetWeight()) continue;
        changed = TryEqualiseWeight(boxes, i, j, target_weight);
      }
    }
  }

  PackedBoxList result;
  for (auto& box : boxes) result.Insert(std::move(box));
  return result;
}

bool WeightRedistributor::TryEqualiseWeight(std::vector<PackedBox>& boxes, std::size_t i,
                                             std::size_t j, double target_weight) const {
  const std::size_t heavy_idx = boxes[i].GetWeight() > boxes[j].GetWeight() ? i : j;
  const std::size_t light_idx = heavy_idx == i ? j : i;

  std::vector<ItemPtr> heavy_items;
  for (const auto& packed_item : boxes[heavy_idx].GetItems()) heavy_items.push_back(packed_item.GetItem());
  std::vector<ItemPtr> light_items;
  for (const auto& packed_item : boxes[light_idx].GetItems()) light_items.push_back(packed_item.GetItem());

  double heavy_weight = boxes[heavy_idx].GetItemWeight();
  double light_weight = boxes[light_idx].GetItemWeight();

  for (std::size_t k = 0; k < heavy_items.size(); ++k) {
    const ItemPtr& candidate = heavy_items[k];

    // Can't move individual linked-group members -- group integrity
    // takes priority over weight balance.
    if (dynamic_cast<const LinkedItem*>(candidate.get()) != nullptr) continue;

    // Moving this item over would overshoot the target on its own --
    // not worth attempting.
    if (candidate->GetWeight() + light_weight > target_weight) continue;

    const double old_variance = Variance(heavy_weight, light_weight);
    const double new_variance =
        Variance(heavy_weight - candidate->GetWeight(), light_weight + candidate->GetWeight());
    if (new_variance >= old_variance) continue;

    std::vector<ItemPtr> new_light_items = light_items;
    new_light_items.push_back(candidate);
    auto new_light_box = RepackSingleBox(new_light_items);
    if (!new_light_box.has_value()) continue;  // no longer fits in a single box

    std::vector<ItemPtr> new_heavy_items = heavy_items;
    new_heavy_items.erase(new_heavy_items.begin() + static_cast<std::ptrdiff_t>(k));

    if (new_heavy_items.empty()) {
      // Moving this item emptied the heavy box entirely -- eliminate it.
      boxes[light_idx] = std::move(*new_light_box);
      boxes.erase(boxes.begin() + static_cast<std::ptrdiff_t>(heavy_idx));
      return true;
    }

    auto new_heavy_box = RepackSingleBox(new_heavy_items);
    if (!new_heavy_box.has_value()) continue;  // n-1 items should still fit, but be safe

    boxes[light_idx] = std::move(*new_light_box);
    boxes[heavy_idx] = std::move(*new_heavy_box);
    return true;
  }

  return false;
}

std::optional<PackedBox> WeightRedistributor::RepackSingleBox(const std::vector<ItemPtr>& items) const {
  if (items.empty()) return std::nullopt;

  std::vector<const Item*> key;
  key.reserve(items.size());
  for (const auto& item : items) key.push_back(item.get());
  if (const auto cached = repack_cache_.find(key); cached != repack_cache_.end()) {
    return cached->second;
  }

  double total_volume = 0.0;
  double total_weight = 0.0;
  for (const auto& item : items) {
    total_volume += item->GetVolume();
    total_weight += item->GetWeight();
  }

  ItemList item_list;
  for (const auto& item : items) item_list.Insert(item);

  std::optional<PackedBox> found;
  for (const auto& box : boxes_) {
    if (!box->GetActive()) continue;

    // All `items` must be placed for a box to count, and placed items never
    // overlap and never exceed the weight budget -- so a box whose volume or
    // weight budget is below the totals cannot succeed. Skipping it returns
    // the same answer VolumePacker would, without running it.
    if (total_volume > box->GetWidth() * box->GetLength() * box->GetDepth() * (1.0 + 1e-9)) continue;
    const double weight_capacity = box->GetWeightCapacityForItems();
    if (weight_capacity >= 0.0 && total_weight > weight_capacity) continue;

    VolumePacker packer(box);
    PackedBox result = packer.Pack(item_list);
    if (result.GetItems().Count() == items.size()) {
      found = std::move(result);
      break;
    }
  }

  repack_cache_.emplace(std::move(key), found);
  return found;
}

}  // namespace fitsolver
