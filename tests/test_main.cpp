#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

// Opt-in timing: normal CTest runs remain correctness-only. Keep the warm-up
// and all measured iterations in one process, excluding process startup.
constexpr int kSamples = 5;
constexpr double kBudgetMs = 2.0;

struct Measurements {
  std::vector<double> milliseconds;
  bool correct = true;
};

class TimingListener : public testing::EmptyTestEventListener {
 public:
  void OnTestIterationStart(const testing::UnitTest&, int iteration) override {
    iteration_ = iteration;
  }

  void OnTestStart(const testing::TestInfo&) override { start_ = Clock::now(); }

  void OnTestEnd(const testing::TestInfo& test) override {
    const double elapsed =
        std::chrono::duration<double, std::milli>(Clock::now() - start_).count();
    auto& entry = results_[std::string(test.test_suite_name()) + "." + test.name()];
    entry.correct = entry.correct && test.result()->Passed() && !test.result()->Skipped();
    if (iteration_ > 0) entry.milliseconds.push_back(elapsed);
  }

  bool WriteReport(const std::string& path) const {
    std::ofstream report(path);
    if (!report) {
      std::cerr << "Cannot write timing report: " << path << '\n';
      return false;
    }
    report << "## C++ test timings\n\n"
           << "Budget: **strictly under " << kBudgetMs << " ms per test**. "
           << "One full-suite warm-up, then " << kSamples << " measured runs; "
           << "the median determines the timing result.\n\n"
           << "Times include test setup, fixture loading, assertions and teardown; "
           << "they exclude process startup and suite-level setup. "
           << "These are complete-test timings, not per-solve benchmarks. "
           << "Skipped or failing tests cannot pass the timing check.\n\n"
           << "| Test | Median (ms) | Min (ms) | Max (ms) | Samples | Result |\n"
           << "| --- | ---: | ---: | ---: | ---: | --- |\n";
    bool passed = !results_.empty();
    std::size_t failures = 0;
    report << std::fixed << std::setprecision(6);
    for (const auto& [name, entry] : results_) {
      auto samples = entry.milliseconds;
      std::sort(samples.begin(), samples.end());
      if (samples.empty()) {
        report << "| " << name << " | — | — | — | 0 | FAIL: no samples |\n";
        passed = false;
        ++failures;
        continue;
      }
      const double median = samples[samples.size() / 2];
      const bool ok = entry.correct && samples.size() == kSamples && median < kBudgetMs;
      passed = passed && ok;
      failures += !ok;
      report << "| " << name << " | " << median << " | " << samples.front()
             << " | " << samples.back() << " | " << samples.size() << " | "
             << (ok ? "PASS" : !entry.correct ? "FAIL: correctness/skipped"
                 : samples.size() != kSamples ? "FAIL: missing samples" : "FAIL: budget")
             << " |\n";
    }
    report << "\n**" << (passed ? "PASS" : "FAIL") << "**: " << failures
           << " of " << results_.size() << " tests failed the timing check.\n";
    if (results_.empty()) report << "No tests ran; this is a failure.\n";
    report.flush();
    std::cout << "Timing report: " << path << " (" << failures << "/"
              << results_.size() << " failed)\n";
    return passed && report.good();
  }

 private:
  using Clock = std::chrono::steady_clock;
  Clock::time_point start_;
  int iteration_ = 0;
  std::map<std::string, Measurements> results_;
};

}  // namespace

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  const char* report_path = std::getenv("FITSOLVER_TIMING_REPORT");
  if (!report_path || !*report_path) return RUN_ALL_TESTS();

  // GoogleTest owns the listener until shutdown.
  auto* timings = new TimingListener;
  testing::UnitTest::GetInstance()->listeners().Append(timings);
  GTEST_FLAG_SET(repeat, kSamples + 1);
  const int correctness_result = RUN_ALL_TESTS();
  const bool timing_passed = timings->WriteReport(report_path);
  return correctness_result == 0 && timing_passed ? 0 : 1;
}
