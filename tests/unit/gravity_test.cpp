// Coverage for what holds a packed item up: Support's geometry,
// ItemReseater, and GravitySettler, which Packer runs on every finished box.
//
// Gravity acts along -Z throughout: z = 0 is the box floor and Depth is the
// vertical axis.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "GravitySettler.h"
#include "ItemReseater.h"
#include "PackedBox.h"
#include "Support.h"
#include "test_helpers.h"

using fitsolver::PackedItem;
using fitsolver::Rotation;
using fitsolver::test::MakeBox;
using fitsolver::test::MakeItem;
using fitsolver::test::MakePackedItem;

namespace {

PackedItem Placed(const std::string& code, double width, double length, double depth, double x,
                  double y, double z, Rotation rotation = Rotation::kNever) {
  return MakePackedItem(MakeItem(code, width, length, depth, 1.0, rotation), x, y, z);
}

std::unordered_map<std::string, PackedItem> ByCode(const std::vector<PackedItem>& items) {
  std::unordered_map<std::string, PackedItem> by_code;
  for (const auto& item : items) by_code.emplace(item.GetItem()->GetItemCode(), item);
  return by_code;
}

bool Overlap(const PackedItem& a, const PackedItem& b) {
  return a.GetX() + a.GetWidth() > b.GetX() + 1e-6 && b.GetX() + b.GetWidth() > a.GetX() + 1e-6 &&
         a.GetY() + a.GetLength() > b.GetY() + 1e-6 && b.GetY() + b.GetLength() > a.GetY() + 1e-6 &&
         a.GetZ() + a.GetDepth() > b.GetZ() + 1e-6 && b.GetZ() + b.GetDepth() > a.GetZ() + 1e-6;
}

// Written out separately from Support.cpp on purpose: an item floats when
// it is off the floor and no other item's top meets its underside.
bool Floats(const PackedItem& item, const std::vector<PackedItem>& items) {
  if (item.GetZ() <= 1e-6) return false;
  for (const auto& other : items) {
    const bool beneath = std::abs(other.GetZ() + other.GetDepth() - item.GetZ()) <= 1e-6;
    const bool shares_floor_area =
        item.GetX() + item.GetWidth() > other.GetX() + 1e-6 &&
        other.GetX() + other.GetWidth() > item.GetX() + 1e-6 &&
        item.GetY() + item.GetLength() > other.GetY() + 1e-6 &&
        other.GetY() + other.GetLength() > item.GetY() + 1e-6;
    if (beneath && shares_floor_area) return false;
  }
  return true;
}

}  // namespace

TEST(SupportTest, FootprintsThatOnlyShareAnEdgeDoNotOverlap) {
  const auto left = Placed("LEFT", 10, 10, 10, 0, 0, 0);
  const auto right = Placed("RIGHT", 10, 10, 10, 10, 0, 0);
  const auto across = Placed("ACROSS", 10, 10, 10, 5, 5, 50);

  EXPECT_FALSE(fitsolver::FootprintsOverlap(left, right));
  EXPECT_TRUE(fitsolver::FootprintsOverlap(left, across)) << "height plays no part in a footprint";
  EXPECT_TRUE(fitsolver::FootprintsOverlap(across, right));
}

TEST(SupportTest, RestingHeightIsTheTallestTopUnderTheFootprint) {
  const std::vector<PackedItem> items{Placed("LOW", 10, 10, 5, 0, 0, 0),
                                      Placed("HIGH", 10, 10, 12, 10, 0, 0),
                                      Placed("ELSEWHERE", 10, 10, 40, 30, 0, 0)};

  EXPECT_DOUBLE_EQ(fitsolver::RestingHeight(Placed("P", 4, 4, 1, 2, 2, 99), items, fitsolver::kNoItem), 5.0);
  EXPECT_DOUBLE_EQ(fitsolver::RestingHeight(Placed("P", 10, 4, 1, 5, 2, 99), items, fitsolver::kNoItem), 12.0);
  EXPECT_DOUBLE_EQ(fitsolver::RestingHeight(Placed("P", 4, 4, 1, 22, 2, 99), items, fitsolver::kNoItem), 0.0)
      << "nothing under the footprint means the floor";
  EXPECT_DOUBLE_EQ(fitsolver::RestingHeight(items[1], items, 1), 0.0) << "an item never rests on itself";
}

