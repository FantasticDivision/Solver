// Basic coverage for WorkingVolume, VolumePacker and WeightRedistributor.

#include <gtest/gtest.h>

#include "BoxList.h"
#include "ItemList.h"
#include "PackedBox.h"
#include "PackedBoxList.h"
#include "VolumePacker.h"
#include "WeightRedistributor.h"
#include "WorkingVolume.h"
#include "test_helpers.h"

using fitsolver::BoxList;
using fitsolver::ItemList;
using fitsolver::test::MakeBox;
using fitsolver::test::MakeItem;
using fitsolver::test::MakePackedItem;

TEST(WorkingVolumeClassTest, IsAnActiveUnlimitedSubregion) {
  fitsolver::WorkingVolume volume(2, 3, 4);
  EXPECT_TRUE(volume.GetActive());
  EXPECT_FALSE(volume.HasWeightLimit());
  EXPECT_DOUBLE_EQ(volume.GetInnerVolume(), 24.0);
}

TEST(VolumePackerClassTest, PacksOnlyItemsThatFitOneBox) {
  ItemList items;
  items.Insert(MakeItem("FITS", 2, 2, 2, 1));
  items.Insert(MakeItem("TOO-BIG", 20, 20, 20, 1));

  const auto packed = fitsolver::VolumePacker(MakeBox("BOX", 10, 10, 10)).Pack(items);
  ASSERT_EQ(packed.GetItems().Count(), 1u);
  EXPECT_EQ(packed.GetItems().begin()->GetItem()->GetItemCode(), "FITS");
}

TEST(WeightRedistributorClassTest, LeavesBalancedBoxesBalanced) {
  BoxList available;
  available.Insert(MakeBox("BOX", 10, 10, 10));
  fitsolver::PackedBox first(MakeBox("BOX", 10, 10, 10));
  fitsolver::PackedBox second(MakeBox("BOX", 10, 10, 10));
  first.AddItem(MakePackedItem(MakeItem("A", 1, 1, 1, 2)));
  second.AddItem(MakePackedItem(MakeItem("B", 1, 1, 1, 2)));
  fitsolver::PackedBoxList original;
  original.Insert(first);
  original.Insert(second);

  const auto result = fitsolver::WeightRedistributor(available).Redistribute(original);
  EXPECT_EQ(result.Count(), 2u);
  EXPECT_DOUBLE_EQ(result.GetWeightVariance(), 0.0);
}
