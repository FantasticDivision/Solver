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

TEST(VolumePackerClassTest, EmptyInputProducesEmptyBox) {
  const auto packed = fitsolver::VolumePacker(MakeBox("BOX", 4, 4, 4)).Pack(ItemList{});
  EXPECT_EQ(packed.GetItems().Count(), 0u);
}

TEST(VolumePackerClassTest, ExactFitFillsEntireBox) {
  ItemList items;
  items.Insert(MakeItem("A", 2, 3, 4, 1, fitsolver::Rotation::kNever));
  const auto packed = fitsolver::VolumePacker(MakeBox("BOX", 2, 3, 4)).Pack(items);
  ASSERT_EQ(packed.GetItems().Count(), 1u);
  EXPECT_DOUBLE_EQ(packed.GetVolumeUtilisation(), 100.0);
}

TEST(VolumePackerClassTest, IncludesTareWeightAtExactCapacity) {
  ItemList items;
  items.Insert(MakeItem("A", 1, 1, 1, 3));
  items.Insert(MakeItem("B", 1, 1, 1, 2));
  const auto packed = fitsolver::VolumePacker(MakeBox("BOX", 4, 4, 4, 7, 2)).Pack(items);
  EXPECT_EQ(packed.GetItems().Count(), 2u);
  EXPECT_DOUBLE_EQ(packed.GetRemainingWeight(), 0.0);
}

TEST(VolumePackerClassTest, SkipsOverweightItemAndContinuesWithLighterItem) {
  ItemList items;
  items.Insert(MakeItem("HEAVY", 3, 3, 3, 6));
  items.Insert(MakeItem("LIGHT", 1, 1, 1, 2));
  const auto packed = fitsolver::VolumePacker(MakeBox("BOX", 4, 4, 4, 5, 1)).Pack(items);
  ASSERT_EQ(packed.GetItems().Count(), 1u);
  EXPECT_EQ(packed.GetItems().begin()->GetItem()->GetItemCode(), "LIGHT");
}

TEST(VolumePackerClassTest, TareExceedingLimitDoesNotBecomeUnlimitedCapacity) {
  ItemList items;
  items.Insert(MakeItem("A", 1, 1, 1, 1));
  const auto packed = fitsolver::VolumePacker(MakeBox("BOX", 4, 4, 4, 2, 3)).Pack(items);
  EXPECT_EQ(packed.GetItems().Count(), 0u);
}

TEST(VolumePackerClassTest, UnlimitedCapacityAcceptsHeavyItems) {
  ItemList items;
  items.Insert(MakeItem("A", 1, 1, 1, 1000000));
  const auto packed = fitsolver::VolumePacker(MakeBox("BOX", 4, 4, 4)).Pack(items);
  EXPECT_EQ(packed.GetItems().Count(), 1u);
}

class VolumePackerRotationTest : public testing::TestWithParam<fitsolver::Rotation> {};

TEST_P(VolumePackerRotationTest, StandingItemOnEndRequiresBestFit) {
  ItemList items;
  items.Insert(MakeItem("A", 2, 3, 4, 1, GetParam()));
  const auto packed = fitsolver::VolumePacker(MakeBox("BOX", 4, 3, 2)).Pack(items);
  EXPECT_EQ(packed.GetItems().Count(), GetParam() == fitsolver::Rotation::kBestFit ? 1u : 0u);
}

TEST_P(VolumePackerRotationTest, SwappingFootprintRequiresRotationPermission) {
  ItemList items;
  items.Insert(MakeItem("A", 2, 3, 4, 1, GetParam()));
  const auto packed = fitsolver::VolumePacker(MakeBox("BOX", 3, 2, 4)).Pack(items);
  EXPECT_EQ(packed.GetItems().Count(), GetParam() == fitsolver::Rotation::kNever ? 0u : 1u);
}

INSTANTIATE_TEST_SUITE_P(RotationPolicies, VolumePackerRotationTest,
    testing::Values(fitsolver::Rotation::kNever, fitsolver::Rotation::kKeepFlat,
                    fitsolver::Rotation::kBestFit),
    [](const testing::TestParamInfo<fitsolver::Rotation>& info) {
      return fitsolver::RotationToString(info.param);
    });

TEST(VolumePackerClassTest, FillsRowsAndLayersWithoutOverlapOrLosingDuplicateCodes) {
  ItemList items;
  for (int i = 0; i < 9; ++i) items.Insert(MakeItem("SAME-CODE", 2, 2, 2, 1));
  const auto packed = fitsolver::VolumePacker(MakeBox("BOX", 4, 4, 4)).Pack(items);
  ASSERT_EQ(packed.GetItems().Count(), 8u);
  EXPECT_EQ(items.Count(), 9u);
  const auto placements = packed.GetItems().ToVector();
  for (std::size_t i = 0; i < placements.size(); ++i) {
    const auto& a = placements[i];
    EXPECT_GE(a.GetX(), 0); EXPECT_GE(a.GetY(), 0); EXPECT_GE(a.GetZ(), 0);
    EXPECT_LE(a.GetX() + a.GetWidth(), 4);
    EXPECT_LE(a.GetY() + a.GetLength(), 4);
    EXPECT_LE(a.GetZ() + a.GetDepth(), 4);
    for (std::size_t j = i + 1; j < placements.size(); ++j) {
      const auto& b = placements[j];
      EXPECT_NE(a.GetItem().get(), b.GetItem().get());
      EXPECT_TRUE(a.GetX() + a.GetWidth() <= b.GetX() ||
                  b.GetX() + b.GetWidth() <= a.GetX() ||
                  a.GetY() + a.GetLength() <= b.GetY() ||
                  b.GetY() + b.GetLength() <= a.GetY() ||
                  a.GetZ() + a.GetDepth() <= b.GetZ() ||
                  b.GetZ() + b.GetDepth() <= a.GetZ());
    }
  }
}

TEST(VolumePackerClassTest, RepackingSameInputDoesNotRetainPlacementState) {
  ItemList items;
  items.Insert(MakeItem("A", 2, 2, 2, 1));
  const fitsolver::VolumePacker packer(MakeBox("BOX", 2, 2, 2));
  const auto first = packer.Pack(items);
  const auto second = packer.Pack(items);
  ASSERT_EQ(first.GetItems().Count(), 1u);
  EXPECT_EQ(first.ToJson(), second.ToJson());
}
