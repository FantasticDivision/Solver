#include "VolumePacker.h"

#include <algorithm>
#include <cmath>

#include "WorkingVolume.h"

namespace fitsolver {

namespace {

// Physical-packability constraint. An item can only be placed where a
// worker could actually set it down: on the box floor, or resting on the
// top face(s) of items already in the box. Anything less than
// kMinSupportRatio of its base area supported would leave it tipping or
// floating in mid-air, so the placement is rejected and the item is left
// for another box / another spot.
constexpr double kSupportEps = 1e-6;
constexpr double kMinSupportRatio = 0.75;

// Fraction (0..1) of the (x, y, w, l) footprint at height z that rests
// on the box floor or on the top face of an already-placed item.
double SupportedFraction(double x, double y, double w, double l, double z,
                          const std::vector<PackedItem>& placed) {
  if (z <= kSupportEps) return 1.0;  // sitting on the box floor
  const double base_area = w * l;
  if (base_area <= kSupportEps) return 1.0;

  double covered = 0.0;
  for (const auto& other : placed) {
    // Only items whose top face is exactly at this height can support it.
    if (std::abs((other.GetZ() + other.GetDepth()) - z) > kSupportEps) continue;

    const double ox = std::max(x, other.GetX());
    const double oy = std::max(y, other.GetY());
    const double ex = std::min(x + w, other.GetX() + other.GetWidth());
    const double ey = std::min(y + l, other.GetY() + other.GetLength());
    if (ex > ox && ey > oy) covered += (ex - ox) * (ey - oy);
  }
  return covered / base_area;
}

bool HasFloorSupport(double x, double y, double w, double l, double z,
                      const std::vector<PackedItem>& placed) {
  return SupportedFraction(x, y, w, l, z, placed) + kSupportEps >= kMinSupportRatio;
}

}  // namespace

VolumePacker::VolumePacker(BoxPtr box) : box_(std::move(box)) {}

PackedBox VolumePacker::Pack(const ItemList& items) const {
  return PackInternal(items.SortedItems(), box_->GetWeightCapacityForItems(), 0);
}

PackedBox VolumePacker::PackInternal(const std::vector<ItemPtr>& sorted_items,
                                      double weight_capacity, int depth) const {
  PackedBox result(box_);
  std::unordered_set<const Item*> placed;
  double weight_used = 0.0;

  PackRegion(0.0, 0.0, 0.0, box_->GetWidth(), box_->GetLength(), box_->GetDepth(), sorted_items,
             weight_capacity, weight_used, placed, result);

  if (depth < kMaxVoidFillDepth) {
    FillVoids(result, sorted_items, placed, weight_capacity, weight_used, depth);
  }

  // Nested WorkingVolume packs get merged + re-offset by the caller, so
  // only reorder once, at the top level: emit the placements strictly
  // bottom layer first, then front-to-back, then left-to-right, so the
  // response reads as a sequence a worker can physically follow (never
  // "place this one after the one it sits under").
  if (depth == 0 && result.GetItems().Count() > 1) {
    std::vector<PackedItem> ordered = result.GetItems().ToVector();
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const PackedItem& a, const PackedItem& b) {
                       if (std::abs(a.GetZ() - b.GetZ()) > kSupportEps) return a.GetZ() < b.GetZ();
                       if (std::abs(a.GetY() - b.GetY()) > kSupportEps) return a.GetY() < b.GetY();
                       return a.GetX() < b.GetX();
                     });
    PackedBox reordered(box_);
    for (auto& item : ordered) reordered.AddItem(std::move(item));
    return reordered;
  }
  return result;
}

void VolumePacker::PackRegion(double origin_x, double origin_y, double origin_z,
                               double region_width, double region_length, double region_depth,
                               const std::vector<ItemPtr>& sorted_items, double weight_capacity,
                               double& weight_used, std::unordered_set<const Item*>& placed,
                               PackedBox& out) const {
  const bool weight_unlimited = weight_capacity < 0.0;
  ShelfCursor cursor{origin_x, origin_y, origin_z, 0.0, 0.0};

  for (const auto& item : sorted_items) {
    if (placed.count(item.get()) > 0) continue;
    if (!weight_unlimited && weight_used + item->GetWeight() > weight_capacity) continue;

    if (auto placed_item = TryPlaceItem(item, cursor, origin_x, origin_y, origin_z, region_width,
                                         region_length, region_depth, out.GetItems().ToVector())) {
      weight_used += item->GetWeight();
      placed.insert(item.get());
      out.AddItem(std::move(*placed_item));
    }
  }
}

