#pragma once

#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/event.h>
#include <unicode/ucnv.h>
#include <algorithm>
#include <atomic>
#include "packages/async/async.h"  // for check_reqs (backend wakeup drain)
#include "compiler/internal/diagnostic.h"  // T3.2 structured compile diagnostics
#include "compiler/internal/diagnostic_render.h"  // T3.3 rendering
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <memory>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <filesystem>
#ifndef _WIN32
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif
#include "base/package_api.h"
#include "base/internal/rc.h"
#include "base/internal/external_port.h"
#include "base/internal/stats.h"
#include "base/internal/stralloc.h"
#include "base/internal/strutils.h"
#include "net/tls.h"
#include "net/websocket.h"

#include "backend.h"
#include "comm.h"
#include "interactive.h"
#include "net/sys_telnet.h"
#include "net/telnet.h"
#include "thirdparty/libtelnet/libtelnet.h"
#include "mainlib.h"
#include "user.h"

#include "compiler/internal/compiler.h"
#include "compiler/internal/lex.h"
#include "compiler/internal/lpc_modern_profile.h"
#include "vm/internal/owner_executor.h"
#include "vm/internal/base/scoped_current_object_as_master.h"
#include "packages/core/call_out.h"
#include "packages/core/dns.h"
#include "packages/core/file.h"
#include "packages/core/heartbeat.h"
#include "packages/core/replace_program.h"
#include "packages/gateway/gateway.h"
#include "packages/sockets/socket_efuns.h"
#include "vm/context.h"
#include "vm/frozen_value.h"
#include "vm/internal/apply.h"
#include "vm/internal/base/apply_cache.h"
#include "vm/internal/base/array.h"
#include "vm/internal/base/mapping.h"
#include "vm/internal/base/object.h"
#include "vm/internal/base/promise.h"
#include "vm/internal/eval_limit.h"
#include "vm/internal/lpc_vm_profile.h"
#include "vm/internal/owner_future_store.h"
#include "vm/internal/owner_scheduler_state.h"
#include "vm/internal/owner_runtime_coordinator.h"
#include "vm/internal/owner_service_registry.h"
#include "vm/internal/otable.h"
#include "vm/internal/simulate.h"
#include "vm/internal/simul_efun.h"
#include "vm/internal/recompile.h"
#include "vm/internal/recompile_layout.h"
#include "vm/object_handle.h"
#include "vm/owner.h"
#include "vm/vm.h"
#include "vm/worker.h"
#include "test_mudlib.h"

extern uint64_t vm_owner_enqueue_test_main_required_message(const char* owner_id, const char* task_key);
extern void vm_owner_test_support_reset_budget_yield_observations();
extern uint64_t vm_owner_enqueue_command_frame_restore(object_t* target);
extern bool vm_object_store_test_support_remove_live_object_ref_for_bridge_readiness(const char* owner_id,
                                                                                    uint64_t object_id);
extern bool vm_call_out_test_support_run_handle(LPC_INT handle);
extern int vm_call_out_test_support_priority(LPC_INT handle);
extern bool vm_async_test_support_dispatch_read_callback(object_t* owner, const char* method, const char* payload);

#ifdef PACKAGE_SOCKETS
extern void f_socket_get_option();
#endif

namespace {
#ifdef PACKAGE_SOCKETS
struct SocketPairGuard {
  int first = -1;
  int second = -1;
  ~SocketPairGuard() {
    if (first >= 0) {
      socket_close(first, 0);
    }
    if (second >= 0) {
      socket_close(second, 0);
    }
  }
};
#endif

// 文件清理守卫：析构时删除测试文件（原 4 份逐字节相同局部 FixtureGuard
// 收敛为单一定义）
struct PathCleanupGuard {
  const char* path;
  ~PathCleanupGuard() { std::remove(path); }
};
// 外部命令恢复守卫：析构时恢复 external_cmd[0]（原 2 份逐字节相同局部
// FixtureGuard 收敛为单一定义）
struct ExternalCommandGuard {
  char* saved_command;
  ~ExternalCommandGuard() { external_cmd[0] = saved_command; }
};
object_t* load_object_for_test(const char* path);
object_t* clone_object_for_test(const char* path);
void destruct_object_for_test(object_t* object);

[[noreturn]] void throw_recompile_non_lpc_exception_for_test() {
  vm_context_set_current_object(vm_context(), nullptr);
  vm_context_set_current_program(vm_context(), nullptr);
  throw std::runtime_error("injected recompile create exception");
}
}
extern bool vm_dns_test_support_dispatch_callback(object_t* owner, const char* method, LPC_INT key);
extern bool vm_socket_test_support_dispatch_callback(object_t* owner, const char* method, LPC_INT fd);
extern int external_start(int which, svalue_t* args, svalue_t* arg1, svalue_t* arg2,
                          svalue_t* arg3);
extern bool decode_mud_port_payload_length_for_test(const char* header, size_t header_size,
                                                    size_t* payload_length);
