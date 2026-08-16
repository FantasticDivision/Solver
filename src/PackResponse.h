#pragma once

#include <string>

#include "ItemList.h"
#include "PackedBoxList.h"
#include "nlohmann/json.hpp"

namespace fitsolver {

// The outcome of a Packer::Pack() run.
class PackResponse {
 public:
  bool success = false;
  PackedBoxList boxes;
  ItemList unplaced_items;

  double GetOverallVolumeUtilisation() const;
  int GetTotalPlacedCount() const;
  int GetTotalUnplacedCount() const { return static_cast<int>(unplaced_items.Count()); }

  nlohmann::json ToJson() const;
  // indent < 0 produces compact (no whitespace) JSON.
  std::string Dump(int indent = 2) const;
};

}  // namespace fitsolver
