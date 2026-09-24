// Regression test for a genuine non-termination bug found via
// benchmarks/boxpacker_comparison/'s random-scenario generator (scenario
// id 29, seed 1337): WeightRedistributor's pairwise variance formula is
// relative to each pair's own mean, not the request's overall target (the
// same shape dvdoug/boxpacker's own WeightRedistributor uses), which is
// not a globally monotone measure -- two boxes can trade an item back and
// forth indefinitely as the redistributor keeps re-pairing them against
// different neighbours in between. Confirmed by construction: the same
// item object was moved from one box to another and later moved back.
// WeightRedistributor.cpp now bounds that phase with a 250ms wall-clock
// deadline rather than an iteration count (see the comment there for why).
// This test is what should fail first if that deadline is ever removed or
// broken.

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>

#include "PackRequest.h"
#include "PackResponse.h"
#include "Packer.h"

namespace {

std::string ReadFile(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("Could not open fixture file: " + path);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

}  // namespace

TEST(WeightRedistribution, TerminatesQuicklyOnACyclingInput) {
  const std::string path =
      std::string(FITSOLVER_FIXTURES_DIR) + "/weight_redistribution_cycle.json";
  const fitsolver::PackRequest request =
      fitsolver::PackRequest::FromJsonString(ReadFile(path));

  const auto start = std::chrono::steady_clock::now();
  const fitsolver::PackResponse response =
      fitsolver::Packer().Pack(request.GetItems(), request.GetBoxes());
  const double ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

  // Generous margin over WeightRedistributor's 250ms deadline -- this
  // fixture used to run for tens of seconds (or longer; every run so far
  // has been killed rather than left to finish) before that deadline
  // existed.
  EXPECT_LT(ms, 750.0) << "Pack() took " << ms << "ms; the redistribution deadline may be broken";

  // The deadline must never produce an invalid result: every item is
  // still placed, just possibly with less evenly balanced box weights.
  EXPECT_TRUE(response.success);
  EXPECT_TRUE(response.unplaced_items.Empty());
}
