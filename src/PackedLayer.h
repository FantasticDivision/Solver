#pragma once

#include <vector>

#include "PackedItem.h"

namespace fitsolver {

// One horizontal slab of a box's packing, built by LayerPacker::PackLayer.
// Tracks its own bounding box (in x/y/z) so VolumePacker can stack the
// next layer directly on top, and LayerStabiliser can reorder layers by
// footprint. Mirrors BoxPacker's `PackedLayer`.
class PackedLayer {
 public:
  void Insert(PackedItem item);
  // Absorbs every item from `other` into this layer (used when a
  // recursive sub-region -- stacked on top, or filling a lengthwise gap
  // -- comes back with placements of its own).
  void Merge(const PackedLayer& other);

  const std::vector<PackedItem>& GetItems() const { return items_; }
  bool Empty() const { return items_.empty(); }

  double GetStartX() const { return start_x_; }
  double GetEndX() const { return end_x_; }
  double GetStartY() const { return start_y_; }
  double GetEndY() const { return end_y_; }
  double GetStartZ() const { return start_z_; }
  double GetEndZ() const { return end_z_; }
  double GetWidth() const { return width_; }
  double GetLength() const { return length_; }
  double GetDepth() const { return depth_; }
  double GetFootprint() const { return footprint_; }
  double GetWeight() const { return weight_; }

 private:
  std::vector<PackedItem> items_;
  double start_x_ = 0.0;
  double end_x_ = 0.0;
  double start_y_ = 0.0;
  double end_y_ = 0.0;
  double start_z_ = 0.0;
  double end_z_ = 0.0;
  double width_ = 0.0;
  double length_ = 0.0;
  double depth_ = 0.0;
  double footprint_ = 0.0;
  double weight_ = 0.0;
};

}  // namespace fitsolver
