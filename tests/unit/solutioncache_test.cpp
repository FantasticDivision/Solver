#include <gtest/gtest.h>

#include <memory>

#include "BoxList.h"
#include "ConcreteBox.h"
#include "ConcreteItem.h"
#include "ItemList.h"
#include "Packer.h"
#include "SolutionCache.h"
#include "test_helpers.h"

using fitsolver::BoxList;
using fitsolver::BoxPtr;
using fitsolver::CanonicalCacheKey;
using fitsolver::ItemList;
using fitsolver::ItemPtr;
using fitsolver::PackCached;
using fitsolver::Packer;
using fitsolver::PackResponse;
using fitsolver::SolutionCache;
using fitsolver::test::MakeBox;
using fitsolver::test::MakeItem;

namespace {

std::pair<ItemList, BoxList> SampleProblem() {
  ItemList items;
  items.Insert(MakeItem("ITM-1", 100, 100, 100, 1.0));
  items.Insert(MakeItem("ITM-2", 50, 50, 50, 0.5));
  BoxList boxes;
  boxes.Insert(MakeBox("SML", 60, 60, 60));
  boxes.Insert(MakeBox("LRG", 300, 300, 300));
  return {std::move(items), std::move(boxes)};
}

}  // namespace

// ---------------------------------------------------------------------
// CanonicalCacheKey
// ---------------------------------------------------------------------

TEST(CanonicalCacheKeyTest, IsIndependentOfInsertionOrder) {
  ItemList items_a;
  items_a.Insert(MakeItem("ITM-1", 10, 10, 10, 1.0));
  items_a.Insert(MakeItem("ITM-2", 20, 20, 20, 2.0));
  BoxList boxes_a;
  boxes_a.Insert(MakeBox("A", 50, 50, 50));
  boxes_a.Insert(MakeBox("B", 60, 60, 60));

  ItemList items_b;
  items_b.Insert(MakeItem("ITM-2", 20, 20, 20, 2.0));
  items_b.Insert(MakeItem("ITM-1", 10, 10, 10, 1.0));
  BoxList boxes_b;
  boxes_b.Insert(MakeBox("B", 60, 60, 60));
  boxes_b.Insert(MakeBox("A", 50, 50, 50));

  EXPECT_EQ(CanonicalCacheKey(items_a, boxes_a), CanonicalCacheKey(items_b, boxes_b));
}

TEST(CanonicalCacheKeyTest, DiffersWhenItemContentDiffers) {
  ItemList items_a;
  items_a.Insert(MakeItem("ITM-1", 10, 10, 10, 1.0));
  ItemList items_b;
  items_b.Insert(MakeItem("ITM-1", 10, 10, 10, 2.0));  // different weight
  BoxList boxes;
  boxes.Insert(MakeBox("A", 50, 50, 50));

  EXPECT_NE(CanonicalCacheKey(items_a, boxes), CanonicalCacheKey(items_b, boxes));
}

TEST(CanonicalCacheKeyTest, DiffersWhenBoxSetDiffers) {
  ItemList items;
  items.Insert(MakeItem("ITM-1", 10, 10, 10, 1.0));
  BoxList boxes_a;
  boxes_a.Insert(MakeBox("A", 50, 50, 50));
  BoxList boxes_b;
  boxes_b.Insert(MakeBox("A", 50, 50, 50));
  boxes_b.Insert(MakeBox("B", 60, 60, 60));

  EXPECT_NE(CanonicalCacheKey(items, boxes_a), CanonicalCacheKey(items, boxes_b));
}

// ---------------------------------------------------------------------
// SolutionCache
// ---------------------------------------------------------------------

TEST(SolutionCacheTest, MissesOnAnEmptyCache) {
  auto [items, boxes] = SampleProblem();
  SolutionCache cache;
  EXPECT_FALSE(cache.Get(items, boxes).has_value());
}

TEST(SolutionCacheTest, PutThenGetReturnsTheStoredResponse) {
  auto [items, boxes] = SampleProblem();
  SolutionCache cache;

  PackResponse original = Packer().Pack(items, boxes);
  cache.Put(items, boxes, original);

  auto cached = cache.Get(items, boxes);
  ASSERT_TRUE(cached.has_value());
  EXPECT_EQ(cached->success, original.success);
  EXPECT_EQ(cached->boxes.Count(), original.boxes.Count());
  EXPECT_EQ(cached->GetTotalPlacedCount(), original.GetTotalPlacedCount());
}

TEST(SolutionCacheTest, CapacityZeroDisablesCaching) {
  auto [items, boxes] = SampleProblem();
  SolutionCache cache(/*capacity=*/0);

  cache.Put(items, boxes, Packer().Pack(items, boxes));
  EXPECT_EQ(cache.Count(), 0u);
  EXPECT_FALSE(cache.Get(items, boxes).has_value());
}

TEST(SolutionCacheTest, ClearEmptiesTheCache) {
  auto [items, boxes] = SampleProblem();
  SolutionCache cache;
  cache.Put(items, boxes, Packer().Pack(items, boxes));
  ASSERT_EQ(cache.Count(), 1u);

  cache.Clear();
  EXPECT_EQ(cache.Count(), 0u);
  EXPECT_FALSE(cache.Get(items, boxes).has_value());
}

