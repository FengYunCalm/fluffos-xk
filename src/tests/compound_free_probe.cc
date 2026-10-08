#include "base/std.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>

#include "vm/context.h"
#include "vm/internal/base/machine.h"
#include "vm/internal/base/promise.h"

namespace {
void require(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "COMPOUND_FREE_ERROR: %s\n", message);
    std::abort();
  }
}

struct Allocation {
  void* pointer;
  size_t size;
};

struct AllocationTracker {
  std::array<Allocation, 64> live{};
  size_t allocations{0};
  size_t outstanding{0};
  size_t bytes{0};
  size_t peak_bytes{0};
  bool enabled{false};
};

// Trivial TLS remains usable by the late-destruction probes.
thread_local AllocationTracker allocations;

void track_allocation(void* pointer, size_t size) {
  if (!allocations.enabled) {
    return;
  }
  for (auto& entry : allocations.live) {
    if (entry.pointer == nullptr) {
      entry = {pointer, size};
      ++allocations.allocations;
      ++allocations.outstanding;
      allocations.bytes += size;
      if (allocations.bytes > allocations.peak_bytes) {
        allocations.peak_bytes = allocations.bytes;
      }
      return;
    }
  }
  require(false, "allocation observer capacity exhausted");
}

void track_deallocation(void* pointer) {
  if (pointer == nullptr || allocations.outstanding == 0) {
    return;
  }
  for (auto& entry : allocations.live) {
    if (entry.pointer == pointer) {
      allocations.bytes -= entry.size;
      --allocations.outstanding;
      entry = {};
      return;
    }
  }
}

using ValueCounts = std::array<uint64_t, 3>;

ValueCounts value_counts() {
  return {num_arrays, num_mappings, num_classes};
}

svalue_t make_chain(int depth) {
  svalue_t value = const0;
  for (int i = 0; i < depth; ++i) {
    auto* node = allocate_array(1);
    node->item[0] = value;
    value.type = T_ARRAY;
    value.subtype = 0;
    value.u.arr = node;
  }
  return value;
}

void warm_queue() {
  auto value = make_chain(2);
  free_svalue(&value, "compound probe warmup");
}

struct CancellationOrder {
  std::array<int, 1024> parent_ids{};
  std::array<int, 1024> child_ids{};
  std::array<int, 2048> seen{};
  size_t count{0};
};

thread_local CancellationOrder cancellation_order;

void record_cancel(void* data) {
  require(cancellation_order.count < cancellation_order.seen.size(), "cancel observer overflow");
  cancellation_order.seen[cancellation_order.count++] = *static_cast<int*>(data);
}

void cancel_parent(void* data) {
  record_cancel(data);
  const auto index = *static_cast<int*>(data);
  auto* child = promise_alloc();
  promise_set_cancel_handler(child, record_cancel, &cancellation_order.child_ids[index]);
  free_promise(child);
}

svalue_t make_wide(int width) {
  cancellation_order.count = 0;
  auto* root = allocate_array(width);
  for (int i = 0; i < width; ++i) {
    cancellation_order.parent_ids[i] = i;
    cancellation_order.child_ids[i] = 10000 + i;
    auto* promise = promise_alloc();
    promise_set_cancel_handler(promise, cancel_parent, &cancellation_order.parent_ids[i]);
    root->item[i].type = T_PROMISE;
    root->item[i].u.prom = promise;
  }
  svalue_t result = const0;
  result.type = T_ARRAY;
  result.u.arr = root;
  return result;
}

void check_wide_order(int width) {
  require(cancellation_order.count == static_cast<size_t>(2 * width), "missing cancellation");
  for (int i = 0; i < width; ++i) {
    require(cancellation_order.seen[2 * i] == i && cancellation_order.seen[2 * i + 1] == 10000 + i,
            "compound LIFO order changed");
  }
}

