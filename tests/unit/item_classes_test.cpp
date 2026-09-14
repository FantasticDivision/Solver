// Basic coverage for Item, LinkedItem, ConcreteItem, DefaultItemSorter and ItemList.

#include <gtest/gtest.h>

#include "ConcreteItem.h"
#include "DefaultItemSorter.h"
#include "ItemList.h"
#include "LinkedItem.h"
#include "test_helpers.h"

using fitsolver::ConcreteItem;
using fitsolver::ConcreteLinkedItem;
using fitsolver::ItemList;
using fitsolver::Rotation;
using fitsolver::test::MakeItem;

TEST(ItemClassTest, ConcreteAndLinkedItemsExposeTheirData) {
  ConcreteItem plain("SKU", "Widget", 2, 3, 4, 1.5, Rotation::kKeepFlat);
  EXPECT_EQ(plain.GetItemCode(), "SKU");
  EXPECT_DOUBLE_EQ(plain.GetVolume(), 24.0);
  EXPECT_EQ(plain.ToJson().at("AllowedRotation"), "KeepFlat");

  ConcreteLinkedItem linked(ConcreteItem("LINK", "Linked", 1, 2, 3, 2), "GROUP");
  EXPECT_EQ(linked.GetBoxGroup(), "GROUP");
  EXPECT_EQ(linked.ToJson().at("BoxGroup"), "GROUP");
}

TEST(ItemClassTest, JsonFactoryCreatesPlainAndLinkedItems) {
  const auto plain = fitsolver::MakeItemFromJson(
      {{"ItemCode", "A"}, {"Width", 1}, {"Length", 2}, {"Depth", 3}});
  const auto linked = fitsolver::MakeItemFromJson(
      {{"ItemCode", "B"}, {"Width", 1}, {"Length", 2}, {"Depth", 3}, {"BoxGroup", "G"}});

  EXPECT_EQ(plain->GetItemReference(), "A");
  EXPECT_EQ(dynamic_cast<const fitsolver::LinkedItem*>(plain.get()), nullptr);
  EXPECT_NE(dynamic_cast<const fitsolver::LinkedItem*>(linked.get()), nullptr);
}

TEST(ItemSorterClassTest, PrefersUngroupedThenLargerItems) {
  fitsolver::DefaultItemSorter sorter;
  ConcreteItem small("S", "S", 1, 1, 1, 1);
  ConcreteItem large("L", "L", 2, 2, 2, 1);
  ConcreteLinkedItem linked(ConcreteItem("G", "G", 1, 1, 1, 1), "GROUP");

  // Ungrouped sorts before linked, even a much bigger linked item (mirrors
  // BoxPacker's DefaultItemSorter -- see its Compare() comment for why).
  EXPECT_LT(sorter.Compare(large, linked), 0);
  EXPECT_LT(sorter.Compare(large, small), 0);
}

TEST(ItemListClassTest, InsertsSortsCountsAndRemovesItems) {
  ItemList items;
  const auto small = MakeItem("S", 1, 1, 1, 1);
  const auto large = MakeItem("L", 2, 2, 2, 1);
  items.Insert(small);
  items.Insert(large);

  EXPECT_EQ(items.Count(), 2u);
  EXPECT_DOUBLE_EQ(items.GetVolume(), 9.0);
  EXPECT_EQ(items.begin()->get(), large.get());
  items.Remove(small);
  EXPECT_EQ(items.Count(), 1u);
}

TEST(ItemListClassTest, InsertionRefreshesPreviouslySortedItems) {
  ItemList items;
  items.Insert(MakeItem("SMALL", 1, 1, 1, 1));
  ASSERT_EQ(items.SortedItems().size(), 1u);
  const auto large = MakeItem("LARGE", 3, 3, 3, 1);
  items.Insert(large);
  ASSERT_EQ(items.SortedItems().size(), 2u);
  EXPECT_EQ(items.SortedItems().front(), large);
}

TEST(ItemListClassTest, RemovalUsesIdentityWhenCodesAreEqual) {
  ItemList items;
  const auto first = MakeItem("SAME", 1, 1, 1, 1);
  const auto second = MakeItem("SAME", 1, 1, 1, 1);
  items.Insert(first);
  items.Insert(second);
  ASSERT_EQ(items.SortedItems().size(), 2u);
  items.Remove(first);
  ASSERT_EQ(items.SortedItems().size(), 1u);
  EXPECT_EQ(items.SortedItems().front(), second);
  items.Remove(first);
  EXPECT_EQ(items.Count(), 1u);
}

TEST(ItemListClassTest, RemovingPackedItemsPreservesUnpackedCopies) {
  ItemList items;
  const auto first = MakeItem("SAME", 1, 1, 1, 1);
  const auto second = MakeItem("SAME", 1, 1, 1, 1);
  items.Insert(first);
  items.Insert(second);
  ASSERT_EQ(items.SortedItems().size(), 2u);
  fitsolver::PackedItemList packed;
  packed.Insert(fitsolver::test::MakePackedItem(first));
  items.RemovePacked(packed);
  ASSERT_EQ(items.SortedItems().size(), 1u);
  EXPECT_EQ(items.SortedItems().front(), second);
}

TEST(ItemListClassTest, LinkedGroupCountsUpdateAfterRemoval) {
  ItemList items;
  const auto linked = std::make_shared<ConcreteLinkedItem>(
      ConcreteItem("A", "A", 1, 1, 1, 1), "GROUP");
  items.Insert(linked);
  items.Insert(MakeItem("PLAIN", 1, 1, 1, 1));
  EXPECT_TRUE(items.HasLinkedItems());
  EXPECT_EQ(items.GetLinkedGroupCounts().at("GROUP"), 1);
  items.Remove(linked);
  EXPECT_FALSE(items.HasLinkedItems());
  EXPECT_TRUE(items.GetLinkedGroupCounts().empty());
}
