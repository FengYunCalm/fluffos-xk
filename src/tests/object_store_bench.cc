#include "base/package_api.h"
#include "test_mudlib.h"

#include "backend.h"
#include "mainlib.h"
#include "vm/internal/base/object.h"
#include "vm/internal/base/scoped_current_object_as_master.h"
#include "vm/internal/simulate.h"
#include "vm/object_handle.h"
#include "vm/owner.h"
#include "vm/vm.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <unistd.h>

#ifdef __linux__
#include <pthread.h>

namespace {
struct ProbeCounters {
  bool enabled{false};
  pthread_rwlock_t* held_lock{nullptr};
  std::chrono::steady_clock::time_point lock_start;
  long long read_locks{0};
  long long unlocks{0};
  long long lock_ns{0};
  long long allocations{0};
};
thread_local ProbeCounters probe;
}  // namespace

extern "C" int __real_pthread_rwlock_rdlock(pthread_rwlock_t* lock);
extern "C" int __real_pthread_rwlock_unlock(pthread_rwlock_t* lock);
extern "C" void* __real__Znwm(size_t size);
extern "C" void __gcov_reset() __attribute__((weak));
extern "C" void __gcov_dump() __attribute__((weak));

extern "C" int __wrap_pthread_rwlock_rdlock(pthread_rwlock_t* lock) {
  const int result = __real_pthread_rwlock_rdlock(lock);
  if (probe.enabled && result == 0) {
    probe.read_locks++;
    if (!probe.held_lock) {
      probe.held_lock = lock;
      probe.lock_start = std::chrono::steady_clock::now();
    }
  }
  return result;
}

extern "C" int __wrap_pthread_rwlock_unlock(pthread_rwlock_t* lock) {
  if (probe.enabled && probe.held_lock == lock) {
    probe.lock_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::steady_clock::now() - probe.lock_start)
                         .count();
    probe.held_lock = nullptr;
    probe.unlocks++;
  }
  return __real_pthread_rwlock_unlock(lock);
}

extern "C" void* __wrap__Znwm(size_t size) {
  if (probe.enabled) {
    probe.allocations++;
  }
  return __real__Znwm(size);
}
#endif

namespace {
using Clock = std::chrono::steady_clock;
constexpr const char *kObjectStoreBenchSchemaV1 = "object_store_bench_v1";

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

  void add(const std::string &name, long long value) { metrics.push_back({name, value}); }
  void add_string(const std::string &name, std::string value) { strings.push_back({name, std::move(value)}); }
};

long long elapsed_ns(Clock::time_point start) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
}

long long percentile(std::vector<long long> samples, double rank) {
  if (samples.empty()) {
    return 0;
  }
  std::sort(samples.begin(), samples.end());
  const auto index = static_cast<size_t>((samples.size() - 1) * rank);
  return samples[index];
}

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

object_t *clone_object_for_bench(const char *path) {
  error_context_t econ{};
  object_t *object = nullptr;
  ScopedCurrentObjectAsMaster master_scope(ScopedCurrentObjectAsMaster::conditional_t{});
  save_context(&econ);
  try {
    object = clone_object(path, 0);
    pop_context(&econ);
  } catch (...) {
    restore_context(&econ);
    throw std::runtime_error(std::string("clone_object failed for ") + path);
  }
  return object;
}

void destruct_object_for_bench(object_t *object) {
  if (object == nullptr || (object->flags & O_DESTRUCTED)) {
    return;
  }
  error_context_t econ{};
  ScopedCurrentObjectAsMaster master_scope(ScopedCurrentObjectAsMaster::conditional_t{});
  save_context(&econ);
  try {
    destruct_object(object);
    pop_context(&econ);
  } catch (...) {
    restore_context(&econ);
    throw std::runtime_error(std::string("destruct_object failed for ") + object->obname);
  }
}

