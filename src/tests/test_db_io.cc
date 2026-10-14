#include "test_lpc_support.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
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

#if defined(USE_MYSQL)
enum MySqlMode {
  kMySqlNormal,
  kMySqlInitFailure,
  kMySqlRealConnectFailure,
  kMySqlSelectFailure,
};

std::atomic<int> mysql_mode{kMySqlNormal};
std::atomic<int> mysql_query_failure{0};
std::atomic<int> mysql_init_calls{0};
std::atomic<int> mysql_real_connect_calls{0};
std::atomic<int> mysql_select_db_calls{0};
std::atomic<int> mysql_error_calls{0};
std::atomic<int> mysql_query_calls{0};
std::atomic<int> mysql_store_result_calls{0};
std::atomic<int> mysql_num_rows_calls{0};
std::atomic<int> mysql_errno_calls{0};
std::atomic<int> mysql_free_result_calls{0};
std::atomic<int> mysql_close_calls{0};
#endif

#if defined(USE_POSTGRES)
enum PostgresConnectMode {
  kPostgresNormal,
  kPostgresNullConnect,
  kPostgresFailedConnect,
};

std::atomic<int> postgres_connect_mode{kPostgresNormal};
std::atomic<int> postgres_exec_status{PGRES_TUPLES_OK};
std::atomic<int> postgres_connect_calls{0};
std::atomic<int> postgres_status_calls{0};
std::atomic<int> postgres_error_calls{0};
std::atomic<int> postgres_finish_calls{0};
std::atomic<int> postgres_exec_calls{0};
std::atomic<int> postgres_result_status_calls{0};
std::atomic<int> postgres_ntuples_calls{0};
std::atomic<int> postgres_result_error_calls{0};
std::atomic<int> postgres_clear_calls{0};
std::atomic<int> postgres_null_clear_calls{0};
#endif

#if defined(USE_SQLITE3)
enum SqliteOpenMode {
  kSqliteNormal,
  kSqliteInjectedFailure,
  kSqliteInjectedNullFailure,
};

std::atomic<int> sqlite_open_mode{kSqliteNormal};
std::atomic<int> sqlite_open_calls{0};
std::atomic<int> sqlite_errmsg_calls{0};
std::atomic<int> sqlite_close_calls{0};
std::atomic<int> sqlite_finalize_calls{0};
std::atomic<int> sqlite_null_finalize_calls{0};
#endif

void reset_db_api_probe() {
#if defined(USE_MYSQL)
  mysql_mode.store(kMySqlNormal);
  mysql_query_failure.store(0);
  mysql_init_calls.store(0);
  mysql_real_connect_calls.store(0);
  mysql_select_db_calls.store(0);
  mysql_error_calls.store(0);
  mysql_query_calls.store(0);
  mysql_store_result_calls.store(0);
  mysql_num_rows_calls.store(0);
  mysql_errno_calls.store(0);
  mysql_free_result_calls.store(0);
  mysql_close_calls.store(0);
#endif
#if defined(USE_POSTGRES)
  postgres_connect_mode.store(kPostgresNormal);
  postgres_exec_status.store(PGRES_TUPLES_OK);
  postgres_connect_calls.store(0);
  postgres_status_calls.store(0);
  postgres_error_calls.store(0);
  postgres_finish_calls.store(0);
  postgres_exec_calls.store(0);
  postgres_result_status_calls.store(0);
  postgres_ntuples_calls.store(0);
  postgres_result_error_calls.store(0);
  postgres_clear_calls.store(0);
  postgres_null_clear_calls.store(0);
#endif
#if defined(USE_SQLITE3)
  sqlite_open_mode.store(kSqliteNormal);
  sqlite_open_calls.store(0);
  sqlite_errmsg_calls.store(0);
  sqlite_close_calls.store(0);
  sqlite_finalize_calls.store(0);
  sqlite_null_finalize_calls.store(0);
#endif
}

