#pragma once

#include <memory>
#include <vector>

#include "Box.h"
#include "BoxSorter.h"
#include "DefaultBoxSorter.h"

namespace fitsolver {

// Ordered collection of box types available to pack into. Ordering comes
// from an injected BoxSorter (DefaultBoxSorter unless told otherwise)
// and is only computed when the list is actually iterated -- mirrors
// BoxPacker's `BoxList`.
class BoxList {
 public:
  explicit BoxList(std::shared_ptr<const BoxSorter> sorter = std::make_shared<DefaultBoxSorter>());

  void Insert(BoxPtr box);

  std::size_t Count() const { return boxes_.size(); }
  bool Empty() const { return boxes_.empty(); }

  // Boxes in sorter-preferred order (index 0 = tried first).
  const std::vector<BoxPtr>& SortedBoxes() const;

  std::vector<BoxPtr>::const_iterator begin() const { return SortedBoxes().begin(); }
  std::vector<BoxPtr>::const_iterator end() const { return SortedBoxes().end(); }

 private:
  void EnsureSorted() const;

  std::shared_ptr<const BoxSorter> sorter_;
  std::vector<BoxPtr> boxes_;
  mutable std::vector<BoxPtr> sorted_cache_;
  mutable bool is_sorted_ = false;
};

}  // namespace fitsolver
