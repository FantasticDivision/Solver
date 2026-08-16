#pragma once

#include <string>

namespace fitsolver {

// How free an item is to be reoriented while packing.
enum class Rotation {
  kNever,     // Must be packed in its given Width/Length/Depth order only.
  kKeepFlat,  // May be turned 90 degrees flat (W/L swapped), but not stood on end.
  kBestFit,   // No restriction; any of the 6 axis permutations is allowed.
};

Rotation RotationFromString(const std::string& value);
std::string RotationToString(Rotation rotation);

}  // namespace fitsolver
