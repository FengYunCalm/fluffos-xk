#include "test_lpc_support.h"

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>

#include "packages/db/db.h"

namespace {
std::mutex pause_mutex;
std::condition_variable pause_condition;
std::thread::id main_thread;
bool main_thread_set = false;
bool pause_armed = false;
bool pause_observed = false;
bool pause_released = false;

void set_main_thread() {
  std::lock_guard<std::mutex> lock(pause_mutex);
  main_thread = std::this_thread::get_id();
  main_thread_set = true;
}

void arm_pause() {
  std::lock_guard<std::mutex> lock(pause_mutex);
  pause_armed = true;
  pause_observed = false;
  pause_released = false;
}

bool wait_for_pause() {
  std::unique_lock<std::mutex> lock(pause_mutex);
  return pause_condition.wait_for(lock, std::chrono::seconds(5), [] {
    return pause_observed;
  });
}

void release_pause() {
  {
    std::lock_guard<std::mutex> lock(pause_mutex);
    pause_released = true;
  }
  pause_condition.notify_all();
}

class PauseGuard {
 public:
  PauseGuard() { arm_pause(); }
  ~PauseGuard() { release_pause(); }
  bool wait() const { return wait_for_pause(); }
  void release() { release_pause(); }
};

class ProbeGuard {
 public:
  explicit ProbeGuard(object_t* object) : object_(object) {}
  ~ProbeGuard() {
    if (!object_) {
      return;
    }
    auto* result = safe_apply("close_current", object_, 0, ORIGIN_DRIVER);
    if (result) {
      vm_apply_return_clear();
    }
    destruct_object_for_test(object_);
  }

 private:
  object_t* object_;
};

void invoke_noarg(object_t* object, const char* method) {
  ASSERT_NE(safe_apply(method, object, 0, ORIGIN_DRIVER), nullptr);
  vm_apply_return_clear();
}

void invoke_number(object_t* object, const char* method, LPC_INT expected) {
  auto* result = safe_apply(method, object, 0, ORIGIN_DRIVER);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_NUMBER);
  EXPECT_EQ(result->u.number, expected);
  vm_apply_return_clear();
}

void prepare_probe(object_t* object) {
  set_main_thread();
  std::remove("audit_db_generation.sqlite");
  invoke_number(object, "open_connection", 1);
}

void close_and_reopen(object_t* object) {
  auto* result = safe_apply("close_and_reopen", object, 0, ORIGIN_DRIVER);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_NUMBER);
  EXPECT_EQ(result->u.number, 1);
  vm_apply_return_clear();
}

void assert_stale_callback_result(object_t* object) {
  auto* result = safe_apply("callback_result", object, 0, ORIGIN_DRIVER);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_STRING);
  EXPECT_NE(std::string(result->u.string).find("stale database connection"), std::string::npos);
  vm_apply_return_clear();
}
}  // namespace

extern "C" void __real__Z13db_lock_mutexv();

extern "C" void __wrap__Z13db_lock_mutexv() {
  {
    std::unique_lock<std::mutex> lock(pause_mutex);
    if (pause_armed && main_thread_set && std::this_thread::get_id() != main_thread) {
      pause_armed = false;
      pause_observed = true;
      pause_condition.notify_all();
      pause_condition.wait(lock, [] { return pause_released; });
    }
  }
  __real__Z13db_lock_mutexv();
}

class DbIoTest : public DriverTest {};

TEST_F(DbIoTest, AuditConnectionGenerationRejectsStaleCallbackRequest) {
  auto* object = load_object_for_test("single/tests/efuns/audit_db_identity");
  ASSERT_NE(object, nullptr);
  ProbeGuard probe(object);
  prepare_probe(object);

  PauseGuard pause;
  invoke_noarg(object, "start_callback");
  ASSERT_TRUE(pause.wait()) << "worker did not reach the database lock barrier";
  close_and_reopen(object);
  pause.release();

  complete_all_asyncio();
  assert_stale_callback_result(object);
}

TEST_F(DbIoTest, AuditConnectionFailureDoesNotPublishSlot) {
  auto* object = load_object_for_test("single/tests/efuns/audit_db_identity");
  ASSERT_NE(object, nullptr);
  ProbeGuard probe(object);
  set_main_thread();
  std::remove("audit_db_generation.sqlite");

  invoke_number(object, "failure_then_open", 1);
}

TEST_F(DbIoTest, AuditConnectionSlotGrowthKeepsAllConnectionsUsable) {
  auto* object = load_object_for_test("single/tests/efuns/audit_db_identity");
  ASSERT_NE(object, nullptr);
  ProbeGuard probe(object);
  set_main_thread();
  std::remove("audit_db_generation.sqlite");

  invoke_number(object, "grow_connections", 12);
  invoke_noarg(object, "close_all_connections");
}

TEST_F(DbIoTest, AuditConnectionGenerationRejectsStalePromiseRequest) {
  auto* object = load_object_for_test("single/tests/efuns/audit_db_identity");
  ASSERT_NE(object, nullptr);
  ProbeGuard probe(object);
  prepare_probe(object);

  PauseGuard pause;
  auto* result = safe_apply("start_promise", object, 0, ORIGIN_DRIVER);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_PROMISE);
  auto* promise = result->u.prom;
  promise->ref++;
  vm_apply_return_clear();
  ASSERT_TRUE(pause.wait()) << "worker did not reach the database lock barrier";
  close_and_reopen(object);
  pause.release();

  complete_all_asyncio();
  ASSERT_EQ(promise->state, PROMISE_REJECTED);
  ASSERT_EQ(promise->result.type, T_STRING);
  EXPECT_NE(std::string(promise->result.u.string).find("stale database connection"),
            std::string::npos);
  promise->handled = true;
  free_promise(promise);
}
