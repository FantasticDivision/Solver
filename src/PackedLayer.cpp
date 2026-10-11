#include "PackedLayer.h"

#include <algorithm>

namespace fitsolver {

void PackedLayer::Insert(PackedItem packed_item) {
  const double item_end_x = packed_item.GetX() + packed_item.GetWidth();
  const double item_end_y = packed_item.GetY() + packed_item.GetLength();
  const double item_end_z = packed_item.GetZ() + packed_item.GetDepth();

  if (items_.empty()) {
    start_x_ = packed_item.GetX();
    end_x_ = item_end_x;
    start_y_ = packed_item.GetY();
    end_y_ = item_end_y;
    start_z_ = packed_item.GetZ();
    end_z_ = item_end_z;
  } else {
    start_x_ = std::min(start_x_, packed_item.GetX());
    end_x_ = std::max(end_x_, item_end_x);
    start_y_ = std::min(start_y_, packed_item.GetY());
    end_y_ = std::max(end_y_, item_end_y);
    start_z_ = std::min(start_z_, packed_item.GetZ());
    end_z_ = std::max(end_z_, item_end_z);
  }

  weight_ += packed_item.GetWeight();
  items_.push_back(std::move(packed_item));

  width_ = end_x_ - start_x_;
  length_ = end_y_ - start_y_;
  depth_ = end_z_ - start_z_;
  footprint_ = width_ * length_;
}

void PackedLayer::Merge(const PackedLayer& other) {
  for (const auto& item : other.GetItems()) Insert(item);
}

}  // namespace fitsolver
