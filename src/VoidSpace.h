#pragma once

namespace fitsolver {

// An empty rectangular region inside a box, found by VoidFinder. Mirrors
// BoxPacker's `VoidSpace`.
class VoidSpace {
 public:
  VoidSpace(double x, double y, double z, double width, double length, double depth);

  double GetX() const { return x_; }
  double GetY() const { return y_; }
  double GetZ() const { return z_; }
  double GetWidth() const { return width_; }
  double GetLength() const { return length_; }
  double GetDepth() const { return depth_; }
  double GetVolume() const { return volume_; }
  double GetFootprint() const { return footprint_; }

 private:
  double x_, y_, z_;
  double width_, length_, depth_;
  double volume_;
  double footprint_;
};

}  // namespace fitsolver
