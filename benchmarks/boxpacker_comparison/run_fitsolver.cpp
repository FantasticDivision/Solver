// Runs every scenario in scenarios.json through fitsolver::Packer, timing
// each solve individually, and writes fitsolver_results.csv:
//   id,total_units,placed,unplaced,boxes_used,solve_ms
//
// Usage: fitsolver_boxpacker_bench <scenarios.json> <results.csv>

#include <algorithm>
#include <chrono>
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
  if (!in) throw std::runtime_error("Could not open " + path);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

// Mirrors app/solver.py's _expand_lines and thommeax_benchmark_test.cpp's
// ExpandCatalogueItems: one catalogue entry + Quantity becomes that many
// individually-coded units, since FitSolver packs physical items, not
// order lines.
nlohmann::json ExpandItems(const nlohmann::json& item_types) {
  nlohmann::json expanded = nlohmann::json::array();
  for (const auto& item : item_types) {
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

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "usage: " << argv[0] << " <scenarios.json> <results.csv>\n";
    return 2;
  }
  const std::string scenarios_path = argv[1];
  const std::string results_path = argv[2];

  const nlohmann::json scenarios = nlohmann::json::parse(ReadFile(scenarios_path));

  std::ofstream out(results_path);
  out << "id,total_units,placed,unplaced,boxes_used,solve_ms\n";
  out << std::fixed << std::setprecision(4);

  std::vector<double> solve_times_ms;
  solve_times_ms.reserve(scenarios.size());
  double over_budget_count = 0;

  using Clock = std::chrono::steady_clock;
  const auto suite_start = Clock::now();
  for (const auto& scenario : scenarios) {
    const int id = scenario.at("id").get<int>();
    nlohmann::json request_json;
    request_json["items"] = ExpandItems(scenario.at("items"));
    request_json["boxes"] = scenario.at("boxes");

    const fitsolver::PackRequest request =
        fitsolver::PackRequest::FromJsonString(request_json.dump());

    const auto start = Clock::now();
    const fitsolver::PackResponse response =
        fitsolver::Packer().Pack(request.GetItems(), request.GetBoxes());
    const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    solve_times_ms.push_back(ms);
    if (ms > 1000.0) ++over_budget_count;

    const int placed = response.GetTotalPlacedCount();
    const int unplaced = static_cast<int>(response.unplaced_items.Count());

    out << id << ',' << scenario.at("total_units").get<int>() << ',' << placed << ',' << unplaced
        << ',' << response.boxes.Count() << ',' << ms << '\n';
    out.flush();

    // Progress to stderr, not stdout, so results.csv's consumer (compare.py)
    // never has to filter it out; every scenario is printed since a single
    // outlier can dominate the whole suite's wall-clock time.
    std::cerr << "id=" << id << " units=" << scenario.at("total_units").get<int>()
              << " boxes=" << response.boxes.Count() << " unplaced=" << unplaced << " ms=" << ms
              << " elapsed_s="
              << std::chrono::duration<double>(Clock::now() - suite_start).count() << '\n'
              << std::flush;
  }
  out.close();

  std::sort(solve_times_ms.begin(), solve_times_ms.end());
  const double total = [&] {
    double sum = 0.0;
    for (double ms : solve_times_ms) sum += ms;
    return sum;
  }();
  const double average = solve_times_ms.empty() ? 0.0 : total / solve_times_ms.size();
  const double median = solve_times_ms.empty() ? 0.0 : solve_times_ms[solve_times_ms.size() / 2];
  const double worst = solve_times_ms.empty() ? 0.0 : solve_times_ms.back();

  std::cout << std::fixed << std::setprecision(3);
  std::cout << "fitsolver: " << solve_times_ms.size() << " scenarios\n";
  std::cout << "  average solve time: " << average << " ms\n";
  std::cout << "  median solve time:  " << median << " ms\n";
  std::cout << "  worst solve time:   " << worst << " ms\n";
  std::cout << "  scenarios over 1000 ms: " << static_cast<long long>(over_budget_count) << "\n";
  std::cout << "results written to " << results_path << "\n";
  return 0;
}
