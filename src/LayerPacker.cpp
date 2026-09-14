#include "LayerPacker.h"

#include <algorithm>
#include <array>

#include "PackedItem.h"

namespace fitsolver {

namespace {

bool SameDimensions(const ItemPtr& a, const ItemPtr& b) {
  if (a == b) return true;
  std::array<double, 3> dims_a{a->GetWidth(), a->GetLength(), a->GetDepth()};
  std::array<double, 3> dims_b{b->GetWidth(), b->GetLength(), b->GetDepth()};
  std::sort(dims_a.begin(), dims_a.end());
  std::sort(dims_b.begin(), dims_b.end());
  return dims_a == dims_b;
}

}  // namespace

PackedLayer LayerPacker::PackLayer(PackQueue& queue, PackedItemList& packed_item_list,
                                    double start_x, double start_y, double start_z,
                                    double width_for_layer, double length_for_layer,
                                    double depth_for_layer, double guideline_layer_depth,
                                    bool consider_stability, const OrientatedItem* first_item) {
  PackedLayer layer;
  double x = start_x;
  double y = start_y;
  const double z = start_z;
  double row_length = 0.0;
  std::optional<OrientatedItem> prev_item;
  std::vector<ItemPtr> skipped_items;

  const double box_weight_capacity = box_->GetWeightCapacityForItems();
  const bool weight_unlimited = box_weight_capacity < 0.0;

  // `first_item`, when given, applies only to the very first item this
  // call extracts -- VolumePacker tries every valid orientation of the
  // box's overall first item, since that choice has an outsized effect
  // on everything placed after it.
  const OrientatedItem* pending_first_item = first_item;

  while (!queue.Empty()) {
    ItemPtr item_to_pack = queue.Extract();

    // Skip items that will never fit this box at all, e.g. too heavy.
    if (!weight_unlimited &&
        item_to_pack->GetWeight() > box_weight_capacity - packed_item_list.GetWeight()) {
      continue;
    }

    std::optional<OrientatedItem> orientated_item;
    if (pending_first_item != nullptr && pending_first_item->GetItem() == item_to_pack) {
      orientated_item = *pending_first_item;
      pending_first_item = nullptr;
    } else {
      orientated_item = orientated_item_factory_.GetBestOrientation(
          item_to_pack, prev_item.has_value() ? &*prev_item : nullptr, queue.Snapshot(),
          width_for_layer - x, length_for_layer - y, depth_for_layer, row_length,
          consider_stability);
    }

    if (orientated_item.has_value()) {
      PackedItem packed_item(*orientated_item, x, y, z);
      layer.Insert(packed_item);
      packed_item_list.Insert(packed_item);

      row_length = std::max(row_length, packed_item.GetLength());
      prev_item = orientated_item;

      // Stack items on top of this one rather than side by side, e.g.
      // when we've packed a tall item and have just put a shorter one
      // next to it -- there's spare depth above the shorter one.
      const double stackable_depth =
          (guideline_layer_depth > 0.0 ? guideline_layer_depth : layer.GetDepth()) -
          packed_item.GetDepth();
      if (stackable_depth > 0.0) {
        PackedLayer stacked_layer =
            PackLayer(queue, packed_item_list, x, y, z + packed_item.GetDepth(),
                      x + packed_item.GetWidth(), y + packed_item.GetLength(), stackable_depth,
                      stackable_depth, consider_stability, nullptr);
        layer.Merge(stacked_layer);
      }

      x += packed_item.GetWidth();

      // There may be room lengthwise, across the width of just this
      // item, up to the current row's established length (left behind
      // by an earlier, longer item in this row).
      if (row_length > packed_item.GetLength()) {
        PackedLayer sideways_layer =
            PackLayer(queue, packed_item_list, x - packed_item.GetWidth(),
                      y + packed_item.GetLength(), z, x, y + row_length, depth_for_layer,
                      layer.GetDepth(), consider_stability, nullptr);
        layer.Merge(sideways_layer);
      }

      if (queue.Empty() && !skipped_items.empty()) {
        queue.Requeue(skipped_items);
        skipped_items.clear();
      }
      continue;
    }

    // Doesn't fit here -- try the next (differently-shaped) item instead
    // of giving up on this region immediately.
    if (!queue.Empty()) {
      skipped_items.push_back(item_to_pack);
      // No point retrying identical shapes right away; they'll fail the
      // same way. Last one is left for the loop to reprocess, since it's
      // what triggers the "nothing more fits" reset logic below.
      while (queue.Count() > 1 && SameDimensions(item_to_pack, queue.Top())) {
        skipped_items.push_back(queue.Extract());
      }
      continue;
    }

    if (x > start_x) {
      // Nothing more fits widthwise: wrap to a new row.
      y += row_length;
      x = start_x;
      row_length = 0.0;
      skipped_items.push_back(item_to_pack);
      queue.Requeue(skipped_items);
      skipped_items.clear();
      prev_item.reset();
      continue;
    }

    // Nothing fits even at the start of a fresh row -- this region is
    // exhausted.
    skipped_items.push_back(item_to_pack);
    queue.Requeue(skipped_items);
    return layer;
  }

  return layer;
}

}  // namespace fitsolver