std::string json_escape(const std::string &value) {
  std::ostringstream out;
  for (char ch : value) {
    switch (ch) {
      case '\\':
        out << "\\\\";
        break;
      case '"':
        out << "\\\"";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        out << ch;
        break;
    }
  }
  return out.str();
}

std::string report_json(const Report &report) {
  std::ostringstream json;
  json << "{\n";
  json << "  \"schema\": \"" << kObjectStoreBenchSchemaV1 << "\",\n";
  json << "  \"runtime\": {\n";
  for (size_t i = 0; i < report.strings.size(); i++) {
    json << "    \"" << json_escape(report.strings[i].name) << "\": \""
         << json_escape(report.strings[i].value) << "\"";
    json << (i + 1 == report.strings.size() ? "\n" : ",\n");
  }
  json << "  },\n";
  json << "  \"metrics\": {\n";
  for (size_t i = 0; i < report.metrics.size(); i++) {
    json << "    \"" << json_escape(report.metrics[i].name) << "\": " << report.metrics[i].value;
    json << (i + 1 == report.metrics.size() ? "\n" : ",\n");
  }
  json << "  }\n";
  json << "}\n";
  return json.str();
}

void write_json_report(const std::string& path, const std::string& json) {
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

void run_object_handle_resolve_bench(Report &report) {
  constexpr long object_count = 32;
  constexpr long iterations = 256;
  std::vector<object_t *> objects;
  std::vector<VMObjectHandle> handles;
  std::vector<long long> resolve_samples;
  std::vector<long long> owner_id_lookup_samples;
  std::vector<long long> owner_path_lookup_samples;
  long long owner_local_fast_path = 0;
  long long global_fallback_used = 0;

  objects.reserve(object_count);
  handles.reserve(object_count);
  for (long i = 0; i < object_count; i++) {
    auto *object = clone_object_for_bench("single/void");
    require(object != nullptr, "failed to clone object store bench probe");
    objects.push_back(object);
    vm_owner_set_id(object, (std::string("owner/bench/") + std::to_string(i % 4)).c_str());
    handles.push_back(vm_object_handle_with_intent(object, "object_store_bench"));
  }

  auto start = Clock::now();
  for (long pass = 0; pass < iterations; pass++) {
    for (const auto &handle : handles) {
      auto resolve_start = Clock::now();
      auto result = vm_object_handle_resolve_status(handle);
      resolve_samples.push_back(elapsed_ns(resolve_start));
      require(result.status == VMObjectHandleResolveStatus::kCurrent,
              std::string("object handle resolve failed: ") + vm_object_handle_resolve_status_name(result.status));
      owner_local_fast_path += result.owner_local_fast_path_used ? 1 : 0;
      global_fallback_used += result.resolved_via_global_index ? 1 : 0;

      auto id_start = Clock::now();
      require(vm_object_store_owner_resolve(handle.owner_id.c_str(), handle.object_id) == result.object,
              "owner id lookup failed");
      owner_id_lookup_samples.push_back(elapsed_ns(id_start));

      auto path_start = Clock::now();
      require(vm_object_store_owner_path_resolve(handle.owner_id.c_str(), handle.object_path.c_str()) == result.object,
              "owner path lookup failed");
      owner_path_lookup_samples.push_back(elapsed_ns(path_start));
    }
  }

  report.add("object_count", object_count);
  report.add("resolve_iterations", object_count * iterations);
  report.add("resolve_elapsed_ns", elapsed_ns(start));
  report.add("owner_local_fast_path_count", owner_local_fast_path);
  report.add("object_resolve_global_fallback_count", global_fallback_used);
  report.add("resolve_latency_p50_ns", percentile(resolve_samples, 0.50));
  report.add("resolve_latency_p95_ns", percentile(resolve_samples, 0.95));
  report.add("resolve_latency_p99_ns", percentile(resolve_samples, 0.99));
  report.add("owner_id_lookup_latency_p50_ns", percentile(owner_id_lookup_samples, 0.50));
  report.add("owner_id_lookup_latency_p95_ns", percentile(owner_id_lookup_samples, 0.95));
  report.add("owner_path_lookup_latency_p50_ns", percentile(owner_path_lookup_samples, 0.50));
  report.add("owner_path_lookup_latency_p95_ns", percentile(owner_path_lookup_samples, 0.95));

  for (auto *object : objects) {
    destruct_object_for_bench(object);
  }
}

void add_samples(Report &report, const std::string &prefix, const std::vector<long long> &samples) {
  report.add(prefix + "_count", static_cast<long long>(samples.size()));
  report.add(prefix + "_p50_ns", percentile(samples, 0.50));
  report.add(prefix + "_p95_ns", percentile(samples, 0.95));
  report.add(prefix + "_p99_ns", percentile(samples, 0.99));
}

void run_clone_destruct_lifecycle_bench(Report &report) {
  constexpr long iterations = 512;
  std::vector<long long> clone_samples;
  std::vector<long long> destruct_samples;
  clone_samples.reserve(iterations);
  destruct_samples.reserve(iterations);

  vm_object_lifecycle_perf_reset();
  vm_object_lifecycle_perf_set_enabled(true);
  auto total_start = Clock::now();
  for (long i = 0; i < iterations; i++) {
    auto clone_start = Clock::now();
    auto *object = clone_object_for_bench("single/void");
    clone_samples.push_back(elapsed_ns(clone_start));
    require(object != nullptr, "clone/destruct lifecycle bench clone failed");

    auto destruct_start = Clock::now();
    destruct_object_for_bench(object);
    destruct_samples.push_back(elapsed_ns(destruct_start));
  }
  auto total_elapsed = elapsed_ns(total_start);
  vm_object_lifecycle_perf_set_enabled(false);
  auto snapshot = vm_object_lifecycle_perf_snapshot();
  auto backlog_after_loop = vm_destructed_object_backlog_size();
  auto cleanup_total_before = vm_destructed_object_cleanup_total();
  auto cleanup_removed = remove_destructed_objects_bounded(backlog_after_loop);
  auto backlog_after_cleanup = vm_destructed_object_backlog_size();

  report.add("clone_destruct_iterations", iterations);
  report.add("clone_destruct_elapsed_ns", total_elapsed);
  report.add("destructed_backlog_after_clone_destruct", static_cast<long long>(backlog_after_loop));
  report.add("destructed_cleanup_removed_after_clone_destruct", static_cast<long long>(cleanup_removed));
  report.add("destructed_backlog_after_cleanup", static_cast<long long>(backlog_after_cleanup));
  report.add("destructed_cleanup_total_delta",
             static_cast<long long>(vm_destructed_object_cleanup_total() - cleanup_total_before));
  add_samples(report, "clone_latency", clone_samples);
  add_samples(report, "destruct_latency", destruct_samples);
  for (size_t i = 0; i < VM_OBJECT_LIFECYCLE_PERF_STAGE_COUNT; i++) {
    std::string stage = vm_object_lifecycle_perf_stage_name(i);
    if (stage.empty()) {
      continue;
    }
    auto count = static_cast<long long>(snapshot.counts[i]);
    auto total_ns = static_cast<long long>(snapshot.total_ns[i]);
    report.add("lifecycle_stage_" + stage + "_count", count);
    report.add("lifecycle_stage_" + stage + "_total_ns", total_ns);
    report.add("lifecycle_stage_" + stage + "_avg_ns", count > 0 ? total_ns / count : 0);
  }
}

int parse_probe_count(const char* text, const char* option) {
  int value = 0;
  const char* end = text + std::strlen(text);
  const auto parsed = std::from_chars(text, end, value);
  if (parsed.ec != std::errc{} || parsed.ptr != end || value <= 0) {
    throw std::runtime_error(std::string("invalid ") + option);
  }
  return value;
}

void run_scaled_store_probe(Report& report, int count, const std::string& operation, int rounds,
                            bool reset_coverage) {
#ifdef __linux__
  require(count == 1000 || count == 10000 || count == 50000,
          "--objects must be 1000, 10000 or 50000");
  require(operation == "current" || operation == "missing" || operation == "stale" ||
              operation == "path" || operation == "status",
          "invalid --operation");
  require(!reset_coverage || (rounds == 1 && __gcov_reset && __gcov_dump),
          "coverage requires one round and linked GCC reset/dump functions");
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;
  CONFIG_INT(__MAX_ARRAY_SIZE__) = std::max(CONFIG_INT(__MAX_ARRAY_SIZE__), count);
  std::vector<object_t*> objects;
  std::vector<VMObjectHandle> handles;
  objects.reserve(count);
  handles.reserve(count);
  // Fixture lifecycle calls are separate evaluations, not one bulk LPC command.
  for (int i = 0; i < count; ++i) {
    set_eval(max_eval_cost);
    auto* object = clone_object_for_bench("single/void");
    require(object != nullptr, "failed to clone probe object");
    objects.push_back(object);
    vm_owner_set_id(object, ("owner/probe/" + std::to_string(i % 4)).c_str());
    handles.push_back(vm_object_handle(object));
  }
  for (int i = 0; i < count; i += 10) {
    set_eval(max_eval_cost);
    destruct_object_for_bench(objects[i]);
  }
  auto handle = handles[1];
  if (operation == "missing") {
    handle.object_id = UINT64_MAX;
    handle.object_path = "missing/store_probe";
  } else if (operation == "stale") {
    handle.owner_epoch++;
    handle.snapshot_version = handle.owner_epoch;
  }

  // Timings use one verified warmup; coverage measures exactly one cold call.
  for (int round = reset_coverage ? 0 : -1; round < rounds; ++round) {
    VMObjectHandleResolveResult result;
    object_t* resolved = nullptr;
    mapping_t* status = nullptr;
    probe = {};
    if (reset_coverage) {
      __gcov_reset();
    }
    const auto start = Clock::now();
    probe.enabled = true;
    if (operation == "status") {
      status = vm_object_store_status();
    } else if (operation == "path") {
      resolved =
          vm_object_store_owner_path_resolve(handle.owner_id.c_str(), handle.object_path.c_str());
    } else {
      result = vm_object_handle_resolve_status(handle);
    }
    probe.enabled = false;
    const auto duration = elapsed_ns(start);
    if (reset_coverage) {
      __gcov_dump();
    }
    require(probe.read_locks == 1 && probe.unlocks == 1 && !probe.held_lock,
            "probe requires exactly one completed read-lock scope");
    if (operation == "status") {
      require(status != nullptr, "missing store status");
      free_mapping(status);
    } else if (operation == "path") {
      require(resolved == objects[1], "path resolved the wrong object");
    } else {
      auto expected = VMObjectHandleResolveStatus::kCurrent;
      if (operation == "missing") {
        expected = VMObjectHandleResolveStatus::kObjectNotFound;
      } else if (operation == "stale") {
        expected = VMObjectHandleResolveStatus::kOwnerEpochMismatch;
      }
      require(result.status == expected, "unexpected handle status");
      if (operation == "current") {
        require(result.object == objects[1] && result.owner_local_fast_path_used,
                "current handle bypassed its owner-local fast path");
        require(probe.allocations == 0, "current handle added a C++ allocation");
      }
    }
    if (round < 0) {
      continue;
    }
    const auto suffix = "_" + std::to_string(round);
    report.add("elapsed_ns" + suffix, duration);
    report.add("read_lock_ns" + suffix, probe.lock_ns);
    report.add("read_lock_count" + suffix, probe.read_locks);
    report.add("cpp_allocations" + suffix, probe.allocations);
  }
  auto* status = vm_object_store_status();
  for (const char* key : {"global_record_total", "global_live_record_total",
                          "global_destructed_record_total", "owner_shards"}) {
    const auto* value = find_string_in_mapping(status, key);
    require(value && value->type == T_NUMBER, "missing store count");
    report.add(key, value->u.number);
  }
  require(find_string_in_mapping(status, "owner_local_global_bridge_consistent")->u.number == 1,
          "probe fixture has inconsistent records");
  free_mapping(status);
  report.add("object_count", count);
  report.add("probe_rounds", rounds);
  report.add_string("probe_operation", operation);
  for (auto* object : objects) {
    set_eval(max_eval_cost);
    destruct_object_for_bench(object);
  }
  remove_destructed_objects();
#else
  throw std::runtime_error("scaled store probe requires Linux ELF link wrapping");
#endif
}

void print_text_report(const Report &report, const std::string &json_path) {
  std::cout << "object_store_bench: schema=" << kObjectStoreBenchSchemaV1 << "\n";
  for (const auto &metric : report.metrics) {
    std::cout << metric.name << "=" << metric.value << "\n";
  }
  if (!json_path.empty()) {
    std::cout << "json_report=" << json_path << "\n";
  }
}
}  // namespace

