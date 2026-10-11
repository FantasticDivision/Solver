#pragma once

#include <algorithm>
#include <vector>

#include "Item.h"

namespace fitsolver {

// A priority-ordered work queue for one VolumePacker::Pack() attempt.
// Items normally come off the front in ItemSorter priority order, but
// Requeue() lets a caller push a batch of skipped items back to the
// FRONT, in the order given, jumping ahead of whatever's already queued
// rather than being re-sorted back to their original position. Mirrors
// BoxPacker's ItemList::extract()/top()/topN()/requeue().
//
// Stored back-to-front in one contiguous vector so the front (the next item
// to pack) is the cheap end: Extract() is a pop_back, and the sorter can read
// "what's still to come" in place (Count/At/Top) instead of every placement
// copying the whole remaining queue.
class PackQueue {
 public:
  explicit PackQueue(const std::vector<ItemPtr>& items) : items_(items.rbegin(), items.rend()) {}

  bool Empty() const { return items_.empty(); }
  std::size_t Count() const { return items_.size(); }
  const ItemPtr& Top() const { return items_.back(); }

  // The i-th item from the front (At(0) == Top()).
  const ItemPtr& At(std::size_t i) const { return items_[items_.size() - 1 - i]; }

  ItemPtr Extract() {
    ItemPtr item = std::move(items_.back());
    items_.pop_back();
    return item;
  }

  void Requeue(const std::vector<ItemPtr>& skipped) {
    items_.insert(items_.end(), skipped.rbegin(), skipped.rend());
  }

 private:
  std::vector<ItemPtr> items_;
};

}  // namespace fitsolver
