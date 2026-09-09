#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "BoxList.h"
#include "ConcreteBox.h"
#include "ConcreteItem.h"
#include "DefaultBoxSorter.h"
#include "DefaultItemSorter.h"
#include "DefaultPackedBoxSorter.h"
#include "ItemList.h"
#include "LimitedSupplyBox.h"
#include "LinkedItem.h"
#include "OrientatedItem.h"
#include "PackedBox.h"
#include "PackedBoxList.h"
#include "PackedItem.h"
#include "PackedItemList.h"
#include "PackRequest.h"
#include "PackResponse.h"
#include "Packer.h"
#include "VolumePacker.h"
#include "WeightRedistributor.h"
#include "WorkingVolume.h"

using fitsolver::BoxList;
using fitsolver::BoxPtr;
using fitsolver::ConcreteBox;
using fitsolver::ConcreteItem;
using fitsolver::ConcreteLimitedSupplyBox;
using fitsolver::ConcreteLinkedItem;
using fitsolver::ItemList;
using fitsolver::ItemPtr;
using fitsolver::Packer;
using fitsolver::PackRequest;
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

fitsolver::PackedItem MakePackedItem(const ItemPtr& item, double x = 0.0, double y = 0.0,
                                     double z = 0.0, double width = -1.0,
                                     double length = -1.0, double depth = -1.0,
                                     const std::string& code = "WLD") {
  return fitsolver::PackedItem(
      fitsolver::OrientatedItem(item, width < 0.0 ? item->GetWidth() : width,
                               length < 0.0 ? item->GetLength() : length,
                               depth < 0.0 ? item->GetDepth() : depth, code),
      x, y, z);
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
  const bool success = parsed.at("success").get<bool>();
  EXPECT_TRUE(success);
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

TEST(ConcreteItem, ExposesPropertiesVolumeAndJson) {
  ConcreteItem item("SKU-1", "Widget", 2.0, 3.0, 4.0, 1.5, Rotation::kKeepFlat);
  const auto json = item.ToJson();
  const double volume = json.at("Volume").get<double>();

  EXPECT_EQ(item.GetItemCode(), "SKU-1");
  EXPECT_EQ(item.GetItemReference(), "Widget");
  EXPECT_DOUBLE_EQ(item.GetVolume(), 24.0);

  EXPECT_EQ(json.at("AllowedRotation"), "KeepFlat");
  EXPECT_TRUE(json.at("BoxGroup").is_null());
  EXPECT_DOUBLE_EQ(volume, 24.0);
}

TEST(ConcreteItem, ParsesDefaultsAndNullOptionalValues) {
  const auto item = fitsolver::MakeItemFromJson({
      {"ItemCode", "SKU-1"}, {"Width", 2}, {"Length", 3}, {"Depth", 4},
      {"ItemReference", nullptr}, {"Weight", nullptr}, {"AllowedRotation", nullptr}});

  EXPECT_EQ(item->GetItemReference(), "SKU-1");
  EXPECT_DOUBLE_EQ(item->GetWeight(), 0.0);
  EXPECT_EQ(item->GetAllowedRotation(), Rotation::kBestFit);
  EXPECT_EQ(dynamic_cast<const fitsolver::LinkedItem*>(item.get()), nullptr);
}

TEST(ConcreteItem, ParsesLinkedItemAndSerialisesGroup) {
  const auto item = fitsolver::MakeItemFromJson({
      {"ItemCode", "SKU-1"}, {"ItemReference", "Widget"}, {"Width", 2},
      {"Length", 3}, {"Depth", 4}, {"Weight", 1.5}, {"BoxGroup", "ORDER-7"}});

  const auto* linked = dynamic_cast<const fitsolver::LinkedItem*>(item.get());
  ASSERT_NE(linked, nullptr);
  EXPECT_EQ(linked->GetBoxGroup(), "ORDER-7");
  EXPECT_EQ(linked->ToJson().at("BoxGroup"), "ORDER-7");
}

TEST(ConcreteItem, RejectsMissingCodeAndEmptyLinkedGroup) {
  EXPECT_THROW(fitsolver::MakeItemFromJson({{"Width", 1}, {"Length", 1}, {"Depth", 1}}),
               std::invalid_argument);
  EXPECT_THROW(ConcreteLinkedItem(ConcreteItem("A", "A", 1, 1, 1, 1), ""),
               std::invalid_argument);
}

TEST(ConcreteBox, CalculatesWeightCapacityAndJson) {
  ConcreteBox box("BOX", 2.0, 3.0, 4.0, 10.0, 1.5, false);

  EXPECT_TRUE(box.HasWeightLimit());
  EXPECT_DOUBLE_EQ(box.GetInnerVolume(), 24.0);
  EXPECT_DOUBLE_EQ(box.GetWeightCapacityForItems(), 8.5);
  const auto json = box.ToJson();
  const bool active = json.at("Active").get<bool>();
  EXPECT_EQ(json.at("Reference"), "BOX");
  EXPECT_FALSE(active);
  EXPECT_TRUE(json.at("MaximumBoxes").is_null());
}

TEST(ConcreteBox, UnlimitedAndExhaustedWeightCapacities) {
  ConcreteBox unlimited("U", 1, 1, 1);
  ConcreteBox exhausted("E", 1, 1, 1, 2.0, 3.0);

  EXPECT_FALSE(unlimited.HasWeightLimit());
  EXPECT_DOUBLE_EQ(unlimited.GetWeightCapacityForItems(), fitsolver::kNoWeightLimit);
  EXPECT_TRUE(unlimited.ToJson().at("MaxWeight").is_null());
  EXPECT_DOUBLE_EQ(exhausted.GetWeightCapacityForItems(), 0.0);
}

TEST(ConcreteBox, ParsesDefaultsAndLimitedSupply) {
  // MaxWeight is a required field -- explicit null or a missing key both
  // reject the box rather than defaulting to "no weight limit" (see
  // ConcreteBox.cpp's MakeBoxFromJson).
  EXPECT_THROW(fitsolver::MakeBoxFromJson(
                   {{"Reference", "A"}, {"Width", 2}, {"Length", 3}, {"Depth", 4},
                    {"MaxWeight", nullptr}, {"BoxWeight", nullptr}, {"Active", nullptr}}),
               std::invalid_argument);

  const auto plain = fitsolver::MakeBoxFromJson(
      {{"Reference", "A"}, {"Width", 2}, {"Length", 3}, {"Depth", 4}, {"MaxWeight", 10},
       {"BoxWeight", nullptr}, {"Active", nullptr}});
  EXPECT_EQ(plain->GetReference(), "A");
  EXPECT_TRUE(plain->GetActive());
  EXPECT_TRUE(plain->HasWeightLimit());

  const auto limited = fitsolver::MakeBoxFromJson(
      {{"Reference", "B"}, {"Width", 2}, {"Length", 3}, {"Depth", 4}, {"MaxWeight", 10},
       {"MaximumBoxes", 7}});
  const auto* supply = dynamic_cast<const fitsolver::LimitedSupplyBox*>(limited.get());
  ASSERT_NE(supply, nullptr);
  EXPECT_EQ(supply->GetQuantityAvailable(), 7);
  EXPECT_EQ(supply->ToJson().at("MaximumBoxes"), 7);
}

TEST(ConcreteBox, RejectsMissingReference) {
  EXPECT_THROW(fitsolver::MakeBoxFromJson({{"Width", 1}, {"Length", 1}, {"Depth", 1}}),
               std::invalid_argument);
}

TEST(ItemList, SortsByLinkedGroupVolumeWeightAndCode) {
  ItemList items;
  items.Insert(MakeItem("B", 1, 1, 1, 1));
  items.Insert(MakeItem("A", 1, 1, 1, 2));
  items.Insert(MakeItem("LARGE", 2, 2, 2, 1));
  items.Insert(std::make_shared<ConcreteLinkedItem>(
      ConcreteItem("GROUP-B", "GROUP-B", 1, 1, 1, 1), "B"));
  items.Insert(std::make_shared<ConcreteLinkedItem>(
      ConcreteItem("GROUP-A", "GROUP-A", 1, 1, 1, 1), "A"));

  const auto& sorted = items.SortedItems();
  ASSERT_EQ(sorted.size(), 5u);
  EXPECT_EQ(sorted[0]->GetItemCode(), "GROUP-A");
  EXPECT_EQ(sorted[1]->GetItemCode(), "GROUP-B");
  EXPECT_EQ(sorted[2]->GetItemCode(), "LARGE");
  EXPECT_EQ(sorted[3]->GetItemCode(), "A");
  EXPECT_EQ(sorted[4]->GetItemCode(), "B");
}

TEST(ItemList, MutationInvalidatesSortCacheAndRemovalUsesIdentity) {
  ItemList items;
  const auto small = MakeItem("S", 1, 1, 1, 1);
  const auto equal_but_distinct = MakeItem("S", 1, 1, 1, 1);
  items.Insert(small);
  EXPECT_EQ(items.SortedItems().front().get(), small.get());
  items.Insert(MakeItem("L", 10, 10, 10, 1));
  EXPECT_EQ(items.SortedItems().front()->GetItemCode(), "L");

  items.Remove(equal_but_distinct);
  EXPECT_EQ(items.Count(), 2u);
  items.Remove(small);
  EXPECT_EQ(items.Count(), 1u);
}

TEST(ItemList, ReportsVolumeAndLinkedGroupCounts) {
  ItemList items;
  items.Insert(MakeItem("PLAIN", 1, 2, 3, 1));
  items.Insert(std::make_shared<ConcreteLinkedItem>(
      ConcreteItem("A", "A", 2, 2, 2, 1), "G"));
  items.Insert(std::make_shared<ConcreteLinkedItem>(
      ConcreteItem("B", "B", 3, 3, 3, 1), "G"));

  EXPECT_TRUE(items.HasLinkedItems());
  EXPECT_DOUBLE_EQ(items.GetVolume(), 41.0);
  EXPECT_EQ(items.GetLinkedGroupCounts().at("G"), 2);
}

TEST(ItemList, RemovePackedRemovesOnlyThePackedInstances) {
  ItemList items;
  const auto first = MakeItem("DUP", 1, 1, 1, 1);
  const auto second = MakeItem("DUP", 1, 1, 1, 1);
  items.Insert(first);
  items.Insert(second);
  fitsolver::PackedItemList packed;
  packed.Insert(MakePackedItem(first));

  items.RemovePacked(packed);

  ASSERT_EQ(items.Count(), 1u);
  EXPECT_EQ(items.begin()->get(), second.get());
}

TEST(BoxList, SortsByVolumeThenEmptyWeightThenCapacity) {
  BoxList boxes;
  boxes.Insert(MakeBox("LARGE", 2, 2, 2));
  boxes.Insert(MakeBox("HEAVY", 1, 1, 1, 20, 2));
  boxes.Insert(MakeBox("LIMITED", 1, 1, 1, 10, 1));
  boxes.Insert(MakeBox("UNLIMITED", 1, 1, 1, fitsolver::kNoWeightLimit, 1));

  const auto& sorted = boxes.SortedBoxes();
  ASSERT_EQ(sorted.size(), 4u);
  EXPECT_EQ(sorted[0]->GetReference(), "UNLIMITED");
  EXPECT_EQ(sorted[1]->GetReference(), "LIMITED");
  EXPECT_EQ(sorted[2]->GetReference(), "HEAVY");
  EXPECT_EQ(sorted[3]->GetReference(), "LARGE");
}

TEST(PackedItem, OffsetPreservesOrientationAndSerialisesPlacement) {
  const auto item = std::make_shared<ConcreteLinkedItem>(
      ConcreteItem("A", "Widget", 2, 3, 4, 1.5), "G");
  const auto packed = MakePackedItem(item, 1, 2, 3, 3, 2, 4, "LWD");
  const auto offset = packed.WithOffset(10, 20, 30);
  const auto json = offset.ToJson();
  const double original_width = json.at("original_width").get<double>();

  EXPECT_DOUBLE_EQ(offset.GetX(), 11);
  EXPECT_DOUBLE_EQ(offset.GetY(), 22);
  EXPECT_DOUBLE_EQ(offset.GetZ(), 33);
  EXPECT_EQ(offset.GetOrientationCode(), "LWD");
  EXPECT_TRUE(offset.IsRotated());
  EXPECT_EQ(json.at("box_group"), "G");
  EXPECT_DOUBLE_EQ(original_width, 2.0);
}

TEST(PackedItemList, TotalsAndLinkedGroupsAreAccurate) {
  fitsolver::PackedItemList packed;
  packed.Insert(MakePackedItem(MakeItem("A", 2, 3, 4, 1.5)));
  packed.Insert(MakePackedItem(std::make_shared<ConcreteLinkedItem>(
      ConcreteItem("B", "B", 1, 2, 3, 2.5), "G")));

  EXPECT_EQ(packed.Count(), 2u);
  EXPECT_DOUBLE_EQ(packed.GetWeight(), 4.0);
  EXPECT_DOUBLE_EQ(packed.GetVolume(), 30.0);
  EXPECT_EQ(packed.GetLinkedGroupCounts().at("G"), 1);
}

TEST(PackedBox, ComputesWeightGeometryUtilisationAndJson) {
  fitsolver::PackedBox packed(MakeBox("BOX", 10, 20, 30, 20, 2));
  packed.AddItem(MakePackedItem(MakeItem("A", 2, 3, 4, 5), 1, 2, 3));
  packed.AddItem(MakePackedItem(MakeItem("B", 3, 2, 1, 4), 5, 7, 11));
  const std::size_t item_count = packed.ToJson().at("item_count").get<std::size_t>();

  EXPECT_DOUBLE_EQ(packed.GetItemWeight(), 9.0);
  EXPECT_DOUBLE_EQ(packed.GetWeight(), 11.0);
  EXPECT_DOUBLE_EQ(packed.GetRemainingWeight(), 9.0);
  EXPECT_DOUBLE_EQ(packed.GetUsedWidth(), 8.0);
  EXPECT_DOUBLE_EQ(packed.GetUsedLength(), 9.0);
  EXPECT_DOUBLE_EQ(packed.GetUsedDepth(), 12.0);
  EXPECT_DOUBLE_EQ(packed.GetUnusedVolume(), 6000.0 - 30.0);

  EXPECT_EQ(item_count, 2u);
}

TEST(PackedBox, UnlimitedWeightAndZeroVolumeAreHandled) {
  fitsolver::PackedBox unlimited(MakeBox("U", 0, 10, 10));
  EXPECT_DOUBLE_EQ(unlimited.GetRemainingWeight(), fitsolver::kNoWeightLimit);
  EXPECT_DOUBLE_EQ(unlimited.GetVolumeUtilisation(), 0.0);
  EXPECT_TRUE(unlimited.ToJson().at("remaining_weight").is_null());
}

TEST(PackedBoxList, SortsBestCandidateAndComputesWeightStatistics) {
  fitsolver::PackedBox one(MakeBox("ONE", 10, 10, 10));
  one.AddItem(MakePackedItem(MakeItem("A", 5, 5, 5, 2)));
  fitsolver::PackedBox two(MakeBox("TWO", 20, 20, 20));
  two.AddItem(MakePackedItem(MakeItem("B", 5, 5, 5, 4)));
  two.AddItem(MakePackedItem(MakeItem("C", 5, 5, 5, 6), 5));

  fitsolver::PackedBoxList boxes;
  boxes.Insert(one);
  boxes.Insert(two);

  EXPECT_EQ(boxes.begin()->GetBox()->GetReference(), "TWO");
  EXPECT_DOUBLE_EQ(boxes.GetMeanItemWeight(), 6.0);
  EXPECT_DOUBLE_EQ(boxes.GetWeightVariance(), 16.0);
}

TEST(PackRequest, ParsesCompleteRequestAndDefaultsMissingArraysToEmpty) {
  const nlohmann::json json = {
      {"items", {{{"ItemCode", "A"}, {"Width", 1}, {"Length", 2}, {"Depth", 3}}}},
      {"boxes",
       {{{"Reference", "B"}, {"Width", 4}, {"Length", 5}, {"Depth", 6}, {"MaxWeight", 10}}}}};
  const PackRequest request = PackRequest::FromJsonString(json.dump());
  EXPECT_EQ(request.GetItems().Count(), 1u);
  EXPECT_EQ(request.GetBoxes().Count(), 1u);

  const PackRequest empty = PackRequest::FromJson(nlohmann::json::object());
  EXPECT_TRUE(empty.GetItems().Empty());
  EXPECT_TRUE(empty.GetBoxes().Empty());
}

TEST(PackRequest, RejectsMalformedJsonAndWrongShapes) {
  EXPECT_THROW(PackRequest::FromJsonString("{"), nlohmann::json::parse_error);
  EXPECT_THROW(PackRequest::FromJson({{"items", "not-an-array"}}), std::invalid_argument);
}

TEST(PackResponse, SummaryAndCompactDumpAccountForEveryItem) {
  ItemList items;
  items.Insert(MakeItem("A", 2, 2, 2, 1));
  items.Insert(MakeItem("B", 50, 50, 50, 1));
  BoxList boxes;
  boxes.Insert(MakeBox("BOX", 10, 10, 10));

  const PackResponse response = Packer().Pack(items, boxes);
  const auto json = nlohmann::json::parse(response.Dump(-1));

  EXPECT_EQ(json.at("summary").at("total_placed"), 1);
  EXPECT_EQ(json.at("summary").at("total_unplaced"), 1);
  EXPECT_EQ(json.at("placements").at(0).at("box_reference"), "BOX");
  EXPECT_EQ(json.at("unplaced_item_codes").at(0), "B");
  EXPECT_EQ(response.Dump(-1).find('\n'), std::string::npos);
}

TEST(WorkingVolume, BehavesAsActiveUnlimitedWeightBox) {
  fitsolver::WorkingVolume volume(2.0, 3.0, 4.0);
  EXPECT_TRUE(volume.GetActive());
  EXPECT_FALSE(volume.HasWeightLimit());
  EXPECT_DOUBLE_EQ(volume.GetInnerVolume(), 24.0);
  EXPECT_NE(volume.GetReference().find("2.000000x3.000000x4.000000"), std::string::npos);
}

TEST(WeightRedistributor, EmptyAndAlreadyBalancedResultsRemainStable) {
  BoxList available;
  available.Insert(MakeBox("BOX", 10, 10, 10));
  fitsolver::WeightRedistributor redistributor(available);

  const fitsolver::PackedBoxList empty_result =
      redistributor.Redistribute(fitsolver::PackedBoxList{});
  EXPECT_TRUE(empty_result.Empty());

  fitsolver::PackedBox first(MakeBox("BOX", 10, 10, 10));
  first.AddItem(MakePackedItem(MakeItem("A", 2, 2, 2, 5)));
  fitsolver::PackedBox second(MakeBox("BOX", 10, 10, 10));
  second.AddItem(MakePackedItem(MakeItem("B", 2, 2, 2, 5)));
  fitsolver::PackedBoxList balanced;
  balanced.Insert(first);
  balanced.Insert(second);

  const fitsolver::PackedBoxList result = redistributor.Redistribute(balanced);
  EXPECT_EQ(result.Count(), 2u);
  EXPECT_DOUBLE_EQ(result.GetWeightVariance(), 0.0);
}

TEST(VolumePacker, EveryPlacedItemRestsOnTheFloorOrOnItemsBelow) {
  // A/B fill the bottom half in two shelves; C would naively land in the
  // air beside B on a fresh layer. The support check must instead push it
  // down into the floor-level gap to the right of A (or leave it out).
  ItemList items;
  items.Insert(MakeItem("A", 60, 100, 50, 1));
  items.Insert(MakeItem("B", 60, 100, 50, 1));
  items.Insert(MakeItem("C", 30, 30, 30, 1));
  items.Insert(MakeItem("D", 25, 25, 25, 1));
  items.Insert(MakeItem("E", 25, 25, 25, 1));

  const fitsolver::PackedBox packed =
      fitsolver::VolumePacker(MakeBox("BOX", 100, 100, 100)).Pack(items);
  const auto& placed = packed.GetItems().ToVector();
  ASSERT_FALSE(placed.empty());

  for (const auto& it : placed) {
    if (it.GetZ() <= 1e-6) continue;  // resting on the box floor
    const double base = it.GetWidth() * it.GetLength();
    double covered = 0.0;
    for (const auto& other : placed) {
      if (&other == &it) continue;
      if (std::abs((other.GetZ() + other.GetDepth()) - it.GetZ()) > 1e-6) continue;
      const double ox = std::max(it.GetX(), other.GetX());
      const double oy = std::max(it.GetY(), other.GetY());
      const double ex = std::min(it.GetX() + it.GetWidth(), other.GetX() + other.GetWidth());
      const double ey = std::min(it.GetY() + it.GetLength(), other.GetY() + other.GetLength());
      if (ex > ox && ey > oy) covered += (ex - ox) * (ey - oy);
    }
    EXPECT_GE(covered / base, 0.75 - 1e-6)
        << it.GetItem()->GetItemCode() << " floats at z=" << it.GetZ();
  }
}

TEST(VolumePacker, PlacementListIsBottomUp) {
  // More items than fit in one layer, so the pack spans several layers;
  // the emitted order must never step down in z (a build sequence a
  // worker can actually follow).
  ItemList items;
  for (int i = 0; i < 12; ++i) items.Insert(MakeItem("I" + std::to_string(i), 25, 25, 25, 1));

  const fitsolver::PackedBox packed =
      fitsolver::VolumePacker(MakeBox("BOX", 60, 60, 60)).Pack(items);
  const auto& placed = packed.GetItems().ToVector();
  ASSERT_GT(placed.size(), 1u);
  for (std::size_t i = 1; i < placed.size(); ++i) {
    EXPECT_GE(placed[i].GetZ() + 1e-6, placed[i - 1].GetZ())
        << "placement " << i << " drops below the previous one";
  }
}

TEST(VolumePacker, WrapsAcrossRowsAndLayersWithoutOverlap) {
  ItemList items;
  for (int i = 0; i < 8; ++i) items.Insert(MakeItem(std::to_string(i), 5, 5, 5, 1));

  const fitsolver::PackedBox packed =
      fitsolver::VolumePacker(MakeBox("BOX", 10, 10, 10)).Pack(items);

  ASSERT_EQ(packed.GetItems().Count(), 8u);
  std::set<std::tuple<double, double, double>> positions;
  for (const auto& item : packed.GetItems()) {
    EXPECT_LE(item.GetX() + item.GetWidth(), 10.0);
    EXPECT_LE(item.GetY() + item.GetLength(), 10.0);
    EXPECT_LE(item.GetZ() + item.GetDepth(), 10.0);
    positions.emplace(item.GetX(), item.GetY(), item.GetZ());
  }
  EXPECT_EQ(positions.size(), 8u);
}

TEST(PackerRegression, ExactFitOnEveryAxisAndWeightBoundarySucceeds) {
  ItemList items;
  items.Insert(MakeItem("EXACT", 10, 20, 30, 8));
  BoxList boxes;
  boxes.Insert(MakeBox("EXACT-BOX", 10, 20, 30, 10, 2));

  const PackResponse response = Packer().Pack(items, boxes);

  ASSERT_TRUE(response.success);
  EXPECT_DOUBLE_EQ(response.boxes.begin()->GetVolumeUtilisation(), 100.0);
  EXPECT_DOUBLE_EQ(response.boxes.begin()->GetRemainingWeight(), 0.0);
}

TEST(PackerRegression, OneDimensionTooLargeCannotBeHiddenByEqualVolume) {
  ItemList items;
  items.Insert(MakeItem("LONG", 11, 10, 10, 1, Rotation::kNever));
  BoxList boxes;
  boxes.Insert(MakeBox("BOX", 10, 10, 11));

  const PackResponse response = Packer().Pack(items, boxes);

  EXPECT_FALSE(response.success);
  EXPECT_EQ(response.unplaced_items.Count(), 1u);
}

TEST(PackerPropertyTest, RandomRequestsPreserveAccountingBoundsAndWeight) {
  std::mt19937 rng(0xB0C5u);
  std::uniform_int_distribution<int> dimension(1, 40);
  std::uniform_int_distribution<int> weight(1, 10);
  std::uniform_int_distribution<int> item_count(0, 25);

  for (int trial = 0; trial < 200; ++trial) {
    ItemList items;
    const int count = item_count(rng);
    for (int i = 0; i < count; ++i) {
      items.Insert(MakeItem("T" + std::to_string(trial) + "-" + std::to_string(i),
                            dimension(rng), dimension(rng), dimension(rng), weight(rng)));
    }
    BoxList boxes;
    boxes.Insert(MakeBox("SMALL", 20, 20, 20, 35, 2));
    boxes.Insert(MakeBox("LARGE", 50, 50, 50, 80, 3));

    const PackResponse response = Packer().Pack(items, boxes);
    EXPECT_EQ(response.GetTotalPlacedCount() + response.GetTotalUnplacedCount(), count)
        << "trial " << trial;

    std::set<const fitsolver::Item*> seen;
    for (const auto& box : response.boxes) {
      EXPECT_LE(box.GetWeight(), box.GetMaxWeight()) << "trial " << trial;
      for (const auto& packed : box.GetItems()) {
        EXPECT_TRUE(seen.insert(packed.GetItem().get()).second) << "trial " << trial;
        EXPECT_GE(packed.GetX(), 0.0);
        EXPECT_GE(packed.GetY(), 0.0);
        EXPECT_GE(packed.GetZ(), 0.0);
        EXPECT_LE(packed.GetX() + packed.GetWidth(), box.GetBox()->GetWidth());
        EXPECT_LE(packed.GetY() + packed.GetLength(), box.GetBox()->GetLength());
        EXPECT_LE(packed.GetZ() + packed.GetDepth(), box.GetBox()->GetDepth());
      }
      const auto& placements = box.GetItems().ToVector();
      for (std::size_t i = 0; i < placements.size(); ++i) {
        for (std::size_t j = i + 1; j < placements.size(); ++j) {
          const auto& a = placements[i];
          const auto& b = placements[j];
          const bool separated = a.GetX() + a.GetWidth() <= b.GetX() ||
                                 b.GetX() + b.GetWidth() <= a.GetX() ||
                                 a.GetY() + a.GetLength() <= b.GetY() ||
                                 b.GetY() + b.GetLength() <= a.GetY() ||
                                 a.GetZ() + a.GetDepth() <= b.GetZ() ||
                                 b.GetZ() + b.GetDepth() <= a.GetZ();
          EXPECT_TRUE(separated) << "overlap in trial " << trial;
        }
      }
    }
    for (const auto& unplaced : response.unplaced_items) {
      EXPECT_TRUE(seen.insert(unplaced.get()).second) << "trial " << trial;
    }
    EXPECT_EQ(seen.size(), static_cast<std::size_t>(count));
  }
}
