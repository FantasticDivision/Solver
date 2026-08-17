#include <gtest/gtest.h>

#include <memory>

#include "BoxList.h"
#include "ConcreteBox.h"
#include "ConcreteItem.h"
#include "ItemList.h"
#include "PackResponse.h"
#include "Packer.h"

using fitsolver::BoxList;
using fitsolver::BoxPtr;
using fitsolver::ConcreteBox;
using fitsolver::ConcreteItem;
using fitsolver::ConcreteLimitedSupplyBox;
using fitsolver::ConcreteLinkedItem;
using fitsolver::ItemList;
using fitsolver::ItemPtr;
using fitsolver::Packer;
using fitsolver::PackResponse;
using fitsolver::Rotation;

namespace {

ItemPtr MakeItem(const std::string& code, double width, double length, double depth,
                  double weight, Rotation rotation = Rotation::kBestFit) {
  return std::make_shared<ConcreteItem>(code, code, width, length, depth, weight, rotation);
}

BoxPtr MakeBox(const std::string& reference, double width, double length, double depth,
                double max_weight = fitsolver::kNoWeightLimit, double empty_weight = 0.0,
                bool active = true) {
  return std::make_shared<ConcreteBox>(reference, width, length, depth, max_weight, empty_weight, active);
}

}  // namespace

TEST(Packer, SingleItemFitsInSingleBox) {
  ItemList items;
  items.Insert(MakeItem("ITM-1", 100, 100, 100, 1.0));
  BoxList boxes;
  boxes.Insert(MakeBox("SML", 200, 200, 200));

  PackResponse response = Packer().Pack(items, boxes);

  ASSERT_TRUE(response.success);
  ASSERT_EQ(response.boxes.Count(), 1u);
  EXPECT_EQ(response.boxes.begin()->GetItems().Count(), 1u);
  EXPECT_TRUE(response.unplaced_items.Empty());
}

TEST(Packer, ItemTooBigForEveryBoxIsReportedUnplaced) {
  ItemList items;
  items.Insert(MakeItem("ITM-1", 500, 500, 500, 1.0));
  BoxList boxes;
  boxes.Insert(MakeBox("SML", 100, 100, 100));

  PackResponse response = Packer().Pack(items, boxes);

  EXPECT_FALSE(response.success);
  EXPECT_TRUE(response.boxes.Empty());
  ASSERT_EQ(response.unplaced_items.Count(), 1u);
  EXPECT_EQ((*response.unplaced_items.begin())->GetItemCode(), "ITM-1");
}

TEST(Packer, PicksSmallestBoxThatFitsEverything) {
  ItemList items;
  items.Insert(MakeItem("ITM-1", 50, 50, 50, 1.0));
  BoxList boxes;
  // Large box inserted first; the solver should still prefer the small one.
  boxes.Insert(MakeBox("LRG", 1000, 1000, 1000));
  boxes.Insert(MakeBox("SML", 60, 60, 60));

  PackResponse response = Packer().Pack(items, boxes);

  ASSERT_TRUE(response.success);
  ASSERT_EQ(response.boxes.Count(), 1u);
  EXPECT_EQ(response.boxes.begin()->GetBox()->GetReference(), "SML");
}

TEST(Packer, InactiveBoxIsNeverUsed) {
  ItemList items;
  items.Insert(MakeItem("ITM-1", 50, 50, 50, 1.0));
  BoxList boxes;
  boxes.Insert(MakeBox("INACTIVE", 1000, 1000, 1000, fitsolver::kNoWeightLimit, 0.0, /*active=*/false));

  PackResponse response = Packer().Pack(items, boxes);

  EXPECT_FALSE(response.success);
  EXPECT_TRUE(response.boxes.Empty());
  ASSERT_EQ(response.unplaced_items.Count(), 1u);
}

TEST(Packer, OverweightItemIsNotPackedIntoUndersizedWeightBox) {
  ItemList items;
  items.Insert(MakeItem("LIGHT", 10, 10, 10, 1.0));
  items.Insert(MakeItem("HEAVY", 10, 10, 10, 100.0));
  BoxList boxes;
  boxes.Insert(MakeBox("SML", 100, 100, 100, /*max_weight=*/5.0, /*empty_weight=*/0.0));

  PackResponse response = Packer().Pack(items, boxes);

  // The light item should be packed; the heavy one cannot fit under this
  // box's weight limit and should be reported unplaced.
  ASSERT_EQ(response.boxes.Count(), 1u);
  //This should Assert before dereferencing
  ASSERT_EQ(response.boxes.begin()->GetItems().Count(), 1u);
  EXPECT_EQ(response.boxes.begin()->GetItems().begin()->GetItem()->GetItemCode(), "LIGHT");
  ASSERT_EQ(response.unplaced_items.Count(), 1u);
  EXPECT_EQ((*response.unplaced_items.begin())->GetItemCode(), "HEAVY");
}

