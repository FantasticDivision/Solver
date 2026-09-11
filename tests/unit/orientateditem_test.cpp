#include <gtest/gtest.h>

#include <memory>

#include "ConcreteItem.h"
#include "OrientatedItem.h"
#include "Rotation.h"

using fitsolver::ConcreteItem;
using fitsolver::GenerateOrientations;
using fitsolver::ItemPtr;
using fitsolver::Rotation;

namespace {

ItemPtr MakeItem(Rotation rotation) {
  return std::make_shared<ConcreteItem>("ITEM-1", "Test item", 10.0, 20.0, 30.0, 2.0,
                                         rotation);
}

}  // namespace

TEST(GenerateOrientationsTest, NeverReturnsOnlyOriginalOrientation) {
  ItemPtr item = MakeItem(Rotation::kNever);

  const auto orientations = GenerateOrientations(item);

  ASSERT_EQ(orientations.size(), 1u);
  EXPECT_EQ(orientations[0].GetOrientationCode(), "WLD");
  EXPECT_DOUBLE_EQ(orientations[0].GetWidth(), 10.0);
  EXPECT_DOUBLE_EQ(orientations[0].GetLength(), 20.0);
  EXPECT_DOUBLE_EQ(orientations[0].GetDepth(), 30.0);
  EXPECT_FALSE(orientations[0].IsRotated());
}

TEST(GenerateOrientationsTest, KeepFlatOnlySwapsWidthAndLength) {
  ItemPtr item = MakeItem(Rotation::kKeepFlat);

  const auto orientations = GenerateOrientations(item);

  ASSERT_EQ(orientations.size(), 2u);

  EXPECT_EQ(orientations[0].GetOrientationCode(), "WLD");
  EXPECT_DOUBLE_EQ(orientations[0].GetWidth(), 10.0);
  EXPECT_DOUBLE_EQ(orientations[0].GetLength(), 20.0);
  EXPECT_DOUBLE_EQ(orientations[0].GetDepth(), 30.0);

  EXPECT_EQ(orientations[1].GetOrientationCode(), "LWD");
  EXPECT_DOUBLE_EQ(orientations[1].GetWidth(), 20.0);
  EXPECT_DOUBLE_EQ(orientations[1].GetLength(), 10.0);
  EXPECT_DOUBLE_EQ(orientations[1].GetDepth(), 30.0);
  EXPECT_TRUE(orientations[1].IsRotated());
}

TEST(GenerateOrientationsTest, BestFitReturnsAllSixOrientations) {
  ItemPtr item = MakeItem(Rotation::kBestFit);

  const auto orientations = GenerateOrientations(item);

  ASSERT_EQ(orientations.size(), 6u);
  EXPECT_EQ(orientations[0].GetOrientationCode(), "WLD");
  EXPECT_EQ(orientations[1].GetOrientationCode(), "LWD");
  EXPECT_EQ(orientations[2].GetOrientationCode(), "WDL");
  EXPECT_EQ(orientations[3].GetOrientationCode(), "LDW");
  EXPECT_EQ(orientations[4].GetOrientationCode(), "DWL");
  EXPECT_EQ(orientations[5].GetOrientationCode(), "DLW");
}

//testing if volume is preserved during orientation. 

TEST(GenerateOrientationsTest, EveryOrientationPreservesVolumeAndOriginalItem) {
  ItemPtr item = MakeItem(Rotation::kBestFit);

  const auto orientations = GenerateOrientations(item);

  ASSERT_EQ(orientations.size(), 6u);
  for (const auto& orientation : orientations) {
    EXPECT_DOUBLE_EQ(orientation.GetVolume(), item->GetVolume())
        << "Volume changed for orientation " << orientation.GetOrientationCode();
    EXPECT_EQ(orientation.GetItem().get(), item.get());
  }
}
