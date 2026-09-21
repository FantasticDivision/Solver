// Times the Thommeax scenarios and prints a fingerprint of the exact packing
// (box order, box type, every item's position/dimensions), so an optimisation
// can be checked to change speed only -- identical fingerprint == identical
// output.
//
//   cmake --build build --config Release --target fitsolver_scenario_bench
//   .\build\Release\fitsolver_scenario_bench.exe [iterations] [Scenario...]

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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

nlohmann::json BuildRequest(const std::string& scenario) {
  const std::string dir = std::string(FITSOLVER_THOMMEAX_DIR) + "/" + scenario;
  const nlohmann::json boxes = nlohmann::json::parse(ReadFile(dir + "/boxes.json"));
  const nlohmann::json items = nlohmann::json::parse(ReadFile(dir + "/items.json"));

  nlohmann::json request;
  request["items"] = nlohmann::json::array();
  for (const auto& item : items) {
    const int quantity = item.value("Quantity", 1);
    for (int unit = 0; unit < quantity; ++unit) {
      nlohmann::json copy = item;
      copy.erase("Quantity");
      copy["ItemCode"] = item.at("ItemCode").get<std::string>() + "::" + std::to_string(unit);
      request["items"].push_back(std::move(copy));
    }
  }
  request["boxes"] = nlohmann::json::array();
  for (const auto& box : boxes) {
    if (box.contains("MaxWeight") && !box.at("MaxWeight").is_null()) request["boxes"].push_back(box);
  }
  return request;
}

std::uint64_t Fnv1a(const std::string& text) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char c : text) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::string Fingerprint(const fitsolver::PackResponse& response) {
  std::ostringstream out;
  out << std::setprecision(12);
  for (const auto& box : response.boxes) {
    out << "B:" << box.GetBox()->GetReference() << ";";
    for (const auto& item : box.GetItems()) {
      out << item.GetItem()->GetItemCode() << "@" << item.GetX() << "," << item.GetY() << ","
          << item.GetZ() << "/" << item.GetWidth() << "x" << item.GetLength() << "x"
          << item.GetDepth() << ";";
    }
  }
  out << "U:" << response.unplaced_items.Count();
  std::ostringstream hex;
  hex << std::hex << std::setw(16) << std::setfill('0') << Fnv1a(out.str());
  return hex.str();
}

}  // namespace

int main(int argc, char** argv) {
  const int iterations = argc > 1 ? std::atoi(argv[1]) : 3;
  std::vector<std::string> scenarios;
  for (int i = 2; i < argc; ++i) scenarios.emplace_back(argv[i]);
  if (scenarios.empty()) scenarios = {"Simple", "SemiRealistic", "Chaotic"};

  using Clock = std::chrono::steady_clock;
  for (const auto& scenario : scenarios) {
    const std::string request_json = BuildRequest(scenario).dump();
    double first_ms = 0.0;
    double best_ms = 1e18;
    std::string fingerprint;
    std::size_t box_count = 0;
    std::size_t unplaced = 0;

    for (int run = 0; run < iterations; ++run) {
      const auto start = Clock::now();
      const fitsolver::PackRequest request = fitsolver::PackRequest::FromJsonString(request_json);
      const fitsolver::PackResponse response =
          fitsolver::Packer().Pack(request.GetItems(), request.GetBoxes());
      const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();

      if (run == 0) first_ms = ms;
      best_ms = std::min(best_ms, ms);
      fingerprint = Fingerprint(response);
      box_count = response.boxes.Count();
      unplaced = response.unplaced_items.Count();
    }

    std::cout << std::left << std::setw(15) << scenario << " boxes=" << std::setw(3) << box_count
              << " unplaced=" << unplaced << "  first(cold)=" << std::fixed << std::setprecision(1)
              << first_ms << "ms  best=" << best_ms << "ms  fp=" << fingerprint << "\n";
  }
  return 0;
}