svalue_t make_mixed() {
  auto* root = allocate_array(5);
  root->item[0] = make_chain(3);
  auto* mapping = allocate_mapping(0);
  svalue_t key = const1;
  *find_for_insert(mapping, &key, 0) = make_chain(3);
  root->item[1].type = T_MAPPING;
  root->item[1].u.map = mapping;
  auto* instance = allocate_class_by_size(1);
  instance->item[0] = make_chain(3);
  root->item[2].type = T_CLASS;
  root->item[2].u.arr = instance;
  auto* promise = promise_alloc();
  auto result = make_chain(3);
  require(promise_settle(promise, &result, 0), "promise settlement failed");
  free_svalue(&result, "compound probe settlement");
  root->item[3].type = T_PROMISE;
  root->item[3].u.prom = promise;
  auto* aliases = allocate_array(2);
  auto shared = make_chain(3);
  assign_svalue_no_free(&aliases->item[0], &shared);
  assign_svalue_no_free(&aliases->item[1], &shared);
  free_svalue(&shared, "compound probe shared reference");
  root->item[4].type = T_ARRAY;
  root->item[4].u.arr = aliases;
  svalue_t value = const0;
  value.type = T_ARRAY;
  value.u.arr = root;
  return value;
}

void release_observed(svalue_t& value) {
  allocations = {};
  allocations.enabled = true;
  free_svalue(&value, "compound probe release");
  allocations.enabled = false;
  require(allocations.outstanding == 0, "worklist storage retained after release");
}

struct LateValue {
  VMContext context;
  svalue_t value{const0};
  const char* report{nullptr};

  ~LateValue() {
    VMContextThreadScope scope(context);
    release_observed(value);
    require(value_counts() == ValueCounts{}, "late value leaked");
    if (report != nullptr) {
      std::printf("COMPOUND_FREE_OK:%s\n", report);
    }
  }
};

LateValue& static_value() {
  static LateValue value;
  return value;
}

void prepare_late(LateValue& holder, bool warm) {
  VMContextThreadScope scope(holder.context);
  holder.value = make_chain(20000);
  if (warm) {
    warm_queue();
  }
}

void throwing_cancel(void*) {
  throw std::runtime_error("cancel probe");
}

void check_exception_reset() {
  auto* promise = promise_alloc();
  promise_set_cancel_handler(promise, throwing_cancel, nullptr);
  bool caught = false;
  try {
    free_promise(promise);
  } catch (const std::runtime_error&) {
    caught = true;
  }
  require(caught && promise->ref == 0, "cancel exception did not propagate");
  // The handler throws before releasing any result or reaction. It has already
  // been detached, so this retained zero-ref allocation can be explicitly retried.
  free_compound(promise, T_PROMISE);
  auto value = make_chain(3);
  release_observed(value);
}
struct ConcurrentRelease {
  std::array<promise_t*, 32> children{};
  std::atomic<unsigned>* entered{nullptr};
  std::thread::id owner;
  size_t destroyed{0};
};

void concurrent_child(void* data) {
  auto& batch = *static_cast<ConcurrentRelease*>(data);
  require(std::this_thread::get_id() == batch.owner, "queue crossed thread boundary");
  ++batch.destroyed;
}

void concurrent_root(void* data) {
  auto& batch = *static_cast<ConcurrentRelease*>(data);
  ++*batch.entered;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (batch.entered->load() != 4) {
    require(std::chrono::steady_clock::now() < deadline, "queues failed to overlap");
    std::this_thread::yield();
  }
  for (auto* child : batch.children) {
    free_promise(child);
  }
}

