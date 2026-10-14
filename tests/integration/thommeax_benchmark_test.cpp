// Runs the Thommeax benchmark scenarios (tests/ThommeaxMinimumTests/) through
// the real Packer and checks we place every unit using no more boxes than
// the reference (dvdoug/boxpacker) result recorded in each scenario's
// RESULTS.md. See tests/ThommeaxMinimumTests/README.md for the data format.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>

#include "BoxList.h"
#include "ItemList.h"
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

// FitSolver requires every box to declare MaxWeight -- there is no
// "unlimited weight" box (see ConcreteBox.cpp's MakeBoxFromJson). A box
// with none configured (e.g. Simple/boxes.json's inactive LRG) would
// otherwise make the whole request throw; every real caller (the portal's
// app/solver.py _to_box_dict included) drops such boxes before sending.
nlohmann::json DropBoxesMissingMaxWeight(const nlohmann::json& boxes) {
  nlohmann::json usable = nlohmann::json::array();
  for (const auto& box : boxes) {
    if (box.contains("MaxWeight") && !box.at("MaxWeight").is_null()) {
      usable.push_back(box);
    }
  }
  return usable;
}

// items.json entries may carry a "Quantity" (how many physical units of
// that catalogue item to pack) -- not part of FitSolver's wire format,
// which packs individually-coded units. Expand each entry into that many
// copies with a unique ItemCode, same as the portal's OrderLine expansion
// (app/solver.py's _expand_lines) does.
nlohmann::json ExpandCatalogueItems(const nlohmann::json& catalogue_items) {
  nlohmann::json expanded = nlohmann::json::array();
  for (const auto& item : catalogue_items) {
    const int quantity = item.value("Quantity", 1);
    const std::string base_code = item.at("ItemCode").get<std::string>();
    for (int unit = 0; unit < quantity; ++unit) {
      nlohmann::json unit_item = item;
      unit_item.erase("Quantity");
      unit_item["ItemCode"] = base_code + "::" + std::to_string(unit);
      expanded.push_back(std::move(unit_item));
    }
  }
  return expanded;
}

// Loads <ThommeaxDir>/<scenario>/{boxes,items}.json and runs them through
// the real Packer.
fitsolver::PackResponse PackScenario(const std::string& scenario) {
  const std::string dir = std::string(FITSOLVER_THOMMEAX_DIR) + "/" + scenario;
  const nlohmann::json boxes = nlohmann::json::parse(ReadFile(dir + "/boxes.json"));
  const nlohmann::json items = nlohmann::json::parse(ReadFile(dir + "/items.json"));

  nlohmann::json request_json;
  request_json["items"] = ExpandCatalogueItems(items);
  request_json["boxes"] = DropBoxesMissingMaxWeight(boxes);

  const fitsolver::PackRequest request = fitsolver::PackRequest::FromJsonString(request_json.dump());
  return fitsolver::Packer().Pack(request.GetItems(), request.GetBoxes());
}

}  // namespace

// Compare physical unit identities as well as totals: an empty response,
// duplicated placement, or silently dropped unit must not pass a benchmark.
void ExpectEveryScenarioUnitPlaced(const std::string& scenario,
                                  const fitsolver::PackResponse& response,
                                  int expected_count) {
  SCOPED_TRACE(scenario);
  const std::string path = std::string(FITSOLVER_THOMMEAX_DIR) + "/" + scenario + "/items.json";
  const auto units = ExpandCatalogueItems(nlohmann::json::parse(ReadFile(path)));
  std::multiset<std::string> expected_codes;
  for (const auto& unit : units) {
    expected_codes.insert(unit.at("ItemCode").get<std::string>());
  }
  ASSERT_EQ(expected_codes.size(), static_cast<std::size_t>(expected_count));
  ASSERT_EQ(std::set<std::string>(expected_codes.begin(), expected_codes.end()).size(),
            expected_codes.size()) << "expanded fixture codes must be unique";

  std::multiset<std::string> placed_codes;
  for (const auto& box : response.boxes) {
    for (const auto& item : box.GetItems()) {
      placed_codes.insert(item.GetItem()->GetItemCode());
    }
  }
  EXPECT_TRUE(response.success);
  EXPECT_EQ(response.GetTotalPlacedCount(), expected_count);
  EXPECT_EQ(placed_codes, expected_codes);
}