object_t* load_cleanup_probe() {
  return load_object_for_test("single/tests/efuns/audit_db_cleanup");
}

void assert_error_result(object_t* object, const char* method) {
  auto* result = safe_apply(method, object, 0, ORIGIN_DRIVER);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_STRING);
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

#if defined(USE_MYSQL)
extern "C" MYSQL* __wrap_mysql_init(MYSQL* mysql) {
  ++mysql_init_calls;
  if (mysql_mode.load() == kMySqlInitFailure) {
    return nullptr;
  }
  return mysql;
}

extern "C" MYSQL* __wrap_mysql_real_connect(MYSQL* mysql, const char*, const char*, const char*,
                                               const char*, unsigned int, const char*,
                                               unsigned long) {
  ++mysql_real_connect_calls;
  if (mysql_mode.load() == kMySqlRealConnectFailure || !mysql) {
    return nullptr;
  }
  return mysql;
}

extern "C" int __wrap_mysql_select_db(MYSQL*, const char*) {
  ++mysql_select_db_calls;
  return mysql_mode.load() == kMySqlSelectFailure ? 1 : 0;
}

extern "C" const char* __wrap_mysql_error(MYSQL*) {
  ++mysql_error_calls;
  if (mysql_mode.load() == kMySqlInitFailure) {
    return "mysql_init failure";
  }
  if (mysql_mode.load() == kMySqlRealConnectFailure) {
    return "mysql_real_connect failure";
  }
  if (mysql_mode.load() == kMySqlSelectFailure) {
    return "mysql_select_db failure";
  }
  return mysql_query_failure.load() ? "mysql_query failure" : "mysql error";
}

extern "C" int __wrap_mysql_query(MYSQL*, const char*) {
  ++mysql_query_calls;
  return mysql_query_failure.load() ? 1 : 0;
}

extern "C" MYSQL_RES* __wrap_mysql_store_result(MYSQL*) {
  ++mysql_store_result_calls;
  return reinterpret_cast<MYSQL_RES*>(static_cast<uintptr_t>(0x1201));
}

extern "C" my_ulonglong __wrap_mysql_num_rows(MYSQL_RES*) {
  ++mysql_num_rows_calls;
  return 2;
}

extern "C" unsigned int __wrap_mysql_errno(MYSQL*) {
  ++mysql_errno_calls;
  return mysql_query_failure.load() ? 1 : 0;
}

extern "C" void __wrap_mysql_free_result(MYSQL_RES*) {
  ++mysql_free_result_calls;
}

extern "C" void __wrap_mysql_close(MYSQL*) {
  ++mysql_close_calls;
}
#endif

#if defined(USE_POSTGRES)
extern "C" PGconn* __wrap_PQconnectdbParams(const char* const*, const char* const*, int) {
  ++postgres_connect_calls;
  switch (postgres_connect_mode.load()) {
    case kPostgresNullConnect:
      return nullptr;
    case kPostgresFailedConnect:
      return reinterpret_cast<PGconn*>(static_cast<uintptr_t>(0x1301));
    default:
      return reinterpret_cast<PGconn*>(static_cast<uintptr_t>(0x1302));
  }
}

extern "C" ConnStatusType __wrap_PQstatus(const PGconn* conn) {
  ++postgres_status_calls;
  if (!conn || postgres_connect_mode.load() == kPostgresFailedConnect) {
    return CONNECTION_BAD;
  }
  return CONNECTION_OK;
}

extern "C" char* __wrap_PQerrorMessage(const PGconn*) {
  ++postgres_error_calls;
  static char error[] = "postgres connection failure";
  return error;
}

extern "C" void __wrap_PQfinish(PGconn*) {
  ++postgres_finish_calls;
}

extern "C" PGresult* __wrap_PQexec(PGconn* conn, const char*) {
  ++postgres_exec_calls;
  if (!conn) {
    return nullptr;
  }
  return reinterpret_cast<PGresult*>(static_cast<uintptr_t>(0x1303));
}