int main(int argc, char** argv) {
  std::string json_path;
  int object_count = 0;
  int probe_rounds = 3;
  std::string operation = "status";
  bool reset_coverage = false;
  bool probe_options_used = false;
  try {
    for (int i = 1; i < argc; i++) {
      std::string arg = argv[i];
      if (arg == "--objects" || arg == "--rounds" || arg == "--operation" ||
          arg == "--reset-coverage") {
        probe_options_used = true;
      }
      if (arg == "--json" && i + 1 < argc) {
        json_path = argv[++i];
      } else if (arg == "--objects" && i + 1 < argc) {
        object_count = parse_probe_count(argv[++i], "--objects");
      } else if (arg == "--rounds" && i + 1 < argc) {
        probe_rounds = parse_probe_count(argv[++i], "--rounds");
      } else if (arg == "--operation" && i + 1 < argc) {
        operation = argv[++i];
      } else if (arg == "--reset-coverage") {
        reset_coverage = true;
      } else if (arg == "--help") {
        std::cout << "usage: object_store_bench [--json path] [--objects 1000|10000|50000 "
                     "--operation current|missing|stale|path|status --rounds N --reset-coverage]\n";
        return 0;
      } else {
        std::cerr << "unknown argument: " << arg << "\n";
        return 2;
      }
    }

    if (!json_path.empty()) {
      json_path = std::filesystem::absolute(json_path).string();
    }
    const auto mudlib = fluffos_test_mudlib::root();
    if (fluffos_test_mudlib::change_directory(mudlib) != 0) {
      std::ostringstream error;
      error << "failed to chdir to " << mudlib.string() << ": " << strerror(errno);
      throw std::runtime_error(error.str());
    }
    init_main("etc/config.test");
    vm_start();

    Report report;
    report.add_string("mode", "diagnostic");
    report.add_string("object_handle_model", kVMObjectHandleCapabilityModelV1);
    report.add_string("owner_fast_path", "owner_shard_resolve_without_global_fallback");

    if (object_count != 0) {
      run_scaled_store_probe(report, object_count, operation, probe_rounds, reset_coverage);
    } else {
      require(!probe_options_used, "probe options require --objects");
      run_object_handle_resolve_bench(report);
      run_clone_destruct_lifecycle_bench(report);
    }

    auto json = report_json(report);
    write_json_report(json_path, json);
    print_text_report(report, json_path);
    std::cout << json;
    std::cout.flush();
    if (!std::cout) {
      throw std::runtime_error("failed to write benchmark stdout");
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "object_store_bench failed: " << error.what() << "\n";
    return 1;
  }
}
