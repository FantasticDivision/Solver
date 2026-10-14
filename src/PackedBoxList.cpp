#include "PackedBoxList.h"

#include <algorithm>

namespace fitsolver {

PackedBoxList::PackedBoxList(std::shared_ptr<const PackedBoxSorter> sorter)
    : sorter_(std::move(sorter)) {}

void PackedBoxList::Insert(PackedBox box) {
  boxes_.push_back(std::move(box));
  is_sorted_ = false;
}

void PackedBoxList::Apply(const std::function<PackedBox(const PackedBox&)>& change) {
  for (auto& box : boxes_) box = change(box);
  is_sorted_ = false;
}

void PackedBoxList::EnsureSorted() const {
  if (is_sorted_) return;
  sorted_cache_ = boxes_;
  std::sort(sorted_cache_.begin(), sorted_cache_.end(),
            [this](const PackedBox& a, const PackedBox& b) { return sorter_->Compare(a, b) < 0; });
  is_sorted_ = true;
}

const std::vector<PackedBox>& PackedBoxList::SortedBoxes() const {
  EnsureSorted();
  return sorted_cache_;
}

double PackedBoxList::GetMeanItemWeight() const {
  if (boxes_.empty()) return 0.0;
  double total = 0.0;
  for (const auto& box : boxes_) total += box.GetItemWeight();
  return total / static_cast<double>(boxes_.size());
}

double PackedBoxList::GetWeightVariance() const {
  if (boxes_.empty()) return 0.0;
  const double mean = GetMeanItemWeight();
  double variance = 0.0;
  for (const auto& box : boxes_) {
    const double diff = box.GetItemWeight() - mean;
    variance += diff * diff;
  }
  return variance / static_cast<double>(boxes_.size());
}

}  // namespace fitsolver
