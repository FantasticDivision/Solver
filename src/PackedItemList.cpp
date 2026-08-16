#include "PackedItemList.h"

#include "LinkedItem.h"

namespace fitsolver {

void PackedItemList::Insert(PackedItem item) { items_.push_back(std::move(item)); }

double PackedItemList::GetWeight() const {
  double total = 0.0;
  for (const auto& item : items_) total += item.GetWeight();
  return total;
}

double PackedItemList::GetVolume() const {
  double total = 0.0;
  for (const auto& item : items_) total += item.GetVolume();
  return total;
}

std::unordered_map<std::string, int> PackedItemList::GetLinkedGroupCounts() const {
  std::unordered_map<std::string, int> counts;
  for (const auto& packed_item : items_) {
    if (const auto* linked = dynamic_cast<const LinkedItem*>(packed_item.GetItem().get())) {
      ++counts[linked->GetBoxGroup()];
    }
  }
  return counts;
}

}  // namespace fitsolver
