#include "DefaultBoxSorter.h"

#include <limits>

namespace fitsolver {

namespace {
// Unlimited (kNoWeightLimit) sorts as "largest possible capacity".
double CapacityForSorting(const Box& box) {
  const double capacity = box.GetWeightCapacityForItems();
  return capacity < 0.0 ? std::numeric_limits<double>::max() : capacity;
}
}  // namespace

int DefaultBoxSorter::Compare(const Box& a, const Box& b) const {
  if (a.GetInnerVolume() != b.GetInnerVolume()) {
    return a.GetInnerVolume() < b.GetInnerVolume() ? -1 : 1;
  }
  if (a.GetEmptyWeight() != b.GetEmptyWeight()) {
    return a.GetEmptyWeight() < b.GetEmptyWeight() ? -1 : 1;
  }
  const double capacity_a = CapacityForSorting(a);
  const double capacity_b = CapacityForSorting(b);
  if (capacity_a != capacity_b) return capacity_a > capacity_b ? -1 : 1;
  return 0;
}

}  // namespace fitsolver
