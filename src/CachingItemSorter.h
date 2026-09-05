#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "ItemSorter.h"

namespace fitsolver {

// Wraps any ItemSorter and memoizes Compare() results by ItemCode, so a
// pair of items compared once is never re-compared -- works with
// DefaultItemSorter or any future custom sorter unmodified, since it
// only depends on the ItemSorter interface, not any specific
// implementation.
//
// NOTE: correctness relies on ItemCode being a stable identifier -- i.e.
// the same ItemCode always implies the same dimensions/weight, system-
// wide. Confirm this holds before relying on this in production; if
// ItemCode can be reused for different physical items across orders,
// this cache will return wrong comparisons.
class CachingItemSorter : public ItemSorter {
 public:
  explicit CachingItemSorter(std::shared_ptr<const ItemSorter> inner) : inner_(std::move(inner)) {}

  int Compare(const Item& a, const Item& b) const override {
    const auto key = std::make_pair(a.GetItemCode(), b.GetItemCode());

    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = cache_.find(key); it != cache_.end()) return it->second;

    const int result = inner_->Compare(a, b);
    cache_.emplace(key, result);
    return result;
  }

 private:
  std::shared_ptr<const ItemSorter> inner_;
  mutable std::map<std::pair<std::string, std::string>, int> cache_;
  mutable std::mutex mutex_;
};

}  // namespace fitsolver