TEST(SupportTest, AnItemOnTheFloorStandsFirmly) {
  const std::vector<PackedItem> items{Placed("A", 10, 10, 10, 0, 0, 0)};
  EXPECT_TRUE(fitsolver::StandsFirmly(items[0], items, 0));
}

TEST(SupportTest, AFloatingItemDoesNotStandFirmly) {
  const std::vector<PackedItem> items{Placed("BASE", 10, 10, 5, 0, 0, 0),
                                      Placed("FLOATING", 10, 10, 5, 0, 0, 9)};
  EXPECT_FALSE(fitsolver::StandsFirmly(items[1], items, 1));
  EXPECT_EQ(fitsolver::CountNotStandingFirmly(items), 1u);
}

TEST(SupportTest, AnItemStandsFirmlyOnlyWithItsCentreOverItsSupport) {
  // A 20-wide ledge under one end of a 60-wide item: the item's centre is
  // 10 past the ledge's edge, so it would tip.
  const std::vector<PackedItem> overhanging{Placed("LEDGE", 20, 10, 30, 40, 0, 0),
                                            Placed("TOP", 60, 10, 10, 0, 0, 30)};
  EXPECT_FALSE(fitsolver::StandsFirmly(overhanging[1], overhanging, 1));

  // The same ledge under the middle of it.
  const std::vector<PackedItem> balanced{Placed("LEDGE", 20, 10, 30, 20, 0, 0),
                                         Placed("TOP", 60, 10, 10, 0, 0, 30)};
  EXPECT_TRUE(fitsolver::StandsFirmly(balanced[1], balanced, 1));
}

TEST(SupportTest, AnItemBridgingTwoSupportsStandsFirmly) {
  // Nothing is directly under the centre, but it lies between the two
  // supports, so the item can't tip either way.
  const std::vector<PackedItem> items{Placed("LEFT", 10, 10, 30, 0, 0, 0),
                                      Placed("RIGHT", 10, 10, 30, 50, 0, 0),
                                      Placed("BRIDGE", 60, 10, 10, 0, 0, 30)};
  EXPECT_TRUE(fitsolver::StandsFirmly(items[2], items, 2));
  EXPECT_EQ(fitsolver::CountNotStandingFirmly(items), 0u);
}

TEST(DropOntoSupportTest, ClosesTheGapUnderAFloatingItem) {
  const auto dropped = ByCode(fitsolver::DropOntoSupport(
      {Placed("BASE", 10, 10, 5, 0, 0, 0), Placed("FLOATING", 10, 10, 5, 0, 0, 9)}));

  EXPECT_DOUBLE_EQ(dropped.at("BASE").GetZ(), 0.0);
  EXPECT_DOUBLE_EQ(dropped.at("FLOATING").GetZ(), 5.0);
}

TEST(DropOntoSupportTest, AStackComesDownTogether) {
  // MIDDLE hangs 2 above BASE, and TOP sits on MIDDLE: both come down 2.
  // Listed top first, since the order items arrive in must not matter.
  const auto dropped = ByCode(fitsolver::DropOntoSupport({Placed("TOP", 10, 10, 5, 0, 0, 12),
                                                         Placed("MIDDLE", 10, 10, 5, 0, 0, 7),
                                                         Placed("BASE", 10, 10, 5, 0, 0, 0)}));

  EXPECT_DOUBLE_EQ(dropped.at("MIDDLE").GetZ(), 5.0);
  EXPECT_DOUBLE_EQ(dropped.at("TOP").GetZ(), 10.0);
}

TEST(DropOntoSupportTest, LandsOnTheTallestItemBeneath) {
  const auto dropped = ByCode(fitsolver::DropOntoSupport({Placed("LOW", 10, 10, 5, 0, 0, 0),
                                                         Placed("HIGH", 10, 10, 12, 10, 0, 0),
                                                         Placed("ACROSS", 20, 10, 5, 0, 0, 40)}));

  EXPECT_DOUBLE_EQ(dropped.at("ACROSS").GetZ(), 12.0);
}

