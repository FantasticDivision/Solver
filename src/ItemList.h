#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "DefaultItemSorter.h"
#include "Item.h"
#include "ItemSorter.h"
#include "PackedItemList.h"

namespace fitsolver {

// Ordered collection of items awaiting packing. Ordering comes from an
// injected ItemSorter (DefaultItemSorter unless told otherwise) and is
// only computed when the list is actually iterated -- mirrors
// BoxPacker's `ItemList`.
class ItemList {
 public:
  explicit ItemList(std::shared_ptr<const ItemSorter> sorter = std::make_shared<DefaultItemSorter>());

  void Insert(ItemPtr item);
  // Removes exactly one occurrence of `item`, matched by identity.
  void Remove(const ItemPtr& item);
  // Removes, by identity, every item that appears in `packed`. Used to
  // shrink the "still unplaced" list after a box is committed -- mirrors
  // BoxPacker's `ItemList::removePackedItems()`.
  void RemovePacked(const PackedItemList& packed);

  std::size_t Count() const { return items_.size(); }
  bool Empty() const { return items_.empty(); }
  double GetVolume() const;

  bool HasLinkedItems() const;
  // Box group id -> count of items in this list carrying that group.
  std::unordered_map<std::string, int> GetLinkedGroupCounts() const;

  // Items in sorter-preferred order (index 0 = highest priority).
  const std::vector<ItemPtr>& SortedItems() const;

  // The ordering this list was built with, so a derived list (e.g. a
  // filtered subset being repacked) keeps the caller's chosen priority
  // instead of silently reverting to DefaultItemSorter.
  const std::shared_ptr<const ItemSorter>& GetSorter() const { return sorter_; }

  std::vector<ItemPtr>::const_iterator begin() const { return SortedItems().begin(); }
  std::vector<ItemPtr>::const_iterator end() const { return SortedItems().end(); }

 private:
  void EnsureSorted() const;

  std::shared_ptr<const ItemSorter> sorter_;
  std::vector<ItemPtr> items_;
  // Lazily-computed sorted view of items_; left untouched (and marked
  // stale) whenever items_ changes, and rebuilt on next access.
  mutable std::vector<ItemPtr> sorted_cache_;
  mutable bool is_sorted_ = false;
};

}  // namespace fitsolver
