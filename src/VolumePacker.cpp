#include "VolumePacker.h"

#include <algorithm>
#include <cmath>

#include "LayerStabiliser.h"
#include "OrientatedItemFactory.h"

namespace fitsolver {

namespace {
constexpr double kOrderEps = 1e-6;
}  // namespace

VolumePacker::VolumePacker(BoxPtr box) : box_(std::move(box)) {}

void VolumePacker::SetSinglePassMode(bool single_pass_mode) { single_pass_mode_ = single_pass_mode; }

PackedBox VolumePacker::Pack(const ItemList& items) const {
  const std::vector<ItemPtr>& sorted_items = items.SortedItems();
  if (sorted_items.empty()) return PackedBox(box_);

  // Sometimes "space available" decisions depend on orientation of the
  // box, so try both ways -- unless every item is rotation-locked, in
  // which case there's nothing to gain from it.
  std::vector<bool> rotations_to_test{false};
  if (!single_pass_mode_ && !AllItemsNoRotation(sorted_items)) rotations_to_test.push_back(true);

  // The orientation of the first item has an outsized effect on
  // everything placed after it, so special-case it and try every valid
  // orientation as the starting placement.
  const OrientatedItemFactory top_item_factory(box_);

  std::vector<PackedBox> attempts;
  for (bool swap_width_length : rotations_to_test) {
    const double box_width = swap_width_length ? box_->GetLength() : box_->GetWidth();
    const double box_length = swap_width_length ? box_->GetWidth() : box_->GetLength();

    std::vector<std::optional<OrientatedItem>> first_item_orientations;
    if (single_pass_mode_) {
      first_item_orientations.emplace_back(std::nullopt);
    } else {
      auto possible = top_item_factory.GetPossibleOrientations(sorted_items.front(), nullptr,
                                                                 box_width, box_length,
                                                                 box_->GetDepth());
      if (possible.empty()) {
        first_item_orientations.emplace_back(std::nullopt);
      } else {
        // Orientations with identical placed dimensions (e.g. every
        // permutation of a cube) produce an identical PackRotation
        // result -- only try each distinct (width, length, depth) once.
        for (auto& orientation : possible) {
          const bool duplicate = std::any_of(
              first_item_orientations.begin(), first_item_orientations.end(),
              [&](const std::optional<OrientatedItem>& seen) {
                return seen.has_value() && seen->GetWidth() == orientation.GetWidth() &&
                       seen->GetLength() == orientation.GetLength() &&
                       seen->GetDepth() == orientation.GetDepth();
              });
          if (!duplicate) first_item_orientations.emplace_back(std::move(orientation));
        }
      }
    }

    for (const auto& first_orientation : first_item_orientations) {
      PackedBox attempt = PackRotation(box_width, box_length,
                                        first_orientation.has_value() ? &*first_orientation : nullptr,
                                        sorted_items);
      if (attempt.GetItems().Count() == sorted_items.size()) return FinaliseOrder(std::move(attempt));
      attempts.push_back(std::move(attempt));
    }
  }

  if (attempts.empty()) return PackedBox(box_);

  std::size_t best_idx = 0;
  for (std::size_t i = 1; i < attempts.size(); ++i) {
    if (attempts[i].GetVolumeUtilisation() > attempts[best_idx].GetVolumeUtilisation()) best_idx = i;
  }
  return FinaliseOrder(std::move(attempts[best_idx]));
}

PackedBox VolumePacker::PackRotation(double box_width, double box_length,
                                      const OrientatedItem* first_item_orientation,
                                      const std::vector<ItemPtr>& sorted_items) const {
  LayerPacker layer_packer(box_);
  layer_packer.SetSinglePassMode(single_pass_mode_);

  PackQueue queue(sorted_items);
  std::vector<PackedLayer> layers;

  while (!queue.Empty()) {
    const double layer_start_depth = SumLayerDepths(layers);
    PackedItemList packed_item_list = FlattenLayers(layers);

    // The special first-item orientation only ever applies to the very
    // first layer of this attempt; once anything at all has been
    // packed, subsequent layers pick their own first item normally.
    const OrientatedItem* effective_first_item =
        packed_item_list.Count() > 0 ? nullptr : first_item_orientation;

    // Do a preliminary layer pack, on a scratch copy of the queue, to
    // discover the depth this layer actually needs.
    PackQueue preliminary_queue = queue;
    PackedItemList preliminary_packed_item_list = packed_item_list;
    PackedLayer preliminary_layer = layer_packer.PackLayer(
        preliminary_queue, preliminary_packed_item_list, 0.0, 0.0, layer_start_depth, box_width,
        box_length, box_->GetDepth() - layer_start_depth, 0.0, true, effective_first_item);

    if (preliminary_layer.Empty()) break;

    const double preliminary_layer_depth = preliminary_layer.GetDepth();
    if (preliminary_layer_depth == preliminary_layer.GetItems().front().GetDepth()) {
      // Preliminary depth matches what the first item alone would give
      // us -- nothing to redo, keep it and commit the scratch queue.
      layers.push_back(std::move(preliminary_layer));
      queue = std::move(preliminary_queue);
    } else {
      // The layer needs more depth than the first item alone implied
      // (something placed later in the layer is taller) -- redo it
      // against the REAL queue with that depth pre-known, so the first
      // item reserves the right amount of headroom to stack on top of.
      PackedLayer final_layer = layer_packer.PackLayer(
          queue, packed_item_list, 0.0, 0.0, layer_start_depth, box_width, box_length,
          box_->GetDepth() - layer_start_depth, preliminary_layer_depth, true, effective_first_item);
      layers.push_back(std::move(final_layer));
    }
  }

  if (!single_pass_mode_ && !layers.empty()) {
    LayerStabiliser stabiliser;
    layers = stabiliser.Stabilise(std::move(layers));
    layers = FillVoids(std::move(layers), queue, layer_packer, box_width, box_length);
  }

  layers = CorrectLayerRotation(std::move(layers), box_width);

  PackedBox result(box_);
  for (const auto& layer : layers) {
    for (const auto& item : layer.GetItems()) result.AddItem(item);
  }
  return result;
}

std::vector<PackedLayer> VolumePacker::FillVoids(std::vector<PackedLayer> layers, PackQueue& queue,
                                                   LayerPacker& layer_packer, double box_width,
                                                   double box_length) const {
  PackedItemList packed_item_list = FlattenLayers(layers);
  std::vector<VoidSpace> voids =
      void_finder_.Find(box_width, box_length, box_->GetDepth(), packed_item_list.ToVector());

  bool progress = true;
  while (!queue.Empty() && !voids.empty() && progress) {
    progress = false;

    // Prefer lower positions, then larger free regions (volume, then
    // footprint).
    std::sort(voids.begin(), voids.end(), [](const VoidSpace& a, const VoidSpace& b) {
      if (a.GetZ() != b.GetZ()) return a.GetZ() < b.GetZ();
      if (a.GetVolume() != b.GetVolume()) return a.GetVolume() > b.GetVolume();
      return a.GetFootprint() > b.GetFootprint();
    });

    for (const auto& void_space : voids) {
      PackedLayer layer = layer_packer.PackLayer(
          queue, packed_item_list, void_space.GetX(), void_space.GetY(), void_space.GetZ(),
          void_space.GetX() + void_space.GetWidth(), void_space.GetY() + void_space.GetLength(),
          void_space.GetDepth(), void_space.GetDepth(),
          /*consider_stability=*/false,  // gap fill; stability assumed from surrounding geometry
          nullptr);

      if (!layer.Empty()) {
        voids = void_finder_.SubtractItems(std::move(voids), layer.GetItems());
        layers.push_back(std::move(layer));
        progress = true;
        break;
      }
    }
  }

  return layers;
}

std::vector<PackedLayer> VolumePacker::CorrectLayerRotation(std::vector<PackedLayer> layers,
                                                              double box_width) const {
  if (box_->GetWidth() == box_width) return layers;

  std::vector<PackedLayer> corrected;
  corrected.reserve(layers.size());
  for (const auto& old_layer : layers) {
    PackedLayer new_layer;
    for (const auto& old_item : old_layer.GetItems()) {
      std::string new_code = old_item.GetOrientationCode();
      if (new_code.size() >= 2) std::swap(new_code[0], new_code[1]);
      OrientatedItem swapped(old_item.GetItem(), old_item.GetLength(), old_item.GetWidth(),
                              old_item.GetDepth(), std::move(new_code));
      new_layer.Insert(PackedItem(std::move(swapped), old_item.GetY(), old_item.GetX(), old_item.GetZ()));
    }
    corrected.push_back(std::move(new_layer));
  }
  return corrected;
}

PackedBox VolumePacker::FinaliseOrder(PackedBox box) const {
  if (box.GetItems().Count() <= 1) return box;

  std::vector<PackedItem> ordered = box.GetItems().ToVector();
  std::stable_sort(ordered.begin(), ordered.end(), [](const PackedItem& a, const PackedItem& b) {
    if (std::abs(a.GetZ() - b.GetZ()) > kOrderEps) return a.GetZ() < b.GetZ();
    if (std::abs(a.GetY() - b.GetY()) > kOrderEps) return a.GetY() < b.GetY();
    return a.GetX() < b.GetX();
  });

  PackedBox reordered(box.GetBox());
  for (auto& item : ordered) reordered.AddItem(std::move(item));
  return reordered;
}

PackedItemList VolumePacker::FlattenLayers(const std::vector<PackedLayer>& layers) {
  PackedItemList list;
  for (const auto& layer : layers) {
    for (const auto& item : layer.GetItems()) list.Insert(item);
  }
  return list;
}

double VolumePacker::SumLayerDepths(const std::vector<PackedLayer>& layers) {
  double depth = 0.0;
  for (const auto& layer : layers) depth += layer.GetDepth();
  return depth;
}

bool VolumePacker::AllItemsNoRotation(const std::vector<ItemPtr>& items) {
  for (const auto& item : items) {
    if (item->GetAllowedRotation() != Rotation::kNever) return false;
  }
  return true;
}

}  // namespace fitsolver
