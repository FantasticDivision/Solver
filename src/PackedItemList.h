#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "PackedItem.h"

namespace fitsolver {

// Collection of items that have been placed inside a single box, in
// placement order. Mirrors BoxPacker's `PackedItemList`.
class PackedItemList {
 public:
  void Insert(PackedItem item);

  std::size_t Count() const { return items_.size(); }
  bool Empty() const { return items_.empty(); }
  double GetWeight() const;
  double GetVolume() const;

  // Box group id -> count of packed items carrying that group.
  std::unordered_map<std::string, int> GetLinkedGroupCounts() const;

  const std::vector<PackedItem>& ToVector() const { return items_; }

  std::vector<PackedItem>::const_iterator begin() const { return items_.begin(); }
  std::vector<PackedItem>::const_iterator end() const { return items_.end(); }

 private:
  std::vector<PackedItem> items_;
};

}  // namespace fitsolver
