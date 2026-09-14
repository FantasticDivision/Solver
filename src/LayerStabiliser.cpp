#include "LayerStabiliser.h"

#include <algorithm>

namespace fitsolver {

std::vector<PackedLayer> LayerStabiliser::Stabilise(std::vector<PackedLayer> layers) const {
  std::stable_sort(layers.begin(), layers.end(), [](const PackedLayer& a, const PackedLayer& b) {
    if (a.GetFootprint() != b.GetFootprint()) return a.GetFootprint() > b.GetFootprint();
    return a.GetDepth() > b.GetDepth();
  });

  std::vector<PackedLayer> stabilised;
  stabilised.reserve(layers.size());
  double current_z = 0.0;
  for (const auto& old_layer : layers) {
    const double old_start_z = old_layer.GetStartZ();
    PackedLayer new_layer;
    for (const auto& old_item : old_layer.GetItems()) {
      const double new_z = old_item.GetZ() - old_start_z + current_z;
      new_layer.Insert(old_item.WithOffset(0.0, 0.0, new_z - old_item.GetZ()));
    }
    current_z += new_layer.GetDepth();
    stabilised.push_back(std::move(new_layer));
  }
  return stabilised;
}

}  // namespace fitsolver
