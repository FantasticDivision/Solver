#include "PackingEntropy.h"

#include <cmath>

namespace fitsolver {

double ShannonEntropy(const std::vector<double>& weights) {
  double total = 0.0;
  for (double w : weights) total += w;
  if (total <= 0.0) return 0.0;

  double entropy = 0.0;
  for (double w : weights) {
    if (w <= 0.0) continue;  // 0 * ln(0) := 0, by the usual convention
    const double p = w / total;
    entropy -= p * std::log(p);
  }
  return entropy;
}

double ShapeEntropy(double width, double length, double depth) {
  return ShannonEntropy({width, length, depth});
}

}  // namespace fitsolver
