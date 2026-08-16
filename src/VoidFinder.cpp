#include "VoidFinder.h"

#include <algorithm>

namespace fitsolver {

std::vector<VoidSpace> VoidFinder::Find(double width, double length, double depth,
                                         const std::vector<PackedItem>& packed_items) const {
  return SubtractItems({VoidSpace(0, 0, 0, width, length, depth)}, packed_items);
}

std::vector<VoidSpace> VoidFinder::SubtractItems(std::vector<VoidSpace> spaces,
                                                  const std::vector<PackedItem>& packed_items) const {
  for (const auto& packed_item : packed_items) {
    const double item_x = packed_item.GetX();
    const double item_y = packed_item.GetY();
    const double item_z = packed_item.GetZ();
    const double item_end_x = item_x + packed_item.GetWidth();
    const double item_end_y = item_y + packed_item.GetLength();
    const double item_end_z = item_z + packed_item.GetDepth();

    std::vector<VoidSpace> next;
    next.reserve(spaces.size());
    for (const auto& space : spaces) {
      const double space_x = space.GetX();
      const double space_y = space.GetY();
      const double space_z = space.GetZ();
      const double space_end_x = space_x + space.GetWidth();
      const double space_end_y = space_y + space.GetLength();
      const double space_end_z = space_z + space.GetDepth();

      // No overlap -- keep the free space as-is.
      if (item_x >= space_end_x || item_end_x <= space_x || item_y >= space_end_y ||
          item_end_y <= space_y || item_z >= space_end_z || item_end_z <= space_z) {
        next.push_back(space);
        continue;
      }

      const double overlap_x = std::max(space_x, item_x);
      const double overlap_y = std::max(space_y, item_y);
      const double overlap_z = std::max(space_z, item_z);
      const double overlap_end_x = std::min(space_end_x, item_end_x);
      const double overlap_end_y = std::min(space_end_y, item_end_y);
      const double overlap_end_z = std::min(space_end_z, item_end_z);

      const double space_length = space.GetLength();
      const double space_depth = space.GetDepth();
      const double overlap_width = overlap_end_x - overlap_x;
      const double overlap_length = overlap_end_y - overlap_y;

      // Residuals around the overlap -- no gaps, no double-counting.
      if (overlap_x > space_x) {  // left
        next.emplace_back(space_x, space_y, space_z, overlap_x - space_x, space_length, space_depth);
      }
      if (overlap_end_x < space_end_x) {  // right
        next.emplace_back(overlap_end_x, space_y, space_z, space_end_x - overlap_end_x, space_length,
                           space_depth);
      }
      if (overlap_y > space_y) {  // front
        next.emplace_back(overlap_x, space_y, space_z, overlap_width, overlap_y - space_y, space_depth);
      }
      if (overlap_end_y < space_end_y) {  // back
        next.emplace_back(overlap_x, overlap_end_y, space_z, overlap_width, space_end_y - overlap_end_y,
                           space_depth);
      }
      if (overlap_z > space_z) {  // below
        next.emplace_back(overlap_x, overlap_y, space_z, overlap_width, overlap_length, overlap_z - space_z);
      }
      if (overlap_end_z < space_end_z) {  // above
        next.emplace_back(overlap_x, overlap_y, overlap_end_z, overlap_width, overlap_length,
                           space_end_z - overlap_end_z);
      }
    }
    spaces = std::move(next);
  }
  return spaces;
}

}  // namespace fitsolver
