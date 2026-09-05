#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "BoxSorter.h"

namespace fitsolver {

// Wraps any BoxSorter and memoizes Compare() results by Reference, so a
// pair of box types compared once is never re-compared -- works with
// DefaultBoxSorter or any future custom sorter unmodified, since it
// only depends on the BoxSorter interface, not any specific
// implementation.
//
// Box catalogs are typically small and reused across every request
// (e.g. the same SML/MED/LRG box types on every order), so this cache
// tends to fill up fast and then stay useful for the app's whole
// lifetime -- unlike item comparisons, which reset in usefulness more
// often since items differ per order.
class CachingBoxSorter : public BoxSorter {
 public:
  explicit CachingBoxSorter(std::shared_ptr<const BoxSorter> inner) : inner_(std::move(inner)) {}

  int Compare(const Box& a, const Box& b) const override {
    const auto key = std::make_pair(a.GetReference(), b.GetReference());

    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = cache_.find(key); it != cache_.end()) return it->second;

    const int result = inner_->Compare(a, b);
    cache_.emplace(key, result);
    return result;
  }

 private:
  std::shared_ptr<const BoxSorter> inner_;
  mutable std::map<std::pair<std::string, std::string>, int> cache_;
  mutable std::mutex mutex_;
};

}  // namespace fitsolver