TEST(Packer, RespectsMaximumBoxesAvailable) {
  ItemList items;
  items.Insert(MakeItem("A", 90, 90, 90, 1.0));
  items.Insert(MakeItem("B", 90, 90, 90, 1.0));

  // Box only fits one item at a time, and only one box is available.
  BoxList boxes;
  boxes.Insert(std::make_shared<ConcreteLimitedSupplyBox>(
      ConcreteBox("SML", 100, 100, 100), /*quantity_available=*/1));

  PackResponse response = Packer().Pack(items, boxes);

  EXPECT_FALSE(response.success);
  EXPECT_EQ(response.boxes.Count(), 1u);
  EXPECT_EQ(response.unplaced_items.Count(), 1u);
}

TEST(Packer, NeverRotationKeepsOriginalOrientation) {
  // Item only fits if placed exactly as given (100 x 10 x 10) -- a box
  // that's 100 wide but only 10 deep should still accept it because no
  // rotation is needed, and reject anything that would require one.
  ItemList items;
  items.Insert(MakeItem("ITM-1", 100, 10, 10, 1.0, Rotation::kNever));
  BoxList boxes;
  boxes.Insert(MakeBox("LONG", 100, 10, 10));

  PackResponse response = Packer().Pack(items, boxes);

  ASSERT_TRUE(response.success);
  ASSERT_EQ(response.boxes.begin()->GetItems().Count(), 1u);
  EXPECT_EQ(response.boxes.begin()->GetItems().begin()->GetOrientationCode(), "WLD");
  EXPECT_FALSE(response.boxes.begin()->GetItems().begin()->IsRotated());
}

TEST(Packer, BestFitRotatesItemToFit) {
  // Item is 10 x 10 x 100 but the box is only 100 wide / 10 deep -- it can
  // only fit if the solver rotates it so its long edge lies along width.
  ItemList items;
  items.Insert(MakeItem("ITM-1", 10, 10, 100, 1.0, Rotation::kBestFit));
  BoxList boxes;
  boxes.Insert(MakeBox("FLAT", 100, 10, 10));

  PackResponse response = Packer().Pack(items, boxes);

  ASSERT_TRUE(response.success);
  ASSERT_EQ(response.boxes.begin()->GetItems().Count(), 1u);
  EXPECT_TRUE(response.boxes.begin()->GetItems().begin()->IsRotated());
}

TEST(Packer, LinkedItemsStayInTheSameBox) {
  // Two items sharing a box group, each of which alone fits one small
  // box, but together they need the larger box. They must not be split.
  ItemList items;
  items.Insert(std::make_shared<ConcreteLinkedItem>(ConcreteItem("A", "A", 60, 60, 60, 1.0), "GROUP-1"));
  items.Insert(std::make_shared<ConcreteLinkedItem>(ConcreteItem("B", "B", 60, 60, 60, 1.0), "GROUP-1"));
  BoxList boxes;
  boxes.Insert(MakeBox("SML", 70, 70, 70));    // fits only one of them
  boxes.Insert(MakeBox("BIG", 200, 200, 200));  // fits both

  PackResponse response = Packer().Pack(items, boxes);

  ASSERT_TRUE(response.success);
  ASSERT_EQ(response.boxes.Count(), 1u);
  EXPECT_EQ(response.boxes.begin()->GetBox()->GetReference(), "BIG");
  EXPECT_EQ(response.boxes.begin()->GetItems().Count(), 2u);
}

TEST(Packer, VolumeUtilisationAndRemainingSpaceAreConsistent) {
  ItemList items;
  items.Insert(MakeItem("ITM-1", 50, 50, 50, 1.0));
  BoxList boxes;
  boxes.Insert(MakeBox("SML", 100, 100, 100));

  PackResponse response = Packer().Pack(items, boxes);

  ASSERT_EQ(response.boxes.Count(), 1u);
  const auto& packed_box = *response.boxes.begin();
  EXPECT_DOUBLE_EQ(packed_box.GetUsedVolume(), 50.0 * 50.0 * 50.0);
  EXPECT_DOUBLE_EQ(packed_box.GetInnerVolume(), 100.0 * 100.0 * 100.0);
  EXPECT_NEAR(packed_box.GetVolumeUtilisation(), 12.5, 1e-9);
  EXPECT_DOUBLE_EQ(packed_box.GetRemainingWidth(), 100.0 - packed_box.GetUsedWidth());
}

TEST(Packer, DumpProducesParseableJson) {
  ItemList items;
  items.Insert(MakeItem("ITM-1", 50, 50, 50, 1.0));
  BoxList boxes;
  boxes.Insert(MakeBox("SML", 100, 100, 100));

  PackResponse response = Packer().Pack(items, boxes);

  const std::string dumped = response.Dump();
  const nlohmann::json parsed = nlohmann::json::parse(dumped);
  EXPECT_TRUE(parsed.at("success").get<bool>());
  EXPECT_EQ(parsed.at("placements").size(), 1u);
  EXPECT_TRUE(parsed.at("unplaced_item_codes").empty());
}

TEST(Packer, EmptyRequestSucceedsTrivially) {
  ItemList items;
  BoxList boxes;
  PackResponse response = Packer().Pack(items, boxes);
  EXPECT_TRUE(response.success);
  EXPECT_TRUE(response.boxes.Empty());
  EXPECT_TRUE(response.unplaced_items.Empty());
}