// Gravity acts along -Z (Depth is vertical; z = 0 is the box floor). Every
// item off the floor must have another item's top meeting its underside.
// Checked here without the engine's own Support helpers, so a mistake in
// those can't hide itself.
void ExpectNothingFloats(const std::string& scenario, const fitsolver::PackResponse& response) {
  SCOPED_TRACE(scenario);
  std::size_t box_number = 0;
  for (const auto& box : response.boxes) {
    ++box_number;
    const auto& items = box.GetItems().ToVector();
    for (const auto& item : items) {
      if (item.GetZ() <= 1e-6) continue;
      const bool held_up = std::any_of(items.begin(), items.end(), [&](const auto& other) {
        return std::abs(other.GetZ() + other.GetDepth() - item.GetZ()) <= 1e-6 &&
               item.GetX() + item.GetWidth() > other.GetX() + 1e-6 &&
               other.GetX() + other.GetWidth() > item.GetX() + 1e-6 &&
               item.GetY() + item.GetLength() > other.GetY() + 1e-6 &&
               other.GetY() + other.GetLength() > item.GetY() + 1e-6;
      });
      EXPECT_TRUE(held_up) << item.GetItem()->GetItemCode() << " floats at z=" << item.GetZ()
                           << " in box " << box_number << " (" << box.GetBox()->GetReference() << ")";
    }
  }
}

TEST(ThommeaxBenchmark, Simple) {
  // RESULTS.md: 2 boxes used, 3 of 3 units packed.
  const fitsolver::PackResponse response = PackScenario("Simple");
  ExpectEveryScenarioUnitPlaced("Simple", response, 3);
  ExpectNothingFloats("Simple", response);

  EXPECT_TRUE(response.unplaced_items.Empty())
      << response.unplaced_items.Count() << " unit(s) left unplaced";
  EXPECT_LE(response.boxes.Count(), 2u)
      << "used " << response.boxes.Count() << " boxes; reference used 2";
}

TEST(ThommeaxBenchmark, SemiRealistic) {
  // RESULTS.md: 4 boxes used, 115 of 115 units packed.
  const fitsolver::PackResponse response = PackScenario("SemiRealistic");
  ExpectEveryScenarioUnitPlaced("SemiRealistic", response, 115);
  ExpectNothingFloats("SemiRealistic", response);

  EXPECT_TRUE(response.unplaced_items.Empty())
      << response.unplaced_items.Count() << " unit(s) left unplaced";
  EXPECT_LE(response.boxes.Count(), 4u)
      << "used " << response.boxes.Count() << " boxes; reference used 4";
}

