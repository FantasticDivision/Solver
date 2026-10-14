#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "DefaultPackedBoxSorter.h"
#include "PackedBox.h"
#include "PackedBoxSorter.h"
#include "nlohmann/json.hpp"

namespace fitsolver {

// Ordered collection of completed PackedBox results. Ordering comes from
// an injected PackedBoxSorter (DefaultPackedBoxSorter unless told
// otherwise). Mirrors BoxPacker's `PackedBoxList`.
class PackedBoxList {
 public:
  explicit PackedBoxList(
      std::shared_ptr<const PackedBoxSorter> sorter = std::make_shared<DefaultPackedBoxSorter>());

  void Insert(PackedBox box);

  // Replaces every box with `change(box)`. Meant for changes that leave
  // what the sorter looks at alone (which items a box holds), so the boxes
  // stay listed in exactly the order they were.
  void Apply(const std::function<PackedBox(const PackedBox&)>& change);

  std::size_t Count() const { return boxes_.size(); }
  bool Empty() const { return boxes_.empty(); }

  // Boxes in sorter-preferred order (index 0 = "best").
  const std::vector<PackedBox>& SortedBoxes() const;

  std::vector<PackedBox>::const_iterator begin() const { return SortedBoxes().begin(); }
  std::vector<PackedBox>::const_iterator end() const { return SortedBoxes().end(); }

  // Diagnostics used by WeightRedistributor.
  double GetMeanItemWeight() const;
  double GetWeightVariance() const;

 private:
  void EnsureSorted() const;

  std::shared_ptr<const PackedBoxSorter> sorter_;
  std::vector<PackedBox> boxes_;
  mutable std::vector<PackedBox> sorted_cache_;
  mutable bool is_sorted_ = false;
};

}  // namespace fitsolver
