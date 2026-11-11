#ifndef FLUFFOS_TESTS_BENCH_SUPPORT_H
#define FLUFFOS_TESTS_BENCH_SUPPORT_H

// Shared scaffolding for the standalone benchmark binaries (lpc_vm_bench,
// object_store_bench, owner_runtime_bench). Each used to carry its own copy of
// the report types, timing helpers and JSON writer; the copies drifted (one had
// a write-failure check, the others silently ignored a failed write).

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace bench {

using Clock = std::chrono::steady_clock;

struct Metric {
  std::string name;
  long long value{0};
};

struct StringMetric {
  std::string name;
  std::string value;
};

struct Report {
  std::vector<Metric> metrics;
  std::vector<StringMetric> strings;

  void add(const std::string& name, long long value) {
    metrics.push_back({name, value});
  }
  void add_string(const std::string& name, std::string value) {
    strings.push_back({name, std::move(value)});
  }
};

inline long long elapsed_ns(Clock::time_point start) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
}

inline long long percentile(std::vector<long long> samples, double rank) {
  if (samples.empty()) {
    return 0;
  }
  std::sort(samples.begin(), samples.end());
  const auto index = static_cast<size_t>((samples.size() - 1) * rank);
  return samples[index];
}

inline void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

inline void write_json_report(const std::string& path, const std::string& json) {
  if (path.empty()) {
    return;
  }
  auto output_path = std::filesystem::path(path);
  if (output_path.has_parent_path()) {
    std::filesystem::create_directories(output_path.parent_path());
  }
  std::ofstream output(output_path);
  if (!output.is_open()) {
    throw std::runtime_error("failed to open benchmark json output: " + path);
  }
  output << json;
  output.close();
  if (!output) {
    throw std::runtime_error("failed to write benchmark json output: " + path);
  }
}

}  // namespace bench

#endif  // FLUFFOS_TESTS_BENCH_SUPPORT_H
