#include "GravitySettler.h"

#include <utility>
#include <vector>

#include "ItemReseater.h"
#include "StableRepacker.h"
#include "Support.h"

namespace fitsolver {

PackedBox GravitySettler::Settle(const PackedBox& box) const {
  std::vector<PackedItem> items = DropOntoSupport(box.GetItems().ToVector());
  items = ItemReseater(box.GetBox()).Reseat(std::move(items));
  if (CountNotStandingFirmly(items) > 0) {
    if (auto layout = StableRepacker(box.GetBox()).Repack(items)) items = std::move(*layout);
  }
  SortBottomUp(items);

  PackedBox settled(box.GetBox());
  for (auto& item : items) settled.AddItem(std::move(item));
  return settled;
}

}  // namespace fitsolver
