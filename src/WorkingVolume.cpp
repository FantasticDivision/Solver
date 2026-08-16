#include "WorkingVolume.h"

namespace fitsolver {

WorkingVolume::WorkingVolume(double width, double length, double depth)
    : width_(width), length_(length), depth_(depth),
      reference_("Working Volume " + std::to_string(width) + "x" + std::to_string(length) + "x" +
                 std::to_string(depth)) {}

}  // namespace fitsolver
