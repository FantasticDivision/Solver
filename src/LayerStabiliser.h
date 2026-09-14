#pragma once

#include <vector>

#include "PackedLayer.h"

namespace fitsolver {

// During packing it's quite possible to build layers that aren't
// physically stable -- e.g. a narrow-footprint layer packed before a
// wide one would leave the wide one overhanging thin air. This reorders
// layers so the largest-footprint one ends up at the bottom (ties broken
// by depth), then recomputes every item's Z so the layers stack
// contiguously from the box floor in that order. Mirrors BoxPacker's
// `LayerStabiliser`.
class LayerStabiliser {
 public:
  std::vector<PackedLayer> Stabilise(std::vector<PackedLayer> layers) const;
};

}  // namespace fitsolver
