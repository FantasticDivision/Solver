#pragma once

#include <algorithm>
#include <deque>
#include <vector>

#include "Item.h"

namespace fitsolver {

// A priority-ordered work queue for one VolumePacker::Pack() attempt.
// Items normally come off the front in ItemSorter priority order, but
// Requeue() lets a caller push a batch of skipped items back to the
// FRONT, in the order given, jumping ahead of whatever's already queued
// rather than being re-sorted back to their original position. Mirrors
// BoxPacker's ItemList::extract()/top()/topN()/requeue().
class PackQueue {
 public:
  explicit PackQueue(std::vector<ItemPtr> items) : items_(items.begin(), items.end()) {}

  bool Empty() const { return items_.empty(); }
  std::size_t Count() const { return items_.size(); }
  const ItemPtr& Top() const { return items_.front(); }

  ItemPtr Extract() {
    ItemPtr item = std::move(items_.front());
    items_.pop_front();
    return item;
  }

  std::vector<ItemPtr> TopN(std::size_t n) const {
    std::vector<ItemPtr> result;
    const std::size_t count = std::min(n, items_.size());
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) result.push_back(items_[i]);
    return result;
  }

  void Requeue(const std::vector<ItemPtr>& skipped) {
    items_.insert(items_.begin(), skipped.begin(), skipped.end());
  }

  // Everything currently left in the queue, in priority order. Used to
  // pass "what's still to come" into look-ahead scoring without exposing
  // the deque directly.
  std::vector<ItemPtr> Snapshot() const { return std::vector<ItemPtr>(items_.begin(), items_.end()); }

 private:
  std::deque<ItemPtr> items_;
};

}  // namespace fitsolver
