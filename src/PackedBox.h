#pragma once

#include "Box.h"
#include "PackedItemList.h"
#include "nlohmann/json.hpp"

namespace fitsolver {

// A box together with everything packed into it. Mirrors BoxPacker's
// `PackedBox`.
class PackedBox {
 public:
  explicit PackedBox(BoxPtr box);

  const BoxPtr& GetBox() const { return box_; }
  const PackedItemList& GetItems() const { return items_; }
  void AddItem(PackedItem item);

  double GetItemWeight() const { return items_.GetWeight(); }
  double GetWeight() const;  // box empty weight + total item weight
  double GetMaxWeight() const { return box_->GetMaxWeight(); }
  double GetRemainingWeight() const;  // kNoWeightLimit if box has no limit

  double GetUsedVolume() const { return items_.GetVolume(); }
  double GetInnerVolume() const { return box_->GetInnerVolume(); }
  double GetUnusedVolume() const { return GetInnerVolume() - GetUsedVolume(); }
  double GetVolumeUtilisation() const;  // percentage, 0-100

  double GetUsedWidth() const;
  double GetUsedLength() const;
  double GetUsedDepth() const;
  double GetRemainingWidth() const { return box_->GetWidth() - GetUsedWidth(); }
  double GetRemainingLength() const { return box_->GetLength() - GetUsedLength(); }
  double GetRemainingDepth() const { return box_->GetDepth() - GetUsedDepth(); }

  nlohmann::json ToJson() const;

 private:
  BoxPtr box_;
  PackedItemList items_;
};

}  // namespace fitsolver
