#include <gtest/gtest.h>

#include <cmath>
#include <memory>

#include "BoxList.h"
#include "ConcreteBox.h"
#include "ConcreteItem.h"
#include "EntropyBoxSorter.h"
#include "EntropyItemSorter.h"
#include "ItemList.h"
#include "PackResponse.h"
#include "Packer.h"
#include "PackingEntropy.h"
#include "test_helpers.h"

using fitsolver::BoxList;
using fitsolver::BoxPtr;
using fitsolver::ConcreteItem;
using fitsolver::ConcreteLinkedItem;
using fitsolver::EntropyBoxSorter;
using fitsolver::EntropyItemSorter;
using fitsolver::ItemList;
using fitsolver::ItemPtr;
using fitsolver::Packer;
using fitsolver::PackResponse;
using fitsolver::ShannonEntropy;
using fitsolver::ShapeEntropy;
using fitsolver::test::MakeBox;
using fitsolver::test::MakeItem;

// ---------------------------------------------------------------------
// PackingEntropy
// ---------------------------------------------------------------------

TEST(ShannonEntropyTest, UniformDistributionIsMaximalEntropy) {
  const double uniform = ShannonEntropy({1.0, 1.0, 1.0});
  EXPECT_NEAR(uniform, std::log(3.0), 1e-9);
}

TEST(ShannonEntropyTest, SingleNonzeroWeightIsZeroEntropy) {
  EXPECT_NEAR(ShannonEntropy({5.0, 0.0, 0.0}), 0.0, 1e-9);
}

TEST(ShannonEntropyTest, AllZeroWeightsIsZeroEntropy) {
  EXPECT_NEAR(ShannonEntropy({0.0, 0.0, 0.0}), 0.0, 1e-9);
}

TEST(ShannonEntropyTest, MoreSkewedDistributionHasLowerEntropy) {
  const double mild = ShannonEntropy({2.0, 1.0, 1.0});
  const double skewed = ShannonEntropy({10.0, 1.0, 1.0});
  EXPECT_LT(skewed, mild);
}

TEST(ShannonEntropyTest, ScaleInvariant) {
  // Entropy depends only on the *shape* of the distribution, not its
  // overall magnitude.
  EXPECT_NEAR(ShannonEntropy({1.0, 2.0, 3.0}), ShannonEntropy({100.0, 200.0, 300.0}), 1e-9);
}

TEST(ShapeEntropyTest, CubeIsMaximalEntropy) {
  EXPECT_NEAR(ShapeEntropy(5.0, 5.0, 5.0), std::log(3.0), 1e-9);
}

TEST(ShapeEntropyTest, DegenerateFlatShapeIsZeroEntropy) {
  // All extent on one axis -- as disordered/skewed as a 3-axis shape
  // can get.
  EXPECT_NEAR(ShapeEntropy(10.0, 0.0, 0.0), 0.0, 1e-9);
}

TEST(ShapeEntropyTest, ElongatedShapeHasLowerEntropyThanNearCube) {
  const double near_cube = ShapeEntropy(10.0, 9.0, 11.0);
  const double elongated = ShapeEntropy(100.0, 1.0, 1.0);
  EXPECT_LT(elongated, near_cube);
}

TEST(ShapeEntropyTest, IsPermutationInvariant) {
  const double base = ShapeEntropy(2.0, 3.0, 5.0);
  EXPECT_NEAR(ShapeEntropy(5.0, 2.0, 3.0), base, 1e-9);
  EXPECT_NEAR(ShapeEntropy(3.0, 5.0, 2.0), base, 1e-9);
}

// ---------------------------------------------------------------------
// EntropyItemSorter
// ---------------------------------------------------------------------

TEST(EntropyItemSorterTest, LargerVolumeSortsFirstRegardlessOfShape) {
  EntropyItemSorter sorter;
  // Small elongated item vs. a larger cube -- volume is still the
  // primary key, so the cube (bigger) must win despite having higher
  // (less "difficult") shape entropy.
  ItemPtr elongated = MakeItem("SMALL-ELONGATED", 50, 1, 1, 1.0);
  ItemPtr big_cube = MakeItem("BIG-CUBE", 10, 10, 10, 1.0);
  EXPECT_GT(sorter.Compare(*elongated, *big_cube), 0);
  EXPECT_LT(sorter.Compare(*big_cube, *elongated), 0);
}

TEST(EntropyItemSorterTest, VolumeTieBreaksTowardMoreElongatedShapeFirst) {
  EntropyItemSorter sorter;
  // Same volume (1000), very different shape.
  ItemPtr cube = MakeItem("CUBE", 10, 10, 10, 1.0);
  ItemPtr plank = MakeItem("PLANK", 100, 10, 1, 1.0);
  ASSERT_DOUBLE_EQ(cube->GetVolume(), plank->GetVolume());

  EXPECT_LT(sorter.Compare(*plank, *cube), 0);  // plank (harder shape) sorts first
  EXPECT_GT(sorter.Compare(*cube, *plank), 0);
}

TEST(EntropyItemSorterTest, LinkedItemsShareABoxGroupAndSortAfterUngrouped) {
  EntropyItemSorter sorter;
  ConcreteItem base_a("A", "A", 1, 1, 1, 1.0);
  ConcreteItem base_b("B", "B", 1, 1, 1, 1.0);
  ItemPtr grouped = std::make_shared<ConcreteLinkedItem>(base_a, "GROUP-1");
  ItemPtr grouped2 = std::make_shared<ConcreteLinkedItem>(base_b, "GROUP-1");
  ItemPtr solo_big = MakeItem("SOLO-BIG", 100, 100, 100, 1.0);

  // An ungrouped item sorts before any grouped one, even a much smaller
  // one -- matches DefaultItemSorter's clustering (ungrouped-first),
  // which EntropyItemSorter must preserve for LinkedItem correctness.
  EXPECT_GT(sorter.Compare(*grouped, *solo_big), 0);
  EXPECT_LT(sorter.Compare(*solo_big, *grouped), 0);
  // Same group and volume -- falls through to the item_code tiebreak
  // ("A" < "B"), same as DefaultItemSorter's final tiebreak.
  EXPECT_LT(sorter.Compare(*grouped, *grouped2), 0);
}

