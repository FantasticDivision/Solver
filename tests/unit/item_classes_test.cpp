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

TEST(ItemSorterClassTest, PrefersLinkedThenLargerItems) {
  fitsolver::DefaultItemSorter sorter;
  ConcreteItem small("S", "S", 1, 1, 1, 1);
  ConcreteItem large("L", "L", 2, 2, 2, 1);
  ConcreteLinkedItem linked(ConcreteItem("G", "G", 1, 1, 1, 1), "GROUP");

  EXPECT_LT(sorter.Compare(linked, large), 0);
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