TEST(DropOntoSupportTest, ChangesNothingButHeightAndLeavesRestingItemsAlone) {
  const std::vector<PackedItem> before{Placed("BASE", 10, 10, 5, 3, 4, 0),
                                       Placed("RESTING", 6, 6, 5, 5, 6, 5),
                                       Placed("FLOATING", 4, 4, 5, 20, 20, 30)};
  const auto dropped = ByCode(fitsolver::DropOntoSupport(before));

  for (const auto& item : before) {
    const PackedItem& after = dropped.at(item.GetItem()->GetItemCode());
    EXPECT_DOUBLE_EQ(after.GetX(), item.GetX());
    EXPECT_DOUBLE_EQ(after.GetY(), item.GetY());
    EXPECT_DOUBLE_EQ(after.GetWidth(), item.GetWidth());
    EXPECT_DOUBLE_EQ(after.GetLength(), item.GetLength());
    EXPECT_DOUBLE_EQ(after.GetDepth(), item.GetDepth());
    EXPECT_LE(after.GetZ(), item.GetZ()) << "an item must never be lifted";
  }
  EXPECT_DOUBLE_EQ(dropped.at("RESTING").GetZ(), 5.0);
  EXPECT_DOUBLE_EQ(dropped.at("FLOATING").GetZ(), 0.0) << "nothing beneath it, so it lands on the floor";
}

TEST(ItemReseaterTest, MovesAnOverhangingItemToASeatWhereItStandsFirmly) {
  // TOP rests on the tall ledge at one end with its centre over LOW, which
  // is 20 further down. The whole of LOW's top is free and big enough.
  const std::vector<PackedItem> items{Placed("LOW", 80, 100, 10, 0, 0, 0),
                                      Placed("LEDGE", 20, 100, 30, 80, 0, 0),
                                      Placed("TOP", 60, 100, 10, 40, 0, 30)};
  ASSERT_EQ(fitsolver::CountNotStandingFirmly(items), 1u);

  const auto reseated = fitsolver::ItemReseater(MakeBox("BOX", 100, 100, 100)).Reseat(items);
  const auto by_code = ByCode(reseated);

  EXPECT_EQ(fitsolver::CountNotStandingFirmly(reseated), 0u);
  EXPECT_DOUBLE_EQ(by_code.at("TOP").GetZ(), 10.0) << "it should now lie flat on LOW";
  EXPECT_LE(by_code.at("TOP").GetX() + by_code.at("TOP").GetWidth(), 80.0)
      << "and entirely on LOW, clear of the ledge";
  EXPECT_DOUBLE_EQ(by_code.at("LOW").GetX(), 0.0) << "only the item that would tip is moved";
  EXPECT_DOUBLE_EQ(by_code.at("LEDGE").GetX(), 80.0);
}

TEST(ItemReseaterTest, LeavesAnItemWhereItIsWhenTheBoxHasNoFirmSeatForIt) {
  // TOP covers the whole floor plan, so there is nowhere else to put it.
  const std::vector<PackedItem> items{Placed("LOW", 80, 100, 10, 0, 0, 0),
                                      Placed("LEDGE", 20, 100, 30, 80, 0, 0),
                                      Placed("TOP", 100, 100, 10, 0, 0, 30)};

  const auto by_code = ByCode(fitsolver::ItemReseater(MakeBox("BOX", 100, 100, 40)).Reseat(items));

  EXPECT_DOUBLE_EQ(by_code.at("TOP").GetX(), 0.0);
  EXPECT_DOUBLE_EQ(by_code.at("TOP").GetY(), 0.0);
  EXPECT_DOUBLE_EQ(by_code.at("TOP").GetZ(), 30.0);
}

