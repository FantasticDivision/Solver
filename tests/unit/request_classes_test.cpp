// Basic coverage for PackRequest and PackResponse.

#include <gtest/gtest.h>

#include "BoxList.h"
#include "ItemList.h"
#include "PackRequest.h"
#include "PackResponse.h"
#include "Packer.h"
#include "test_helpers.h"

using fitsolver::BoxList;
using fitsolver::ItemList;
using fitsolver::Packer;
using fitsolver::PackRequest;
using fitsolver::PackResponse;
using fitsolver::test::MakeBox;
using fitsolver::test::MakeItem;

TEST(PackRequestClassTest, ParsesItemsAndBoxes) {
  const auto request = PackRequest::FromJson({
      {"items", {{{"ItemCode", "A"}, {"Width", 1}, {"Length", 2}, {"Depth", 3}}}},
      {"boxes",
       {{{"Reference", "B"}, {"Width", 4}, {"Length", 5}, {"Depth", 6}, {"MaxWeight", 10}}}}});

  EXPECT_EQ(request.GetItems().Count(), 1u);
  EXPECT_EQ(request.GetBoxes().Count(), 1u);
}

TEST(PackResponseClassTest, ReportsPlacedAndUnplacedTotals) {
  ItemList items;
  items.Insert(MakeItem("FITS", 1, 1, 1, 1));
  items.Insert(MakeItem("TOO-BIG", 20, 20, 20, 1));
  BoxList boxes;
  boxes.Insert(MakeBox("BOX", 10, 10, 10));

  const PackResponse response = Packer().Pack(items, boxes);
  const auto json = response.ToJson();
  EXPECT_EQ(response.GetTotalPlacedCount(), 1);
  EXPECT_EQ(response.GetTotalUnplacedCount(), 1);
  EXPECT_EQ(json.at("summary").at("box_count"), 1);
}
