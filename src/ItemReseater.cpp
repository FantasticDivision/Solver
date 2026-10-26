#include "ItemReseater.h"

#include <algorithm>
#include <numeric>
#include <optional>
#include <tuple>
#include <utility>

#include "OrientatedItem.h"
#include "Support.h"

namespace fitsolver {

namespace {

constexpr double kEps = 1e-6;

// Where a width x length footprint might sit: the box's corners, and for
// every other item the corners and middle of its top and the spots flush
// against its sides. A handful per item rather than every combination of
// edges, which finds nearly as many seats for a fraction of the work.
std::vector<std::pair<double, double>> Seats(const std::vector<PackedItem>& items, std::size_t skip,
                                             double width, double length, const Box& box) {
  const double max_x = box.GetWidth() - width;
  const double max_y = box.GetLength() - length;

  std::vector<std::pair<double, double>> seats{{0.0, 0.0}, {max_x, 0.0}, {0.0, max_y}, {max_x, max_y}};
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i == skip) continue;
    const PackedItem& other = items[i];
    // Flush with each of `other`'s four edges from the inside...
    const double x0 = other.GetX();
    const double y0 = other.GetY();
    const double x1 = other.GetX() + other.GetWidth() - width;
    const double y1 = other.GetY() + other.GetLength() - length;
    seats.insert(seats.end(), {{x0, y0},
                               {x1, y0},
                               {x0, y1},
                               {x1, y1},
                               {(x0 + x1) / 2.0, (y0 + y1) / 2.0},
                               // ...and butted up against it from the outside.
                               {other.GetX() + other.GetWidth(), y0},
                               {x0, other.GetY() + other.GetLength()},
                               {x0 - width, y0},
                               {x0, y0 - length}});
  }

  seats.erase(std::remove_if(seats.begin(), seats.end(),
                             [&](const std::pair<double, double>& seat) {
                               return seat.first < -kEps || seat.first > max_x + kEps ||
                                      seat.second < -kEps || seat.second > max_y + kEps;
                             }),
              seats.end());
  for (auto& seat : seats) {
    seat.first = std::max(0.0, seat.first);
    seat.second = std::max(0.0, seat.second);
  }
  std::sort(seats.begin(), seats.end());
  seats.erase(std::unique(seats.begin(), seats.end()), seats.end());
  return seats;
}

// The best seat in the box where items[index] would stand firmly, if any:
// the one that leaves its top lowest, then the one that needs it turned
// least, then the one nearest to where it is now. `work` counts the item
// comparisons spent, and the search stops once it reaches `max_work`.
std::optional<PackedItem> BestSeat(const std::vector<PackedItem>& items, std::size_t index,
                                   const Box& box, std::size_t& work, std::size_t max_work) {
  const PackedItem& current = items[index];
  const std::vector<OrientatedItem> orientations = OrientationsForMoving(current);

  std::optional<PackedItem> best;
  std::tuple<double, std::size_t, double, double, double> best_rank;
  for (std::size_t turn = 0; turn < orientations.size(); ++turn) {
    const OrientatedItem& orientation = orientations[turn];
    for (const auto& [x, y] :
         Seats(items, index, orientation.GetWidth(), orientation.GetLength(), box)) {
      if (work >= max_work) return best;
      work += items.size();

      const double z = RestingHeight(PackedItem(orientation, x, y, 0.0), items, index);
      const double top = z + orientation.GetDepth();
      if (top > box.GetDepth() + kEps) continue;

      PackedItem seated(orientation, x, y, z);
      if (!StandsFirmly(seated, items, index)) continue;

      const double dx = x - current.GetX();
      const double dy = y - current.GetY();
      const auto rank = std::make_tuple(top, turn, dx * dx + dy * dy, x, y);
      if (!best.has_value() || rank < best_rank) {
        best = std::move(seated);
        best_rank = rank;
      }
    }
  }
  return best;
}

}  // namespace

std::vector<PackedItem> ItemReseater::Reseat(std::vector<PackedItem> items) const {
  if (items.size() < 2) return items;  // A lone item is on the floor.

  std::size_t not_firm = CountNotStandingFirmly(items);
  std::size_t work = 0;

  for (int pass = 0; pass < kMaxPasses && not_firm > 0; ++pass) {
    // Lowest first, so an item is re-seated before the ones resting on it.
    std::vector<std::size_t> order(items.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
      return items[a].GetZ() < items[b].GetZ();
    });

    bool moved = false;
    for (const std::size_t index : order) {
      if (not_firm == 0 || work >= kMaxWork) break;
      if (StandsFirmly(items[index], items, index)) continue;

      std::optional<PackedItem> seat = BestSeat(items, index, *box_, work, kMaxWork);
      if (!seat.has_value()) continue;

      // Taking the item out from under others lets them fall, so judge
      // the move on the whole box once everything has settled again.
      // That compares every item with every other, which counts as work.
      work += items.size() * items.size();
      std::vector<PackedItem> trial = items;
      trial[index] = std::move(*seat);
      trial = DropOntoSupport(std::move(trial));
      const std::size_t trial_not_firm = CountNotStandingFirmly(trial);
      if (trial_not_firm < not_firm) {
        items = std::move(trial);
        not_firm = trial_not_firm;
        moved = true;
      }
    }
    if (!moved) break;
  }
  return items;
}

}  // namespace fitsolver
