#include "ItemList.h"

#include <algorithm>

#include "LinkedItem.h"

namespace fitsolver {

ItemList::ItemList(std::shared_ptr<const ItemSorter> sorter) : sorter_(std::move(sorter)) {}

void ItemList::Insert(ItemPtr item) {
  items_.push_back(std::move(item));
  is_sorted_ = false;
}

void ItemList::Remove(const ItemPtr& item) {
  const auto it = std::find_if(items_.begin(), items_.end(),
                                [&](const ItemPtr& candidate) { return candidate.get() == item.get(); });
  if (it != items_.end()) {
    items_.erase(it);
    is_sorted_ = false;
  }
}

void ItemList::RemovePacked(const PackedItemList& packed) {
  for (const auto& packed_item : packed) {
    const Item* target = packed_item.GetItem().get();
    const auto it = std::find_if(items_.begin(), items_.end(),
                                  [&](const ItemPtr& candidate) { return candidate.get() == target; });
    if (it != items_.end()) {
      items_.erase(it);
      is_sorted_ = false;
    }
  }
}

double ItemList::GetVolume() const {
  double total = 0.0;
  for (const auto& item : items_) total += item->GetVolume();
  return total;
}

bool ItemList::HasLinkedItems() const {
  for (const auto& item : items_) {
    if (dynamic_cast<const LinkedItem*>(item.get()) != nullptr) return true;
  }
  return false;
}

std::unordered_map<std::string, int> ItemList::GetLinkedGroupCounts() const {
  std::unordered_map<std::string, int> counts;
  for (const auto& item : items_) {
    if (const auto* linked = dynamic_cast<const LinkedItem*>(item.get())) {
      ++counts[linked->GetBoxGroup()];
    }
  }
  return counts;
}

void ItemList::EnsureSorted() const {
  if (is_sorted_) return;
  sorted_cache_ = items_;
  std::sort(sorted_cache_.begin(), sorted_cache_.end(), [this](const ItemPtr& a, const ItemPtr& b) {
    return sorter_->Compare(*a, *b) < 0;
  });
  is_sorted_ = true;
}

const std::vector<ItemPtr>& ItemList::SortedItems() const {
  EnsureSorted();
  return sorted_cache_;
}

}  // namespace fitsolver
