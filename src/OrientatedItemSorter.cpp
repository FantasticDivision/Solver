#include "OrientatedItemSorter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "ItemList.h"
#include "OrientatedItemFactory.h"
#include "PackedBox.h"
#include "VolumePacker.h"
#include "WorkingVolume.h"

namespace fitsolver {

namespace {

// Max (smallest/largest) edge ratio for an item to be treated as
// slab-like. At or below this, scoring prefers the thin edge vertical.
constexpr double kSlabAspectRatio = 0.3;

// Only special-case "fit the whole remaining run on a grid" when at
// least this many same-size items are still to place (including the
// current one).
constexpr int kMinSameSizeItems = 2;

long long IntDiv(double a, double b) {
  return b > 0.0 ? static_cast<long long>(a / b) : 0;
}

// How many upcoming items the look-ahead sub-packs.
constexpr std::size_t kLookAheadItems = 7;

// Six decimal places, the same resolution the look-ahead cache key has always
// had (it used std::to_string(double)), so floating-point noise in a
// remaining-space subtraction doesn't defeat the cache.
long long Quantise(double value) { return std::llround(value * 1e6); }

// Six geometry values, the item count, then width/length/depth/weight/rotation
// for each look-ahead item.
struct LookAheadKey {
  std::array<long long, 7 + 5 * kLookAheadItems> values{};
  bool operator==(const LookAheadKey& other) const { return values == other.values; }
};

struct LookAheadKeyHash {
  std::size_t operator()(const LookAheadKey& key) const {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const long long value : key.values) {
      hash ^= static_cast<std::uint64_t>(value);
      hash *= 1099511628211ULL;
    }
    return static_cast<std::size_t>(hash);
  }
};

// A long-lived process would otherwise grow the cache without bound.
constexpr std::size_t kMaxLookAheadCacheEntries = 1u << 18;

}  // namespace

OrientatedItemSorter::OrientatedItemSorter(const OrientatedItemFactory& factory,
                                            bool single_pass_mode, double width_left,
                                            double length_left, double depth_left,
                                            const PackQueue& next_items,
                                            double row_length)
    : factory_(factory), single_pass_mode_(single_pass_mode), width_left_(width_left),
      length_left_(length_left), depth_left_(depth_left), next_items_(next_items),
      row_length_(row_length) {}

int OrientatedItemSorter::ExactFitDecider(double a_left, double b_left) {
  if (a_left <= 0.0 && b_left > 0.0) return -1;
  if (a_left > 0.0 && b_left <= 0.0) return 1;
  return 0;
}

int OrientatedItemSorter::Compare(const OrientatedItem& a, const OrientatedItem& b) const {
  // Prefer exact fits, checked width then length then depth.
  const double a_width_left = width_left_ - a.GetWidth();
  const double b_width_left = width_left_ - b.GetWidth();
  if (const int decider = ExactFitDecider(a_width_left, b_width_left); decider != 0) return decider;

  const double a_length_left = length_left_ - a.GetLength();
  const double b_length_left = length_left_ - b.GetLength();
  if (const int decider = ExactFitDecider(a_length_left, b_length_left); decider != 0) return decider;

  const double a_depth_left = depth_left_ - a.GetDepth();
  const double b_depth_left = depth_left_ - b.GetDepth();
  if (const int decider = ExactFitDecider(a_depth_left, b_depth_left); decider != 0) return decider;

  // Many same-size items left: prefer whichever orientation fits more of
  // them into this free space on a simple grid.
  if (const int decider = SameSizeItemsDecider(a, b); decider != 0) return decider;

  // Prefer leaving room for the next item(s) -- a bounded look-ahead.
  if (const int decider = LookAheadDecider(a, b, a_width_left, b_width_left); decider != 0) {
    return decider;
  }

  // For slab-like items with free rotation, prefer the thin edge as
  // depth (keeps it lying flat rather than standing on end).
  if (a.GetItem()->GetAllowedRotation() == Rotation::kBestFit && IsSlabLike(a.GetItem())) {
    const double thin =
        std::min({a.GetItem()->GetWidth(), a.GetItem()->GetLength(), a.GetItem()->GetDepth()});
    const bool b_thin = std::abs(b.GetDepth() - thin) < 1e-9;
    const bool a_thin = std::abs(a.GetDepth() - thin) < 1e-9;
    if (a_thin != b_thin) return a_thin ? -1 : 1;
  }

  // Otherwise prefer leaving the smallest possible gap, then the
  // greatest footprint.
  const double a_min_gap = std::min(a_width_left, a_length_left);
  const double b_min_gap = std::min(b_width_left, b_length_left);
  if (a_min_gap != b_min_gap) return a_min_gap < b_min_gap ? -1 : 1;
  if (a.GetSurfaceFootprint() != b.GetSurfaceFootprint()) {
    return a.GetSurfaceFootprint() < b.GetSurfaceFootprint() ? -1 : 1;
  }
  return 0;
}

int OrientatedItemSorter::SameSizeItemsDecider(const OrientatedItem& a,
                                                const OrientatedItem& b) const {
  if (!AllRemainingSameSize(a)) return 0;

  const long long remaining = static_cast<long long>(next_items_.Count()) + 1;
  if (remaining < kMinSameSizeItems) return 0;

  const long long fit_a = HowManyFitWithOrientation(a.GetWidth(), a.GetLength(), a.GetDepth());
  const long long fit_b = HowManyFitWithOrientation(b.GetWidth(), b.GetLength(), b.GetDepth());

  const bool covers_all_a = fit_a >= remaining;
  const bool covers_all_b = fit_b >= remaining;
  if (covers_all_a != covers_all_b) return covers_all_a ? -1 : 1;

  if (fit_a != fit_b) return fit_a > fit_b ? -1 : 1;
  return 0;
}

