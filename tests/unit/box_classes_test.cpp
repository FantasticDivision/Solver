// Basic coverage for Box, LimitedSupplyBox, ConcreteBox, DefaultBoxSorter and BoxList.

#include <gtest/gtest.h>

#include "BoxList.h"
#include "ConcreteBox.h"
#include "DefaultBoxSorter.h"
#include "LimitedSupplyBox.h"
#include "test_helpers.h"

using fitsolver::BoxList;
using fitsolver::ConcreteBox;
using fitsolver::test::MakeBox;

TEST(BoxClassTest, ConcreteBoxReportsCapacityVolumeAndJson) {
  ConcreteBox box("BOX", 2, 3, 4, 10, 1, true);
  EXPECT_TRUE(box.HasWeightLimit());
  EXPECT_DOUBLE_EQ(box.GetInnerVolume(), 24.0);
  EXPECT_DOUBLE_EQ(box.GetWeightCapacityForItems(), 9.0);
  EXPECT_EQ(box.ToJson().at("Reference"), "BOX");
}

TEST(BoxClassTest, LimitedSupplyFactoryPreservesQuantity) {
  const auto box = fitsolver::MakeBoxFromJson(
      {{"Reference", "LIMITED"}, {"Width", 2}, {"Length", 3}, {"Depth", 4}, {"MaxWeight", 10},
       {"MaximumBoxes", 2}});
  const auto* limited = dynamic_cast<const fitsolver::LimitedSupplyBox*>(box.get());

  ASSERT_NE(limited, nullptr);
  EXPECT_EQ(limited->GetQuantityAvailable(), 2);
  EXPECT_EQ(limited->ToJson().at("MaximumBoxes"), 2);
}

TEST(BoxSorterClassTest, PrefersTheSmallestBox) {
  fitsolver::DefaultBoxSorter sorter;
  ConcreteBox small("S", 2, 2, 2);
  ConcreteBox large("L", 4, 4, 4);
  EXPECT_LT(sorter.Compare(small, large), 0);
}

TEST(BoxListClassTest, InsertsCountsAndSortsBoxes) {
  BoxList boxes;
  boxes.Insert(MakeBox("L", 4, 4, 4));
  boxes.Insert(MakeBox("S", 2, 2, 2));

  EXPECT_EQ(boxes.Count(), 2u);
  EXPECT_EQ((*boxes.begin())->GetReference(), "S");
}
