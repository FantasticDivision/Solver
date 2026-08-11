#include "packer.h"

#include <gtest/gtest.h>

TEST(AddItemCountTest, AddsTwoPositiveNumbers) {
  EXPECT_EQ(fitsolver::AddItemCount(2, 3), 5);
}

TEST(AddItemCountTest, HandlesZero) {
  EXPECT_EQ(fitsolver::AddItemCount(0, 0), 0);
}
