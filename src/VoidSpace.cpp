#include "VoidSpace.h"

namespace fitsolver {

VoidSpace::VoidSpace(double x, double y, double z, double width, double length, double depth)
    : x_(x), y_(y), z_(z), width_(width), length_(length), depth_(depth),
      volume_(width * length * depth), footprint_(width * length) {}

}  // namespace fitsolver
