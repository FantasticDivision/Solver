#include "EntropyItemSorter.h"

#include "LinkedItem.h"
#include "PackingEntropy.h"

namespace fitsolver {

namespace {
const std::string* BoxGroupOf(const Item& item) {
  if (const auto* linked = dynamic_cast<const LinkedItem*>(&item)) {
    return &linked->GetBoxGroup();
  }
  return nullptr;
}
}  // namespace

int EntropyItemSorter::Compare(const Item& a, const Item& b) const {
  // Same box-group clustering as DefaultItemSorter -- this is what keeps
  // a LinkedItem group placeable together, not a tunable heuristic.
  const std::string* group_a = BoxGroupOf(a);
  const std::string* group_b = BoxGroupOf(b);
  if ((group_a != nullptr) != (group_b != nullptr)) {
    return group_a != nullptr ? -1 : 1;
  }
  if (group_a != nullptr && group_b != nullptr && *group_a != *group_b) {
    return *group_a < *group_b ? -1 : 1;
  }

  if (a.GetVolume() != b.GetVolume()) return a.GetVolume() > b.GetVolume() ? -1 : 1;

  // Volume tie: prefer placing the more awkwardly-shaped item first --
  // lower shape entropy means more elongated/irregular, which is harder
  // to place well once space is fragmented.
  const double entropy_a = ShapeEntropy(a.GetWidth(), a.GetLength(), a.GetDepth());
  const double entropy_b = ShapeEntropy(b.GetWidth(), b.GetLength(), b.GetDepth());
  if (entropy_a != entropy_b) return entropy_a < entropy_b ? -1 : 1;

  if (a.GetWeight() != b.GetWeight()) return a.GetWeight() > b.GetWeight() ? -1 : 1;
  if (a.GetItemCode() != b.GetItemCode()) return a.GetItemCode() < b.GetItemCode() ? -1 : 1;
  return 0;
}

}  // namespace fitsolver
