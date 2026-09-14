// Runs the Thommeax benchmark scenarios (tests/ThommeaxMinimumTests/) through
// the real Packer and checks we place every unit using no more boxes than
// the reference (dvdoug/boxpacker) result recorded in each scenario's
// RESULTS.md. See tests/ThommeaxMinimumTests/README.md for the data format.

#include <gtest/gtest.h>

#include <fstream>
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

TEST(ThommeaxBenchmark, Simple) {
  // RESULTS.md: 2 boxes used, 3 of 3 units packed.
  const fitsolver::PackResponse response = PackScenario("Simple");

  EXPECT_TRUE(response.unplaced_items.Empty())
      << response.unplaced_items.Count() << " unit(s) left unplaced";
  EXPECT_LE(response.boxes.Count(), 2u)
      << "used " << response.boxes.Count() << " boxes; reference used 2";
}

TEST(ThommeaxBenchmark, SemiRealistic) {
  // RESULTS.md: 4 boxes used, 115 of 115 units packed.
  const fitsolver::PackResponse response = PackScenario("SemiRealistic");

  EXPECT_TRUE(response.unplaced_items.Empty())
      << response.unplaced_items.Count() << " unit(s) left unplaced";
  EXPECT_LE(response.boxes.Count(), 4u)
      << "used " << response.boxes.Count() << " boxes; reference used 4";
}

TEST(ThommeaxBenchmark, Chaotic) {
  // RESULTS.md: 67 boxes used, 300 of 300 units packed.
  //
  // History: originally 211/300 unplaced. Packer::EnforceGroupSegregation
  // fixed the big one -- the engine was enforcing "every unit of a
  // BoxGroup must end up in ONE box together", but the reference result
  // proves the real rule is weaker ("a box may not mix two DIFFERENT
  // groups"; a single group's units are free to spread across many
  // boxes). Getting that right dropped this to 7/300 unplaced, using
  // fewer boxes than the reference (64 vs 67).
  //
  // The remaining 7 (GLASS x3, LEAD x2, ANVIL x2) split into two
  // different, NOT-yet-fixed causes:
  //  - GLASS/LEAD: blocked by ConcreteBox::GetWeightCapacityForItems()
  //    subtracting BoxWeight from MaxWeight (capacity = MaxWeight -
  //    BoxWeight). The reference's own numbers (e.g. its Picture
  //    Box+LEAD result reports 4.20kg gross against a 4kg MaxWeight)
  //    imply MaxWeight is meant as a contents-only cap, BoxWeight purely
  //    informational for gross-weight reporting. Deliberately NOT
  //    changed here: ConcreteBox, UnlimitedAndExhaustedWeightCapacities
  //    and PackerRegression, ExactFitOnEveryAxisAndWeightBoundarySucceeds
  //    all explicitly assert today's subtracted-capacity behaviour, and
  //    every real box in the portal's catalogue depends on whichever
  //    meaning wins -- a product call, not an engine bug fix.
  //  - ANVIL: a scarcity problem, not infeasibility -- it fits Standard
  //    Carton's weight/size fine, but by the time the round-based greedy
  //    loop would place it there, both MaximumBoxes=2 Standard Cartons
  //    are already spent on rounds that scored higher (e.g. 46x BEAD).
  //    Needs the box-selection loop to reserve limited-supply capacity
  //    for items with few/no alternatives -- real algorithm work, not
  //    attempted here.
  const fitsolver::PackResponse response = PackScenario("Chaotic");

  EXPECT_TRUE(response.unplaced_items.Empty())
      << response.unplaced_items.Count() << " unit(s) left unplaced";
  EXPECT_LE(response.boxes.Count(), 67u)
      << "used " << response.boxes.Count() << " boxes; reference used 67";
}