extern "C" ExecStatusType __wrap_PQresultStatus(const PGresult*) {
  ++postgres_result_status_calls;
  const auto configured = static_cast<ExecStatusType>(postgres_exec_status.load());
  if (configured != PGRES_TUPLES_OK) {
    return configured;
  }
  return postgres_exec_calls.load() == 1 ? PGRES_TUPLES_OK : PGRES_COMMAND_OK;
}

extern "C" int __wrap_PQntuples(const PGresult*) {
  ++postgres_ntuples_calls;
  return 2;
}

extern "C" char* __wrap_PQresultErrorMessage(const PGresult*) {
  ++postgres_result_error_calls;
  static char error[] = "postgres query failure";
  return error;
}

extern "C" void __wrap_PQclear(PGresult* result) {
  ++postgres_clear_calls;
  if (!result) {
    ++postgres_null_clear_calls;
  }
}
#endif

#if defined(USE_SQLITE3)
extern "C" int __real_sqlite3_open(const char*, sqlite3**);
extern "C" const char* __real_sqlite3_errmsg(sqlite3*);
extern "C" int __real_sqlite3_close(sqlite3*);
extern "C" int __real_sqlite3_finalize(sqlite3_stmt*);

extern "C" int __wrap_sqlite3_open(const char* filename, sqlite3** database) {
  ++sqlite_open_calls;
  if (sqlite_open_mode.load() == kSqliteInjectedFailure) {
    *database = reinterpret_cast<sqlite3*>(static_cast<uintptr_t>(0x1401));
    return SQLITE_CANTOPEN;
  }
  if (sqlite_open_mode.load() == kSqliteInjectedNullFailure) {
    *database = nullptr;
    return SQLITE_CANTOPEN;
  }
  return __real_sqlite3_open(filename, database);
}

extern "C" const char* __wrap_sqlite3_errmsg(sqlite3* database) {
  ++sqlite_errmsg_calls;
  if (!database || database == reinterpret_cast<sqlite3*>(static_cast<uintptr_t>(0x1401))) {
    return "injected sqlite open failure";
  }
  return __real_sqlite3_errmsg(database);
}

extern "C" int __wrap_sqlite3_close(sqlite3* database) {
  ++sqlite_close_calls;
  if (!database || database == reinterpret_cast<sqlite3*>(static_cast<uintptr_t>(0x1401))) {
    return SQLITE_OK;
  }
  return __real_sqlite3_close(database);
}

extern "C" int __wrap_sqlite3_finalize(sqlite3_stmt* statement) {
  ++sqlite_finalize_calls;
  if (!statement) {
    ++sqlite_null_finalize_calls;
    return SQLITE_OK;
  }
  return __real_sqlite3_finalize(statement);
}
#endif

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

#if defined(USE_SQLITE3)
TEST_F(DbIoTest, AuditBackendCleanupSqliteOpenFailureClosesAndNullsPartialHandle) {
  reset_db_api_probe();
  sqlite_open_mode.store(kSqliteInjectedFailure);
  auto* object = load_cleanup_probe();
  ASSERT_NE(object, nullptr);
  ProbeGuard probe(object);

  assert_error_result(object, "sqlite_open_failure");

  EXPECT_EQ(sqlite_open_calls.load(), 1);
  EXPECT_EQ(sqlite_errmsg_calls.load(), 1);
  EXPECT_EQ(sqlite_close_calls.load(), 1);
  EXPECT_EQ(sqlite_finalize_calls.load(), 0);
}

TEST_F(DbIoTest, AuditBackendCleanupSqliteNullOpenFailureDoesNotCloseNullHandle) {
  reset_db_api_probe();
  sqlite_open_mode.store(kSqliteInjectedNullFailure);
  auto* object = load_cleanup_probe();
  ASSERT_NE(object, nullptr);
  ProbeGuard probe(object);

  assert_error_result(object, "sqlite_open_failure");

  EXPECT_EQ(sqlite_open_calls.load(), 1);
  EXPECT_EQ(sqlite_errmsg_calls.load(), 0);
  EXPECT_EQ(sqlite_close_calls.load(), 0);
}

