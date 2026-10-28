#pragma once

#include <cstddef>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "BoxList.h"
#include "ItemList.h"
#include "PackResponse.h"
#include "Packer.h"

namespace fitsolver {

// A canonical, order-independent key for an (items, boxes) packing
// problem, built from every item's/box's full ToJson() (ItemCode,
// dimensions, weight, BoxGroup, AllowedRotation for items; Reference,
// dimensions, weight limits, Active, MaximumBoxes for boxes), sorted by
// ItemCode/Reference before serializing. Two ItemList/BoxList pairs
// describing the same problem produce the same key regardless of what
// order items/boxes were Insert()-ed in.
//
// Caveat: the key depends only on item/box *content*, not on which
// ItemSorter/BoxSorter was injected into the ItemList/BoxList -- two
// otherwise-identical problems packed with different sorters (e.g.
// DefaultItemSorter vs EntropyItemSorter, see EntropyItemSorter.h) can
// legitimately produce different PackResponses but will hash to the
// same key. This is safe for the common case (PackRequest::FromJson
// always uses the default sorters -- see PackRequest.cpp), but don't
// share one SolutionCache between callers that inject custom sorters
// for the same item/box content.
std::string CanonicalCacheKey(const ItemList& items, const BoxList& boxes);

// A thread-safe, bounded-size lookup table from a packing problem to
// its already-computed PackResponse -- an explicit memoization layer
// over Packer::Pack, not part of Packer itself. Wrap a Packer call with
// this when repeat/duplicate requests are expected (e.g. behind a
// service endpoint), so an identical problem seen before is returned
// straight from the table instead of being solved again from scratch.
//
// Least-recently-used eviction keeps memory bounded for a long-running
// process -- an unbounded cache is a slow memory leak waiting to
// happen. Every method is mutex-guarded, so one SolutionCache can
// safely be shared across threads handling concurrent requests.
class SolutionCache {
 public:
  // capacity == 0 disables caching entirely: Get() always misses, Put()
  // is a no-op. Useful as a drop-in "off" switch without changing call
  // sites.
  explicit SolutionCache(std::size_t capacity = 256);

  // Returns the cached response for exactly this (items, boxes) pair,
  // if present -- also promotes it to most-recently-used. std::nullopt
  // on a cache miss.
  std::optional<PackResponse> Get(const std::string key);

  // Stores `response` for this (items, boxes) pair, evicting the
  // least-recently-used entry first if already at capacity. Overwrites
  // (and promotes to most-recently-used) if the key is already present.
  void Put(const std::string key, PackResponse response);

  std::size_t Count() const;
  void Clear();

 private:
  struct Entry {
    PackResponse response;
    std::list<std::string>::iterator recency_it;
  };

  std::size_t capacity_;
  mutable std::mutex mutex_;
  // Front = most recently used; back = eviction candidate. Kept
  // separate from entries_ so both "touch" and "evict oldest" are O(1)
  // instead of scanning for the least-recently-used entry.
  std::list<std::string> recency_;
  std::unordered_map<std::string, Entry> entries_;
};

// Convenience wrapper: look up (items, boxes) in `cache` first; on a
// miss, run `packer.Pack(items, boxes)`, store the result, and return
// it. This is the "easily recover a solved solution" entry point --
// most callers only need this, not SolutionCache's methods directly.
PackResponse PackCached(const Packer& packer, const ItemList& items, const BoxList& boxes, SolutionCache& cache);

}  // namespace fitsolver