TEST(ThommeaxBenchmark, Chaotic) {
  // RESULTS.md: 67 boxes used, 300 of 300 units packed.
  //
  // History, 211 -> 0 unplaced, two confirmed bugs found by diffing
  // behaviour against the actual dvdoug/boxpacker PHP source (not just
  // RESULTS.md's numbers):
  //
  // 1. Packer::EnforceGroupSegregation (was EnforceLinkedGroups). The
  //    engine enforced "every unit of a BoxGroup must end up in ONE box
  //    together"; LinkedItemGroupEnforcer.php proves the real rule is
  //    weaker -- "a box may not mix two DIFFERENT groups", one group's
  //    units are free to spread across many boxes. 211 -> 7 unplaced.
  //
  // 2. DefaultItemSorter/EntropyItemSorter's group-priority direction
  //    was backwards. DefaultItemSorter.php sorts `$groupA <=> $groupB`,
  //    and an ungrouped item's group is `null`, which PHP treats as ""
  //    against a string -- so ungrouped items sort BEFORE any group, not
  //    after. Getting this backwards meant a big ungrouped item (e.g.
  //    ANVIL, almost an exact fit for Standard Carton) was always
  //    evaluated after every grouped item, by which point smaller
  //    grouped items had already fragmented the box it needed. Fixing
  //    the sort direction placed ANVIL correctly: 7 -> 5 unplaced
  //    (GLASS x3, LEAD x2).
  //
  // 77 -> 75: VolumePacker/LayerPacker/OrientatedItemFactory/
  // OrientatedItemSorter/LayerStabiliser were then rewritten as a
  // faithful port of BoxPacker's actual layer-packing engine (previously
  // this project used a simpler single-scan shelf+void-fill approach),
  // to close the density gap -- see those classes' headers.
  //
  // 75 -> 69: the last 5 unplaced had been blamed on two box weights
  // being mistranscribed, but the real bug was bigger and covered every
  // box in all three scenarios. RESULTS.md's `Max Weight` column is a
  // *contents* limit -- its `Gross` column is contents plus the box's
  // own weight, and box 1 sits exactly on it (20.00 - 2.0 = 18.00
  // against an 18) -- while FitSolver's `MaxWeight`, like BoxPacker's
  // `Box::getMaxWeight()` which `LayerPacker` budgets as
  // `getMaxWeight() - getEmptyWeight()`, is *gross*. The fixtures had
  // that column copied straight across, understating every box by its
  // own tare (2kg per Golf Bag Box), which made the reference's own box
  // 1 unachievable here. Converting all three to
  // `MaxWeight = contents + BoxWeight` also left Simple and
  // SemiRealistic matching the reference exactly.
  //
  // 69 -> 65, two changes. RESULTS.md's own header says the reference was
  // produced in two passes (ungrouped items with one group, FOOD on its
  // own). Reproducing that split used to "reach 67" only by spending
  // extra Standard Cartons against the `max 2` limit -- which was a real
  // bug, not a property of the split: WeightRedistributor repacked boxes
  // into whichever type fitted with no regard for supply left, so any
  // result of 12 boxes or fewer (like FOOD's own pass) could overspend a
  // quota. It now tracks remaining supply the way BoxPacker's
  // doVolumeRepack() does. With that fixed, Packer tries the per-group
  // split itself whenever a request has two or more groups, alongside the
  // single pass, and keeps the better result: ungrouped + FOOD first,
  // then CORROSIVE alone, packs everything quota-correctly in 65.
  //
  // For comparison, dvdoug/boxpacker 4.3 itself on this exact input (see
  // benchmarks/boxpacker_comparison/php/run_thommeax.php): its LinkedItem
  // means "whole group in one box", so a single pass leaves 48 units
  // unplaced; the same two-pass split packs everything in 62.
  const fitsolver::PackResponse response = PackScenario("Chaotic");
  ExpectEveryScenarioUnitPlaced("Chaotic", response, 300);
  ExpectNothingFloats("Chaotic", response);

  EXPECT_TRUE(response.unplaced_items.Empty())
      << response.unplaced_items.Count() << " unit(s) left unplaced";
  EXPECT_LE(response.boxes.Count(), 67u)
      << "used " << response.boxes.Count() << " boxes; reference used 67";

  // Box quantity limits are what make the box count meaningful -- a run
  // that quietly overspends them can "beat" the reference on paper.
  std::map<std::string, int> used;
  for (const auto& box : response.boxes) ++used[box.GetBox()->GetReference()];
  EXPECT_LE(used["Golf Bag Box"], 12) << "Golf Bag Box is limited to 12";
  EXPECT_LE(used["Standard Carton"], 2) << "Standard Carton is limited to 2";

  // Likewise segregation: FOOD and CORROSIVE never share a box.
  for (const auto& box : response.boxes) {
    EXPECT_LE(box.GetItems().GetLinkedGroupCounts().size(), 1u)
        << box.GetBox()->GetReference() << " mixes box groups";
  }
}