TEST_F(DbIoTest, AuditBackendCleanupSqliteResultsAreReleasedBeforeReuseAndClose) {
  reset_db_api_probe();
  auto* object = load_cleanup_probe();
  ASSERT_NE(object, nullptr);
  ProbeGuard probe(object);

  invoke_number(object, "sqlite_exec_sequence", 1);

  EXPECT_EQ(sqlite_finalize_calls.load(), 2);
  EXPECT_EQ(sqlite_null_finalize_calls.load(), 0);
  EXPECT_EQ(sqlite_close_calls.load(), 1);
}

TEST_F(DbIoTest, AuditBackendCleanupSqliteFailedExecuteReleasesPreviousResult) {
  reset_db_api_probe();
  auto* object = load_cleanup_probe();
  ASSERT_NE(object, nullptr);
  ProbeGuard probe(object);

  invoke_number(object, "sqlite_exec_failure", 1);

  EXPECT_EQ(sqlite_finalize_calls.load(), 1);
  EXPECT_EQ(sqlite_null_finalize_calls.load(), 0);
  EXPECT_EQ(sqlite_close_calls.load(), 1);
}
#endif

#if defined(USE_MYSQL)
TEST_F(DbIoTest, AuditBackendCleanupMysqlInitFailureDoesNotCallLaterApis) {
  reset_db_api_probe();
  mysql_mode.store(kMySqlInitFailure);
  auto* object = load_cleanup_probe();
  ASSERT_NE(object, nullptr);
  ProbeGuard probe(object);

  assert_error_result(object, "mysql_init_failure");

  EXPECT_EQ(mysql_init_calls.load(), 1);
  EXPECT_EQ(mysql_real_connect_calls.load(), 0);
  EXPECT_EQ(mysql_error_calls.load(), 0);
  EXPECT_EQ(mysql_close_calls.load(), 0);
}

TEST_F(DbIoTest, AuditBackendCleanupMysqlRealConnectFailureClosesTemporaryHandle) {
  reset_db_api_probe();
  mysql_mode.store(kMySqlRealConnectFailure);
  auto* object = load_cleanup_probe();
  ASSERT_NE(object, nullptr);
  ProbeGuard probe(object);

  assert_error_result(object, "mysql_real_connect_failure");

  EXPECT_EQ(mysql_init_calls.load(), 1);
  EXPECT_EQ(mysql_real_connect_calls.load(), 1);
  EXPECT_EQ(mysql_error_calls.load(), 1);
  EXPECT_EQ(mysql_close_calls.load(), 1);
}

TEST_F(DbIoTest, AuditBackendCleanupMysqlSelectFailureClosesConnectionOnce) {
  reset_db_api_probe();
  mysql_mode.store(kMySqlSelectFailure);
  auto* object = load_cleanup_probe();
  ASSERT_NE(object, nullptr);
  ProbeGuard probe(object);

  assert_error_result(object, "mysql_select_failure");

  EXPECT_EQ(mysql_init_calls.load(), 1);
  EXPECT_EQ(mysql_real_connect_calls.load(), 1);
  EXPECT_EQ(mysql_select_db_calls.load(), 1);
  EXPECT_EQ(mysql_error_calls.load(), 1);
  EXPECT_EQ(mysql_close_calls.load(), 1);
}

TEST_F(DbIoTest, AuditBackendCleanupMysqlResultsAreReleasedBeforeReuseAndClose) {
  reset_db_api_probe();
  auto* object = load_cleanup_probe();
  ASSERT_NE(object, nullptr);
  ProbeGuard probe(object);

  invoke_number(object, "mysql_exec_sequence", 1);

  EXPECT_EQ(mysql_query_calls.load(), 2);
  EXPECT_EQ(mysql_store_result_calls.load(), 2);
  EXPECT_EQ(mysql_free_result_calls.load(), 2);
  EXPECT_EQ(mysql_close_calls.load(), 1);
}