TEST(ItemReseaterTest, TurnsAnItemToReachASeatOnlyIfItsRotationAllows) {
  // TOP spans the box's full width, so unturned it always ends up on the
  // ledge. Turned a quarter-turn it fits flat on LOW.
  const auto layout = [](Rotation rotation) {
    return std::vector<PackedItem>{Placed("LOW", 40, 60, 10, 0, 0, 0),
                                   Placed("LEDGE", 20, 60, 30, 40, 0, 0),
                                   Placed("TOP", 60, 20, 10, 0, 0, 30, rotation)};
  };
  const fitsolver::ItemReseater reseater(MakeBox("BOX", 60, 60, 50));

  const auto turned = ByCode(reseater.Reseat(layout(Rotation::kKeepFlat)));
  EXPECT_DOUBLE_EQ(turned.at("TOP").GetWidth(), 20.0);
  EXPECT_DOUBLE_EQ(turned.at("TOP").GetLength(), 60.0);
  EXPECT_DOUBLE_EQ(turned.at("TOP").GetZ(), 10.0);
  EXPECT_EQ(turned.at("TOP").GetOrientationCode(), "LWD");
  EXPECT_TRUE(turned.at("TOP").IsRotated());

  const auto fixed = ByCode(reseater.Reseat(layout(Rotation::kNever)));
  EXPECT_DOUBLE_EQ(fixed.at("TOP").GetWidth(), 60.0);
  EXPECT_DOUBLE_EQ(fixed.at("TOP").GetZ(), 30.0);
  EXPECT_FALSE(fixed.at("TOP").IsRotated());
}

// Random non-overlapping stacks, each item dropped in from above with a
// random gap left under it. After the drop nothing may float; after the
// re-seat nothing may float, overlap or leave the box, and no more items
// may be left not standing firmly than before. One instance per seed, so
// each stays quick and a failure names the seed that found it.
class ItemReseaterRandomLayouts : public testing::TestWithParam<int> {};

