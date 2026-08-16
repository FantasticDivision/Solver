#include "Rotation.h"

#include <algorithm>
#include <cctype>

namespace fitsolver {

namespace {
std::string ToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                  [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}
}  // namespace

Rotation RotationFromString(const std::string& value) {
  const std::string lowered = ToLower(value);
  if (lowered == "never") return Rotation::kNever;
  if (lowered == "keepflat" || lowered == "keep_flat") return Rotation::kKeepFlat;
  return Rotation::kBestFit;
}

std::string RotationToString(Rotation rotation) {
  switch (rotation) {
    case Rotation::kNever:
      return "Never";
    case Rotation::kKeepFlat:
      return "KeepFlat";
    case Rotation::kBestFit:
    default:
      return "BestFit";
  }
}

}  // namespace fitsolver
