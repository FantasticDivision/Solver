#include "BoxList.h"

#include <algorithm>

namespace fitsolver {

BoxList::BoxList(std::shared_ptr<const BoxSorter> sorter) : sorter_(std::move(sorter)) {}

void BoxList::Insert(BoxPtr box) {
  boxes_.push_back(std::move(box));
  is_sorted_ = false;
}

void BoxList::EnsureSorted() const {
  if (is_sorted_) return;
  sorted_cache_ = boxes_;
  std::sort(sorted_cache_.begin(), sorted_cache_.end(), [this](const BoxPtr& a, const BoxPtr& b) {
    return sorter_->Compare(*a, *b) < 0;
  });
  is_sorted_ = true;
}

const std::vector<BoxPtr>& BoxList::SortedBoxes() const {
  EnsureSorted();
  return sorted_cache_;
}

}  // namespace fitsolver