TEST_P(ItemReseaterRandomLayouts, NeverLeavesABoxWorseOff) {
  std::mt19937 random(20261014u + static_cast<unsigned>(GetParam()));
  const auto pick = [&random](int low, int high) {
    return static_cast<double>(std::uniform_int_distribution<int>(low, high)(random));
  };

  for (int layout = 0; layout < 6; ++layout) {
    const auto box = MakeBox("BOX", 100, 100, 400);
    std::vector<PackedItem> items;
    const int count = static_cast<int>(pick(2, 8));
    for (int n = 0; n < count; ++n) {
      const double width = pick(10, 60);
      const double length = pick(10, 60);
      const double depth = pick(5, 30);
      const double x = pick(0, 100 - static_cast<int>(width));
      const double y = pick(0, 100 - static_cast<int>(length));
      const bool leave_gap = pick(0, 1) > 0.5;
      const double gap = leave_gap ? pick(1, 20) : 0.0;
      const PackedItem probe = Placed("probe", width, length, depth, x, y, 0, Rotation::kBestFit);
      const double z = fitsolver::RestingHeight(probe, items, fitsolver::kNoItem) + gap;
      items.push_back(Placed("I" + std::to_string(n), width, length, depth, x, y, z, Rotation::kBestFit));
    }

    const auto dropped = fitsolver::DropOntoSupport(items);
    for (const auto& item : dropped) {
      ASSERT_FALSE(Floats(item, dropped)) << "layout " << layout << ": " << item.GetItem()->GetItemCode();
    }

    const auto reseated = fitsolver::ItemReseater(box).Reseat(dropped);
    ASSERT_EQ(reseated.size(), dropped.size());
    EXPECT_LE(fitsolver::CountNotStandingFirmly(reseated), fitsolver::CountNotStandingFirmly(dropped))
        << "layout " << layout;
    for (std::size_t i = 0; i < reseated.size(); ++i) {
      const PackedItem& item = reseated[i];
      EXPECT_FALSE(Floats(item, reseated)) << "layout " << layout << ": " << item.GetItem()->GetItemCode();
      EXPECT_GE(item.GetX(), -1e-6);
      EXPECT_GE(item.GetY(), -1e-6);
      EXPECT_GE(item.GetZ(), -1e-6);
      EXPECT_LE(item.GetX() + item.GetWidth(), 100 + 1e-6);
      EXPECT_LE(item.GetY() + item.GetLength(), 100 + 1e-6);
      EXPECT_LE(item.GetZ() + item.GetDepth(), 400 + 1e-6);
      for (std::size_t j = i + 1; j < reseated.size(); ++j) {
        EXPECT_FALSE(Overlap(item, reseated[j]))
            << "layout " << layout << ": " << item.GetItem()->GetItemCode() << " overlaps "
            << reseated[j].GetItem()->GetItemCode();
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(Seeds, ItemReseaterRandomLayouts, testing::Range(0, 25));

TEST(GravitySettlerTest, SettlesTheLayoutTheChaoticGolfBagBoxUsedToComeBackWith) {
  // Box 12 of the Thommeax Chaotic order as the engine returned it before
  // GravitySettler: the first layer is as deep as its bricks (110), so all
  // three poles of the second layer start at z = 110 -- one of them 40
  // above the pole beneath it, the other two held only by the bricks at
  // one end.
  const auto box = MakeBox("Golf Bag Box", 1000, 250, 200);
  fitsolver::PackedBox packed(box);
  packed.AddItem(Placed("POLE-5", 990, 70, 70, 0, 0, 0));
  packed.AddItem(Placed("ROD-B-0", 585, 85, 85, 0, 70, 0));
  packed.AddItem(Placed("BRICK-2", 200, 80, 110, 585, 70, 0));
  packed.AddItem(Placed("BRICK-20", 200, 80, 110, 785, 70, 0));
  packed.AddItem(Placed("ROD-B-1", 585, 85, 85, 0, 155, 0));
  packed.AddItem(Placed("BATON-5", 410, 60, 60, 585, 155, 0));
  packed.AddItem(Placed("POLE-2", 990, 70, 70, 0, 0, 110));
  packed.AddItem(Placed("POLE-3", 990, 70, 70, 0, 70, 110));
  packed.AddItem(Placed("POLE-4", 990, 70, 70, 0, 140, 110));
  ASSERT_TRUE(Floats(packed.GetItems().ToVector()[6], packed.GetItems().ToVector()));
  ASSERT_EQ(fitsolver::CountNotStandingFirmly(packed.GetItems().ToVector()), 3u);

  const fitsolver::PackedBox settled = fitsolver::GravitySettler().Settle(packed);
  const auto& items = settled.GetItems().ToVector();
  const auto by_code = ByCode(items);

  ASSERT_EQ(items.size(), 9u);
  EXPECT_EQ(settled.GetBox(), box);
  for (const auto& item : items) {
    EXPECT_FALSE(Floats(item, items)) << item.GetItem()->GetItemCode() << " is floating";
    EXPECT_LE(item.GetZ() + item.GetDepth(), 200.0);
  }
  EXPECT_DOUBLE_EQ(by_code.at("POLE-2").GetZ(), 70.0) << "it should lie on the pole beneath it";

  // One pole can be given a firm seat (along the rod in the back row);
  // the box has none for the other, which stays on the bricks.
  EXPECT_EQ(fitsolver::CountNotStandingFirmly(items), 1u);

  // Relisted bottom-up, as a sequence a worker could follow.
  for (std::size_t i = 1; i < items.size(); ++i) {
    EXPECT_GE(items[i].GetZ() + 1e-6, items[i - 1].GetZ()) << "placement " << i << " steps down";
  }
}

TEST(GravitySettlerTest, LeavesABoxThatIsAlreadySettledExactlyAsItWas) {
  fitsolver::PackedBox packed(MakeBox("BOX", 100, 100, 100));
  packed.AddItem(Placed("BASE", 100, 100, 20, 0, 0, 0));
  packed.AddItem(Placed("TOP", 50, 50, 10, 0, 0, 20));

  const fitsolver::PackedBox settled = fitsolver::GravitySettler().Settle(packed);
  const auto& items = settled.GetItems().ToVector();

  ASSERT_EQ(items.size(), 2u);
  EXPECT_EQ(items[0].GetItem()->GetItemCode(), "BASE");
  EXPECT_DOUBLE_EQ(items[0].GetZ(), 0.0);
  EXPECT_EQ(items[1].GetItem()->GetItemCode(), "TOP");
  EXPECT_DOUBLE_EQ(items[1].GetX(), 0.0);
  EXPECT_DOUBLE_EQ(items[1].GetY(), 0.0);
  EXPECT_DOUBLE_EQ(items[1].GetZ(), 20.0);
}
