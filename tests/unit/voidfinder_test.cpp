#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "ConcreteItem.h"
#include "OrientatedItem.h"
#include "PackedItem.h"
#include "VoidFinder.h"
#include "VoidSpace.h"

using fitsolver::ConcreteItem;
using fitsolver::ItemPtr;
using fitsolver::OrientatedItem;
using fitsolver::PackedItem;
using fitsolver::Rotation;
using fitsolver::VoidFinder;
using fitsolver::VoidSpace;

namespace {

PackedItem MakePackedItem(double x, double y, double z, double width, double length,
                          double depth) {
  ItemPtr item = std::make_shared<ConcreteItem>("ITEM-1", "Test item", width, length, depth,
                                                 1.0, Rotation::kNever);
  OrientatedItem orientation(item, width, length, depth, "WLD");
  return PackedItem(std::move(orientation), x, y, z);
}

bool Matches(const VoidSpace& space, double x, double y, double z, double width, double length,
             double depth) {
  return space.GetX() == x && space.GetY() == y && space.GetZ() == z &&
         space.GetWidth() == width && space.GetLength() == length &&
         space.GetDepth() == depth;
}

bool ContainsSpace(const std::vector<VoidSpace>& spaces, double x, double y, double z,
                   double width, double length, double depth) {
  return std::any_of(spaces.begin(), spaces.end(), [&](const VoidSpace& space) {
    return Matches(space, x, y, z, width, length, depth);
  });
}

double TotalVolume(const std::vector<VoidSpace>& spaces) {
  double total = 0.0;
  for (const auto& space : spaces) total += space.GetVolume();
  return total;
}

}  // namespace

TEST(VoidFinderTest, EmptyBoxReturnsEntireBoxAsOneVoid) {
  VoidFinder finder;
  const std::vector<PackedItem> packed_items;

  const auto spaces = finder.Find(10.0, 20.0, 30.0, packed_items);

  ASSERT_EQ(spaces.size(), 1u);
  EXPECT_TRUE(Matches(spaces[0], 0.0, 0.0, 0.0, 10.0, 20.0, 30.0));
}

TEST(VoidFinderTest, ItemFillingEntireBoxLeavesNoVoids) {
  VoidFinder finder;
  const std::vector<PackedItem> packed_items = {
      MakePackedItem(0.0, 0.0, 0.0, 10.0, 20.0, 30.0)};

  const auto spaces = finder.Find(10.0, 20.0, 30.0, packed_items);

  EXPECT_TRUE(spaces.empty());
}

TEST(VoidFinderTest, ItemAgainstOneBoundaryLeavesSingleResidualVoid) {
  VoidFinder finder;
  const std::vector<PackedItem> packed_items = {
      MakePackedItem(0.0, 0.0, 0.0, 4.0, 10.0, 10.0)};

  const auto spaces = finder.Find(10.0, 10.0, 10.0, packed_items);

  ASSERT_EQ(spaces.size(), 1u);
  EXPECT_TRUE(Matches(spaces[0], 4.0, 0.0, 0.0, 6.0, 10.0, 10.0));
  EXPECT_DOUBLE_EQ(spaces[0].GetVolume(), 600.0);
}

TEST(VoidFinderTest, InteriorItemCreatesSixResidualVoids) {
  VoidFinder finder;
  const std::vector<PackedItem> packed_items = {
      MakePackedItem(2.0, 2.0, 2.0, 4.0, 4.0, 4.0)};

  const auto spaces = finder.Find(10.0, 10.0, 10.0, packed_items);

  ASSERT_EQ(spaces.size(), 6u);
  EXPECT_TRUE(ContainsSpace(spaces, 0.0, 0.0, 0.0, 2.0, 10.0, 10.0));   // left
  EXPECT_TRUE(ContainsSpace(spaces, 6.0, 0.0, 0.0, 4.0, 10.0, 10.0));   // right
  EXPECT_TRUE(ContainsSpace(spaces, 2.0, 0.0, 0.0, 4.0, 2.0, 10.0));    // front
  EXPECT_TRUE(ContainsSpace(spaces, 2.0, 6.0, 0.0, 4.0, 4.0, 10.0));    // back
  EXPECT_TRUE(ContainsSpace(spaces, 2.0, 2.0, 0.0, 4.0, 4.0, 2.0));     // below
  EXPECT_TRUE(ContainsSpace(spaces, 2.0, 2.0, 6.0, 4.0, 4.0, 4.0));     // above
  EXPECT_DOUBLE_EQ(TotalVolume(spaces), 936.0);
}

TEST(VoidFinderTest, SubtractItemsLeavesNonIntersectingSpaceUnchanged) {
  VoidFinder finder;
  std::vector<VoidSpace> spaces = {VoidSpace(0.0, 0.0, 0.0, 5.0, 5.0, 5.0)};
  const std::vector<PackedItem> packed_items = {
      MakePackedItem(10.0, 10.0, 10.0, 2.0, 2.0, 2.0)};

  const auto result = finder.SubtractItems(std::move(spaces), packed_items);

  ASSERT_EQ(result.size(), 1u);
  EXPECT_TRUE(Matches(result[0], 0.0, 0.0, 0.0, 5.0, 5.0, 5.0));
}
