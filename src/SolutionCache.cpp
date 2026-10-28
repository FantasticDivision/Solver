#include "SolutionCache.h"

#include <algorithm>
#include <vector>

namespace fitsolver {

namespace {

std::string BuildCanonicalKey(const ItemList& items, const BoxList& boxes) {
  std::vector<nlohmann::json> item_entries;
  item_entries.reserve(items.Count());
  for (const auto& item : items) item_entries.push_back(item->ToJson());
  std::sort(item_entries.begin(), item_entries.end(), [](const auto& a, const auto& b) {
    return a.at("ItemCode").template get<std::string>() <
           b.at("ItemCode").template get<std::string>();
  });

  std::vector<nlohmann::json> box_entries;
  box_entries.reserve(boxes.Count());
  for (const auto& box : boxes) box_entries.push_back(box->ToJson());
  std::sort(box_entries.begin(), box_entries.end(), [](const auto& a, const auto& b) {
    return a.at("Reference").template get<std::string>() <
           b.at("Reference").template get<std::string>();
  });

  nlohmann::json key;
  key["items"] = std::move(item_entries);
  key["boxes"] = std::move(box_entries);
  // nlohmann::json objects serialize keys in a fixed (alphabetical)
  // order regardless of insertion order, so this dump is deterministic
  // for a given item/box content set.
  return key.dump();
}

}  // namespace

std::string CanonicalCacheKey(const ItemList& items, const BoxList& boxes) {
  return BuildCanonicalKey(items, boxes);
}

SolutionCache::SolutionCache(std::size_t capacity) : capacity_(capacity) {}

std::optional<PackResponse> SolutionCache::Get(const std::string key) {
  if (capacity_ == 0) return std::nullopt;
  std::lock_guard<std::mutex> lock(mutex_);

  const auto it = entries_.find(key);
  if (it == entries_.end()) return std::nullopt;

  // Touch: move this key to the front (most-recently-used) of recency_.
  recency_.erase(it->second.recency_it);
  recency_.push_front(key);
  it->second.recency_it = recency_.begin();

  return it->second.response;
}

void SolutionCache::Put(const std::string key, PackResponse response) {
  if (capacity_ == 0) return;

  std::lock_guard<std::mutex> lock(mutex_);

  const auto existing = entries_.find(key);
  if (existing != entries_.end()) {
    recency_.erase(existing->second.recency_it);
    entries_.erase(existing);
  } else if (entries_.size() >= capacity_) {
    // Evict the least-recently-used entry (back of recency_) to make room.
    const std::string& lru_key = recency_.back();
    entries_.erase(lru_key);
    recency_.pop_back();
  }

  recency_.push_front(key);
  entries_.emplace(key, Entry{std::move(response), recency_.begin()});
}

std::size_t SolutionCache::Count() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return entries_.size();
}

void SolutionCache::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  entries_.clear();
  recency_.clear();
}

PackResponse PackCached(const Packer& packer, const ItemList& items, const BoxList& boxes, SolutionCache& cache) {
  const std::string key = BuildCanonicalKey(items, boxes);
  if (auto cached = cache.Get(key)) return *cached;

  PackResponse response = packer.Pack(items, boxes);
  cache.Put(key, response);
  return response;
}

}  // namespace fitsolver