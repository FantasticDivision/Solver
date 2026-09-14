#pragma once

#include "Box.h"
#include "OrientatedItemFactory.h"
#include "PackQueue.h"
#include "PackedItemList.h"
#include "PackedLayer.h"

namespace fitsolver {

// Packs one horizontal slab ("layer") of a box: fills a shelf row by
// row, recursively stacking items on top of a tall neighbour when there's
// spare depth, and recursively filling the lengthwise gap left behind a
// short item next to a taller one -- see PackLayer's comment. Mirrors
// BoxPacker's `LayerPacker` (minus ConstrainedPlacementItem support).
class LayerPacker {
 public:
  explicit LayerPacker(BoxPtr box) : box_(box), orientated_item_factory_(std::move(box)) {}

  void SetSinglePassMode(bool single_pass_mode) {
    orientated_item_factory_.SetSinglePassMode(single_pass_mode);
  }

  // Pulls items out of `queue` (in priority order, with skipped items
  // requeued to the front for the next attempt -- see PackQueue) and
  // places as many as fit into the widthForLayer x lengthForLayer x
  // depthForLayer region starting at (startX, startY, startZ).
  //
  // `guideline_layer_depth`, when > 0, is the layer's already-known
  // final depth (from a preliminary pass -- see VolumePacker), so the
  // very first item placed reserves the right amount of headroom to
  // stack on top of; 0 means "not yet known, use whatever this call
  // discovers as it goes". `first_item`, when set, is used for the very
  // first item pulled instead of asking the factory (VolumePacker tries
  // every valid orientation of the overall first item across the whole
  // box, since that choice has an outsized effect on everything after).
  PackedLayer PackLayer(PackQueue& queue, PackedItemList& packed_item_list, double start_x,
                        double start_y, double start_z, double width_for_layer,
                        double length_for_layer, double depth_for_layer,
                        double guideline_layer_depth, bool consider_stability,
                        const OrientatedItem* first_item);

 private:
  BoxPtr box_;
  OrientatedItemFactory orientated_item_factory_;
};

}  // namespace fitsolver
