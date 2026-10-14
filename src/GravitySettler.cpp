#include "GravitySettler.h"

#include <utility>
#include <vector>

#include "ItemReseater.h"
#include "Support.h"

namespace fitsolver {

PackedBox GravitySettler::Settle(const PackedBox& box) const {
  std::vector<PackedItem> items = DropOntoSupport(box.GetItems().ToVector());
  items = ItemReseater(box.GetBox()).Reseat(std::move(items));
  SortBottomUp(items);

  PackedBox settled(box.GetBox());
  for (auto& item : items) settled.AddItem(std::move(item));
  return settled;
}

}  // namespace fitsolver
