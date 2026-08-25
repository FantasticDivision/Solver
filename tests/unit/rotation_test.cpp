#include <gtest/gtest.h>
#include "Rotation.h"

using fitsolver::Rotation;
using fitsolver::RotationFromString;
using fitsolver::RotationToString;

TEST(RotationFromStringTest, ParsesNever) {
  EXPECT_EQ(RotationFromString("Never"), Rotation::kNever);
}

TEST(RotationFromStringTest, ParsesKeepFlat) {
  EXPECT_EQ(RotationFromString("KeepFlat"), Rotation::kKeepFlat);
}

TEST(RotationFromStringTest, ParsesBestFit) {
  EXPECT_EQ(RotationFromString("BestFit"), Rotation::kBestFit);
}

TEST(RotationFromStringTest, IsCaseInsensitive) {
  EXPECT_EQ(RotationFromString("NEVER"), Rotation::kNever);
  EXPECT_EQ(RotationFromString("keepflat"), Rotation::kKeepFlat);
  EXPECT_EQ(RotationFromString("BESTFIT"), Rotation::kBestFit);
}

TEST(RotationFromStringTest, AcceptsKeepFlatWithUnderscore) {
  EXPECT_EQ(RotationFromString("keep_flat"), Rotation::kKeepFlat);
}

TEST(RotationFromStringTest, UnknownValueDefaultsToBestFit) {
  EXPECT_EQ(RotationFromString("unknown"), Rotation::kBestFit);
}

TEST(RotationToStringTest, ConvertsAllRotationValues) {
  EXPECT_EQ(RotationToString(Rotation::kNever), "Never");
  EXPECT_EQ(RotationToString(Rotation::kKeepFlat), "KeepFlat");
  EXPECT_EQ(RotationToString(Rotation::kBestFit), "BestFit");
}
