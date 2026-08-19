// Basic coverage for PackedItem/List, PackedBox/List and DefaultPackedBoxSorter.

#include <gtest/gtest.h>

#include "DefaultPackedBoxSorter.h"
#include "PackedBox.h"
#include "PackedBoxList.h"
#include "PackedItemList.h"
#include "test_helpers.h"

using fitsolver::test::MakeBox;
using fitsolver::test::MakeItem;
using fitsolver::test::MakePackedItem;

TEST(PackedItemClassTest, StoresPlacementAndAppliesOffsets) {
  const auto item = MakeItem("A", 2, 3, 4, 1.5);
  const auto packed = MakePackedItem(item, 1, 2, 3);
  const auto moved = packed.WithOffset(10, 20, 30);

  EXPECT_DOUBLE_EQ(moved.GetX(), 11.0);
  EXPECT_DOUBLE_EQ(moved.GetY(), 22.0);
  EXPECT_DOUBLE_EQ(moved.GetZ(), 33.0);
  EXPECT_EQ(moved.ToJson().at("ItemCode"), "A");
}

TEST(PackedItemListClassTest, CalculatesCountWeightAndVolume) {
  fitsolver::PackedItemList items;
  items.Insert(MakePackedItem(MakeItem("A", 2, 3, 4, 1.5)));
  items.Insert(MakePackedItem(MakeItem("B", 1, 2, 3, 2.5)));

  EXPECT_EQ(items.Count(), 2u);
  EXPECT_DOUBLE_EQ(items.GetWeight(), 4.0);
  EXPECT_DOUBLE_EQ(items.GetVolume(), 30.0);
}

TEST(PackedBoxClassTest, CalculatesWeightSpaceAndUtilisation) {
  fitsolver::PackedBox box(MakeBox("BOX", 10, 10, 10, 10, 1));
  box.AddItem(MakePackedItem(MakeItem("A", 5, 5, 5, 2)));

  EXPECT_DOUBLE_EQ(box.GetWeight(), 3.0);
  EXPECT_DOUBLE_EQ(box.GetRemainingWeight(), 7.0);
  EXPECT_DOUBLE_EQ(box.GetVolumeUtilisation(), 12.5);
  EXPECT_EQ(box.ToJson().at("item_count"), 1);
}

TEST(PackedBoxSorterClassTest, PrefersTheBoxContainingMoreItems) {
  fitsolver::PackedBox one(MakeBox("ONE", 10, 10, 10));
  fitsolver::PackedBox two(MakeBox("TWO", 10, 10, 10));
  one.AddItem(MakePackedItem(MakeItem("A", 1, 1, 1, 1)));
  two.AddItem(MakePackedItem(MakeItem("A", 1, 1, 1, 1)));
  two.AddItem(MakePackedItem(MakeItem("B", 1, 1, 1, 1), 1));

  fitsolver::DefaultPackedBoxSorter sorter;
  EXPECT_LT(sorter.Compare(two, one), 0);
}

TEST(PackedBoxListClassTest, SortsAndCalculatesWeightStatistics) {
  fitsolver::PackedBox light(MakeBox("LIGHT", 10, 10, 10));
  fitsolver::PackedBox heavy(MakeBox("HEAVY", 10, 10, 10));
  light.AddItem(MakePackedItem(MakeItem("A", 1, 1, 1, 2)));
  heavy.AddItem(MakePackedItem(MakeItem("B", 1, 1, 1, 6)));

  fitsolver::PackedBoxList boxes;
  boxes.Insert(light);
  boxes.Insert(heavy);
  EXPECT_EQ(boxes.Count(), 2u);
  EXPECT_DOUBLE_EQ(boxes.GetMeanItemWeight(), 4.0);
  EXPECT_DOUBLE_EQ(boxes.GetWeightVariance(), 4.0);
}
