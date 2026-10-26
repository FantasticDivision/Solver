#pragma once

#include <cstddef>
#include <limits>
#include <vector>

#include "OrientatedItem.h"
#include "PackedItem.h"

namespace fitsolver {

// What an item rests on: the geometry GravitySettler, ItemReseater and
// StableRepacker share.
//
// Gravity acts along -Z: a box's floor is its Width x Length face at z = 0,
// and Depth is the vertical axis that layers stack along.

// Passed as `skip` when no entry of the list should be ignored.
constexpr std::size_t kNoItem = std::numeric_limits<std::size_t>::max();

// Whether two items' footprints share any floor area. Edges that merely
// touch don't count: neighbours standing side by side don't rest on each
// other.
bool FootprintsOverlap(const PackedItem& a, const PackedItem& b);

// The height `item` would come to rest at if lowered from above onto
// `items` where it is in x/y: the top of the tallest one under its
// footprint, or the floor. `items[skip]` is ignored (the item itself).
double RestingHeight(const PackedItem& item, const std::vector<PackedItem>& items, std::size_t skip);

// The same for a width x length footprint with its corner at (x, y),
// before any item is placed there.
double RestingHeight(double x, double y, double width, double length,
                     const std::vector<PackedItem>& items, std::size_t skip);

// Whether `item` stands firmly where it is: on the box floor, or with the
// centre of its base inside the area held up from beneath (the convex hull
// of the patches where its underside touches the tops of other items). An
// item that fails has its centre over thin air, so with its weight spread
// evenly it would tip; one that touches nothing at all is floating.
// `items[skip]` is ignored (the item itself).
bool StandsFirmly(const PackedItem& item, const std::vector<PackedItem>& items, std::size_t skip);

// The same for a width x length footprint with its corner at (x, y, z),
// before any item is placed there.
bool StandsFirmly(double x, double y, double z, double width, double length,
                  const std::vector<PackedItem>& items, std::size_t skip);

// How many of `items` don't stand firmly.
std::size_t CountNotStandingFirmly(const std::vector<PackedItem>& items);

// Drops every item straight down until it rests on the box floor or on the
// top of another item, like letting go of each one in turn from the bottom
// up. Only z changes. Items never move up and never end up overlapping.
std::vector<PackedItem> DropOntoSupport(std::vector<PackedItem> items);

// The ways `placed` may be turned when it is moved: as it is now first,
// then every other orientation its Rotation allows that either keeps it
// the same way up or passes BoxPacker's own tall-and-thin check
// (OrientatedItem::IsStable).
std::vector<OrientatedItem> OrientationsForMoving(const PackedItem& placed);

}  // namespace fitsolver