extern int comm_reserve_input_space_for_test(interactive_t* ip, size_t reserve);
extern int comm_append_input_for_test(interactive_t* ip, const unsigned char* data, int len);
extern int replace_interactive(object_t *ob, object_t *obfrom);
extern bool gateway_dispatch_message_for_test(int fd, const char *payload);
extern int gateway_dispatch_buffered_frames_for_test(GatewayMaster *master, int budget);
extern void gateway_set_read_dispatch_pending_for_test(GatewayMaster *master, bool pending);
extern void gateway_service_admitted_receive_tasks_for_test();
extern bool gateway_master_has_buffered_input_for_test(const GatewayMaster *master);


namespace {
struct StringStatsSnapshot {
  uint64_t num_distinct_strings_value;
  uint64_t bytes_distinct_strings_value;
  uint64_t overhead_bytes_value;
  uint64_t allocd_strings_value;
  uint64_t allocd_bytes_value;

  StringStatsSnapshot()
      : num_distinct_strings_value(num_distinct_strings.load(std::memory_order_relaxed)),
        bytes_distinct_strings_value(bytes_distinct_strings.load(std::memory_order_relaxed)),
        overhead_bytes_value(overhead_bytes.load(std::memory_order_relaxed)),
        allocd_strings_value(allocd_strings.load(std::memory_order_relaxed)),
        allocd_bytes_value(allocd_bytes.load(std::memory_order_relaxed)) {}

  ~StringStatsSnapshot() {
    num_distinct_strings.store(num_distinct_strings_value, std::memory_order_relaxed);
    bytes_distinct_strings.store(bytes_distinct_strings_value, std::memory_order_relaxed);
    overhead_bytes.store(overhead_bytes_value, std::memory_order_relaxed);
    allocd_strings.store(allocd_strings_value, std::memory_order_relaxed);
    allocd_bytes.store(allocd_bytes_value, std::memory_order_relaxed);
  }
};

OwnerFutureRecord owner_future_store_test_record(uint64_t future_id, uint64_t target_task_id) {
  OwnerFutureRecord record;
  record.future_id = future_id;
  record.target_task_id = target_task_id;
  record.state = "pending";
  return record;
}
}  // namespace

namespace {
void test_set_env(const char* name, const char* value) {
#ifdef _WIN32
  (void)_putenv_s(name, value);
#else
  (void)setenv(name, value, 1);
#endif
}

void test_unset_env(const char* name) {
#ifdef _WIN32
  (void)_putenv_s(name, "");
#else
  (void)unsetenv(name);
#endif
}

}  // namespace

// Test fixture class
namespace {
/* LPC mappings returned by the owner/gateway introspection helpers are
 * refcounted allocations the caller owns. A test that holds one across
 * assertions leaks it as soon as one fails and returns early (LSan sees it at
 * exit), so tests wrap them in this guard instead of a trailing
 * free_mapping(). */
class ScopedLpcMapping {
 public:
  explicit ScopedLpcMapping(mapping_t *map) : map_(map) {}
  ~ScopedLpcMapping() {
    if (map_) {
      free_mapping(map_);
    }
  }
  ScopedLpcMapping(const ScopedLpcMapping &) = delete;
  ScopedLpcMapping &operator=(const ScopedLpcMapping &) = delete;
  mapping_t *get() const { return map_; }

 private:
  mapping_t *map_;
};
}  // namespace

class DriverTest : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    std::string mudlib;
    try {
      mudlib = fluffos_test_mudlib::root_string();
    } catch (const std::exception &error) {
      FAIL() << "invalid FLUFFOS_TEST_MUDLIB: " << error.what();
      return;
    }
    ASSERT_EQ(0, fluffos_test_mudlib::change_directory(mudlib))
        << "failed to chdir to " << mudlib << ": " << strerror(errno);
    // Initialize libevent, This should be done before executing LPC.
    auto* base = init_main("etc/config.test");
    vm_start();
  }

 protected:
  void SetUp() override { clear_state(); }

  void TearDown() override {
    vm_worker_stop();
    vm_owner_thread_stop();
    clear_state();
  }
};


namespace {
std::string read_source_file_for_test(const char* path) {
  std::ifstream file(path);
  EXPECT_TRUE(file.is_open()) << "failed to open source file: " << path;
  std::ostringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

object_t* load_object_for_test(const char* path) {
  error_context_t econ{};
  object_t* object = nullptr;
  ScopedCurrentObjectAsMaster master_scope(ScopedCurrentObjectAsMaster::conditional_t{});
  save_context(&econ);
  try {
    object = load_object(path, 1);
    pop_context(&econ);
  } catch (...) {
    restore_context(&econ);
    ADD_FAILURE() << "load_object failed for " << path;
  }
  return object;
}

object_t* clone_object_for_test(const char* path) {
  error_context_t econ{};
  object_t* object = nullptr;
  ScopedCurrentObjectAsMaster master_scope(ScopedCurrentObjectAsMaster::conditional_t{});
  save_context(&econ);
  try {
    object = clone_object(path, 0);
    pop_context(&econ);
  } catch (...) {
    restore_context(&econ);
    ADD_FAILURE() << "clone_object failed for " << path;
  }
  return object;
}

void destruct_object_for_test(object_t* object) {
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
    ADD_FAILURE() << "destruct_object failed for " << object->obname;
  }
}
}  // namespace
