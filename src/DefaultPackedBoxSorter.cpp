#include "DefaultPackedBoxSorter.h"

#include "PackedBox.h"

namespace fitsolver {

int DefaultPackedBoxSorter::Compare(const PackedBox& a, const PackedBox& b) const {
  if (a.GetItems().Count() != b.GetItems().Count()) {
    return a.GetItems().Count() > b.GetItems().Count() ? -1 : 1;
  }
  if (a.GetVolumeUtilisation() != b.GetVolumeUtilisation()) {
    return a.GetVolumeUtilisation() > b.GetVolumeUtilisation() ? -1 : 1;
  }
  if (a.GetUsedVolume() != b.GetUsedVolume()) {
    return a.GetUsedVolume() > b.GetUsedVolume() ? -1 : 1;
  }
  return 0;
}

}  // namespace fitsolver
