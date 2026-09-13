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
  // KNOWN GAP, not a regression: as of this test's addition our engine
  // leaves 211 of 300 units unplaced here (confirmed identical on both
  // the pre- and post-gravity-constraint VolumePacker, so the greedy
  // heuristic itself -- not the support check -- is the limiting factor
  // on a dataset this large and mixed, especially with the MaximumBoxes
  // caps on Standard Carton/Golf Bag Box). Left failing deliberately so
  // this benchmark stays visible until the packing algorithm improves;
  // see the repo's "Configurable objective" / "exhaustive packing"
  // follow-up ideas.
  const fitsolver::PackResponse response = PackScenario("Chaotic");

  EXPECT_TRUE(response.unplaced_items.Empty())
      << response.unplaced_items.Count() << " unit(s) left unplaced";
  EXPECT_LE(response.boxes.Count(), 67u)
      << "used " << response.boxes.Count() << " boxes; reference used 67";
}
