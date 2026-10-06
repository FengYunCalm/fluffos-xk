// #1247 C-S2: compile throughput A/B benchmark.
//
// Compiles a fixed representative LPC corpus through the real
// compile_file() path (the same entry the loader uses) and reports
// throughput, round latencies, peak RSS and -- when the compile arena
// exists (post C-S1) -- arena chunk/retained statistics.
//
// Usage: bench_compile [--rounds N] [--corpus DIR] [--json PATH]
//   - runs from the testsuite directory (chdir + init_main like
//     owner_runtime_bench), corpus defaults to tools/perf/corpus
//   - rounds: number of full-corpus passes (default 5)

#include "base/package_api.h"
#include "test_mudlib.h"

#include "backend.h"
#include "compiler/internal/LexStream.h"
#include "compiler/internal/compiler.h"
#include "mainlib.h"
#include "vm/internal/base/program.h"

#include "compiler/internal/compile_arena.h"

#include <fcntl.h>
#if !defined(_WIN32)
#include <sys/resource.h>
#endif
#include <unistd.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

namespace fs = std::filesystem;

namespace {

using Clock = std::chrono::steady_clock;
using Json = nlohmann::json;

struct CompileSample {
  double seconds = 0;
  bool success = false;
};

struct RoundSample {
  double seconds = 0;
  std::vector<CompileSample> files;
};

Json corpus_identity(const std::vector<fs::path>& files) {
  Json identities = Json::array();
  for (const auto& file : files) {
    std::ifstream input(file, std::ios::binary);
    input.exceptions(std::ios::badbit);
    if (!input) {
      throw std::runtime_error("cannot read corpus: " + file.string());
    }
    auto digest = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>(EVP_MD_CTX_new(),
                                                                      EVP_MD_CTX_free);
    if (!digest || EVP_DigestInit_ex(digest.get(), EVP_sha256(), nullptr) != 1) {
      throw std::runtime_error("cannot initialize corpus digest");
    }
    char buffer[16384];
    size_t bytes = 0;
    while (input.read(buffer, sizeof(buffer)) || input.gcount() != 0) {
      const auto count = static_cast<size_t>(input.gcount());
      bytes += count;
      if (EVP_DigestUpdate(digest.get(), buffer, count) != 1) {
        throw std::runtime_error("cannot hash corpus");
      }
    }
    if (!input.eof()) {
      throw std::runtime_error("cannot finish reading corpus: " + file.string());
    }
    input.clear();
    input.close();
    if (!input) {
      throw std::runtime_error("cannot close corpus: " + file.string());
    }
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_size = 0;
    if (EVP_DigestFinal_ex(digest.get(), hash, &hash_size) != 1) {
      throw std::runtime_error("cannot finalize corpus digest");
    }
    constexpr char kHex[] = "0123456789abcdef";
    std::string hex;
    for (unsigned int i = 0; i < hash_size; ++i) {
      hex += kHex[hash[i] >> 4];
      hex += kHex[hash[i] & 15];
    }
    identities.push_back({{"path", file.string()}, {"bytes", bytes}, {"sha256", hex}});
  }
  return identities;
}

long peak_rss_kb() {
#if defined(_WIN32)
  // The benchmark has no Windows-specific RSS dependency; report the metric
  // as unavailable rather than requiring the optional PSAPI library.
  return -1;
#else
  struct rusage ru;
  if (getrusage(RUSAGE_SELF, &ru) != 0) {
    return -1;
  }
#if defined(__APPLE__)
  // macOS reports ru_maxrss in bytes; Linux reports kilobytes.
  return ru.ru_maxrss / 1024;
#else
  return ru.ru_maxrss;
#endif
#endif
}

int compile_one(const fs::path& f) {
  const std::string filename = f.string();
  int fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  auto stream = std::make_unique<FileLexStream>(fd);
  program_t* prog = nullptr;
  error_context_t econ{};
  save_context(&econ);
  try {
    prog = compile_file(std::move(stream), filename.c_str());
    pop_context(&econ);
  } catch (...) {
    // error() threw (simulate.cc:2325); restore the error context like
    // test_lpc.cc does. The stream dtor ran during unwind (fd closed);
    // compile_file's RAII DEFERs restored guard/current_file and ended
    // the arena scope, so the next compile is unaffected.
    restore_context(&econ);
    return -1;
  }
  if (!prog) {
    // Parse error / inherit_file abandon path: epilog returns nullptr
    // (compiler.cc:2251-2255) without throwing. Count it as a failure.
    return -1;
  }
  free_prog(&prog);
  return 0;
}

int run_benchmark(int argc, char** argv) {
  int rounds = 5;
  fs::path corpus_dir;
  fs::path json_path;
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--rounds" && i + 1 < argc) {
      const std::string value = argv[++i];
      const auto parsed = std::from_chars(value.data(), value.data() + value.size(), rounds);
      if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || rounds < 1) {
        std::fprintf(stderr, "rounds must be a positive integer\n");
        return 2;
      }
    } else if (std::string(argv[i]) == "--corpus" && i + 1 < argc) {
      corpus_dir = argv[++i];
    } else if (std::string(argv[i]) == "--json" && i + 1 < argc) {
      json_path = argv[++i];
    } else if (std::string(argv[i]) == "--help") {
      std::printf("usage: bench_compile [--rounds N] [--corpus DIR] [--json PATH]\n");
      return 0;
    } else {
      std::fprintf(stderr, "unknown argument: %s\n", argv[i]);
      return 2;
    }
  }
  if (corpus_dir.empty()) {
    corpus_dir = fs::path(TESTSUITE_DIR) / ".." / "tools" / "perf" / "corpus";
  }
  corpus_dir = fs::absolute(corpus_dir);

  // Full driver environment (shared string table, mem blocks, error
  // context, master/simul_efun load) -- same bootstrap as the benches
  // in src/tests.
  const auto mudlib = fluffos_test_mudlib::root();
  if (fluffos_test_mudlib::change_directory(mudlib) != 0) {
    std::perror("chdir(test mudlib)");
    return 2;
  }
  init_main("etc/config.test");

  std::vector<fs::path> files;
  if (fs::is_directory(corpus_dir)) {
    for (const auto& e : fs::directory_iterator(corpus_dir)) {
      auto ext = e.path().extension();
      if (ext == ".c" || ext == ".lpc") {
        files.push_back(e.path());
      }
    }
  } else if (fs::is_regular_file(corpus_dir)) {
    files.push_back(corpus_dir);
  }
  std::sort(files.begin(), files.end());
  const std::string corpus_dir_string = corpus_dir.string();
  if (files.empty()) {
    std::fprintf(stderr, "no corpus files under %s\n", corpus_dir_string.c_str());
    return 2;
  }

  if (!json_path.empty() && fs::exists(json_path)) {
    for (const auto& file : files) {
      if (fs::equivalent(json_path, file)) {
        throw std::runtime_error("JSON output aliases corpus input");
      }
    }
  }
  const auto identities = corpus_identity(files);

  // Allocate recording storage before warmup, outside the timed region.
  std::vector<RoundSample> samples(rounds);
  for (auto& round : samples) {
    round.files.resize(files.size());
  }
  std::vector<bool> warmup_success;
  size_t warmup_failures = 0;
  for (const auto& file : files) {
    const bool success = compile_one(file) == 0;
    warmup_success.push_back(success);
    warmup_failures += !success;
  }
  const size_t warmup_mallocs = compile_arena::chunk_mallocs();

  for (auto& round : samples) {
    const auto start = Clock::now();
    for (size_t i = 0; i < files.size(); ++i) {
      const auto file_start = Clock::now();
      auto& sample = round.files[i];
      sample.success = compile_one(files[i]) == 0;
      sample.seconds = std::chrono::duration<double>(Clock::now() - file_start).count();
    }
    round.seconds = std::chrono::duration<double>(Clock::now() - start).count();
  }
  const auto rss_kb = peak_rss_kb();
  const size_t final_mallocs = compile_arena::chunk_mallocs();
  if (corpus_identity(files) != identities) {
    throw std::runtime_error("corpus changed during measurement");
  }

  size_t failures = 0;
  std::vector<double> round_secs;
  std::vector<double> operation_secs;
  Json round_samples = Json::array();
  for (const auto& round : samples) {
    if (!std::isfinite(round.seconds) || round.seconds <= 0) {
      throw std::runtime_error("invalid round duration");
    }
    round_secs.push_back(round.seconds);
    Json operations = Json::array();
    for (const auto& sample : round.files) {
      if (!std::isfinite(sample.seconds) || sample.seconds <= 0) {
        throw std::runtime_error("invalid operation duration");
      }
      failures += !sample.success;
      operation_secs.push_back(sample.seconds);
      operations.push_back({{"seconds", sample.seconds}, {"success", sample.success}});
    }
    round_samples.push_back({{"seconds", round.seconds}, {"files", std::move(operations)}});
  }

  // Compare the same complete corpus in the first and last chronological windows.
  Json lifecycle = nullptr;
  if (rounds > 1) {
    const size_t window = std::max<size_t>(1, round_secs.size() / 10);
    const double head =
        std::accumulate(round_secs.begin(), round_secs.begin() + window, 0.0) / window;
    const double tail =
        std::accumulate(round_secs.end() - window, round_secs.end(), 0.0) / window;
    lifecycle = {{"window_rounds", window},
                 {"head_mean_secs", head},
                 {"tail_mean_secs", tail},
                 {"ratio", tail / head}};
  }
  std::vector<double> per_file_secs;
  for (const auto& sample : samples.back().files) {
    per_file_secs.push_back(sample.seconds);
  }
  // Legacy fields keep their original meaning; raw samples above remain chronological.
  std::sort(round_secs.begin(), round_secs.end());
  std::sort(operation_secs.begin(), operation_secs.end());
  double total_secs = std::accumulate(round_secs.begin(), round_secs.end(), 0.0);
  double median = round_secs[rounds / 2];
  double p95 = round_secs[static_cast<size_t>(rounds * 0.95)];
  double p99 = round_secs[static_cast<size_t>(rounds * 0.99)];

  // This legacy ratio describes different inputs, not lifecycle degradation.
  size_t tail_n = std::max<size_t>(1, per_file_secs.size() / 10);
  double head_mean =
      std::accumulate(per_file_secs.begin(), per_file_secs.begin() + tail_n, 0.0) / tail_n;
  double tail_mean =
      std::accumulate(per_file_secs.end() - tail_n, per_file_secs.end(), 0.0) / tail_n;
  double degradation = head_mean > 0 ? tail_mean / head_mean : 0.0;

  std::printf("bench_compile: files=%zu rounds=%d corpus=%s\n", files.size(), rounds,
              corpus_dir_string.c_str());
  std::printf("throughput: %.2f files/s (%.2f s total)\n", files.size() * rounds / total_secs,
              total_secs);
  std::printf("round_secs: median=%.4f p95=%.4f p99=%.4f min=%.4f max=%.4f\n", median, p95, p99,
              round_secs.front(), round_secs.back());
  std::printf("failures: %zu\n", failures);
  std::printf("warmup_failures: %zu\n", warmup_failures);
  std::printf("per_file_last_round: head10_mean=%.6f tail10_mean=%.6f degradation=%.4f\n",
              head_mean, tail_mean, degradation);
  std::printf("peak_rss_kb: %ld\n", rss_kb);
  std::printf("arena: warmup_chunk_mallocs=%zu final_chunk_mallocs=%zu delta=%zu\n",
              warmup_mallocs, final_mallocs, final_mallocs - warmup_mallocs);
  std::printf("arena: retained_chunks=%zu retained_heap_bytes=%zu cycle_bytes=%zu "
              "peak_cycle_bytes=%zu resets=%zu\n",
              compile_arena::retained_chunks(), compile_arena::retained_heap_bytes(),
              compile_arena::cycle_bytes(), compile_arena::peak_cycle_bytes(),
              compile_arena::reset_count());
  const auto count = operation_secs.size();
  const Json operation_stats = {{"sample_count", count},
                               {"median", operation_secs[count / 2]},
                               {"p95", operation_secs[static_cast<size_t>(count * 0.95)]},
                               {"p99", operation_secs[static_cast<size_t>(count * 0.99)]}};
  std::cout << "operation_secs: " << operation_stats.dump() << '\n';
  std::cout << "lifecycle: " << lifecycle.dump() << '\n';
  const Json report = {
      {"schema_version", 2},
      {"files", files.size()},
      {"rounds", rounds},
      {"corpus", corpus_dir.string()},
      {"corpus_files", identities},
      {"warmup_success", warmup_success},
      {"warmup_failures", warmup_failures},
      {"successful_compiles", count - failures},
      {"throughput_files_per_s", count / total_secs},
      {"total_secs", total_secs},
      {"round_secs", round_secs},
      {"round_samples", std::move(round_samples)},
      {"per_file_secs_last_round", per_file_secs},
      {"failures", failures},
      {"degradation", degradation},
      {"lifecycle", lifecycle},
      {"operation_secs", operation_stats},
      {"peak_rss_kb", rss_kb},
      {"arena",
       {{"warmup_chunk_mallocs", warmup_mallocs},
        {"final_chunk_mallocs", final_mallocs},
        {"delta", final_mallocs - warmup_mallocs},
        {"retained_chunks", compile_arena::retained_chunks()},
        {"retained_heap_bytes", compile_arena::retained_heap_bytes()},
        {"cycle_bytes", compile_arena::cycle_bytes()},
        {"peak_cycle_bytes", compile_arena::peak_cycle_bytes()},
        {"resets", compile_arena::reset_count()}}}};
  if (!json_path.empty()) {
    std::ofstream out;
    out.exceptions(std::ios::failbit | std::ios::badbit);
    out.open(json_path);
    out << report.dump(2) << '\n';
    out.close();
  }
  std::cout.flush();
  if (!std::cout || std::fflush(stdout) != 0 || std::ferror(stdout)) {
    throw std::runtime_error("cannot write benchmark stdout");
  }
  return failures > 0 || warmup_failures > 0 ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    return run_benchmark(argc, argv);
  } catch (const std::exception& error) {
    std::fprintf(stderr, "bench_compile: %s\n", error.what());
    return 2;
  }
}
