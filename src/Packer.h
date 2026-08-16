#pragma once

#include <cstddef>

#include "PackRequest.h"
#include "PackResponse.h"

namespace fitsolver {

// Top-level packing orchestrator -- the main entry point for this
// library. Mirrors BoxPacker's `Packer`: for the still-unplaced items,
// tries every active box type via VolumePacker, commits whichever
// single box instance fits the most (ties broken by volume
// utilisation), and repeats until nothing more fits or no box can hold
// what's left. If more than one box was used, runs WeightRedistributor
// afterwards to even out weight between them.
//
// Never throws for "doesn't fit" -- items that can't be placed in any
// box end up in PackResponse::unplaced_items and `success` is false.
class Packer {
 public:
  PackResponse Pack(const PackRequest& request) const;
  PackResponse Pack(const ItemList& items, const BoxList& boxes) const;

 private:
  // Above this many boxes, weight-balancing's extra repacking passes
  // cost more than they're worth. Mirrors BoxPacker's
  // maxBoxesToBalanceWeight default.
  static constexpr std::size_t kMaxBoxesToBalanceWeight = 12;
};

}  // namespace fitsolver
