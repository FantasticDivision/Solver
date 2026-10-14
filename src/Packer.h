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
// what's left. When the items carry two or more box groups, also tries
// packing the ungrouped items alongside each group in turn (every other
// group then packed on its own) and keeps whichever strategy places the
// most items in the fewest boxes. If more than one box was used, runs
// WeightRedistributor afterwards to even out weight between them. Last of
// all, GravitySettler lets every item in each finished box come to rest on
// the floor or on another item, so nothing in a response floats (not part
// of BoxPacker).
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

  // Box-group strategies beyond the single-pass baseline: at most this
  // many (one per group, largest groups first), and between them at most
  // kGroupStrategyWorkFactor times as many single-box packing attempts as
  // the baseline made. Work rather than time, so the result is the same on
  // every machine. The baseline itself always runs to completion, so this
  // only ever bounds the extra work, never whether a result comes back.
  static constexpr std::size_t kMaxGroupStrategies = 4;
  static constexpr std::size_t kGroupStrategyWorkFactor = 3;
};

}  // namespace fitsolver
