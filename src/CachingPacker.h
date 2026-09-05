#pragma once

#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "BoxList.h"
#include "ItemList.h"
#include "Packer.h"
#include "PackRequest.h"
#include "PackResponse.h"

namespace fitsolver {

// Wraps any Packer and memoizes full solves by request content (which
// exact items + which exact boxes were offered), independent of the
// order they were listed in. Safe to call concurrently from multiple
// threads.
//
// IMPORTANT: this must be constructed ONCE and reused across every
// request it serves (e.g. as a `static` in a long-lived service or
// shared library entry point) -- constructing a fresh CachingPacker
// per-request defeats caching entirely, since each one starts with an
// empty cache.
//
// KNOWN LIMITATION: the cache currently has no eviction -- it grows for
// as long as the process runs, one entry per distinct request ever
// seen. Fine for an MVP / moderate order volume; would need an eviction
// strategy (e.g. LRU with a max size) before this ships somewhere that
// runs for a long time against a large number of *distinct* orders.
class CachingPacker {
 public:
  explicit CachingPacker(Packer inner = Packer()) : inner_(std::move(inner)) {}

  PackResponse Pack(const PackRequest& request) {
    const std::string key = CanonicalKey(request);

    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (auto it = cache_.find(key); it != cache_.end()) return it->second;
    }

    PackResponse response = inner_.Pack(request);

    std::lock_guard<std::mutex> lock(mutex_);
    cache_.emplace(key, response);
    return response;
  }

  // Mirrors Packer's other overload (used by pymodule.cpp's pack_dicts/pack).
  PackResponse Pack(const ItemList& items, const BoxList& boxes) {
    return Pack(PackRequest(items, boxes));
  }

 private:
  // Canonical, order-independent fingerprint of a request's contents.
  // Relies on Item::ToJson()/Box::ToJson() being virtual, so it picks up
  // subtype-specific fields (LinkedItem's box group, LimitedSupplyBox's
  // quantity, ...) automatically -- no per-subtype logic to maintain
  // here as new Item/Box subtypes get added.
  static std::string CanonicalKey(const PackRequest& request) {
    std::vector<std::string> item_dumps;
    for (const auto& item : request.GetItems()) item_dumps.push_back(item->ToJson().dump());
    std::sort(item_dumps.begin(), item_dumps.end());

    std::vector<std::string> box_dumps;
    for (const auto& box : request.GetBoxes()) box_dumps.push_back(box->ToJson().dump());
    std::sort(box_dumps.begin(), box_dumps.end());

    std::string key;
    for (const auto& s : item_dumps) key += s;
    key += '|';
    for (const auto& s : box_dumps) key += s;
    return key;
  }

  Packer inner_;
  std::unordered_map<std::string, PackResponse> cache_;
  std::mutex mutex_;
};

}  // namespace fitsolver