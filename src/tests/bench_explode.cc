#include "base/package_api.h"
#include "mainlib.h"
#include "test_mudlib.h"

#include <charconv>
#include <chrono>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#if defined(__linux__) && defined(__GNUC__)
extern "C" void __gcov_reset() __attribute__((weak));
#endif

namespace {

int run_benchmark(int argc, char** argv) {
  if (argc != 3 && argc != 4) {
    throw std::runtime_error("usage: bench_explode ascii|unicode 1000|10000|50000 [--coverage]");
  }
  const std::string_view kind(argv[1]);
  if (kind != "ascii" && kind != "unicode") {
    throw std::runtime_error("kind must be ascii or unicode");
  }
  int count = 0;
  const std::string_view count_text(argv[2]);
  const auto parsed =
      std::from_chars(count_text.data(), count_text.data() + count_text.size(), count);
  if (parsed.ec != std::errc{} || parsed.ptr != count_text.data() + count_text.size() ||
      (count != 1000 && count != 10000 && count != 50000)) {
    throw std::runtime_error("token count must be 1000, 10000 or 50000");
  }
  const bool coverage = argc == 4;
  if (coverage && std::string_view(argv[3]) != "--coverage") {
    throw std::runtime_error("unknown argument");
  }
  if (fluffos_test_mudlib::change_directory(fluffos_test_mudlib::root()) != 0) {
    throw std::runtime_error("cannot enter test mudlib");
  }
  init_main("etc/config.test");
  CONFIG_INT(__MAX_ARRAY_SIZE__) = count;

  const std::string token = kind == "ascii" ? "x" : "中";
  std::string source;
  source.reserve(static_cast<size_t>(count) * (token.size() + 1));
  for (int i = 0; i < count; ++i) {
    if (i != 0) {
      source += '|';
    }
    source += token;
  }
  const auto measure = [&] {
    const auto start = std::chrono::steady_clock::now();
    std::unique_ptr<array_t, decltype(&free_array)> result(
        explode_string(source.data(), static_cast<int>(source.size()), "|", 1, false), free_array);
    const auto end = std::chrono::steady_clock::now();
    if (result->size != count) {
      throw std::runtime_error("wrong token count");
    }
    for (int i = 0; i < count; ++i) {
      const auto& value = result->item[i];
      if (value.type != T_STRING ||
          std::string_view(value.u.string, SVALUE_STRLEN(&value)) != token) {
        throw std::runtime_error("token bytes changed");
      }
    }
    return std::chrono::duration<double>(end - start).count();
  };
  measure();
  if (coverage) {
#if defined(__linux__) && defined(__GNUC__)
    if (__gcov_reset == nullptr) {
      throw std::runtime_error("requires a GCC --coverage build");
    }
    __gcov_reset();
#else
    throw std::runtime_error("coverage collection requires Linux and GCC");
#endif
  }
  nlohmann::json samples = nlohmann::json::array();
  const int rounds = coverage ? 1 : 5;
  for (int i = 0; i < rounds; ++i) {
    samples.push_back(measure());
  }
  const nlohmann::json report = {{"kind", kind},         {"token", token},
                                 {"tokens", count},      {"source_bytes", source.size()},
                                 {"coverage", coverage}, {"seconds", samples},
                                 {"verified", true}};
  if (std::printf("BENCH_EXPLODE_JSON=%s\n", report.dump().c_str()) < 0 ||
      std::fflush(stdout) != 0 || std::ferror(stdout)) {
    throw std::runtime_error("benchmark output failed");
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    return run_benchmark(argc, argv);
  } catch (const std::exception& error) {
    std::fprintf(stderr, "bench_explode: %s\n", error.what());
    return 1;
  }
}
