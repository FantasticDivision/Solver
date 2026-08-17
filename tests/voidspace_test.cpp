#include <gtest/gtest.h>

#include <vector>

#include "PackedItem.h"

#include "VoidSpace.h"


using fitsolver::VoidSpace;

TEST(VoidSpaceTest, StoresOriginCoordinates) {
  // Arrange and act
  VoidSpace space(
      1.0, 2.0, 3.0,  // x, y, z
      4.0, 5.0, 6.0); // width, length, depth

  
  EXPECT_DOUBLE_EQ(space.GetX(), 1);
  EXPECT_DOUBLE_EQ(space.GetY(), 2);
  EXPECT_DOUBLE_EQ(space.GetZ(), 3);
  
}

TEST(VoidSpaceTest, StoresDimensions) {
  VoidSpace space(
      1.0, 2.0, 3.0,
      4.0, 5.0, 6.0);

  EXPECT_DOUBLE_EQ(space.GetWidth(), 4);
  EXPECT_DOUBLE_EQ(space.GetLength(), 5);
  EXPECT_DOUBLE_EQ(space.GetDepth(), 6);
}

TEST(VoidSpaceTest, CalculatesVolume) {
  VoidSpace space(
      0.0, 0.0, 0.0,
      4.0, 5.0, 6.0);

  // Volume should be width × length × depth.
  EXPECT_DOUBLE_EQ(space.GetVolume(), 120);
}

TEST(VoidSpaceTest, CalculatesFootprint) {
  VoidSpace space(
      0.0, 0.0, 0.0,
      4.0, 5.0, 6.0);

  // Footprint should be width × length.
  
  EXPECT_DOUBLE_EQ(space.GetFootprint(), 20);
}

TEST(VoidSpaceTest, ZeroWidthProducesZeroVolumeAndFootprint) {
  VoidSpace space(
      0.0, 0.0, 0.0,
      0.0, 5.0, 6.0);



  EXPECT_DOUBLE_EQ(space.GetFootprint(), 0);
  EXPECT_DOUBLE_EQ(space.GetVolume(), 0);
}

TEST(VoidSpaceTest, OriginDoesNotAffectVolumeOrFootprint) {
  VoidSpace first(
      0.0, 0.0, 0.0,
      4.0, 5.0, 6.0);

  VoidSpace second(
      100.0, 200.0, 300.0,
      4.0, 5.0, 6.0);

  EXPECT_DOUBLE_EQ(first.GetVolume(), second.GetVolume());
  EXPECT_DOUBLE_EQ(first.GetFootprint(), second.GetFootprint());
}

