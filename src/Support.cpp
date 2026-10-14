#include "Support.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace fitsolver {

namespace {

constexpr double kEps = 1e-6;

using Point = std::pair<double, double>;

// > 0 when o -> a -> b turns anticlockwise.
double Cross(const Point& o, const Point& a, const Point& b) {
  return (a.first - o.first) * (b.second - o.second) - (a.second - o.second) * (b.first - o.first);
}

// Andrew's monotone chain; the hull comes back anticlockwise.
std::vector<Point> ConvexHull(std::vector<Point> points) {
  std::sort(points.begin(), points.end());
  points.erase(std::unique(points.begin(), points.end()), points.end());
  if (points.size() < 3) return points;

  std::vector<Point> hull;
  hull.reserve(points.size() + 1);
  for (int pass = 0; pass < 2; ++pass) {
    const std::size_t floor = hull.size() + 1;
    for (const Point& point : points) {
      while (hull.size() > floor && Cross(hull[hull.size() - 2], hull.back(), point) <= 0.0) {
        hull.pop_back();
      }
      hull.push_back(point);
    }
    hull.pop_back();  // The last point of one chain starts the other.
    std::reverse(points.begin(), points.end());
  }
  return hull;
}

// Inside or on the boundary of an anticlockwise convex polygon.
bool Contains(const std::vector<Point>& hull, const Point& point) {
  if (hull.size() < 3) return false;
  for (std::size_t i = 0; i < hull.size(); ++i) {
    const Point& from = hull[i];
    const Point& to = hull[(i + 1) % hull.size()];
    const double scale =
        std::max(1.0, std::abs(to.first - from.first) + std::abs(to.second - from.second));
    if (Cross(from, to, point) < -kEps * scale) return false;
  }
  return true;
}

}  // namespace

bool FootprintsOverlap(const PackedItem& a, const PackedItem& b) {
  return a.GetX() + a.GetWidth() > b.GetX() + kEps && b.GetX() + b.GetWidth() > a.GetX() + kEps &&
         a.GetY() + a.GetLength() > b.GetY() + kEps && b.GetY() + b.GetLength() > a.GetY() + kEps;
}

double RestingHeight(const PackedItem& item, const std::vector<PackedItem>& items,
                     std::size_t skip) {
  double height = 0.0;
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i == skip || !FootprintsOverlap(item, items[i])) continue;
    height = std::max(height, items[i].GetZ() + items[i].GetDepth());
  }
  return height;
}

bool StandsFirmly(const PackedItem& item, const std::vector<PackedItem>& items, std::size_t skip) {
  if (item.GetZ() <= kEps) return true;

  std::vector<Point> contact;
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i == skip) continue;
    const PackedItem& other = items[i];
    if (std::abs(other.GetZ() + other.GetDepth() - item.GetZ()) > kEps) continue;
    if (!FootprintsOverlap(item, other)) continue;

    const double x0 = std::max(item.GetX(), other.GetX());
    const double x1 = std::min(item.GetX() + item.GetWidth(), other.GetX() + other.GetWidth());
    const double y0 = std::max(item.GetY(), other.GetY());
    const double y1 = std::min(item.GetY() + item.GetLength(), other.GetY() + other.GetLength());
    contact.insert(contact.end(), {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}});
  }

  const Point centre{item.GetX() + item.GetWidth() / 2.0, item.GetY() + item.GetLength() / 2.0};
  return Contains(ConvexHull(std::move(contact)), centre);
}

std::size_t CountNotStandingFirmly(const std::vector<PackedItem>& items) {
  std::size_t count = 0;
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (!StandsFirmly(items[i], items, i)) ++count;
  }
  return count;
}

std::vector<PackedItem> DropOntoSupport(std::vector<PackedItem> items) {
  // Lowest first. Two items that share floor area can't share a height
  // range, so the lower one of any such pair always comes first, and by
  // the time an item is dropped everything beneath it has already landed.
  std::vector<std::size_t> order(items.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
    return items[a].GetZ() < items[b].GetZ();
  });

  for (std::size_t n = 0; n < order.size(); ++n) {
    const PackedItem& item = items[order[n]];

    double resting_z = 0.0;
    for (std::size_t below = 0; below < n; ++below) {
      const PackedItem& landed = items[order[below]];
      if (FootprintsOverlap(item, landed)) {
        resting_z = std::max(resting_z, landed.GetZ() + landed.GetDepth());
      }
    }

    // Never lift an item: if the surface beneath is already at or above
    // its underside it's resting (or the input was already overlapping,
    // which isn't this function's to fix).
    if (resting_z < item.GetZ() - kEps) {
      items[order[n]] = item.WithOffset(0.0, 0.0, resting_z - item.GetZ());
    }
  }
  return items;
}

}  // namespace fitsolver
