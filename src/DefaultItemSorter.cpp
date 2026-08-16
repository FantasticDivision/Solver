#include "DefaultItemSorter.h"

#include "LinkedItem.h"

namespace fitsolver {

namespace {
const std::string* BoxGroupOf(const Item& item) {
  if (const auto* linked = dynamic_cast<const LinkedItem*>(&item)) {
    return &linked->GetBoxGroup();
  }
  return nullptr;
}
}  // namespace

int DefaultItemSorter::Compare(const Item& a, const Item& b) const {
  // Items sharing a box group sort next to each other, same as BoxPacker,
  // so the shelf packer tends to place them close together.
  const std::string* group_a = BoxGroupOf(a);
  const std::string* group_b = BoxGroupOf(b);
  if ((group_a != nullptr) != (group_b != nullptr)) {
    return group_a != nullptr ? -1 : 1;
  }
  if (group_a != nullptr && group_b != nullptr && *group_a != *group_b) {
    return *group_a < *group_b ? -1 : 1;
  }

  if (a.GetVolume() != b.GetVolume()) return a.GetVolume() > b.GetVolume() ? -1 : 1;
  if (a.GetWeight() != b.GetWeight()) return a.GetWeight() > b.GetWeight() ? -1 : 1;
  if (a.GetItemCode() != b.GetItemCode()) return a.GetItemCode() < b.GetItemCode() ? -1 : 1;
  return 0;
}

}  // namespace fitsolver
