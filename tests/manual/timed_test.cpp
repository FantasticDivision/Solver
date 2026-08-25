#include <chrono>
#include <cstddef>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>


#include "PackRequest.h"
#include "PackResponse.h"
#include "Packer.h"


//for testing algs time locally:

//cmake -S . -B build

//cmake --build build --config Release --target fitsolver_tests


//time all tests
//$timer = [System.Diagnostics.Stopwatch]::StartNew(); .\build\fitsolver_tests.exe --gtest_brief=1; $timer.Stop(); Write-Host "Elapsed:" $timer.Elapsed.TotalSeconds "seconds"


//time 200 random item packing reqs - RandomRequestsPreserveAccountingBoundsAndWeight - packer_test.cpp - line 598
//$timer = [System.Diagnostics.Stopwatch]::StartNew(); .\build\fitsolver_tests.exe --gtest_filter=PackerPropertyTest.*; $timer.Stop(); Write-Host "Elapsed:" $timer.Elapsed.TotalSeconds "seconds"


//define this only once in ter:
// function Measure-GTest { param([string]$Name) $timer = [System.Diagnostics.Stopwatch]::StartNew(); & .\build\fitsolver_tests.exe "--gtest_filter=$Name" --gtest_brief=1; $exitCode = $LASTEXITCODE; $timer.Stop(); Write-Host "Test:" $Name "| Exit:" $exitCode "| Time:" $timer.Elapsed.TotalMilliseconds "ms" }

//then time and test:
//Measure-GTest "Packer.SingleItemFitsInSingleBox"


//list available test names:
//.\build\fitsolver_tests.exe --gtest_list_tests

//to run this file x times:
//.\build\fitsolver_timed_test.exe x


namespace {

constexpr std::size_t kDefaultIterations = 100;

// Keep the complete request here so additional benchmark cases can be added
// without depending on the process's working directory or fixture files.
const std::string kBasicRequest = R"json(
{
  "items": [
    {"ItemCode":"ITEM-A", "Width":100, "Length":200, "Depth":50, "Weight":1.0},
    {"ItemCode":"ITEM-B", "Width":300, "Length":150, "Depth":75, "Weight":2.8},
    {"ItemCode":"ITEM-C", "Width":80, "Length":80, "Depth":120, "Weight":0.82}
    
  ],
  "boxes": [
    {"Reference":"SMALL", "Width":150, "Length":150, "Depth":150,
     "MaxWeight":8.5, "BoxWeight":0.5, "Active":true},
    {"Reference":"MEDIUM", "Width":400, "Length":400, "Depth":400,
     "MaxWeight":15.2, "BoxWeight":0.75, "Active":true}
  ]
}
)json";

const std::string kMediumRequest = R"json(
{
  "items": [
    {"ItemCode":"ITEM-A", "Width":100, "Length":200, "Depth":50, "Weight":1.0},
    {"ItemCode":"ITEM-B", "Width":300, "Length":150, "Depth":75, "Weight":2.8},
    {"ItemCode":"ITEM-C", "Width":80, "Length":80, "Depth":120, "Weight":0.82},
    {"ItemCode":"ITEM-D", "Width":80, "Length":80, "Depth":120, "Weight":0.82},
    {"ItemCode":"ITEM-E", "Width":80, "Length":80, "Depth":120, "Weight":0.82},
    {"ItemCode":"ITEM-F", "Width":80, "Length":80, "Depth":120, "Weight":0.82}
  ],
  "boxes": [
    {"Reference":"SMALL", "Width":150, "Length":150, "Depth":150,
     "MaxWeight":8.5, "BoxWeight":0.5, "Active":true},
    {"Reference":"MEDIUM", "Width":400, "Length":400, "Depth":400,
     "MaxWeight":15.2, "BoxWeight":0.75, "Active":true},
    {"Reference":"SMALL2", "Width":150, "Length":150, "Depth":150,
     "MaxWeight":8.5, "BoxWeight":0.5, "Active":true}
  ]
}
)json";