long long OrientatedItemSorter::HowManyFitWithOrientation(double width, double length,
                                                           double depth) const {
  if (width <= 0.0 || length <= 0.0 || depth <= 0.0) return 0;
  return IntDiv(width_left_, width) * IntDiv(length_left_, length) * IntDiv(depth_left_, depth);
}

bool OrientatedItemSorter::AllRemainingSameSize(const OrientatedItem& oriented) const {
  if (all_remaining_same_size_cache_.has_value()) return *all_remaining_same_size_cache_;

  for (std::size_t i = 0; i < next_items_.Count(); ++i) {
    if (!oriented.IsSameDimensions(next_items_.At(i))) {
      all_remaining_same_size_cache_ = false;
      return false;
    }
  }
  all_remaining_same_size_cache_ = true;
  return true;
}

bool OrientatedItemSorter::IsSlabLike(const ItemPtr& item) {
  std::array<double, 3> dims{item->GetWidth(), item->GetLength(), item->GetDepth()};
  std::sort(dims.begin(), dims.end());
  if (dims[2] <= 0.0) return false;
  return (dims[0] / dims[2]) <= kSlabAspectRatio;
}

int OrientatedItemSorter::LookAheadDecider(const OrientatedItem& a, const OrientatedItem& b,
                                            double a_width_left, double b_width_left) const {
  if (next_items_.Empty()) return 0;

  const auto fit_a = factory_.GetPossibleOrientations(next_items_.Top(), &a, a_width_left,
                                                        length_left_, depth_left_);
  const auto fit_b = factory_.GetPossibleOrientations(next_items_.Top(), &b, b_width_left,
                                                        length_left_, depth_left_);
  if (!fit_a.empty() && fit_b.empty()) return -1;
  if (!fit_b.empty() && fit_a.empty()) return 1;

  // Not an easy either/or: do a partial look-ahead.
  const int additional_a = CalculateAdditionalItemsPacked(a);
  const int additional_b = CalculateAdditionalItemsPacked(b);
  if (additional_a != additional_b) return additional_a > additional_b ? -1 : 1;
  return 0;
}

// Approximation of a forward-looking pack: not a real packing pass (no
// stability/void-fill refinement), just "how much of the next few items
// would geometrically fit" -- enough signal to break a tie between two
// otherwise-equal orientations of the current item.
int OrientatedItemSorter::CalculateAdditionalItemsPacked(const OrientatedItem& prev_item) const {
  if (single_pass_mode_) return 0;

  const double current_row_length = std::max(prev_item.GetLength(), row_length_);
  const std::size_t look_ahead_count = std::min<std::size_t>(next_items_.Count(), kLookAheadItems);

  LookAheadKey key;
  key.values[0] = Quantise(width_left_);
  key.values[1] = Quantise(length_left_);
  key.values[2] = Quantise(prev_item.GetWidth());
  key.values[3] = Quantise(prev_item.GetLength());
  key.values[4] = Quantise(current_row_length);
  key.values[5] = Quantise(depth_left_);
  key.values[6] = static_cast<long long>(look_ahead_count);
  for (std::size_t i = 0; i < look_ahead_count; ++i) {
    const ItemPtr& item = next_items_.At(i);
    const std::size_t base = 7 + 5 * i;
    key.values[base] = Quantise(item->GetWidth());
    key.values[base + 1] = Quantise(item->GetLength());
    key.values[base + 2] = Quantise(item->GetDepth());
    key.values[base + 3] = Quantise(item->GetWeight());
    key.values[base + 4] = static_cast<long long>(item->GetAllowedRotation());
  }

  // thread_local: solves can run concurrently (the portal serves requests on a
  // thread pool), and a shared cache would be a data race.
  thread_local std::unordered_map<LookAheadKey, int, LookAheadKeyHash> lookahead_cache;
  if (const auto it = lookahead_cache.find(key); it != lookahead_cache.end()) return it->second;
  if (lookahead_cache.size() >= kMaxLookAheadCacheEntries) lookahead_cache.clear();

  ItemList pool;
  for (std::size_t i = 0; i < look_ahead_count; ++i) pool.Insert(next_items_.At(i));

  auto remaining_row_box = std::make_shared<WorkingVolume>(
      width_left_ - prev_item.GetWidth(), current_row_length, depth_left_);
  VolumePacker remaining_row_packer(remaining_row_box);
  remaining_row_packer.SetSinglePassMode(true);
  const PackedBox remaining_row_packed = remaining_row_packer.Pack(pool);
  pool.RemovePacked(remaining_row_packed.GetItems());

  auto next_rows_box =
      std::make_shared<WorkingVolume>(width_left_, length_left_ - current_row_length, depth_left_);
  VolumePacker next_rows_packer(next_rows_box);
  next_rows_packer.SetSinglePassMode(true);
  const PackedBox next_rows_packed = next_rows_packer.Pack(pool);
  pool.RemovePacked(next_rows_packed.GetItems());

  const int packed_count = static_cast<int>(look_ahead_count - pool.Count());
  lookahead_cache.emplace(key, packed_count);
  return packed_count;
}

}  // namespace fitsolver