TEST(EntropyItemSorterTest, SortedItemsOrdersByThisSorter) {
  ItemList items(std::make_shared<EntropyItemSorter>());
  items.Insert(MakeItem("CUBE", 10, 10, 10, 1.0));
  items.Insert(MakeItem("PLANK", 100, 10, 1, 1.0));
  items.Insert(MakeItem("TINY", 1, 1, 1, 1.0));

  const auto& sorted = items.SortedItems();
  ASSERT_EQ(sorted.size(), 3u);
  EXPECT_EQ(sorted[0]->GetItemCode(), "PLANK");  // tied-volume winner (see test above)
  EXPECT_EQ(sorted[1]->GetItemCode(), "CUBE");
  EXPECT_EQ(sorted[2]->GetItemCode(), "TINY");
}

// ---------------------------------------------------------------------
// EntropyBoxSorter
// ---------------------------------------------------------------------

TEST(EntropyBoxSorterTest, SmallerInnerVolumeSortsFirstRegardlessOfShape) {
  EntropyBoxSorter sorter;
  BoxPtr small_odd = MakeBox("SMALL-ODD", 5, 5, 5);
  BoxPtr big_cube = MakeBox("BIG-CUBE", 10, 10, 10);
  EXPECT_LT(sorter.Compare(*small_odd, *big_cube), 0);
}

TEST(EntropyBoxSorterTest, VolumeTieBreaksTowardMoreCubeLikeBoxFirst) {
  EntropyBoxSorter sorter;
  BoxPtr cube = MakeBox("CUBE", 10, 10, 10);
  BoxPtr oblong = MakeBox("OBLONG", 100, 10, 1);
  ASSERT_DOUBLE_EQ(cube->GetInnerVolume(), oblong->GetInnerVolume());

  EXPECT_LT(sorter.Compare(*cube, *oblong), 0);  // cube (more versatile) sorts first
  EXPECT_GT(sorter.Compare(*oblong, *cube), 0);
}

TEST(EntropyBoxSorterTest, SortedBoxesOrdersByThisSorter) {
  BoxList boxes(std::make_shared<EntropyBoxSorter>());
  boxes.Insert(MakeBox("OBLONG", 100, 10, 1));
  boxes.Insert(MakeBox("CUBE", 10, 10, 10));
  boxes.Insert(MakeBox("BIG", 20, 20, 20));

  const auto& sorted = boxes.SortedBoxes();
  ASSERT_EQ(sorted.size(), 3u);
  EXPECT_EQ(sorted[0]->GetReference(), "CUBE");  // tied-volume winner (see test above)
  EXPECT_EQ(sorted[1]->GetReference(), "OBLONG");
  EXPECT_EQ(sorted[2]->GetReference(), "BIG");
}

// ---------------------------------------------------------------------
// Integration: the entropy sorters plug into the real Packer end to end
// ---------------------------------------------------------------------

TEST(EntropySortersIntegrationTest, PackerStillProducesAValidSuccessfulPack) {
  ItemList items(std::make_shared<EntropyItemSorter>());
  items.Insert(MakeItem("ITM-1", 100, 100, 100, 1.0));
  items.Insert(MakeItem("ITM-2", 50, 200, 20, 0.5));
  items.Insert(MakeItem("ITM-3", 30, 30, 30, 0.2));

  BoxList boxes(std::make_shared<EntropyBoxSorter>());
  boxes.Insert(MakeBox("SML", 60, 60, 60));
  boxes.Insert(MakeBox("LRG", 300, 300, 300, /*max_weight=*/50.0));

  PackResponse response = Packer().Pack(items, boxes);

  EXPECT_TRUE(response.success);
  EXPECT_EQ(response.GetTotalUnplacedCount(), 0);
  EXPECT_EQ(response.GetTotalPlacedCount(), 3);
}

TEST(EntropySortersIntegrationTest, RespectsLinkedItemsWithEntropySorter) {
  ItemList items(std::make_shared<EntropyItemSorter>());
  ConcreteItem base_a("A", "A", 10, 10, 10, 1.0);
  ConcreteItem base_b("B", "B", 10, 10, 10, 1.0);
  items.Insert(std::make_shared<ConcreteLinkedItem>(base_a, "GROUP-1"));
  items.Insert(std::make_shared<ConcreteLinkedItem>(base_b, "GROUP-1"));

  BoxList boxes(std::make_shared<EntropyBoxSorter>());
  boxes.Insert(MakeBox("A-ONLY", 12, 12, 10));  // fits exactly one item, not both
  boxes.Insert(MakeBox("BOTH", 30, 30, 30));    // fits both

  PackResponse response = Packer().Pack(items, boxes);

  // The group must end up together in one box (BOTH), not split across
  // A-ONLY and a second box.
  ASSERT_EQ(response.boxes.Count(), 1u);
  EXPECT_EQ(response.boxes.SortedBoxes()[0].GetBox()->GetReference(), "BOTH");
  EXPECT_EQ(response.boxes.SortedBoxes()[0].GetItems().Count(), 2u);
}
