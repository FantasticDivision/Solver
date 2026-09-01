#include "EntropyBoxSorter.h"

#include <limits>

#include "PackingEntropy.h"

namespace fitsolver {

namespace {
// Unlimited (kNoWeightLimit) sorts as "largest possible capacity".
double CapacityForSorting(const Box& box) {
  const double capacity = box.GetWeightCapacityForItems();
  return capacity < 0.0 ? std::numeric_limits<double>::max() : capacity;
}
}  // namespace

int EntropyBoxSorter::Compare(const Box& a, const Box& b) const {
  if (a.GetInnerVolume() != b.GetInnerVolume()) {
    return a.GetInnerVolume() < b.GetInnerVolume() ? -1 : 1;
  }

  // Volume tie: prefer the more cube-like (higher shape entropy) box --
  // it accommodates a wider range of item orientations without wasting
  // space than an equally-sized but oddly-proportioned one.
  const double entropy_a = ShapeEntropy(a.GetWidth(), a.GetLength(), a.GetDepth());
  const double entropy_b = ShapeEntropy(b.GetWidth(), b.GetLength(), b.GetDepth());
  if (entropy_a != entropy_b) return entropy_a > entropy_b ? -1 : 1;

  if (a.GetEmptyWeight() != b.GetEmptyWeight()) {
    return a.GetEmptyWeight() < b.GetEmptyWeight() ? -1 : 1;
  }
  const double capacity_a = CapacityForSorting(a);
  const double capacity_b = CapacityForSorting(b);
  if (capacity_a != capacity_b) return capacity_a > capacity_b ? -1 : 1;
  return 0;
}

}  // namespace fitsolver