TEST(SolutionCacheTest, EvictsLeastRecentlyUsedEntryWhenOverCapacity) {
  SolutionCache cache(/*capacity=*/2);

  auto MakeSingleItemProblem = [](const std::string& code) {
    ItemList items;
    items.Insert(MakeItem(code, 10, 10, 10, 1.0));
    BoxList boxes;
    boxes.Insert(MakeBox("BOX", 100, 100, 100));
    return std::pair<ItemList, BoxList>(std::move(items), std::move(boxes));
  };

  auto [items_1, boxes_1] = MakeSingleItemProblem("A");
  auto [items_2, boxes_2] = MakeSingleItemProblem("B");
  auto [items_3, boxes_3] = MakeSingleItemProblem("C");

  cache.Put(items_1, boxes_1, Packer().Pack(items_1, boxes_1));
  cache.Put(items_2, boxes_2, Packer().Pack(items_2, boxes_2));
  ASSERT_EQ(cache.Count(), 2u);

  // A third distinct entry pushes the cache over capacity -- "A" (the
  // least recently used, since "B" was inserted after it and neither
  // has been touched again) should be the one evicted.
  cache.Put(items_3, boxes_3, Packer().Pack(items_3, boxes_3));

  EXPECT_EQ(cache.Count(), 2u);
  EXPECT_FALSE(cache.Get(items_1, boxes_1).has_value());  // "A" evicted
  EXPECT_TRUE(cache.Get(items_2, boxes_2).has_value());   // "B" survives
  EXPECT_TRUE(cache.Get(items_3, boxes_3).has_value());   // "C" survives
}

TEST(SolutionCacheTest, GetPromotesAnEntryToMostRecentlyUsed) {
  SolutionCache cache(/*capacity=*/2);

  auto MakeSingleItemProblem = [](const std::string& code) {
    ItemList items;
    items.Insert(MakeItem(code, 10, 10, 10, 1.0));
    BoxList boxes;
    boxes.Insert(MakeBox("BOX", 100, 100, 100));
    return std::pair<ItemList, BoxList>(std::move(items), std::move(boxes));
  };

  auto [items_1, boxes_1] = MakeSingleItemProblem("A");
  auto [items_2, boxes_2] = MakeSingleItemProblem("B");
  auto [items_3, boxes_3] = MakeSingleItemProblem("C");

  cache.Put(items_1, boxes_1, Packer().Pack(items_1, boxes_1));
  cache.Put(items_2, boxes_2, Packer().Pack(items_2, boxes_2));

  // Touch "A" so "B" becomes the least-recently-used one instead.
  ASSERT_TRUE(cache.Get(items_1, boxes_1).has_value());

  cache.Put(items_3, boxes_3, Packer().Pack(items_3, boxes_3));

  EXPECT_TRUE(cache.Get(items_1, boxes_1).has_value());   // "A" survives (was touched)
  EXPECT_FALSE(cache.Get(items_2, boxes_2).has_value());  // "B" evicted instead
  EXPECT_TRUE(cache.Get(items_3, boxes_3).has_value());
}

// ---------------------------------------------------------------------
// PackCached
// ---------------------------------------------------------------------

TEST(PackCachedTest, FirstCallMatchesADirectPackerCall) {
  auto [items, boxes] = SampleProblem();
  SolutionCache cache;
  Packer packer;

  PackResponse direct = packer.Pack(items, boxes);
  PackResponse via_cache = PackCached(packer, items, boxes, cache);

  EXPECT_EQ(via_cache.success, direct.success);
  EXPECT_EQ(via_cache.boxes.Count(), direct.boxes.Count());
  EXPECT_EQ(via_cache.GetTotalPlacedCount(), direct.GetTotalPlacedCount());
}

TEST(PackCachedTest, RepeatedCallsWithTheSameProblemReuseOneCacheEntry) {
  auto [items, boxes] = SampleProblem();
  SolutionCache cache;
  Packer packer;

  PackResponse first = PackCached(packer, items, boxes, cache);
  ASSERT_EQ(cache.Count(), 1u);

  PackResponse second = PackCached(packer, items, boxes, cache);
  EXPECT_EQ(cache.Count(), 1u);  // same key -- no new entry
  EXPECT_EQ(second.success, first.success);
  EXPECT_EQ(second.GetTotalPlacedCount(), first.GetTotalPlacedCount());
}

TEST(PackCachedTest, DifferentProblemsGetSeparateCacheEntries) {
  auto [items_a, boxes_a] = SampleProblem();
  ItemList items_b;
  items_b.Insert(MakeItem("OTHER", 10, 10, 10, 1.0));
  BoxList boxes_b;
  boxes_b.Insert(MakeBox("OTHER-BOX", 100, 100, 100));

  SolutionCache cache;
  Packer packer;

  PackCached(packer, items_a, boxes_a, cache);
  PackCached(packer, items_b, boxes_b, cache);

  EXPECT_EQ(cache.Count(), 2u);
}
