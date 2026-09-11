#pragma once

#include <algorithm>
#include <vector>

namespace fitsolver {

// Tracks, within one packing region, the current topmost occupied Z
// under any given (x, y) footprint -- so a new item can be placed flush
// on top of whatever is actually beneath it, instead of assuming every
// item sharing a "layer" has the same height (which is what caused
// items to float: a layer mixing a 75mm-deep item and a 150mm-deep item
// has an uneven real top surface, not one flat plane).
//
// Exact, not discretized: each entry is the real rectangular footprint
// and top Z of one placed item, so queries are geometrically precise.
// Fine for the item counts a single box/void realistically holds
// (dozens to a few hundred) -- an O(n) scan per query, not a spatial
// index, which would only start to matter at far larger item counts.
class HeightMap {
 public:
  explicit HeightMap(double floor_z) : floor_z_(floor_z) {}

  // The Z an item with this (x, y) footprint needs to sit at to rest on
  // top of whatever's already there -- or the region floor, if this
  // footprint is still completely empty.
  double HeightUnder(double x, double y, double width, double length) const {
    double max_top = floor_z_;
    for (const auto& column : columns_) {
      if (x >= column.x + column.width || x + width <= column.x ||
          y >= column.y + column.length || y + length <= column.y) {
        continue;  // footprints don't overlap in X/Y
      }
      max_top = std::max(max_top, column.top_z);
    }
    return max_top;
  }

  // Records that an item now occupies this footprint up to `top_z`.
  void Add(double x, double y, double width, double length, double top_z) {
    columns_.push_back({x, y, width, length, top_z});
  }

 private:
  struct Column {
    double x, y, width, length, top_z;
  };

  double floor_z_;
  std::vector<Column> columns_;
};

}  // namespace fitsolver