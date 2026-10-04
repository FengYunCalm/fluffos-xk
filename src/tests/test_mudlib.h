#pragma once

// Test-only mudlib selection shared by driver-backed tests and tools.  The
// compiled-in path remains the historical default; an explicit override is
// accepted only for a complete, marked sandbox so a test cannot silently
// fall back to the source tree after an isolation failure.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#ifndef TESTSUITE_DIR
#error "TESTSUITE_DIR must be defined for test mudlib selection"
#endif

namespace fluffos_test_mudlib {

namespace fs = std::filesystem;

inline std::string describe(const fs::path &path) {
  return path.empty() ? std::string("<empty>") : path.string();
}

inline fs::path canonical_directory(const fs::path &path, const char *label) {
  std::error_code error;
  if (!path.is_absolute()) {
    throw std::runtime_error(std::string("FLUFFOS_TEST_MUDLIB ") + label +
                             " must be an absolute directory: " + describe(path));
  }
  if (!fs::is_directory(path, error) || error) {
    throw std::runtime_error(std::string("FLUFFOS_TEST_MUDLIB ") + label +
                             " is not an existing directory: " + describe(path));
  }
  const fs::path canonical = fs::canonical(path, error);
  if (error) {
    throw std::runtime_error(std::string("cannot canonicalize test mudlib ") +
                             describe(path) + ": " + error.message());
  }
  return canonical;
}

inline fs::path root() {
  const char *override_path = std::getenv("FLUFFOS_TEST_MUDLIB");
  const fs::path source_root = canonical_directory(fs::path(TESTSUITE_DIR), "source");
  if (override_path == nullptr) {
    return source_root;
  }
  if (*override_path == '\0') {
    throw std::runtime_error("FLUFFOS_TEST_MUDLIB is set but empty");
  }

  const fs::path candidate = canonical_directory(fs::path(override_path), "override");
  if (candidate == source_root) {
    throw std::runtime_error(
        "FLUFFOS_TEST_MUDLIB must not resolve to the source testsuite");
  }

  const fs::path marker = candidate / ".fluffos-test-sandbox";
  std::error_code error;
  if (!fs::is_regular_file(marker, error) || error) {
    throw std::runtime_error("test mudlib is missing .fluffos-test-sandbox: " +
                             marker.string());
  }
  std::ifstream input(marker, std::ios::binary);
  if (!input) {
    throw std::runtime_error("cannot read test mudlib marker: " + marker.string());
  }
  const std::string contents((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
  if (contents != "version=1\n") {
    throw std::runtime_error(
        "test mudlib marker must contain exactly version=1\\n: " + marker.string());
  }
  return candidate;
}

inline std::string root_string() { return root().string(); }

inline int change_directory(const fs::path &path) {
#ifdef _WIN32
  return _chdir(path.string().c_str());
#else
  return chdir(path.string().c_str());
#endif
}

}  // namespace fluffos_test_mudlib