const std::string kAdvancedRequest = R"json(
{
  "items": [
    {"ItemCode":"ITEM-A", "Width":100, "Length":200, "Depth":50, "Weight":1.0},
    {"ItemCode":"ITEM-B", "Width":300, "Length":150, "Depth":75, "Weight":2.8},
    {"ItemCode":"ITEM-C", "Width":80, "Length":80, "Depth":120, "Weight":0.82},
    {"ItemCode":"ITEM-D", "Width":80, "Length":80, "Depth":120, "Weight":0.82},
    {"ItemCode":"ITEM-E", "Width":80, "Length":80, "Depth":120, "Weight":0.82},
    {"ItemCode":"ITEM-F", "Width":80, "Length":80, "Depth":120, "Weight":0.82},
    {"ItemCode":"ITEM-G", "Width":100, "Length":200, "Depth":50, "Weight":1.0},
    {"ItemCode":"ITEM-H", "Width":300, "Length":150, "Depth":75, "Weight":2.8},
    {"ItemCode":"ITEM-I", "Width":80, "Length":80, "Depth":120, "Weight":0.82},
    {"ItemCode":"ITEM-J", "Width":80, "Length":80, "Depth":120, "Weight":0.82},
    {"ItemCode":"ITEM-K", "Width":80, "Length":80, "Depth":120, "Weight":0.82},
    {"ItemCode":"ITEM-L", "Width":80, "Length":80, "Depth":120, "Weight":0.82}
  ],
  "boxes": [
    {"Reference":"SMALL", "Width":150, "Length":150, "Depth":150,
     "MaxWeight":8.5, "BoxWeight":0.5, "Active":true},
    {"Reference":"MEDIUM", "Width":400, "Length":400, "Depth":400,
     "MaxWeight":15.2, "BoxWeight":0.75, "Active":true},
    {"Reference":"SMALL2", "Width":150, "Length":150, "Depth":150,
     "MaxWeight":8.5, "BoxWeight":0.5, "Active":true},
    {"Reference":"SMALL3", "Width":150, "Length":150, "Depth":150,
     "MaxWeight":8.5, "BoxWeight":0.5, "Active":true},
    {"Reference":"MEDIUM", "Width":400, "Length":400, "Depth":400,
     "MaxWeight":15.2, "BoxWeight":0.75, "Active":true},
    {"Reference":"SMALL2", "Width":150, "Length":150, "Depth":150,
     "MaxWeight":8.5, "BoxWeight":0.5, "Active":true}
  ]
}
)json";

std::size_t ReadIterations(int argc, char** argv) {
  if (argc < 2) return kDefaultIterations;

  const std::size_t iterations = std::stoull(argv[1]);
  if (iterations == 0) throw std::invalid_argument("iterations must be greater than zero");
  return iterations;
}

fitsolver::PackResponse RunBasicCase() {
  const fitsolver::PackRequest request =
      fitsolver::PackRequest::FromJsonString(kBasicRequest);
  return fitsolver::Packer().Pack(request);
}

bool HasExpectedResult(const fitsolver::PackResponse& response,
                       std::size_t expected_placed,
                       const std::string& phase) {
  if (response.success &&
      response.GetTotalPlacedCount() == static_cast<int>(expected_placed) &&
      response.GetTotalUnplacedCount() == 0) {
    return true;
  }

  std::cerr << "Basic case produced an unexpected result during " << phase << ":\n"
            << "  success:  " << std::boolalpha << response.success << '\n'
            << "  placed:   " << response.GetTotalPlacedCount()
            << " (expected " << expected_placed << ")\n"
            << "  unplaced: " << response.GetTotalUnplacedCount() << "\n\n"
            << response.Dump(2) << '\n';
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const std::size_t iterations = ReadIterations(argc, argv);
    const std::size_t expected_placed =
        fitsolver::PackRequest::FromJsonString(kBasicRequest).GetItems().Count();

    // Warm up allocator/code paths before starting the clock.
    const fitsolver::PackResponse warmup = RunBasicCase();
    if (!HasExpectedResult(warmup, expected_placed, "warm-up")) return 1;

    std::size_t output_checksum = 0;
    const auto start = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < iterations; ++i) {
      const fitsolver::PackResponse response = RunBasicCase();
      if (!HasExpectedResult(response, expected_placed,
                             "timed iteration " + std::to_string(i)))
        return 1;

      // Serializing makes this an end-to-end test and consuming the size keeps
      // the work observable to an optimizing compiler.
      output_checksum += response.Dump(-1).size();
    }

    const auto finish = std::chrono::steady_clock::now();
    const double total_ms =
        std::chrono::duration<double, std::milli>(finish - start).count();
    const double average_ms = total_ms / static_cast<double>(iterations);
    const double runs_per_second = 1000.0 / average_ms;

    std::cout << "Basic end-to-end packing benchmark\n"
              << "  iterations:     " << iterations << '\n'
              << std::fixed << std::setprecision(3)
              << "  total:          " << total_ms << " ms\n"
              << "  average:        " << average_ms << " ms/run\n"
              << "  throughput:     " << runs_per_second << " runs/s\n"
              << "  output checksum: " << output_checksum << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "fitsolver_timed_test: " << error.what() << '\n';
    return 1;
  }
}