void check_concurrent() {
  std::atomic<unsigned> entered{0};
  std::array<std::thread, 4> threads;
  for (auto& thread : threads) {
    thread = std::thread([&entered] {
      VMContext context;
      VMContextThreadScope scope(context);
      ConcurrentRelease batch;
      batch.entered = &entered;
      batch.owner = std::this_thread::get_id();
      // Plain pending Promises have no reaction, origin, or shared LPC state.
      // Avoid array/class process-wide statistics in this queue-only fixture.
      for (auto& child : batch.children) {
        child = promise_alloc();
        promise_set_cancel_handler(child, concurrent_child, &batch);
      }
      auto* root = promise_alloc();
      promise_set_cancel_handler(root, concurrent_root, &batch);
      allocations = {};
      allocations.enabled = true;
      free_promise(root);
      allocations.enabled = false;
      require(batch.destroyed == batch.children.size(), "concurrent queue did not drain");
      require(allocations.outstanding == 0, "concurrent worklist retained storage");
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
}

void benchmark_release(bool wide) {
  allocations = {};
  auto measured = wide ? make_wide(64) : make_chain(64);
  allocations.enabled = true;
  free_svalue(&measured, "compound probe allocation measurement");
  allocations.enabled = false;
  const auto allocation_count = allocations.allocations;
  const auto peak_bytes = allocations.peak_bytes;
  const auto retained_bytes = allocations.bytes;
  constexpr int kIterations = 2000;
  int64_t elapsed_ns = 0;
  for (int i = 0; i < kIterations; ++i) {
    auto value = wide ? make_wide(64) : make_chain(64);
    const auto start = std::chrono::steady_clock::now();
    free_svalue(&value, "compound probe timed release");
    elapsed_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
                      std::chrono::steady_clock::now() - start)
                      .count();
  }
  require(value_counts() == ValueCounts{}, "benchmark value leaked");
  std::printf(
      "COMPOUND_FREE_BENCH:{\"iterations\":%d,\"elapsed_ns\":%lld,"
      "\"cold_allocations\":%zu,\"cold_peak_bytes\":%zu,\"retained_bytes\":%zu}\n",
      kIterations, static_cast<long long>(elapsed_ns), allocation_count, peak_bytes,
      retained_bytes);
}
}  // namespace

extern "C" {
void* __real__Znwm(size_t size);
void* __wrap__Znwm(size_t size) {
  void* pointer = __real__Znwm(size);
  track_allocation(pointer, size);
  return pointer;
}
void __real__ZdlPv(void* pointer);
void __wrap__ZdlPv(void* pointer) {
  track_deallocation(pointer);
  __real__ZdlPv(pointer);
}
void __real__ZdlPvm(void* pointer, size_t size);
void __wrap__ZdlPvm(void* pointer, size_t size) {
  track_deallocation(pointer);
  __real__ZdlPvm(pointer, size);
}
}

int main(int argc, char** argv) {
  if (argc != 2) {
    return 2;
  }
  CONFIG_INT(__MAX_ARRAY_SIZE__) = 4096;
  CONFIG_INT(__MAX_MAPPING_SIZE__) = 4096;
  VMContext context;
  VMContextThreadScope scope(context);
  // MDfree initializes a process-wide journal on its first call. Do that before
  // observing allocations, using a scalar-only array that never queues a value.
  free_array(allocate_array(1));
  const char* mode = argv[1];
  if (std::strcmp(mode, "bench-narrow") == 0 || std::strcmp(mode, "bench-wide") == 0) {
    benchmark_release(std::strcmp(mode, "bench-wide") == 0);
    std::printf("COMPOUND_FREE_OK:%s\n", mode);
    return 0;
  }
  if (std::strcmp(mode, "static-warm") == 0 || std::strcmp(mode, "static-cold") == 0 ||
      std::strcmp(mode, "explicit-exit") == 0) {
    auto& holder = static_value();
    holder.report = mode;
    prepare_late(holder, std::strcmp(mode, "static-cold") != 0);
    if (std::strcmp(mode, "explicit-exit") == 0) {
      std::exit(0);
    }
    return 0;
  }
  if (std::strcmp(mode, "thread-late") == 0) {
    // These value counters are process-wide, not per VMContext. Isolate TLS
    // destruction here; concurrent queue draining has a separate fixture.
    std::thread thread([] {
      thread_local LateValue holder;
      prepare_late(holder, true);
    });
    thread.join();
  } else if (std::strcmp(mode, "concurrent") == 0) {
    check_concurrent();
  } else {
    warm_queue();
    const auto before = value_counts();
    if (std::strcmp(mode, "mixed") == 0) {
      auto value = make_mixed();
      release_observed(value);
    } else if (std::strcmp(mode, "deep") == 0) {
      auto value = make_chain(200000);
      release_observed(value);
      require(allocations.allocations == 0, "narrow chain allocated worklist storage");
    } else if (std::strcmp(mode, "inline") == 0 || std::strcmp(mode, "overflow") == 0 ||
               std::strcmp(mode, "wide") == 0) {
      int width = 1024;
      if (std::strcmp(mode, "inline") == 0) {
        width = 16;
      } else if (std::strcmp(mode, "overflow") == 0) {
        width = 17;
      }
      auto value = make_wide(width);
      release_observed(value);
      check_wide_order(width);
      require(width > 16 || allocations.allocations == 0, "inline worklist allocated");
    } else if (std::strcmp(mode, "exception") == 0) {
      check_exception_reset();
    } else {
      return 2;
    }
    require(value_counts() == before, "compound value leaked");
  }
  std::printf("COMPOUND_FREE_OK:%s\n", mode);
  return 0;
}