void VolumePacker::FillVoids(PackedBox& out, const std::vector<ItemPtr>& sorted_items,
                              std::unordered_set<const Item*>& placed, double weight_capacity,
                              double& weight_used, int depth) const {
  std::vector<ItemPtr> leftover;
  for (const auto& item : sorted_items) {
    if (placed.count(item.get()) == 0) leftover.push_back(item);
  }
  if (leftover.empty()) return;

  std::vector<VoidSpace> voids =
      void_finder_.Find(box_->GetWidth(), box_->GetLength(), box_->GetDepth(), out.GetItems().ToVector());

  bool progress = true;
  while (!leftover.empty() && !voids.empty() && progress) {
    progress = false;

    // Prefer lower positions, then larger free regions.
    std::sort(voids.begin(), voids.end(), [](const VoidSpace& a, const VoidSpace& b) {
      if (a.GetZ() != b.GetZ()) return a.GetZ() < b.GetZ();
      if (a.GetVolume() != b.GetVolume()) return a.GetVolume() > b.GetVolume();
      return a.GetFootprint() > b.GetFootprint();
    });

    for (const auto& void_space : voids) {
      if (leftover.empty()) break;

      // A void with nothing under its floor can't be packed by hand --
      // its contents would hang in the air. Only fill voids that sit on
      // the box floor or on already-placed items.
      if (void_space.GetZ() > kSupportEps &&
          !HasFloorSupport(void_space.GetX(), void_space.GetY(), void_space.GetWidth(),
                            void_space.GetLength(), void_space.GetZ(), out.GetItems().ToVector())) {
        continue;
      }

      // Pack this void as if it were its own (weightless, since weight
      // is tracked at this level) little box, using a fresh
      // VolumePacker scoped to its dimensions.
      auto working = std::make_shared<WorkingVolume>(void_space.GetWidth(), void_space.GetLength(),
                                                       void_space.GetDepth());
      VolumePacker sub_packer(working);
      PackedBox sub_result =
          sub_packer.PackInternal(leftover, weight_capacity < 0.0 ? weight_capacity
                                                                   : weight_capacity - weight_used,
                                   depth + 1);
      if (sub_result.GetItems().Count() == 0) continue;

      std::vector<PackedItem> newly_placed;
      for (const auto& packed_item : sub_result.GetItems()) {
        PackedItem offset_item =
            packed_item.WithOffset(void_space.GetX(), void_space.GetY(), void_space.GetZ());
        weight_used += offset_item.GetWeight();
        placed.insert(offset_item.GetItem().get());
        newly_placed.push_back(offset_item);
        out.AddItem(std::move(offset_item));
      }

      leftover.erase(std::remove_if(leftover.begin(), leftover.end(),
                                     [&](const ItemPtr& item) { return placed.count(item.get()) > 0; }),
                      leftover.end());

      voids = void_finder_.SubtractItems(std::move(voids), newly_placed);
      progress = true;
      break;  // restart with the refreshed, re-sorted void list
    }
  }
}

std::optional<OrientatedItem> VolumePacker::BestFittingOrientation(const ItemPtr& item,
                                                                    double width_left,
                                                                    double length_left,
                                                                    double depth_left) const {
  std::optional<OrientatedItem> best;
  double best_width_gap = 0.0;
  double best_length_gap = 0.0;

  for (auto& orientation : GenerateOrientations(item)) {
    if (orientation.GetWidth() > width_left || orientation.GetLength() > length_left ||
        orientation.GetDepth() > depth_left) {
      continue;
    }
    const double width_gap = width_left - orientation.GetWidth();
    const double length_gap = length_left - orientation.GetLength();
    if (!best.has_value() || width_gap < best_width_gap ||
        (width_gap == best_width_gap && length_gap < best_length_gap)) {
      best = orientation;
      best_width_gap = width_gap;
      best_length_gap = length_gap;
    }
  }
  return best;
}

std::optional<PackedItem> VolumePacker::TryPlaceItem(const ItemPtr& item, ShelfCursor& cursor,
                                                      double region_x, double region_y,
                                                      double region_z, double region_width,
                                                      double region_length, double region_depth,
                                                      const std::vector<PackedItem>& placed) const {
  const double region_end_x = region_x + region_width;
  const double region_end_y = region_y + region_length;
  const double region_end_z = region_z + region_depth;

  // 1. Current row.
  if (auto orientation =
          BestFittingOrientation(item, region_end_x - cursor.x, region_end_y - cursor.y,
                                  region_end_z - cursor.z)) {
    if (HasFloorSupport(cursor.x, cursor.y, orientation->GetWidth(), orientation->GetLength(),
                        cursor.z, placed)) {
      PackedItem placed_item(*orientation, cursor.x, cursor.y, cursor.z);
      cursor.x += orientation->GetWidth();
      cursor.row_length = std::max(cursor.row_length, orientation->GetLength());
      cursor.layer_depth = std::max(cursor.layer_depth, orientation->GetDepth());
      return placed_item;
    }
  }

  // 2. New row within the same layer.
  const double new_y = cursor.y + cursor.row_length;
  if (new_y < region_end_y) {
    if (auto orientation = BestFittingOrientation(item, region_width, region_end_y - new_y,
                                                    region_end_z - cursor.z)) {
      if (HasFloorSupport(region_x, new_y, orientation->GetWidth(), orientation->GetLength(),
                          cursor.z, placed)) {
        cursor.y = new_y;
        cursor.x = region_x;
        cursor.row_length = 0.0;
        PackedItem placed_item(*orientation, cursor.x, cursor.y, cursor.z);
        cursor.x += orientation->GetWidth();
        cursor.row_length = orientation->GetLength();
        cursor.layer_depth = std::max(cursor.layer_depth, orientation->GetDepth());
        return placed_item;
      }
    }
  }

  // 3. New layer stacked on top.
  const double new_z = cursor.z + cursor.layer_depth;
  if (new_z < region_end_z) {
    if (auto orientation =
            BestFittingOrientation(item, region_width, region_length, region_end_z - new_z)) {
      if (HasFloorSupport(region_x, region_y, orientation->GetWidth(), orientation->GetLength(),
                          new_z, placed)) {
        cursor.z = new_z;
        cursor.x = region_x;
        cursor.y = region_y;
        cursor.row_length = 0.0;
        cursor.layer_depth = 0.0;
        PackedItem placed_item(*orientation, cursor.x, cursor.y, cursor.z);
        cursor.x += orientation->GetWidth();
        cursor.row_length = orientation->GetLength();
        cursor.layer_depth = orientation->GetDepth();
        return placed_item;
      }
    }
  }

  return std::nullopt;
}

}  // namespace fitsolver
