#include "StableRepacker.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <tuple>
#include <utility>

#include "OrientatedItem.h"
#include "Support.h"

namespace fitsolver {

namespace {

constexpr double kEps = 1e-6;
constexpr std::uint64_t kShuffleSeed = 20261027u;

// One item to lay out again, and the ways it may be turned.
struct Unit {
  const PackedItem* packed;
  std::vector<OrientatedItem> turns;
};

// Which of the firm spots open to an item it takes: the one with its base
// lowest, or the one with its top lowest. Either way ties go to the spot
// furthest back, then furthest left, then the one that turns it least.
enum class Rank { kLowestBase, kLowestTop };

// SplitMix64. Its output is fixed by its definition, unlike std::shuffle's,
// so a box comes out the same whichever compiler built the engine.
class SplitMix64 {
 public:
  explicit SplitMix64(std::uint64_t seed) : state_(seed) {}

  std::uint64_t Next() {
    std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }

 private:
  std::uint64_t state_;
};

void Shuffle(std::vector<const Unit*>& order, SplitMix64& random) {
  for (std::size_t i = order.size(); i > 1; --i) {
    std::swap(order[i - 1], order[random.Next() % i]);
  }
}

using Key = std::array<double, 5>;

double Footprint(const PackedItem& item) { return item.GetWidth() * item.GetLength(); }

// The orders tried before any shuffle. Each ends on the item's packed
// position, so no two items ever tie.
Key AsPacked(const Unit& unit) {
  const PackedItem& p = *unit.packed;
  return {p.GetZ(), p.GetY(), p.GetX(), 0.0, 0.0};
}

Key BiggestFootprintFirst(const Unit& unit) {
  const PackedItem& p = *unit.packed;
  return {-Footprint(p), -p.GetDepth(), p.GetZ(), p.GetY(), p.GetX()};
}

Key BiggestFootprintAnyWayFirst(const Unit& unit) {
  double biggest = 0.0;
  for (const auto& turn : unit.turns) biggest = std::max(biggest, turn.GetWidth() * turn.GetLength());
  const PackedItem& p = *unit.packed;
  return {-biggest, p.GetZ(), p.GetY(), p.GetX(), 0.0};
}

Key TallestFirst(const Unit& unit) {
  const PackedItem& p = *unit.packed;
  return {-p.GetDepth(), -Footprint(p), p.GetZ(), p.GetY(), p.GetX()};
}

Key BiggestFirst(const Unit& unit) {
  const PackedItem& p = *unit.packed;
  return {-Footprint(p) * p.GetDepth(), p.GetZ(), p.GetY(), p.GetX(), 0.0};
}

std::vector<const Unit*> SortedBy(const std::vector<Unit>& units, Key (*key)(const Unit&)) {
  std::vector<const Unit*> order;
  order.reserve(units.size());
  for (const auto& unit : units) order.push_back(&unit);
  std::stable_sort(order.begin(), order.end(),
                   [key](const Unit* a, const Unit* b) { return key(*a) < key(*b); });
  return order;
}

// Where along one side of the box something `size` long may start: against
// the wall, or against the far side of an item already placed.
std::vector<double> Starts(const std::vector<PackedItem>& placed, double size, double limit,
                           bool along_width) {
  std::vector<double> starts{0.0};
  for (const auto& item : placed) {
    starts.push_back(along_width ? item.GetX() + item.GetWidth() : item.GetY() + item.GetLength());
  }
  starts.erase(std::remove_if(starts.begin(), starts.end(),
                              [&](double start) { return start + size > limit + kEps; }),
               starts.end());
  std::sort(starts.begin(), starts.end());
  starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
  return starts;
}

// Lays the items out in `order`, each at the best firm spot left for it.
// Nothing if an item finds none, or once `work` reaches `max_work`.
std::optional<std::vector<PackedItem>> Build(const std::vector<const Unit*>& order, const Box& box,
                                             Rank rank, std::size_t& work, std::size_t max_work) {
  std::vector<PackedItem> placed;
  placed.reserve(order.size());
  for (const Unit* unit : order) {
    // The best firm spot so far, ranked as (height, y, x, turn), and its z.
    std::optional<std::tuple<double, double, double, std::size_t>> best;
    double best_z = 0.0;
    for (std::size_t turn = 0; turn < unit->turns.size(); ++turn) {
      const OrientatedItem& orientation = unit->turns[turn];
      const double width = orientation.GetWidth();
      const double length = orientation.GetLength();
      const auto xs = Starts(placed, width, box.GetWidth(), true);
      const auto ys = Starts(placed, length, box.GetLength(), false);
      for (const double x : xs) {
        for (const double y : ys) {
          if (work >= max_work) return std::nullopt;
          work += 2 * placed.size() + 1;

          const double z = RestingHeight(x, y, width, length, placed, kNoItem);
          const double top = z + orientation.GetDepth();
          if (top > box.GetDepth() + kEps) continue;

          // Only a spot that would beat the best so far is worth checking.
          const auto seat = std::make_tuple(rank == Rank::kLowestBase ? z : top, y, x, turn);
          if (best.has_value() && !(seat < *best)) continue;
          if (!StandsFirmly(x, y, z, width, length, placed, kNoItem)) continue;
          best = seat;
          best_z = z;
        }
      }
    }
    if (!best.has_value()) return std::nullopt;
    placed.emplace_back(unit->turns[std::get<3>(*best)], std::get<2>(*best), std::get<1>(*best),
                        best_z);
  }
  return placed;
}

}  // namespace

std::optional<std::vector<PackedItem>> StableRepacker::Repack(
    const std::vector<PackedItem>& items) const {
  if (items.empty()) return std::nullopt;

  std::vector<Unit> units;
  units.reserve(items.size());
  for (const auto& item : items) units.push_back({&item, OrientationsForMoving(item)});

  std::size_t work = 0;
  const std::vector<const Unit*> packed_order = SortedBy(units, AsPacked);
  for (const auto key :
       {AsPacked, BiggestFootprintFirst, BiggestFootprintAnyWayFirst, TallestFirst, BiggestFirst}) {
    const std::vector<const Unit*> order = SortedBy(units, key);
    for (const Rank rank : {Rank::kLowestBase, Rank::kLowestTop}) {
      if (auto layout = Build(order, *box_, rank, work, kMaxWork)) return layout;
    }
  }

  SplitMix64 random(kShuffleSeed);
  for (int n = 0; n < kShuffles && work < kMaxWork; ++n) {
    std::vector<const Unit*> order = packed_order;
    Shuffle(order, random);
    if (auto layout = Build(order, *box_, Rank::kLowestBase, work, kMaxWork)) return layout;
  }
  return std::nullopt;
}

}  // namespace fitsolver
