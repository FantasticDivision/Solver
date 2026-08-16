#pragma once

#include "Box.h"

namespace fitsolver {

// An anonymous, boundless-weight Box representing a bounded sub-region
// rather than a real box type -- used by VolumePacker to run its own
// placement logic recursively against a leftover void space found by
// VoidFinder (a "box within a box"). Mirrors BoxPacker's
// `WorkingVolume`.
class WorkingVolume : public Box {
 public:
  WorkingVolume(double width, double length, double depth);

  const std::string& GetReference() const override { return reference_; }
  double GetWidth() const override { return width_; }
  double GetLength() const override { return length_; }
  double GetDepth() const override { return depth_; }
  double GetEmptyWeight() const override { return 0.0; }
  double GetMaxWeight() const override { return kNoWeightLimit; }
  bool GetActive() const override { return true; }

 private:
  double width_;
  double length_;
  double depth_;
  std::string reference_;
};

}  // namespace fitsolver