TEST_F(DbIoTest, AuditBackendCleanupMysqlFailedExecuteClosesConnection) {
  reset_db_api_probe();
  mysql_query_failure.store(1);
  auto* object = load_cleanup_probe();
  ASSERT_NE(object, nullptr);
  ProbeGuard probe(object);

  invoke_number(object, "mysql_exec_failure", 1);

  EXPECT_EQ(mysql_query_calls.load(), 1);
  EXPECT_EQ(mysql_errno_calls.load(), 0);
  EXPECT_EQ(mysql_error_calls.load(), 1);
  EXPECT_EQ(mysql_free_result_calls.load(), 0);
  EXPECT_EQ(mysql_close_calls.load(), 1);
}
#endif

#if defined(USE_POSTGRES)
TEST_F(DbIoTest, AuditBackendCleanupPostgresNullConnectDoesNotCallStatus) {
  reset_db_api_probe();
  postgres_connect_mode.store(kPostgresNullConnect);
  auto* object = load_cleanup_probe();
  ASSERT_NE(object, nullptr);
  ProbeGuard probe(object);

  assert_error_result(object, "postgres_null_connect");

  EXPECT_EQ(postgres_connect_calls.load(), 1);
  EXPECT_EQ(postgres_status_calls.load(), 0);
  EXPECT_EQ(postgres_error_calls.load(), 0);
  EXPECT_EQ(postgres_finish_calls.load(), 0);
}

TEST_F(DbIoTest, AuditBackendCleanupPostgresFailedConnectFinishesConnection) {
  reset_db_api_probe();
  postgres_connect_mode.store(kPostgresFailedConnect);
  auto* object = load_cleanup_probe();
  ASSERT_NE(object, nullptr);
  ProbeGuard probe(object);

  assert_error_result(object, "postgres_failed_connect");

  EXPECT_EQ(postgres_connect_calls.load(), 1);
  EXPECT_EQ(postgres_status_calls.load(), 1);
  EXPECT_EQ(postgres_error_calls.load(), 1);
  EXPECT_EQ(postgres_finish_calls.load(), 1);
}

TEST_F(DbIoTest, AuditBackendCleanupPostgresResultsAreReleasedBeforeReuseAndClose) {
  reset_db_api_probe();
  auto* object = load_cleanup_probe();
  ASSERT_NE(object, nullptr);
  ProbeGuard probe(object);

  invoke_number(object, "postgres_exec_sequence", 1);

  EXPECT_EQ(postgres_exec_calls.load(), 2);
  EXPECT_EQ(postgres_result_status_calls.load(), 2);
  EXPECT_EQ(postgres_ntuples_calls.load(), 1);
  EXPECT_EQ(postgres_clear_calls.load(), 2);
  EXPECT_EQ(postgres_null_clear_calls.load(), 0);
  EXPECT_EQ(postgres_finish_calls.load(), 1);
}

TEST_F(DbIoTest, AuditBackendCleanupPostgresFailedExecuteReleasesResultOnClose) {
  reset_db_api_probe();
  postgres_exec_status.store(PGRES_FATAL_ERROR);
  auto* object = load_cleanup_probe();
  ASSERT_NE(object, nullptr);
  ProbeGuard probe(object);

  invoke_number(object, "postgres_exec_failure", 1);

  EXPECT_EQ(postgres_exec_calls.load(), 1);
  EXPECT_EQ(postgres_result_error_calls.load(), 1);
  EXPECT_EQ(postgres_error_calls.load(), 1);
  EXPECT_EQ(postgres_clear_calls.load(), 1);
  EXPECT_EQ(postgres_null_clear_calls.load(), 0);
  EXPECT_EQ(postgres_finish_calls.load(), 1);
}
#endif
