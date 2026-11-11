#include "test_lpc_support.h"

TEST_F(DriverTest, TestVmOwnerThreadRunsRegisteredReadonlyLpcTaskWithMultipleWorkers) {
  const char* owner = "owner/test/thread/lpc-task";
  const char* other_owner = "owner/test/thread/lpc-task-other";
  ASSERT_TRUE(vm_context_is_main_thread());

  vm_owner_thread_stop();
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);
  auto owner_epoch = vm_owner_epoch(probe);

  auto mapping_entry = [](mapping_t* map, const char* key) -> mapping_t* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_MAPPING);
    return value && value->type == T_MAPPING ? value->u.map : nullptr;
  };

  auto* before = vm_owner_thread_status();
  auto before_executed = mapping_number(before, "thread_lpc_task_executed");
  auto before_succeeded = mapping_number(before, "thread_lpc_task_succeeded");
  auto before_failed = mapping_number(before, "thread_lpc_task_failed");
  auto before_rejected = mapping_number(before, "thread_lpc_task_rejected");
  auto before_owner_cleared = mapping_number(before, "thread_owner_cleared");
  auto before_execution_cleared = mapping_number(before, "thread_execution_cleared");
  auto before_controlled_lpc_cleared = mapping_number(before, "thread_lpc_canary_flag_cleared");
  auto before_context_leaks = mapping_number(before, "thread_context_leak_detected");
  auto before_claims = mapping_number(before, "executor_owner_claims");
  auto before_releases = mapping_number(before, "executor_owner_releases");
  free_mapping(before);

  auto* submitted = vm_owner_lpc_task(probe, owner, "owner_task_readonly");
  auto task_id = mapping_number(submitted, "task_id");
  auto future_id = mapping_number(submitted, "future_id");
  ASSERT_EQ(mapping_number(submitted, "success"), 1);
  ASSERT_GT(future_id, 0);
  ASSERT_EQ(mapping_number(submitted, "requires_owner_thread"), 1);
  ASSERT_EQ(mapping_number(submitted, "requires_owner_message_completion"), 1);
  ASSERT_EQ(mapping_number(submitted, "payload_frozen"), 1);
  ASSERT_EQ(mapping_number(submitted, "registered_task"), 1);
  ASSERT_EQ(mapping_number(submitted, "frozen_result_required"), 1);
  ASSERT_EQ(mapping_number(submitted, "ordinary_lpc_default_closed"), 1);
  ASSERT_EQ(mapping_number(submitted, "ordinary_lpc_activation_policy_ready"), 1);
  ASSERT_STREQ(mapping_string(submitted, "ordinary_lpc_activation_policy"),
               "default_closed_explicit_open");
  ASSERT_STREQ(mapping_string(submitted, "ordinary_lpc_next_blocker"), "");
  ASSERT_EQ(mapping_number(submitted, "direct_cross_owner_write"), 0);
  ASSERT_EQ(mapping_number(submitted, "owner_epoch"), static_cast<long>(owner_epoch));
  ASSERT_STREQ(mapping_string(submitted, "task_type"), "lpc_task");
  ASSERT_STREQ(mapping_string(submitted, "method"), "owner_task_readonly");
  ASSERT_STREQ(mapping_string(submitted, "executor_mode"), "executor_safe_allowlist");
  ASSERT_STREQ(mapping_string(submitted, "route"), "owner_executor");
  ASSERT_STREQ(mapping_string(submitted, "result_policy"), "frozen_result_required");
  ASSERT_STREQ(mapping_string(submitted, "contract_reason"),
               "registered readonly owner task with frozen result");
  auto* submitted_contract = mapping_entry(submitted, "task_contract");
  ASSERT_STREQ(mapping_string(submitted_contract, "method"), "owner_task_readonly");
  ASSERT_EQ(mapping_number(submitted_contract, "executor_safe"), 1);
  ASSERT_EQ(mapping_number(submitted_contract, "requires_target"), 1);
  ASSERT_EQ(mapping_number(submitted_contract, "frozen_result_required"), 1);
  free_mapping(submitted);
  auto* pending_future = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(pending_future, "success"), 1);
  ASSERT_STREQ(mapping_string(pending_future, "state"), "pending");
  ASSERT_EQ(mapping_number(pending_future, "target_task_id"), task_id);
  ASSERT_EQ(mapping_number(pending_future, "requires_owner_message_completion"), 1);
  free_mapping(pending_future);
  auto* pending_runtime = vm_owner_runtime_status();
  ASSERT_GE(mapping_number(pending_runtime, "pending_futures"), 1);
  ASSERT_EQ(mapping_number(pending_runtime, "lpc_task_allowlist_count"), 18);
  auto* runtime_allowlist = find_string_in_mapping(pending_runtime, "lpc_task_allowlist");
  ASSERT_NE(runtime_allowlist, nullptr);
  ASSERT_EQ(runtime_allowlist->type, T_ARRAY);
  ASSERT_EQ(runtime_allowlist->u.arr->size, 18);
  ASSERT_EQ(runtime_allowlist->u.arr->item[0].type, T_STRING);
  ASSERT_STREQ(runtime_allowlist->u.arr->item[0].u.string, "owner_task_readonly");
  bool runtime_has_player_domain = false;
  bool runtime_has_economy_domain = false;
  for (int i = 0; i < runtime_allowlist->u.arr->size; i++) {
    ASSERT_EQ(runtime_allowlist->u.arr->item[i].type, T_STRING);
    auto method = std::string(runtime_allowlist->u.arr->item[i].u.string);
    runtime_has_player_domain = runtime_has_player_domain || method == "owner_task_player";
    runtime_has_economy_domain = runtime_has_economy_domain || method == "owner_task_economy";
  }
  ASSERT_TRUE(runtime_has_player_domain);
  ASSERT_TRUE(runtime_has_economy_domain);
  ASSERT_EQ(mapping_number(pending_runtime, "registered_owner_task_domains_ready"), 1);
  ASSERT_EQ(mapping_number(pending_runtime, "registered_owner_task_domain_count"), 18);
  ASSERT_EQ(mapping_number(pending_runtime, "target_owner_message_executor_ready"), 1);
  ASSERT_EQ(mapping_number(pending_runtime, "normal_path_main_fallback_count"), 0);
  ASSERT_EQ(mapping_number(pending_runtime, "normal_path_main_fallback_ready"), 1);
  ASSERT_EQ(mapping_number(pending_runtime, "service_shard_executor_ready"), 1);
  ASSERT_EQ(mapping_number(pending_runtime, "domain_task_registry_mudlib_aligned"), 1);
  ASSERT_EQ(mapping_number(pending_runtime, "keyed_service_shard_ready"), 1);
  ASSERT_EQ(mapping_number(pending_runtime, "hot_path_service_owner_single_point"), 0);
  ASSERT_EQ(mapping_number(pending_runtime, "target_owner_message_main_fallback"), 0);
  ASSERT_EQ(mapping_number(pending_runtime, "production_perfect_contract_ready"), 1);
  ASSERT_EQ(mapping_number(pending_runtime, "facade_only_runtime_claims"), 0);
  free_mapping(pending_runtime);
  auto* pending_thread_status = vm_owner_thread_status();
  ASSERT_GE(mapping_number(pending_thread_status, "pending_futures"), 1);
  ASSERT_EQ(mapping_number(pending_thread_status, "lpc_task_allowlist_count"), 18);
  auto* thread_allowlist = find_string_in_mapping(pending_thread_status, "lpc_task_allowlist");
  ASSERT_NE(thread_allowlist, nullptr);
  ASSERT_EQ(thread_allowlist->type, T_ARRAY);
  ASSERT_EQ(thread_allowlist->u.arr->size, 18);
  ASSERT_EQ(thread_allowlist->u.arr->item[0].type, T_STRING);
  ASSERT_STREQ(thread_allowlist->u.arr->item[0].u.string, "owner_task_readonly");
  bool thread_has_player_domain = false;
  bool thread_has_economy_domain = false;
  for (int i = 0; i < thread_allowlist->u.arr->size; i++) {
    ASSERT_EQ(thread_allowlist->u.arr->item[i].type, T_STRING);
    auto method = std::string(thread_allowlist->u.arr->item[i].u.string);
    thread_has_player_domain = thread_has_player_domain || method == "owner_task_player";
    thread_has_economy_domain = thread_has_economy_domain || method == "owner_task_economy";
  }
  ASSERT_TRUE(thread_has_player_domain);
  ASSERT_TRUE(thread_has_economy_domain);
  ASSERT_EQ(mapping_number(pending_thread_status, "registered_owner_task_domains_ready"), 1);
  ASSERT_EQ(mapping_number(pending_thread_status, "registered_owner_task_domain_count"), 18);
  ASSERT_EQ(mapping_number(pending_thread_status, "target_owner_message_executor_ready"), 1);
  ASSERT_EQ(mapping_number(pending_thread_status, "normal_path_main_fallback_count"), 0);
  ASSERT_EQ(mapping_number(pending_thread_status, "normal_path_main_fallback_ready"), 1);
  ASSERT_EQ(mapping_number(pending_thread_status, "service_shard_executor_ready"), 1);
  ASSERT_EQ(mapping_number(pending_thread_status, "domain_task_registry_mudlib_aligned"), 1);
  ASSERT_EQ(mapping_number(pending_thread_status, "keyed_service_shard_ready"), 1);
  ASSERT_EQ(mapping_number(pending_thread_status, "hot_path_service_owner_single_point"), 0);
  ASSERT_EQ(mapping_number(pending_thread_status, "target_owner_message_main_fallback"), 0);
  ASSERT_EQ(mapping_number(pending_thread_status, "production_perfect_contract_ready"), 1);
  ASSERT_EQ(mapping_number(pending_thread_status, "facade_only_runtime_claims"), 0);
  free_mapping(pending_thread_status);

  auto other_task = vm_owner_enqueue_task(other_owner, "owner_state", "other-owner-state");
  ASSERT_GT(other_task, 0u);

  vm_owner_thread_start(2);
  for (int i = 0; i < 100; i++) {
    auto* status = vm_owner_mailbox_status(owner);
    auto owner_depth = mapping_number(status, "owner_queue_depth");
    free_mapping(status);
    status = vm_owner_mailbox_status(other_owner);
    auto other_depth = mapping_number(status, "owner_queue_depth");
    free_mapping(status);
    auto* polled = vm_owner_future_poll(static_cast<uint64_t>(future_id));
    auto completed = std::string(mapping_string(polled, "state")) == "completed";
    free_mapping(polled);
    if (owner_depth == 0 && other_depth == 0 && completed) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  auto* running = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(running, "enabled"), 1);
  ASSERT_EQ(mapping_number(running, "thread_count"), 2);
  ASSERT_GE(mapping_number(running, "max_owner_threads"), 2);
  ASSERT_GE(mapping_number(running, "thread_lpc_task_executed"), before_executed + 1);
  ASSERT_GE(mapping_number(running, "thread_lpc_task_succeeded"), before_succeeded + 1);
  ASSERT_EQ(mapping_number(running, "thread_lpc_task_failed"), before_failed);
  ASSERT_EQ(mapping_number(running, "thread_lpc_task_rejected"), before_rejected);
  ASSERT_GE(mapping_number(running, "thread_owner_cleared"), before_owner_cleared + 1);
  ASSERT_GE(mapping_number(running, "thread_execution_cleared"), before_execution_cleared + 1);
  ASSERT_GE(mapping_number(running, "thread_lpc_canary_flag_cleared"),
            before_controlled_lpc_cleared + 1);
  ASSERT_EQ(mapping_number(running, "thread_context_leak_detected"), before_context_leaks);
  ASSERT_EQ(mapping_number(running, "active_owners"), 0);
  ASSERT_GE(mapping_number(running, "executor_owner_claims"), before_claims + 2);
  ASSERT_GE(mapping_number(running, "executor_owner_releases"), before_releases + 2);
  ASSERT_EQ(mapping_number(running, "executor_owner_claims") - before_claims,
            mapping_number(running, "executor_owner_releases") - before_releases);
  free_mapping(running);

  auto* trace = vm_owner_task_trace(24);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  int lpc_task_succeeded = 0;
  for (int i = 0; i < events->u.arr->size; i++) {
    auto* event = events->u.arr->item[i].u.map;
    if (mapping_number(event, "task_id") == task_id &&
        std::string(mapping_string(event, "state")) == "thread_lpc_task_succeeded") {
      lpc_task_succeeded = 1;
    }
  }
  ASSERT_EQ(lpc_task_succeeded, 1);
  free_mapping(trace);

  auto* completed_future = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(completed_future, "success"), 1);
  ASSERT_STREQ(mapping_string(completed_future, "state"), "completed");
  ASSERT_STREQ(mapping_string(completed_future, "result_key"), "owner_task_readonly");
  ASSERT_EQ(mapping_number(completed_future, "requires_owner_message_completion"), 0);
  ASSERT_EQ(mapping_number(completed_future, "frozen_result"), 1);
  auto* result = find_string_in_mapping(completed_future, "result");
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_NUMBER);
  ASSERT_EQ(result->u.number, 1);
  free_mapping(completed_future);
  auto* completed_runtime = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(completed_runtime, "pending_futures"), 0);
  free_mapping(completed_runtime);
  auto* completed_thread_status = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(completed_thread_status, "pending_futures"), 0);
  free_mapping(completed_thread_status);

  vm_owner_thread_stop();
  destruct_object(probe);
}

TEST_F(DriverTest, TestVmOwnerLpcTaskRejectsTargetOwnerMismatchAtSubmit) {
  const char* owner = "owner/test/thread/lpc-task-submit-owner";
  const char* other_owner = "owner/test/thread/lpc-task-submit-other";

  vm_owner_thread_stop();
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);


  auto* before_runtime = vm_owner_runtime_status();
  auto before_pending_futures = mapping_number(before_runtime, "pending_futures");
  free_mapping(before_runtime);
  auto* before_owner_queue = vm_owner_mailbox_status(owner);
  auto before_owner_depth = mapping_number(before_owner_queue, "owner_queue_depth");
  free_mapping(before_owner_queue);
  auto* before_other_queue = vm_owner_mailbox_status(other_owner);
  auto before_other_depth = mapping_number(before_other_queue, "owner_queue_depth");
  free_mapping(before_other_queue);

  auto* submitted = vm_owner_lpc_task(probe, other_owner, "owner_task_readonly");
  ASSERT_EQ(mapping_number(submitted, "success"), 0);
  ASSERT_EQ(mapping_number(submitted, "future_id"), 0);
  ASSERT_EQ(mapping_number(submitted, "task_id"), 0);
  ASSERT_STREQ(mapping_string(submitted, "owner_id"), other_owner);
  ASSERT_STREQ(mapping_string(submitted, "target_owner_id"), owner);
  ASSERT_STREQ(mapping_string(submitted, "state"), "rejected");
  ASSERT_STREQ(mapping_string(submitted, "error"), "owner lpc task target owner mismatch");
  ASSERT_STREQ(mapping_string(submitted, "executor_mode"), "rejected");
  ASSERT_EQ(mapping_number(submitted, "requires_owner_thread"), 0);
  ASSERT_EQ(mapping_number(submitted, "requires_owner_message_completion"), 0);
  ASSERT_EQ(mapping_number(submitted, "direct_cross_owner_write"), 0);
  free_mapping(submitted);

  auto* after_runtime = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(after_runtime, "pending_futures"), before_pending_futures);
  free_mapping(after_runtime);
  auto* after_owner_queue = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(after_owner_queue, "owner_queue_depth"), before_owner_depth);
  free_mapping(after_owner_queue);
  auto* after_other_queue = vm_owner_mailbox_status(other_owner);
  ASSERT_EQ(mapping_number(after_other_queue, "owner_queue_depth"), before_other_depth);
  free_mapping(after_other_queue);

  destruct_object(probe);
}

// F02: When the future store is at terminal capacity, every submission path
// must reject atomically: no enqueue, no trace/message index, no accepted
// future with ID 0, and a stable rejected result.
TEST_F(DriverTest, TestOwnerSubmissionRejectsAtomicallyWhenFutureStoreFull) {
  const char* owner = "owner/test/future/capacity-reject";

  vm_owner_thread_stop();
  // Suite-order independent baseline: earlier tests may legitimately leave
  // terminal/pending records; this test must only assert that its own fill
  // and the four rejection paths leave the store exactly as they found it.
  const auto baseline_terminal = owner_future_store_instance().terminal_record_count();
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);


  // Fill the future store to the terminal cap with payload-bearing records
  // (payload-bearing terminal records are never auto-reaped).
  const auto cap = OwnerFutureStore::kMaxTerminalRecords;
  const uint64_t fill_base = 900000;
  std::vector<uint64_t> fill_ids;
  fill_ids.reserve(cap);
  {
    auto& store = owner_future_store_instance();
    for (size_t i = 0; i < cap; i++) {
      auto record = owner_future_store_test_record(fill_base + i, fill_base + i);
      record.state = OwnerFutureState::kCompleted;
      record.terminal_at_ns = 1;
      record.result = std::make_shared<VMFrozenValue>();
      if (store.restore_terminal_checked(std::move(record))) {
        fill_ids.push_back(fill_base + i);
      }
    }
    ASSERT_EQ(store.terminal_record_count(), cap);
  }

  auto free_fill_records = [&]() {
    auto& store = owner_future_store_instance();
    for (auto future_id : fill_ids) {
      store.take(future_id);
    }
    fill_ids.clear();
  };

  const auto before_frozen = owner_future_store_instance().size();

  // Path 1: frozen-string callback submission.
  std::atomic<int> projector_runs{0};
  const auto string_submission =
      vm_owner_submit_frozen_string_task(probe, "room_output_projection", "capacity-reject-string",
                                         [&projector_runs](std::string* output) {
                                           projector_runs.fetch_add(1, std::memory_order_relaxed);
                                           *output = "must-not-run";
                                           return true;
                                         });
  ASSERT_FALSE(string_submission.queued);
  ASSERT_EQ(string_submission.task_id, 0u);
  ASSERT_EQ(string_submission.future_id, 0u);
  ASSERT_EQ(projector_runs.load(std::memory_order_relaxed), 0);

  // Path 2: LPC task submission.
  auto* lpc_submission = vm_owner_lpc_task(probe, owner, "owner_task_readonly");
  ASSERT_EQ(mapping_number(lpc_submission, "success"), 0);
  ASSERT_EQ(mapping_number(lpc_submission, "future_id"), 0);
  ASSERT_EQ(mapping_number(lpc_submission, "task_id"), 0);
  ASSERT_STREQ(mapping_string(lpc_submission, "state"), "rejected");
  ASSERT_STREQ(mapping_string(lpc_submission, "error"), "future_store_capacity");
  free_mapping(lpc_submission);

  // Path 3: ordinary LPC submission.
  auto* ordinary_submission = vm_owner_ordinary_lpc_task(probe, owner, "owner_task_player", 1);
  ASSERT_EQ(mapping_number(ordinary_submission, "success"), 0);
  ASSERT_EQ(mapping_number(ordinary_submission, "future_id"), 0);
  ASSERT_EQ(mapping_number(ordinary_submission, "task_id"), 0);
  ASSERT_STREQ(mapping_string(ordinary_submission, "state"), "rejected");
  ASSERT_STREQ(mapping_string(ordinary_submission, "error"), "future_store_capacity");
  free_mapping(ordinary_submission);

  // Path 4: owner message submission (handle-less path).
  auto* message_submission =
      vm_owner_submit_message("owner/test/future/source", owner, "message", "capacity/reject");
  ASSERT_EQ(mapping_number(message_submission, "success"), 0);
  ASSERT_EQ(mapping_number(message_submission, "message_id"), 0);
  ASSERT_EQ(mapping_number(message_submission, "target_task_id"), 0);
  ASSERT_STREQ(mapping_string(message_submission, "state"), "rejected");
  ASSERT_STREQ(mapping_string(message_submission, "error"), "future_store_capacity");
  free_mapping(message_submission);

  // No task may have been enqueued for any rejected path.
  auto* mailbox = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(mailbox, "owner_queue_depth"), 0);
  free_mapping(mailbox);
  // No message index may remain for the rejected message.
  auto* owner_status = vm_object_store_owner_status(owner);
  ASSERT_EQ(mapping_number(owner_status, "pending_messages"), 0);
  free_mapping(owner_status);
  // The store must not have grown on any rejected path.
  ASSERT_EQ(owner_future_store_instance().size(), before_frozen);

  free_fill_records();
  // The store must be back to the pre-test baseline (no records from this
  // test's own fill or from the four rejected paths).
  ASSERT_EQ(owner_future_store_instance().terminal_record_count(), baseline_terminal);
  destruct_object(probe);
}

TEST_F(DriverTest, TestVmOwnerOrdinaryLpcRequiresExplicitOpen) {
  const char* owner = "owner/test/thread/ordinary-lpc-closed";
  const char* other_owner = "owner/test/thread/ordinary-lpc-other";

  vm_owner_thread_stop();
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);

  auto mapping_entry = [](mapping_t* map, const char* key) -> mapping_t* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_MAPPING);
    return value && value->type == T_MAPPING ? value->u.map : nullptr;
  };

  auto* before_runtime = vm_owner_runtime_status();
  auto before_pending_futures = mapping_number(before_runtime, "pending_futures");
  free_mapping(before_runtime);
  auto* before_queue = vm_owner_mailbox_status(owner);
  auto before_depth = mapping_number(before_queue, "owner_queue_depth");
  free_mapping(before_queue);

  auto* closed = vm_owner_ordinary_lpc_task(probe, owner, "owner_task_player", 0);
  ASSERT_EQ(mapping_number(closed, "success"), 0);
  ASSERT_EQ(mapping_number(closed, "future_id"), 0);
  ASSERT_EQ(mapping_number(closed, "task_id"), 0);
  ASSERT_STREQ(mapping_string(closed, "task_type"), "ordinary_lpc");
  ASSERT_STREQ(mapping_string(closed, "method"), "owner_task_player");
  ASSERT_STREQ(mapping_string(closed, "state"), "rejected");
  ASSERT_STREQ(mapping_string(closed, "error"), "ordinary LPC requires explicit open");
  ASSERT_STREQ(mapping_string(closed, "contract_reason"), "ordinary LPC requires explicit open");
  ASSERT_STREQ(mapping_string(closed, "executor_mode"), "rejected");
  ASSERT_EQ(mapping_number(closed, "requires_owner_thread"), 0);
  ASSERT_EQ(mapping_number(closed, "requires_owner_message_completion"), 0);
  ASSERT_EQ(mapping_number(closed, "ordinary_lpc_explicit_open"), 0);
  ASSERT_EQ(mapping_number(closed, "ordinary_lpc_dispatch_path_ready"), 1);
  ASSERT_STREQ(mapping_string(closed, "ordinary_lpc_next_blocker"), "");
  auto* closed_contract = mapping_entry(closed, "task_contract");
  ASSERT_EQ(mapping_number(closed_contract, "rejected"), 1);
  free_mapping(closed);

  auto* mismatch = vm_owner_ordinary_lpc_task(probe, other_owner, "owner_task_player", 1);
  ASSERT_EQ(mapping_number(mismatch, "success"), 0);
  ASSERT_EQ(mapping_number(mismatch, "future_id"), 0);
  ASSERT_EQ(mapping_number(mismatch, "task_id"), 0);
  ASSERT_STREQ(mapping_string(mismatch, "owner_id"), other_owner);
  ASSERT_STREQ(mapping_string(mismatch, "target_owner_id"), owner);
  ASSERT_STREQ(mapping_string(mismatch, "state"), "rejected");
  ASSERT_STREQ(mapping_string(mismatch, "error"), "ordinary LPC target owner mismatch");
  ASSERT_EQ(mapping_number(mismatch, "ordinary_lpc_explicit_open"), 1);
  free_mapping(mismatch);

  auto* after_runtime = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(after_runtime, "pending_futures"), before_pending_futures);
  free_mapping(after_runtime);
  auto* after_queue = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(after_queue, "owner_queue_depth"), before_depth);
  free_mapping(after_queue);

  destruct_object(probe);
}

TEST_F(DriverTest, TestVmOwnerThreadRunsExplicitOpenOrdinaryLpcTask) {
  const char* owner = "owner/test/thread/ordinary-lpc-open";
  ASSERT_TRUE(vm_context_is_main_thread());

  vm_owner_thread_stop();
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);
  auto owner_epoch = vm_owner_epoch(probe);

  auto mapping_entry = [](mapping_t* map, const char* key) -> mapping_t* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_MAPPING);
    return value && value->type == T_MAPPING ? value->u.map : nullptr;
  };

  auto* before = vm_owner_thread_status();
  auto before_executed = mapping_number(before, "thread_ordinary_lpc_executed");
  auto before_succeeded = mapping_number(before, "thread_ordinary_lpc_succeeded");
  auto before_failed = mapping_number(before, "thread_ordinary_lpc_failed");
  auto before_rejected = mapping_number(before, "thread_ordinary_lpc_rejected");
  auto before_context_leaks = mapping_number(before, "thread_context_leak_detected");
  auto before_safe_dispatched = mapping_number(before, "executor_safe_task_dispatched");
  free_mapping(before);
  auto* before_runtime = vm_owner_runtime_status();
  auto before_pending_futures = mapping_number(before_runtime, "pending_futures");
  free_mapping(before_runtime);

  auto* submitted = vm_owner_ordinary_lpc_task(probe, owner, "owner_task_player", 1);
  auto task_id = mapping_number(submitted, "task_id");
  auto future_id = mapping_number(submitted, "future_id");
  ASSERT_EQ(mapping_number(submitted, "success"), 1);
  ASSERT_GT(task_id, 0);
  ASSERT_GT(future_id, 0);
  ASSERT_STREQ(mapping_string(submitted, "task_type"), "ordinary_lpc");
  ASSERT_STREQ(mapping_string(submitted, "method"), "owner_task_player");
  ASSERT_EQ(mapping_number(submitted, "owner_epoch"), static_cast<long>(owner_epoch));
  ASSERT_STREQ(mapping_string(submitted, "executor_mode"), "executor_safe_explicit_open");
  ASSERT_STREQ(mapping_string(submitted, "route"), "owner_executor");
  ASSERT_STREQ(mapping_string(submitted, "result_policy"), "frozen_result_required");
  ASSERT_STREQ(mapping_string(submitted, "contract_reason"),
               "generic owner LPC dispatch requires explicit open and frozen result");
  ASSERT_EQ(mapping_number(submitted, "requires_owner_thread"), 1);
  ASSERT_EQ(mapping_number(submitted, "requires_owner_message_completion"), 1);
  ASSERT_EQ(mapping_number(submitted, "ordinary_lpc_explicit_open"), 1);
  ASSERT_EQ(mapping_number(submitted, "ordinary_lpc_dispatch_path_ready"), 1);
  ASSERT_EQ(mapping_number(submitted, "frozen_result_required"), 1);
  auto* submitted_contract = mapping_entry(submitted, "task_contract");
  ASSERT_STREQ(mapping_string(submitted_contract, "dispatch_model"), "generic_owner_lpc_dispatch");
  ASSERT_EQ(mapping_number(submitted_contract, "explicit_open_required"), 1);
  ASSERT_EQ(mapping_number(submitted_contract, "frozen_result_required"), 1);
  free_mapping(submitted);

  auto* pending_runtime = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(pending_runtime, "pending_futures"), before_pending_futures + 1);
  free_mapping(pending_runtime);
  auto* pending_future = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_STREQ(mapping_string(pending_future, "state"), "pending");
  ASSERT_EQ(mapping_number(pending_future, "target_task_id"), task_id);
  ASSERT_EQ(mapping_number(pending_future, "requires_owner_message_completion"), 1);
  free_mapping(pending_future);

  vm_owner_thread_start(2);
  for (int i = 0; i < 100; i++) {
    auto* polled = vm_owner_future_poll(static_cast<uint64_t>(future_id));
    auto completed = std::string(mapping_string(polled, "state")) == "completed";
    free_mapping(polled);
    if (completed) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  auto* running = vm_owner_thread_status();
  ASSERT_GE(mapping_number(running, "thread_ordinary_lpc_executed"), before_executed + 1);
  ASSERT_GE(mapping_number(running, "thread_ordinary_lpc_succeeded"), before_succeeded + 1);
  ASSERT_EQ(mapping_number(running, "thread_ordinary_lpc_failed"), before_failed);
  ASSERT_EQ(mapping_number(running, "thread_ordinary_lpc_rejected"), before_rejected);
  ASSERT_GE(mapping_number(running, "executor_safe_task_dispatched"), before_safe_dispatched + 1);
  ASSERT_EQ(mapping_number(running, "thread_context_leak_detected"), before_context_leaks);
  free_mapping(running);

  auto* completed_future = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(completed_future, "success"), 1);
  ASSERT_STREQ(mapping_string(completed_future, "state"), "completed");
  ASSERT_STREQ(mapping_string(completed_future, "result_key"), "owner_task_player");
  ASSERT_EQ(mapping_number(completed_future, "requires_owner_message_completion"), 0);
  ASSERT_EQ(mapping_number(completed_future, "frozen_result"), 1);
  auto* result = find_string_in_mapping(completed_future, "result");
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_NUMBER);
  ASSERT_EQ(result->u.number, 1);
  free_mapping(completed_future);

  auto* trace = vm_owner_task_trace(32);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  int ordinary_lpc_succeeded = 0;
  for (int i = 0; i < events->u.arr->size; i++) {
    auto* event = events->u.arr->item[i].u.map;
    if (mapping_number(event, "task_id") == task_id &&
        std::string(mapping_string(event, "task_type")) == "ordinary_lpc" &&
        std::string(mapping_string(event, "state")) == "thread_ordinary_lpc_succeeded") {
      ordinary_lpc_succeeded = 1;
      ASSERT_STREQ(mapping_string(event, "task_key"), "owner_task_player");
      ASSERT_STREQ(mapping_string(event, "owner_id"), owner);
    }
  }
  ASSERT_EQ(ordinary_lpc_succeeded, 1);
  free_mapping(trace);

  auto* completed_runtime = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(completed_runtime, "pending_futures"), before_pending_futures);
  free_mapping(completed_runtime);

  vm_owner_thread_stop();
  ASSERT_TRUE(vm_context_is_main_thread());
  destruct_object(probe);
}

TEST_F(DriverTest, TestVmOwnerThreadRejectsUnregisteredLpcTask) {
  const char* owner = "owner/test/thread/lpc-task-reject";
  ASSERT_TRUE(vm_context_is_main_thread());

  vm_owner_thread_stop();
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);

  auto mapping_entry = [](mapping_t* map, const char* key) -> mapping_t* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_MAPPING);
    return value && value->type == T_MAPPING ? value->u.map : nullptr;
  };

  auto* before = vm_owner_thread_status();
  auto before_succeeded = mapping_number(before, "thread_lpc_task_succeeded");
  auto before_rejected = mapping_number(before, "thread_lpc_task_rejected");
  free_mapping(before);
  auto* before_runtime = vm_owner_runtime_status();
  auto before_pending_futures = mapping_number(before_runtime, "pending_futures");
  auto before_future_failures = mapping_number(before_runtime, "futures_failed");
  free_mapping(before_runtime);

  auto* submitted = vm_owner_lpc_task(probe, owner, "owner_task_unregistered");
  auto task_id = mapping_number(submitted, "task_id");
  auto future_id = mapping_number(submitted, "future_id");
  ASSERT_EQ(mapping_number(submitted, "success"), 1);
  ASSERT_EQ(mapping_number(submitted, "registered_task"), 0);
  ASSERT_STREQ(mapping_string(submitted, "executor_mode"), "rejected");
  ASSERT_STREQ(mapping_string(submitted, "route"), "owner_executor");
  ASSERT_STREQ(mapping_string(submitted, "result_policy"), "none");
  ASSERT_STREQ(mapping_string(submitted, "contract_reason"), "ordinary LPC remains default closed");
  ASSERT_EQ(mapping_number(submitted, "frozen_result_required"), 0);
  auto* submitted_contract = mapping_entry(submitted, "task_contract");
  ASSERT_STREQ(mapping_string(submitted_contract, "executor_mode"), "rejected");
  ASSERT_EQ(mapping_number(submitted_contract, "executor_safe"), 0);
  ASSERT_EQ(mapping_number(submitted_contract, "rejected"), 1);
  free_mapping(submitted);
  auto* pending_future = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(pending_future, "success"), 1);
  ASSERT_STREQ(mapping_string(pending_future, "state"), "pending");
  ASSERT_EQ(mapping_number(pending_future, "target_task_id"), task_id);
  ASSERT_EQ(mapping_number(pending_future, "requires_owner_message_completion"), 1);
  free_mapping(pending_future);
  auto* pending_runtime = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(pending_runtime, "pending_futures"), before_pending_futures + 1);
  free_mapping(pending_runtime);

  vm_owner_thread_start(2);
  for (int i = 0; i < 100; i++) {
    auto* status = vm_owner_mailbox_status(owner);
    auto depth = mapping_number(status, "owner_queue_depth");
    free_mapping(status);
    auto* polled = vm_owner_future_poll(static_cast<uint64_t>(future_id));
    auto failed = std::string(mapping_string(polled, "state")) == "failed";
    free_mapping(polled);
    if (depth == 0 && failed) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  auto* running = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(running, "thread_lpc_task_succeeded"), before_succeeded);
  ASSERT_GE(mapping_number(running, "thread_lpc_task_rejected"), before_rejected + 1);
  free_mapping(running);

  auto* failed_future = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(failed_future, "success"), 1);
  ASSERT_STREQ(mapping_string(failed_future, "state"), "failed");
  ASSERT_STREQ(mapping_string(failed_future, "error"), "owner lpc task rejected");
  ASSERT_EQ(mapping_number(failed_future, "requires_owner_message_completion"), 0);
  ASSERT_EQ(mapping_number(failed_future, "frozen_result"), 0);
  free_mapping(failed_future);
  auto* failed_runtime = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(failed_runtime, "pending_futures"), before_pending_futures);
  ASSERT_GE(mapping_number(failed_runtime, "futures_failed"), before_future_failures + 1);
  free_mapping(failed_runtime);

  vm_owner_thread_stop();
  destruct_object(probe);
}

TEST_F(DriverTest, TestVmOwnerThreadRunsRegisteredDomainLpcTasks) {
  const char* owner = "owner/test/thread/lpc-domain-task";
  const char* methods[] = {"owner_task_player",      "owner_task_room",      "owner_task_session",
                           "owner_task_item",        "owner_task_economy",   "owner_task_combat",
                           "owner_task_mail",        "owner_task_reward",    "owner_task_world",
                           "owner_task_persistence", "owner_task_team",      "owner_task_guild",
                           "owner_task_sect",        "owner_task_quest",     "owner_task_rank",
                           "owner_task_crafting",    "owner_task_life_skill"};
  const int method_count = static_cast<int>(sizeof(methods) / sizeof(methods[0]));
  ASSERT_TRUE(vm_context_is_main_thread());

  vm_owner_thread_stop();
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);

  auto mapping_entry = [](mapping_t* map, const char* key) -> mapping_t* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_MAPPING);
    return value && value->type == T_MAPPING ? value->u.map : nullptr;
  };

  auto* before = vm_owner_thread_status();
  auto before_succeeded = mapping_number(before, "thread_lpc_task_succeeded");
  auto before_failed = mapping_number(before, "thread_lpc_task_failed");
  auto before_rejected = mapping_number(before, "thread_lpc_task_rejected");
  auto before_claims = mapping_number(before, "executor_owner_claims");
  auto before_releases = mapping_number(before, "executor_owner_releases");
  free_mapping(before);
  auto* before_runtime = vm_owner_runtime_status();
  auto before_pending_futures = mapping_number(before_runtime, "pending_futures");
  auto before_future_completions = mapping_number(before_runtime, "futures_completed");
  auto before_future_failures = mapping_number(before_runtime, "futures_failed");
  free_mapping(before_runtime);

  std::vector<long> future_ids;
  future_ids.reserve(method_count);
  for (const auto* method : methods) {
    auto* submitted = vm_owner_lpc_task(probe, owner, method);
    auto future_id = mapping_number(submitted, "future_id");
    ASSERT_EQ(mapping_number(submitted, "success"), 1);
    ASSERT_GT(future_id, 0);
    ASSERT_EQ(mapping_number(submitted, "registered_task"), 1) << method;
    ASSERT_STREQ(mapping_string(submitted, "executor_mode"), "executor_safe_allowlist") << method;
    ASSERT_STREQ(mapping_string(submitted, "route"), "owner_executor") << method;
    ASSERT_NE(std::string(mapping_string(submitted, "contract_reason")).find("registered"),
              std::string::npos)
        << method;
    auto* task_contract = mapping_entry(submitted, "task_contract");
    ASSERT_EQ(mapping_number(task_contract, "executor_safe"), 1) << method;
    ASSERT_EQ(mapping_number(task_contract, "main_required"), 0) << method;
    ASSERT_EQ(mapping_number(task_contract, "requires_owner_thread"), 1) << method;
    ASSERT_EQ(mapping_number(task_contract, "frozen_result_required"), 1) << method;
    ASSERT_EQ(mapping_number(task_contract, "rejected"), 0) << method;
    free_mapping(submitted);
    auto* pending_future = vm_owner_future_poll(static_cast<uint64_t>(future_id));
    ASSERT_EQ(mapping_number(pending_future, "success"), 1);
    ASSERT_STREQ(mapping_string(pending_future, "state"), "pending") << method;
    ASSERT_EQ(mapping_number(pending_future, "requires_owner_message_completion"), 1) << method;
    free_mapping(pending_future);
    future_ids.push_back(future_id);
  }
  auto* pending_runtime = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(pending_runtime, "pending_futures"),
            before_pending_futures + method_count);
  free_mapping(pending_runtime);

  vm_owner_thread_start(4);
  for (int i = 0; i < 200; i++) {
    auto* status = vm_owner_thread_status();
    auto succeeded = mapping_number(status, "thread_lpc_task_succeeded");
    auto active = mapping_number(status, "active_owners");
    free_mapping(status);
    if (succeeded >= before_succeeded + method_count && active == 0) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  auto* running = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(running, "enabled"), 1);
  ASSERT_EQ(mapping_number(running, "thread_count"), 4);
  ASSERT_GE(mapping_number(running, "thread_lpc_task_succeeded"), before_succeeded + method_count);
  ASSERT_EQ(mapping_number(running, "thread_lpc_task_failed"), before_failed);
  ASSERT_EQ(mapping_number(running, "thread_lpc_task_rejected"), before_rejected);
  ASSERT_EQ(mapping_number(running, "active_owners"), 0);
  ASSERT_EQ(mapping_number(running, "executor_owner_claims"), before_claims + 1);
  ASSERT_EQ(mapping_number(running, "executor_owner_releases"), before_releases + 1);
  free_mapping(running);
  for (int i = 0; i < method_count; i++) {
    auto* completed_future = vm_owner_future_poll(static_cast<uint64_t>(future_ids[i]));
    ASSERT_EQ(mapping_number(completed_future, "success"), 1);
    ASSERT_STREQ(mapping_string(completed_future, "state"), "completed") << methods[i];
    ASSERT_STREQ(mapping_string(completed_future, "result_key"), methods[i]);
    ASSERT_STREQ(mapping_string(completed_future, "error"), "");
    ASSERT_EQ(mapping_number(completed_future, "requires_owner_message_completion"), 0);
    ASSERT_EQ(mapping_number(completed_future, "frozen_result"), 1);
    free_mapping(completed_future);
  }
  auto* completed_runtime = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(completed_runtime, "pending_futures"), before_pending_futures);
  ASSERT_GE(mapping_number(completed_runtime, "futures_completed"),
            before_future_completions + method_count);
  ASSERT_EQ(mapping_number(completed_runtime, "futures_failed"), before_future_failures);
  free_mapping(completed_runtime);

  vm_owner_thread_stop();
  destruct_object(probe);
}

TEST_F(DriverTest, TestVmOwnerLpcTaskMainDrainAndScheduleFailPendingFuture) {
  const char* drain_owner = "owner/test/thread/lpc-task-main-drain";
  const char* schedule_owner = "owner/test/thread/lpc-task-schedule";
  ASSERT_TRUE(vm_context_is_main_thread());

  vm_owner_thread_stop();
  free_mapping(vm_owner_purge_mailbox(drain_owner));
  free_mapping(vm_owner_purge_mailbox(schedule_owner));
  object_t* drain_probe = load_object_for_test("single/void");
  object_t* schedule_probe = load_object_for_test("single/void");
  ASSERT_NE(drain_probe, nullptr);
  ASSERT_NE(schedule_probe, nullptr);
  vm_owner_set_id(drain_probe, drain_owner);
  vm_owner_set_id(schedule_probe, schedule_owner);


  auto* before_runtime = vm_owner_runtime_status();
  auto before_pending_futures = mapping_number(before_runtime, "pending_futures");
  auto before_future_failures = mapping_number(before_runtime, "futures_failed");
  free_mapping(before_runtime);

  auto* drain_submitted = vm_owner_lpc_task(drain_probe, drain_owner, "owner_task_readonly");
  auto drain_future_id = mapping_number(drain_submitted, "future_id");
  ASSERT_EQ(mapping_number(drain_submitted, "success"), 1);
  ASSERT_EQ(mapping_number(drain_submitted, "registered_task"), 1);
  free_mapping(drain_submitted);
  auto* drain_pending = vm_owner_future_poll(static_cast<uint64_t>(drain_future_id));
  ASSERT_STREQ(mapping_string(drain_pending, "state"), "pending");
  ASSERT_EQ(mapping_number(drain_pending, "requires_owner_message_completion"), 1);
  free_mapping(drain_pending);
  auto* after_drain_submit = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(after_drain_submit, "pending_futures"), before_pending_futures + 1);
  free_mapping(after_drain_submit);

  auto* drained = vm_owner_drain_mailbox(drain_owner, 1);
  ASSERT_EQ(mapping_number(drained, "drained"), 1);
  auto* drained_tasks = find_string_in_mapping(drained, "tasks");
  ASSERT_NE(drained_tasks, nullptr);
  ASSERT_EQ(drained_tasks->type, T_ARRAY);
  ASSERT_EQ(drained_tasks->u.arr->size, 1);
  ASSERT_STREQ(mapping_string(drained_tasks->u.arr->item[0].u.map, "task_type"), "lpc_task");
  ASSERT_STREQ(mapping_string(drained_tasks->u.arr->item[0].u.map, "task_key"),
               "owner_task_readonly");
  ASSERT_STREQ(mapping_string(drained_tasks->u.arr->item[0].u.map, "executor_mode"),
               "executor_safe");
  ASSERT_STREQ(mapping_string(drained_tasks->u.arr->item[0].u.map, "route"), "owner_executor");
  ASSERT_EQ(mapping_number(drained_tasks->u.arr->item[0].u.map, "executor_safe"), 1);
  ASSERT_EQ(mapping_number(drained_tasks->u.arr->item[0].u.map, "main_required"), 0);
  ASSERT_EQ(mapping_number(drained_tasks->u.arr->item[0].u.map, "requires_owner_mailbox"), 1);
  ASSERT_EQ(mapping_number(drained_tasks->u.arr->item[0].u.map, "requires_owner_main_queue"), 0);
  free_mapping(drained);
  auto* drain_failed = vm_owner_future_poll(static_cast<uint64_t>(drain_future_id));
  ASSERT_STREQ(mapping_string(drain_failed, "state"), "failed");
  ASSERT_STREQ(mapping_string(drain_failed, "error"), "owner lpc task requires owner thread");
  ASSERT_EQ(mapping_number(drain_failed, "requires_owner_message_completion"), 0);
  free_mapping(drain_failed);

  auto* schedule_submitted =
      vm_owner_lpc_task(schedule_probe, schedule_owner, "owner_task_readonly");
  auto schedule_future_id = mapping_number(schedule_submitted, "future_id");
  ASSERT_EQ(mapping_number(schedule_submitted, "success"), 1);
  ASSERT_EQ(mapping_number(schedule_submitted, "registered_task"), 1);
  free_mapping(schedule_submitted);
  auto* schedule_pending = vm_owner_future_poll(static_cast<uint64_t>(schedule_future_id));
  ASSERT_STREQ(mapping_string(schedule_pending, "state"), "pending");
  ASSERT_EQ(mapping_number(schedule_pending, "requires_owner_message_completion"), 1);
  free_mapping(schedule_pending);
  auto* after_schedule_submit = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(after_schedule_submit, "pending_futures"), before_pending_futures + 1);
  free_mapping(after_schedule_submit);

  auto* scheduled = vm_owner_schedule(1);
  ASSERT_EQ(mapping_number(scheduled, "dispatched"), 1);
  auto* scheduled_tasks = find_string_in_mapping(scheduled, "tasks");
  ASSERT_NE(scheduled_tasks, nullptr);
  ASSERT_EQ(scheduled_tasks->type, T_ARRAY);
  ASSERT_EQ(scheduled_tasks->u.arr->size, 1);
  ASSERT_STREQ(mapping_string(scheduled_tasks->u.arr->item[0].u.map, "task_type"), "lpc_task");
  ASSERT_STREQ(mapping_string(scheduled_tasks->u.arr->item[0].u.map, "task_key"),
               "owner_task_readonly");
  ASSERT_STREQ(mapping_string(scheduled_tasks->u.arr->item[0].u.map, "executor_mode"),
               "executor_safe");
  ASSERT_STREQ(mapping_string(scheduled_tasks->u.arr->item[0].u.map, "route"), "owner_executor");
  ASSERT_EQ(mapping_number(scheduled_tasks->u.arr->item[0].u.map, "executor_safe"), 1);
  ASSERT_EQ(mapping_number(scheduled_tasks->u.arr->item[0].u.map, "main_required"), 0);
  ASSERT_EQ(mapping_number(scheduled_tasks->u.arr->item[0].u.map, "requires_owner_mailbox"), 1);
  ASSERT_EQ(mapping_number(scheduled_tasks->u.arr->item[0].u.map, "requires_owner_main_queue"), 0);
  free_mapping(scheduled);
  auto* schedule_failed = vm_owner_future_poll(static_cast<uint64_t>(schedule_future_id));
  ASSERT_STREQ(mapping_string(schedule_failed, "state"), "failed");
  ASSERT_STREQ(mapping_string(schedule_failed, "error"), "owner lpc task requires owner thread");
  ASSERT_EQ(mapping_number(schedule_failed, "requires_owner_message_completion"), 0);
  free_mapping(schedule_failed);

  auto* failed_runtime = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(failed_runtime, "pending_futures"), before_pending_futures);
  ASSERT_GE(mapping_number(failed_runtime, "futures_failed"), before_future_failures + 2);
  free_mapping(failed_runtime);

  vm_owner_clear_id(drain_probe);
  vm_owner_clear_id(schedule_probe);
  destruct_object(drain_probe);
  destruct_object(schedule_probe);
}

TEST_F(DriverTest, TestVmOwnerThreadRejectsUnsafeLpcCanaryRequestsDeltas) {
  const char* owner = "owner/test/thread/lpc-canary-reject";
  ASSERT_TRUE(vm_context_is_main_thread());

  vm_owner_thread_stop();
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);
  auto stale_epoch = vm_owner_epoch(probe);


  auto* before = vm_owner_thread_status();
  auto before_succeeded = mapping_number(before, "thread_lpc_canary_succeeded");
  auto before_rejected = mapping_number(before, "thread_lpc_canary_rejected");
  auto before_owner_cleared = mapping_number(before, "thread_owner_cleared");
  auto before_execution_cleared = mapping_number(before, "thread_execution_cleared");
  auto before_canary_flag_cleared = mapping_number(before, "thread_lpc_canary_flag_cleared");
  auto before_context_leaks = mapping_number(before, "thread_context_leak_detected");
  free_mapping(before);

  auto* wrong_method = vm_owner_lpc_canary(probe, owner, "owner_lpc_probe");
  ASSERT_EQ(mapping_number(wrong_method, "success"), 1);
  free_mapping(wrong_method);
  vm_owner_set_id(probe, "owner/test/thread/lpc-canary-current");
  ASSERT_GT(vm_owner_epoch(probe), stale_epoch);
  auto* stale_owner = vm_owner_lpc_canary(probe, owner, "owner_lpc_canary");
  ASSERT_EQ(mapping_number(stale_owner, "success"), 1);
  free_mapping(stale_owner);

  vm_owner_thread_start(1);
  for (int i = 0; i < 100; i++) {
    auto* status = vm_owner_thread_status();
    auto rejected = mapping_number(status, "thread_lpc_canary_rejected");
    free_mapping(status);
    if (rejected >= before_rejected + 2) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  auto* running = vm_owner_thread_status();
  ASSERT_GE(mapping_number(running, "thread_lpc_canary_rejected"), before_rejected + 2);
  ASSERT_EQ(mapping_number(running, "thread_lpc_canary_succeeded"), before_succeeded);
  ASSERT_GE(mapping_number(running, "thread_owner_cleared"), before_owner_cleared + 2);
  ASSERT_GE(mapping_number(running, "thread_execution_cleared"), before_execution_cleared + 2);
  ASSERT_GE(mapping_number(running, "thread_lpc_canary_flag_cleared"),
            before_canary_flag_cleared + 2);
  ASSERT_EQ(mapping_number(running, "thread_context_leak_detected"), before_context_leaks);
  free_mapping(running);

  vm_owner_thread_stop();
  destruct_object(probe);
}

TEST_F(DriverTest, TestVmOwnerMessageAndCommitTracesAreSpecOnly) {
  const char* source_owner = "owner/test/message/source";
  const char* target_owner = "owner/test/message/target";


  free_mapping(vm_owner_purge_mailbox(target_owner));
  auto* submitted = vm_owner_submit_message(source_owner, target_owner, "room_snapshot", "room/v1");
  ASSERT_EQ(mapping_number(submitted, "success"), 1);
  ASSERT_GT(mapping_number(submitted, "message_id"), 0);
  ASSERT_GT(mapping_number(submitted, "target_task_id"), 0);
  ASSERT_EQ(mapping_number(submitted, "requires_owner_mailbox"), 1);
  ASSERT_EQ(mapping_number(submitted, "message_only_cross_owner"), 1);
  ASSERT_EQ(mapping_number(submitted, "direct_cross_owner_write"), 0);
  ASSERT_EQ(mapping_number(submitted, "payload_frozen"), 1);
  ASSERT_STREQ(mapping_string(submitted, "source_owner_id"), source_owner);
  ASSERT_STREQ(mapping_string(submitted, "target_owner_id"), target_owner);
  ASSERT_STREQ(mapping_string(submitted, "message_type"), "room_snapshot");

  auto message_id = mapping_number(submitted, "message_id");
  auto target_task_id = mapping_number(submitted, "target_task_id");
  auto* queued = vm_owner_mailbox_status(target_owner);
  ASSERT_EQ(mapping_number(queued, "owner_queue_depth"), 1);
  free_mapping(queued);

  auto* message_trace = vm_owner_message_trace(1);
  ASSERT_STREQ(mapping_string(message_trace, "trace_kind"), "owner_message_trace");
  ASSERT_STREQ(mapping_string(message_trace, "trace_model"), "owner_message_lifecycle_trace");
  auto* message_events = find_string_in_mapping(message_trace, "events");
  ASSERT_NE(message_events, nullptr);
  ASSERT_EQ(message_events->type, T_ARRAY);
  ASSERT_EQ(message_events->u.arr->size, 1);
  auto* message_event = message_events->u.arr->item[0].u.map;
  ASSERT_STREQ(mapping_string(message_event, "trace_model"), "owner_message_lifecycle_event");
  ASSERT_EQ(mapping_number(message_event, "message_id"), message_id);
  ASSERT_EQ(mapping_number(message_event, "target_task_id"), target_task_id);
  ASSERT_EQ(mapping_number(message_event, "direct_cross_owner_write"), 0);
  ASSERT_EQ(mapping_number(message_event, "payload_frozen"), 1);
  ASSERT_STREQ(mapping_string(message_event, "state"), "message_submitted");
  ASSERT_STREQ(mapping_string(message_event, "route"), "owner_mailbox");
  ASSERT_STREQ(mapping_string(message_event, "result_key"), "");
  ASSERT_STREQ(mapping_string(message_event, "error"), "");
  ASSERT_STREQ(mapping_string(message_event, "target_handle_status"), "current");
  ASSERT_EQ(mapping_number(message_event, "pending"), 1);
  ASSERT_EQ(mapping_number(message_event, "completed"), 0);
  ASSERT_EQ(mapping_number(message_event, "failed"), 0);
  ASSERT_EQ(mapping_number(message_event, "terminal"), 0);
  ASSERT_EQ(mapping_number(message_event, "frozen_result"), 0);
  ASSERT_EQ(mapping_number(message_event, "has_target_handle"), 0);
  ASSERT_EQ(mapping_number(message_event, "target_handle_current"), 1);
  ASSERT_EQ(mapping_number(message_event, "requires_owner_mailbox"), 1);
  ASSERT_EQ(mapping_number(message_event, "requires_owner_main_queue"), 0);
  ASSERT_EQ(mapping_number(message_event, "main_required"), 0);
  ASSERT_EQ(mapping_number(message_event, "queued_on_main"), 0);
  ASSERT_EQ(mapping_number(message_event, "message_only_cross_owner"), 1);
  free_mapping(message_trace);

  auto* commit = vm_owner_record_commit_boundary(source_owner, target_owner, "move_object",
                                                 message_id, "prepared");
  ASSERT_EQ(mapping_number(commit, "success"), 1);
  ASSERT_EQ(mapping_number(commit, "message_id"), message_id);
  ASSERT_EQ(mapping_number(commit, "direct_write"), 0);
  ASSERT_EQ(mapping_number(commit, "commit_boundary_only"), 1);
  ASSERT_STREQ(mapping_string(commit, "operation"), "move_object");
  ASSERT_STREQ(mapping_string(commit, "state"), "prepared");
  free_mapping(commit);

  const auto observed_commit_id = vm_owner_observe_commit_boundary(
      source_owner, target_owner, "move_object", message_id, "committed");
  ASSERT_GT(observed_commit_id, 0u);

  auto* commit_trace = vm_owner_commit_trace(1);
  ASSERT_STREQ(mapping_string(commit_trace, "trace_kind"), "owner_commit_trace");
  ASSERT_STREQ(mapping_string(commit_trace, "trace_model"), "owner_commit_boundary_trace");
  auto* commit_events = find_string_in_mapping(commit_trace, "events");
  ASSERT_NE(commit_events, nullptr);
  ASSERT_EQ(commit_events->type, T_ARRAY);
  ASSERT_EQ(commit_events->u.arr->size, 1);
  auto* commit_event = commit_events->u.arr->item[0].u.map;
  ASSERT_STREQ(mapping_string(commit_event, "trace_model"), "owner_commit_boundary_event");
  ASSERT_EQ(mapping_number(commit_event, "message_id"), message_id);
  ASSERT_EQ(mapping_number(commit_event, "direct_write"), 0);
  ASSERT_EQ(mapping_number(commit_event, "commit_boundary_only"), 1);
  ASSERT_STREQ(mapping_string(commit_event, "operation"), "move_object");
  ASSERT_STREQ(mapping_string(commit_event, "state"), "committed");
  free_mapping(commit_trace);

  auto* drained = vm_owner_drain_mailbox(target_owner, 1);
  auto* tasks = find_string_in_mapping(drained, "tasks");
  ASSERT_NE(tasks, nullptr);
  ASSERT_EQ(tasks->type, T_ARRAY);
  ASSERT_EQ(tasks->u.arr->size, 1);
  ASSERT_EQ(mapping_number(tasks->u.arr->item[0].u.map, "task_id"), target_task_id);
  ASSERT_STREQ(mapping_string(tasks->u.arr->item[0].u.map, "task_type"), "owner_message");
  ASSERT_STREQ(mapping_string(tasks->u.arr->item[0].u.map, "task_key"), "room_snapshot");
  free_mapping(drained);

  message_trace = vm_owner_message_trace(1);
  message_events = find_string_in_mapping(message_trace, "events");
  ASSERT_NE(message_events, nullptr);
  ASSERT_EQ(message_events->type, T_ARRAY);
  ASSERT_EQ(message_events->u.arr->size, 1);
  message_event = message_events->u.arr->item[0].u.map;
  ASSERT_EQ(mapping_number(message_event, "message_id"), message_id);
  ASSERT_EQ(mapping_number(message_event, "payload_frozen"), 1);
  ASSERT_STREQ(mapping_string(message_event, "state"), "completed");
  ASSERT_STREQ(mapping_string(message_event, "route"), "owner_mailbox");
  ASSERT_STREQ(mapping_string(message_event, "result_key"), "room_snapshot");
  ASSERT_STREQ(mapping_string(message_event, "error"), "");
  ASSERT_EQ(mapping_number(message_event, "pending"), 0);
  ASSERT_EQ(mapping_number(message_event, "completed"), 1);
  ASSERT_EQ(mapping_number(message_event, "failed"), 0);
  ASSERT_EQ(mapping_number(message_event, "terminal"), 1);
  ASSERT_EQ(mapping_number(message_event, "frozen_result"), 0);
  ASSERT_EQ(mapping_number(message_event, "requires_owner_mailbox"), 1);
  ASSERT_EQ(mapping_number(message_event, "requires_owner_main_queue"), 0);
  free_mapping(message_trace);
  free_mapping(submitted);
}

TEST_F(DriverTest, TestVmOwnerFuturePollTracksMessageCompletion) {
  const char* source_owner = "owner/test/future/source";
  const char* target_owner = "owner/test/future/target";


  free_mapping(vm_owner_purge_mailbox(target_owner));
  auto* submitted =
      vm_owner_submit_message(source_owner, target_owner, "future_method", "future/payload");
  auto future_id = mapping_number(submitted, "future_id");
  auto target_task_id = mapping_number(submitted, "target_task_id");
  ASSERT_GT(future_id, 0);
  ASSERT_GT(target_task_id, 0);

  auto* pending = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(pending, "success"), 1);
  ASSERT_EQ(mapping_number(pending, "future_id"), future_id);
  ASSERT_EQ(mapping_number(pending, "target_task_id"), target_task_id);
  ASSERT_EQ(mapping_number(pending, "requires_owner_message_completion"), 1);
  ASSERT_EQ(mapping_number(pending, "payload_frozen"), 1);
  ASSERT_EQ(mapping_number(pending, "frozen_result"), 0);
  ASSERT_STREQ(mapping_string(pending, "state"), "pending");
  free_mapping(pending);

  free_mapping(vm_owner_drain_mailbox(target_owner, 1));
  auto* completed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(completed, "success"), 1);
  ASSERT_EQ(mapping_number(completed, "requires_owner_message_completion"), 0);
  ASSERT_EQ(mapping_number(completed, "payload_frozen"), 1);
  ASSERT_EQ(mapping_number(completed, "frozen_result"), 0);
  ASSERT_STREQ(mapping_string(completed, "state"), "completed");
  ASSERT_STREQ(mapping_string(completed, "result_key"), "future_method");
  free_mapping(completed);
  free_mapping(submitted);
}

TEST_F(DriverTest, TestVmOwnerFutureTakeConsumesOnlyCompletedFuture) {
  const char* source_owner = "owner/test/future/take-source";
  const char* target_owner = "owner/test/future/take-target";


  free_mapping(vm_owner_purge_mailbox(target_owner));
  auto* submitted =
      vm_owner_submit_message(source_owner, target_owner, "future_take", "future/take");
  auto future_id = mapping_number(submitted, "future_id");
  ASSERT_GT(future_id, 0);
  free_mapping(submitted);

  auto* pending_take = vm_owner_future_take(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(pending_take, "success"), 1);
  ASSERT_EQ(mapping_number(pending_take, "consumed"), 0);
  ASSERT_STREQ(mapping_string(pending_take, "state"), "pending");
  free_mapping(pending_take);

  auto* still_pending = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(still_pending, "success"), 1);
  ASSERT_STREQ(mapping_string(still_pending, "state"), "pending");
  free_mapping(still_pending);

  free_mapping(vm_owner_drain_mailbox(target_owner, 1));
  auto* completed_take = vm_owner_future_take(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(completed_take, "success"), 1);
  ASSERT_EQ(mapping_number(completed_take, "consumed"), 1);
  ASSERT_STREQ(mapping_string(completed_take, "state"), "completed");
  free_mapping(completed_take);

  auto* consumed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(consumed, "success"), 0);
  ASSERT_STREQ(mapping_string(consumed, "state"), "unknown");
  free_mapping(consumed);
}

TEST_F(DriverTest, TestVmOwnerFutureTakeConsumesFailedFutureOnce) {
  const char* source_owner = "owner/test/future/take-failed-source";
  const char* target_owner = "owner/test/future/take-failed-target";


  free_mapping(vm_owner_purge_mailbox(target_owner));
  auto* submitted =
      vm_owner_submit_message(source_owner, target_owner, "future_take", "future/take-failed");
  auto future_id = mapping_number(submitted, "future_id");
  ASSERT_GT(future_id, 0);
  free_mapping(submitted);

  free_mapping(vm_owner_purge_mailbox(target_owner));
  auto* failed_take = vm_owner_future_take(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(failed_take, "success"), 1);
  ASSERT_EQ(mapping_number(failed_take, "consumed"), 1);
  ASSERT_STREQ(mapping_string(failed_take, "state"), "failed");
  ASSERT_STREQ(mapping_string(failed_take, "error"), "purged");
  free_mapping(failed_take);

  auto* consumed = vm_owner_future_take(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(consumed, "success"), 0);
  ASSERT_EQ(mapping_number(consumed, "consumed"), 0);
  ASSERT_STREQ(mapping_string(consumed, "state"), "unknown");
  free_mapping(consumed);
}

TEST_F(DriverTest, TestVmOwnerPurgeFailsPendingFuture) {
  const char* source_owner = "owner/test/future/purge-source";
  const char* target_owner = "owner/test/future/purge-target";


  free_mapping(vm_owner_purge_mailbox(target_owner));
  auto* submitted =
      vm_owner_submit_message(source_owner, target_owner, "future_method", "future/purge");
  auto future_id = mapping_number(submitted, "future_id");
  auto target_task_id = mapping_number(submitted, "target_task_id");
  ASSERT_GT(future_id, 0);
  ASSERT_GT(target_task_id, 0);
  free_mapping(submitted);

  auto* purged = vm_owner_purge_mailbox(target_owner);
  ASSERT_EQ(mapping_number(purged, "purged"), 1);
  free_mapping(purged);

  auto* failed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(failed, "success"), 1);
  ASSERT_EQ(mapping_number(failed, "requires_owner_message_completion"), 0);
  ASSERT_EQ(mapping_number(failed, "payload_frozen"), 1);
  ASSERT_EQ(mapping_number(failed, "frozen_result"), 0);
  ASSERT_STREQ(mapping_string(failed, "state"), "failed");
  ASSERT_STREQ(mapping_string(failed, "error"), "purged");
  free_mapping(failed);

  auto* message_trace = vm_owner_message_trace(1);
  auto* message_events = find_string_in_mapping(message_trace, "events");
  ASSERT_NE(message_events, nullptr);
  ASSERT_EQ(message_events->type, T_ARRAY);
  ASSERT_EQ(message_events->u.arr->size, 1);
  auto* message_event = message_events->u.arr->item[0].u.map;
  ASSERT_EQ(mapping_number(message_event, "target_task_id"), target_task_id);
  ASSERT_STREQ(mapping_string(message_event, "state"), "failed");
  ASSERT_STREQ(mapping_string(message_event, "route"), "owner_mailbox");
  ASSERT_STREQ(mapping_string(message_event, "error"), "purged");
  ASSERT_EQ(mapping_number(message_event, "failed"), 1);
  ASSERT_EQ(mapping_number(message_event, "terminal"), 1);
  ASSERT_EQ(mapping_number(message_event, "frozen_result"), 0);
  free_mapping(message_trace);
}

TEST_F(DriverTest, TestVmOwnerObjectMessageFailsStaleTargetHandle) {
  const char* old_owner = "owner/test/future/object-old";
  const char* new_owner = "owner/test/future/object-new";


  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  vm_owner_set_id(obj, old_owner);
  auto handle = vm_object_handle(obj);

  auto* submitted = vm_owner_submit_object_message("owner/test/future/object-source", handle,
                                                   "object_method", "object/payload");
  auto future_id = mapping_number(submitted, "future_id");
  auto target_task_id = mapping_number(submitted, "target_task_id");
  ASSERT_EQ(mapping_number(submitted, "has_target_handle"), 1);
  ASSERT_EQ(mapping_number(submitted, "target_handle_current"), 1);
  ASSERT_STREQ(mapping_string(submitted, "target_handle_status"), "current");
  ASSERT_EQ(mapping_number(submitted, "requires_owner_mailbox"), 1);
  ASSERT_EQ(mapping_number(submitted, "requires_owner_main_queue"), 0);
  ASSERT_EQ(mapping_number(submitted, "main_required"), 0);
  ASSERT_EQ(mapping_number(submitted, "queued_on_main"), 0);
  free_mapping(submitted);

  vm_owner_set_id(obj, new_owner);
  auto stale_status = vm_object_handle_resolve_status(handle);
  ASSERT_EQ(stale_status.object, nullptr);
  ASSERT_EQ(stale_status.status, VMObjectHandleResolveStatus::kOwnerMismatch);
  ASSERT_STREQ(vm_object_handle_resolve_status_name(stale_status.status), "owner_mismatch");
  ASSERT_TRUE(stale_status.diagnosed_via_owner_local_store);
  ASSERT_TRUE(stale_status.diagnosed_via_owner_local_cross_shard);
  ASSERT_FALSE(stale_status.owner_local_object_pointer_index_found);
  ASSERT_FALSE(stale_status.diagnosed_via_global_index);
  vm_owner_thread_start(1);
  for (int i = 0; i < 100; i++) {
    auto* maybe_failed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
    auto failed_state = std::string(mapping_string(maybe_failed, "state"));
    free_mapping(maybe_failed);
    if (failed_state == "failed") {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  vm_owner_thread_stop();

  auto* failed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(failed, "success"), 1);
  ASSERT_EQ(mapping_number(failed, "requires_owner_message_completion"), 0);
  ASSERT_EQ(mapping_number(failed, "has_target_handle"), 1);
  ASSERT_EQ(mapping_number(failed, "target_handle_current"), 0);
  ASSERT_STREQ(mapping_string(failed, "target_handle_status"), "owner_mismatch");
  ASSERT_EQ(mapping_number(failed, "payload_frozen"), 1);
  ASSERT_EQ(mapping_number(failed, "frozen_result"), 0);
  ASSERT_STREQ(mapping_string(failed, "state"), "failed");
  ASSERT_STREQ(mapping_string(failed, "error"), "stale target: owner_mismatch");
  free_mapping(failed);

  auto* owner_status = vm_object_store_owner_status(old_owner);
  ASSERT_EQ(mapping_number(owner_status, "pending_messages"), 0);
  free_mapping(owner_status);

  auto* message_trace = vm_owner_message_trace(1);
  auto* message_events = find_string_in_mapping(message_trace, "events");
  ASSERT_NE(message_events, nullptr);
  ASSERT_EQ(message_events->type, T_ARRAY);
  ASSERT_EQ(message_events->u.arr->size, 1);
  auto* message_event = message_events->u.arr->item[0].u.map;
  ASSERT_EQ(mapping_number(message_event, "target_task_id"), target_task_id);
  ASSERT_STREQ(mapping_string(message_event, "state"), "failed");
  ASSERT_STREQ(mapping_string(message_event, "route"), "owner_mailbox");
  ASSERT_STREQ(mapping_string(message_event, "error"), "stale target: owner_mismatch");
  ASSERT_STREQ(mapping_string(message_event, "target_handle_status"), "owner_mismatch");
  ASSERT_EQ(mapping_number(message_event, "failed"), 1);
  ASSERT_EQ(mapping_number(message_event, "terminal"), 1);
  ASSERT_EQ(mapping_number(message_event, "has_target_handle"), 1);
  ASSERT_EQ(mapping_number(message_event, "target_handle_current"), 0);
  ASSERT_EQ(mapping_number(message_event, "requires_owner_mailbox"), 1);
  ASSERT_EQ(mapping_number(message_event, "requires_owner_main_queue"), 0);
  ASSERT_EQ(mapping_number(message_event, "main_required"), 0);
  ASSERT_EQ(mapping_number(message_event, "queued_on_main"), 0);
  ASSERT_EQ(mapping_number(message_event, "frozen_result"), 0);
  free_mapping(message_trace);

  vm_owner_clear_id(obj);
  destruct_object(obj);
}

TEST_F(DriverTest, TestVmOwnerObjectMessageRejectsStaleTargetHandleAtSubmit) {
  const char* old_owner = "owner/test/future/object-stale-submit-old";
  const char* new_owner = "owner/test/future/object-stale-submit-new";


  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  vm_owner_set_id(obj, old_owner);
  auto handle = vm_object_handle(obj);
  vm_owner_set_id(obj, new_owner);

  auto stale_status = vm_object_handle_resolve_status(handle);
  ASSERT_EQ(stale_status.object, nullptr);
  ASSERT_EQ(stale_status.status, VMObjectHandleResolveStatus::kOwnerMismatch);
  ASSERT_STREQ(vm_object_handle_resolve_status_name(stale_status.status), "owner_mismatch");

  auto* submitted = vm_owner_submit_object_message("owner/test/future/object-source", handle,
                                                   "object_method", "stale-at-submit");
  auto future_id = mapping_number(submitted, "future_id");
  auto target_task_id = mapping_number(submitted, "target_task_id");
  ASSERT_EQ(mapping_number(submitted, "success"), 1);
  ASSERT_EQ(mapping_number(submitted, "has_target_handle"), 1);
  ASSERT_EQ(mapping_number(submitted, "target_handle_current"), 0);
  ASSERT_STREQ(mapping_string(submitted, "target_handle_status"), "owner_mismatch");
  ASSERT_EQ(mapping_number(submitted, "requires_owner_mailbox"), 0);
  ASSERT_EQ(mapping_number(submitted, "requires_owner_main_queue"), 0);
  ASSERT_EQ(mapping_number(submitted, "main_required"), 0);
  ASSERT_EQ(mapping_number(submitted, "queued_on_main"), 0);
  free_mapping(submitted);

  auto* failed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(failed, "success"), 1);
  ASSERT_EQ(mapping_number(failed, "requires_owner_message_completion"), 0);
  ASSERT_EQ(mapping_number(failed, "has_target_handle"), 1);
  ASSERT_EQ(mapping_number(failed, "target_handle_current"), 0);
  ASSERT_STREQ(mapping_string(failed, "target_handle_status"), "owner_mismatch");
  ASSERT_EQ(mapping_number(failed, "payload_frozen"), 1);
  ASSERT_EQ(mapping_number(failed, "frozen_result"), 0);
  ASSERT_STREQ(mapping_string(failed, "state"), "failed");
  ASSERT_STREQ(mapping_string(failed, "error"), "stale target: owner_mismatch");
  free_mapping(failed);

  auto* owner_status = vm_object_store_owner_status(old_owner);
  ASSERT_EQ(mapping_number(owner_status, "pending_messages"), 0);
  free_mapping(owner_status);
  ASSERT_EQ(vm_owner_drain_main_tasks(1), 0);

  auto* message_trace = vm_owner_message_trace(1);
  auto* message_events = find_string_in_mapping(message_trace, "events");
  ASSERT_NE(message_events, nullptr);
  ASSERT_EQ(message_events->type, T_ARRAY);
  ASSERT_EQ(message_events->u.arr->size, 1);
  auto* message_event = message_events->u.arr->item[0].u.map;
  ASSERT_EQ(mapping_number(message_event, "target_task_id"), target_task_id);
  ASSERT_STREQ(mapping_string(message_event, "state"), "failed");
  ASSERT_STREQ(mapping_string(message_event, "route"), "owner_mailbox");
  ASSERT_STREQ(mapping_string(message_event, "error"), "stale target: owner_mismatch");
  ASSERT_STREQ(mapping_string(message_event, "target_handle_status"), "owner_mismatch");
  ASSERT_EQ(mapping_number(message_event, "failed"), 1);
  ASSERT_EQ(mapping_number(message_event, "terminal"), 1);
  ASSERT_EQ(mapping_number(message_event, "has_target_handle"), 1);
  ASSERT_EQ(mapping_number(message_event, "target_handle_current"), 0);
  ASSERT_EQ(mapping_number(message_event, "requires_owner_mailbox"), 0);
  ASSERT_EQ(mapping_number(message_event, "requires_owner_main_queue"), 0);
  ASSERT_EQ(mapping_number(message_event, "main_required"), 0);
  ASSERT_EQ(mapping_number(message_event, "queued_on_main"), 0);
  ASSERT_EQ(mapping_number(message_event, "frozen_result"), 0);
  free_mapping(message_trace);

  vm_owner_clear_id(obj);
  destruct_object(obj);
}

TEST_F(DriverTest, TestVmOwnerObjectMessageReportsDestructedTargetHandle) {
  const char* owner = "owner/test/future/object-destructed";


  object_t* obj = load_object_for_test("single/on_destruct_good");
  ASSERT_NE(obj, nullptr);
  vm_owner_set_id(obj, owner);
  auto handle = vm_object_handle(obj);

  auto* submitted = vm_owner_submit_object_message("owner/test/future/object-source", handle,
                                                   "dummy", "object/destructed");
  auto future_id = mapping_number(submitted, "future_id");
  auto target_task_id = mapping_number(submitted, "target_task_id");
  ASSERT_EQ(mapping_number(submitted, "has_target_handle"), 1);
  ASSERT_EQ(mapping_number(submitted, "target_handle_current"), 1);
  ASSERT_STREQ(mapping_string(submitted, "target_handle_status"), "current");
  ASSERT_EQ(mapping_number(submitted, "requires_owner_mailbox"), 1);
  ASSERT_EQ(mapping_number(submitted, "requires_owner_main_queue"), 0);
  ASSERT_EQ(mapping_number(submitted, "main_required"), 0);
  ASSERT_EQ(mapping_number(submitted, "queued_on_main"), 0);
  free_mapping(submitted);

  destruct_object(obj);
  auto stale_status = vm_object_handle_resolve_status(handle);
  ASSERT_EQ(stale_status.object, nullptr);
  ASSERT_EQ(stale_status.status, VMObjectHandleResolveStatus::kRecordDestructed);
  ASSERT_STREQ(vm_object_handle_resolve_status_name(stale_status.status), "record_destructed");
  ASSERT_TRUE(stale_status.diagnosed_via_owner_local_store);
  ASSERT_FALSE(stale_status.diagnosed_via_owner_local_cross_shard);
  ASSERT_FALSE(stale_status.owner_local_object_pointer_index_found);
  ASSERT_FALSE(stale_status.diagnosed_via_global_index);
  vm_owner_thread_start(1);
  for (int i = 0; i < 100; i++) {
    auto* maybe_failed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
    auto failed_state = std::string(mapping_string(maybe_failed, "state"));
    free_mapping(maybe_failed);
    if (failed_state == "failed") {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  vm_owner_thread_stop();

  auto* failed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(failed, "success"), 1);
  ASSERT_EQ(mapping_number(failed, "requires_owner_message_completion"), 0);
  ASSERT_EQ(mapping_number(failed, "has_target_handle"), 1);
  ASSERT_EQ(mapping_number(failed, "target_handle_current"), 0);
  ASSERT_STREQ(mapping_string(failed, "target_handle_status"), "record_destructed");
  ASSERT_STREQ(mapping_string(failed, "state"), "failed");
  ASSERT_STREQ(mapping_string(failed, "error"), "stale target: record_destructed");
  ASSERT_EQ(mapping_number(failed, "frozen_result"), 0);
  free_mapping(failed);

  auto* owner_status = vm_object_store_owner_status(owner);
  ASSERT_EQ(mapping_number(owner_status, "pending_messages"), 0);
  free_mapping(owner_status);

  auto* message_trace = vm_owner_message_trace(1);
  auto* message_events = find_string_in_mapping(message_trace, "events");
  ASSERT_NE(message_events, nullptr);
  ASSERT_EQ(message_events->type, T_ARRAY);
  ASSERT_EQ(message_events->u.arr->size, 1);
  auto* message_event = message_events->u.arr->item[0].u.map;
  ASSERT_EQ(mapping_number(message_event, "target_task_id"), target_task_id);
  ASSERT_STREQ(mapping_string(message_event, "state"), "failed");
  ASSERT_STREQ(mapping_string(message_event, "route"), "owner_mailbox");
  ASSERT_STREQ(mapping_string(message_event, "error"), "stale target: record_destructed");
  ASSERT_STREQ(mapping_string(message_event, "target_handle_status"), "record_destructed");
  ASSERT_EQ(mapping_number(message_event, "failed"), 1);
  ASSERT_EQ(mapping_number(message_event, "terminal"), 1);
  ASSERT_EQ(mapping_number(message_event, "has_target_handle"), 1);
  ASSERT_EQ(mapping_number(message_event, "target_handle_current"), 0);
  ASSERT_EQ(mapping_number(message_event, "requires_owner_mailbox"), 1);
  ASSERT_EQ(mapping_number(message_event, "requires_owner_main_queue"), 0);
  ASSERT_EQ(mapping_number(message_event, "main_required"), 0);
  ASSERT_EQ(mapping_number(message_event, "queued_on_main"), 0);
  ASSERT_EQ(mapping_number(message_event, "frozen_result"), 0);
  free_mapping(message_trace);
}

TEST_F(DriverTest, TestVmOwnerFuturePollReportsUnknownFuture) {

  auto* result = vm_owner_future_poll(999999999u);
  ASSERT_EQ(mapping_number(result, "success"), 0);
  ASSERT_EQ(mapping_number(result, "requires_owner_message_completion"), 0);
  ASSERT_STREQ(mapping_string(result, "state"), "unknown");
  free_mapping(result);
}

TEST_F(DriverTest, TestVmObjectHandleRejectsStaleOwnerEpoch) {
  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);

  vm_owner_set_id(obj, "owner/test/handle/epoch");
  auto handle = vm_object_handle(obj);
  ASSERT_TRUE(handle.valid);
  ASSERT_EQ(vm_object_handle_resolve(handle), obj);

  vm_owner_clear_id(obj);
  vm_owner_set_id(obj, "owner/test/handle/epoch");
  auto stale_status = vm_object_handle_resolve_status(handle);
  ASSERT_EQ(stale_status.object, nullptr);
  ASSERT_EQ(stale_status.status, VMObjectHandleResolveStatus::kOwnerEpochMismatch);
  ASSERT_STREQ(vm_object_handle_resolve_status_name(stale_status.status), "owner_epoch_mismatch");
  ASSERT_TRUE(stale_status.diagnosed_via_owner_local_store);
  ASSERT_FALSE(stale_status.diagnosed_via_owner_local_cross_shard);
  ASSERT_FALSE(stale_status.owner_local_object_pointer_index_found);
  ASSERT_FALSE(stale_status.diagnosed_via_global_index);
  ASSERT_FALSE(vm_object_handle_is_current(handle));

  vm_owner_clear_id(obj);
  destruct_object(obj);
}

TEST_F(DriverTest, TestVmObjectHandleReportsCapabilityMetadata) {

  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);

  vm_owner_set_id(obj, "owner/test/handle/capability");
  auto default_handle = vm_object_handle(obj);
  ASSERT_TRUE(default_handle.valid);
  ASSERT_STREQ(default_handle.permission_intent.c_str(), kVMObjectHandleDefaultPermissionIntent);
  ASSERT_GT(default_handle.snapshot_version, 0u);
  ASSERT_EQ(default_handle.snapshot_version, default_handle.owner_epoch);

  auto snapshot_handle = vm_object_handle_with_intent(obj, "snapshot_payload");
  ASSERT_TRUE(snapshot_handle.valid);
  ASSERT_STREQ(snapshot_handle.permission_intent.c_str(), "snapshot_payload");
  ASSERT_EQ(snapshot_handle.snapshot_version, snapshot_handle.owner_epoch);
  ASSERT_EQ(vm_object_handle_resolve(snapshot_handle), obj);

  auto* status = vm_object_handle_status_with_intent(obj, "owner_async_message");
  ASSERT_EQ(mapping_number(status, "success"), 1);
  ASSERT_EQ(mapping_number(status, "object_handle_capability_ready"), 1);
  ASSERT_STREQ(mapping_string(status, "capability_model"), kVMObjectHandleCapabilityModelV1);
  ASSERT_STREQ(mapping_string(status, "permission_intent"), "owner_async_message");
  ASSERT_EQ(mapping_number(status, "snapshot_version"), default_handle.owner_epoch);
  ASSERT_EQ(mapping_number(status, "capability_epoch_guard"), 1);
  ASSERT_EQ(mapping_number(status, "current"), 1);
  ASSERT_STREQ(mapping_string(status, "resolve_status"), "current");
  free_mapping(status);

  vm_owner_clear_id(obj);
  destruct_object(obj);
}

// F05: worker threads must never mutate plain object_t refcounts. After a
// handle/message workload with owner threads enabled, the probe counter must
// stay at zero (main-thread acquire + deferred release only).
TEST_F(DriverTest, TestVmOwnerWorkerNeverMutatesObjectRefcounts) {
  const char* owner = "owner/test/worker/ref-probe";

  vm_owner_thread_stop();
  vm_object_store_test_support_reset_worker_ref_mutation_count();

  // Calibrate the probe once so a permanently-zero/unwired counter cannot
  // make the workload assertion pass vacuously.
  std::thread probe_calibration([] {
    vm_object_store_note_object_ref_mutation();
  });
  probe_calibration.join();
  ASSERT_EQ(vm_object_store_test_support_worker_ref_mutation_count(), 1u);
  vm_object_store_test_support_reset_worker_ref_mutation_count();

  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);
  auto handle = vm_object_handle(probe);


  vm_owner_thread_start(1);
  // Drive handle + message workloads on the worker: submit messages with a
  // target handle, let the worker dispatch them, drain everything.
  for (int i = 0; i < 8; i++) {
    auto* submitted = vm_owner_submit_object_message("owner/test/worker/ref-probe-source", handle,
                                                     "object_method", "ref/probe");
    ASSERT_EQ(mapping_number(submitted, "success"), 1);
    free_mapping(submitted);
  }
  for (int i = 0; i < 200; i++) {
    auto* mailbox = vm_owner_mailbox_status(owner);
    auto depth = mapping_number(mailbox, "owner_queue_depth");
    free_mapping(mailbox);
    if (depth == 0) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  vm_owner_thread_stop();
  free_mapping(vm_owner_purge_mailbox(owner));
  vm_owner_drain_main_tasks(1);

  // The worker path consumed pre-acquired task.target references only; the
  // deferred release queue must be empty and the mutation probe must be 0.
  ASSERT_EQ(vm_object_store_test_support_worker_ref_mutation_count(), 0u);
  auto* status = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(status, "deferred_target_releases"), 0);
  free_mapping(status);

  vm_owner_clear_id(probe);
  destruct_object(probe);
}

TEST_F(DriverTest, TestWebsocketTrustedProxyCidrParser) {
  std::vector<websocket_trusted_proxy_cidr_t> cidrs;
  std::string error;
  ASSERT_TRUE(websocket_parse_trusted_proxy_cidrs(
      "127.0.0.1/32, ::ffff:192.0.2.0/120, 2001:db8::/32", &cidrs, &error))
      << error;
  ASSERT_EQ(cidrs.size(), 3u);
  EXPECT_EQ(cidrs[0].family, AF_INET);
  EXPECT_EQ(cidrs[0].prefix_length, 32);
  EXPECT_EQ(cidrs[0].network[0], 127);
  EXPECT_EQ(cidrs[1].family, AF_INET);
  EXPECT_EQ(cidrs[1].prefix_length, 24);
  EXPECT_EQ(cidrs[1].network[0], 192);
  EXPECT_EQ(cidrs[1].network[1], 0);
  EXPECT_EQ(cidrs[1].network[2], 2);
  EXPECT_EQ(cidrs[2].family, AF_INET6);
  EXPECT_EQ(cidrs[2].prefix_length, 32);

  for (const char* invalid :
       {"127.0.0.1/33", "example.com/32", "127.0.0.1/32,", "::ffff:192.0.2.0/95"}) {
    cidrs.clear();
    error.clear();
    EXPECT_FALSE(websocket_parse_trusted_proxy_cidrs(invalid, &cidrs, &error)) << invalid;
    EXPECT_TRUE(cidrs.empty()) << invalid;
    EXPECT_FALSE(error.empty()) << invalid;
  }
}

TEST_F(DriverTest, TestTlsClientIdentityAndMinimumProtocol) {
  auto* ctx = tls_client_init();
  ASSERT_NE(ctx, nullptr);
  EXPECT_EQ(SSL_CTX_get_min_proto_version(ctx), kTlsMinimumProtocolVersion);

  auto* ssl = SSL_new(ctx);
  ASSERT_NE(ssl, nullptr);
  EXPECT_TRUE(tls_configure_client_identity(ssl, nullptr, 0, "example.com", true));
  EXPECT_TRUE(tls_configure_client_identity(ssl, nullptr, 0, "example.com", false));
  EXPECT_FALSE(tls_configure_client_identity(ssl, nullptr, 0, "", true));
  EXPECT_FALSE(tls_configure_client_identity(ssl, nullptr, 0, nullptr, true));

  auto* server = tls_server_init(external_port[3].tls_cert, external_port[3].tls_key);
  ASSERT_NE(server, nullptr);
  EXPECT_EQ(SSL_CTX_get_min_proto_version(server), kTlsMinimumProtocolVersion);
  tls_server_close(server);

  SSL_free(ssl);
  SSL_CTX_free(ctx);
}

// R2-F12: sys_reload_tls() management contract matrix. Fixed check order:
// main thread first, then master authorization, then index/TLS-type
// validation. Worker + authorized identity is still rejected before any
// listener state is touched; a failed reload never destroys the old
// SSL_CTX.
TEST_F(DriverTest, TestSysReloadTlsPermissionAndThreadMatrix) {
  ASSERT_TRUE(vm_context_is_main_thread());
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  ASSERT_NE(master_ob, nullptr);

  auto set_authorized = [](int allowed) {
    push_number(allowed);
    auto* ret = safe_apply("set_sys_reload_tls_allowed", master_ob, 1, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
  };
  auto call_reload = [probe](int port_index) -> std::string {
    push_number(port_index);
    auto* ret = safe_apply("call_sys_reload_tls", probe, 1, ORIGIN_DRIVER);
    if (!ret || ret->type != T_STRING) {
      return "<no-string-result>";
    }
    return std::string(ret->u.string);
  };

  // The lpc_tests driver never binds ports (init_user_conn runs in
  // driver_main, not init_main), so no SSL_CTX exists yet: establish one
  // for the TLS port exactly like init_user_conn would, and tear it down
  // when the test finishes.
  auto& tls_port = external_port[3];
  ASSERT_FALSE(tls_port.tls_cert.empty());
  ASSERT_EQ(tls_port.ssl, nullptr);
  tls_port.ssl = tls_server_init(tls_port.tls_cert, tls_port.tls_key);
  ASSERT_NE(tls_port.ssl, nullptr);
  struct TlsCtxGuard {
    port_def_t& port;
    ~TlsCtxGuard() {
      if (port.ssl) {
        tls_server_close(port.ssl);
        port.ssl = nullptr;
      }
    }
  } tls_guard{tls_port};

  // Row: main + UNAUTHORIZED + valid TLS port -> rejected before validation.
  set_authorized(0);
  EXPECT_NE(call_reload(4).find("master authorization"), std::string::npos);

  // Row: main + authorized + valid TLS telnet port (4) -> success.
  set_authorized(1);
  EXPECT_EQ(call_reload(4), "ok");

  // Row: main + authorized + websocket TLS port (3) -> stable rejection.
  EXPECT_NE(call_reload(3).find("websocket"), std::string::npos);

  // Row: main + authorized + non-TLS port (1) -> stable rejection.
  EXPECT_NE(call_reload(1).find("not TLS enabled"), std::string::npos);

  // Row: main + authorized + extreme indexes -> rejected before arithmetic.
  EXPECT_NE(call_reload(0).find("Invalid port index"), std::string::npos);
  EXPECT_NE(call_reload(100).find("Invalid port index"), std::string::npos);
  EXPECT_NE(call_reload(-1).find("Invalid port index"), std::string::npos);

  // Row: reload failure keeps the old SSL_CTX: point the port at a missing
  // certificate; tls_server_init fails, the error fires, and the old
  // context pointer must be unchanged.
  const auto saved_cert = tls_port.tls_cert;
  const auto saved_key = tls_port.tls_key;
  const auto saved_ctx = tls_port.ssl;
  tls_port.tls_cert = "/nonexistent/sys-reload-tls-cert.pem";
  EXPECT_NE(call_reload(4).find("Failed to reload TLS context"), std::string::npos);
  EXPECT_EQ(tls_port.ssl, saved_ctx) << "failed reload must keep the old SSL_CTX";
  tls_port.tls_cert = saved_cert;
  tls_port.tls_key = "/nonexistent/sys-reload-tls-key.pem";
  EXPECT_NE(call_reload(4).find("Failed to reload TLS context"), std::string::npos);
  EXPECT_EQ(tls_port.ssl, saved_ctx) << "failed private-key load must keep the old SSL_CTX";
  tls_port.tls_key = saved_key;
  // The listener still works after the failed reload: a subsequent reload
  // with the valid certificate succeeds.
  EXPECT_EQ(call_reload(4), "ok");

  // Row: worker + authorized identity -> rejected for the thread, before
  // any listener state is touched. The probe runs on the owner worker via
  // the ordinary LPC route and returns the catch() text.
  {
    const char* owner = "owner/test/reload-tls-worker";
    vm_owner_thread_stop();
    object_t* probe2 = load_object_for_test("single/void");
    ASSERT_NE(probe2, nullptr);
    vm_owner_set_id(probe2, owner);
    auto* submitted = vm_owner_ordinary_lpc_task(probe2, owner, "call_sys_reload_tls_probe", 1);
    auto future_id = static_cast<uint64_t>(mapping_number(submitted, "future_id"));
    ASSERT_EQ(mapping_number(submitted, "success"), 1);
    free_mapping(submitted);
    vm_owner_thread_start(1);
    for (int i = 0; i < 200; i++) {
      auto* polled = vm_owner_future_poll(future_id);
      auto state = std::string(mapping_string(polled, "state"));
      free_mapping(polled);
      if (state != "pending") {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    auto* completed = vm_owner_future_poll(future_id);
    ASSERT_STREQ(mapping_string(completed, "state"), "completed");
    auto* result = find_string_in_mapping(completed, "result");
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->type, T_STRING);
    EXPECT_NE(std::string(result->u.string).find("main thread"), std::string::npos)
        << "worker + authorized must be rejected for the thread: " << result->u.string;
    free_mapping(completed);
    auto* taken = vm_owner_future_take(future_id);
    ASSERT_EQ(mapping_number(taken, "consumed"), 1);
    free_mapping(taken);
    vm_owner_thread_stop();
    vm_owner_clear_id(probe2);
    destruct_object(probe2);
  }

  set_authorized(1);
  destruct_object(probe);
}

// R2-F06: allowlisted LPC running ON the owner worker must not be able to
// submit object-target tasks. Every nested submission (owner_call_async,
// owner_async, vm_owner_lpc_task, ordinary LPC) is stably rejected with
// main_thread_admission_required, and no plain refcount is mutated off-main.
TEST_F(DriverTest, TestVmOwnerWorkerNestedObjectTargetSubmissionRejected) {
  const char* owner = "owner/test/nested-submission";

  vm_owner_thread_stop();
  vm_object_store_test_support_reset_worker_ref_mutation_count();

  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);

  auto mapping_mapping = [](mapping_t* map, const char* key) -> mapping_t* {
    auto* value = map ? find_string_in_mapping(map, key) : nullptr;
    EXPECT_NE(value, nullptr) << key;
    EXPECT_EQ(value ? value->type : T_INVALID, T_MAPPING) << key;
    return value && value->type == T_MAPPING ? value->u.map : nullptr;
  };

  // Run the nested-submission probe on the worker via the ordinary LPC
  // route (mapping results accepted; the probe method itself then attempts
  // owner_call_async / owner_async / vm_owner_lpc_task / ordinary LPC
  // submissions from the worker).
  auto* submitted = vm_owner_ordinary_lpc_task(probe, owner, "owner_nested_submission", 1);
  auto future_id = static_cast<uint64_t>(mapping_number(submitted, "future_id"));
  ASSERT_EQ(mapping_number(submitted, "success"), 1);
  ASSERT_GT(future_id, 0u);
  free_mapping(submitted);

  vm_owner_thread_start(1);
  for (int i = 0; i < 200; i++) {
    auto* polled = vm_owner_future_poll(future_id);
    auto state = std::string(mapping_string(polled, "state"));
    free_mapping(polled);
    if (state != "pending") {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  auto* completed = vm_owner_future_poll(future_id);
  ASSERT_STREQ(mapping_string(completed, "state"), "completed");
  auto* result = mapping_mapping(completed, "result");
  ASSERT_NE(result, nullptr);
  const char* kProbeKeys[] = {"owner_call_async", "owner_async", "vm_owner_lpc_task",
                              "vm_owner_ordinary_lpc_task"};
  for (const char* key : kProbeKeys) {
    auto* nested = mapping_mapping(result, key);
    ASSERT_NE(nested, nullptr) << key;
    EXPECT_EQ(mapping_number(nested, "success"), 0) << key;
    EXPECT_STREQ(mapping_string(nested, "error"), "main_thread_admission_required") << key;
  }
  free_mapping(completed);

  // The worker never mutated a plain refcount; the deferred release queue
  // drains to zero after stop + main-task drain.
  ASSERT_EQ(vm_object_store_test_support_worker_ref_mutation_count(), 0u);
  vm_owner_thread_stop();
  free_mapping(vm_owner_purge_mailbox(owner));
  vm_owner_drain_main_tasks(1);
  auto* status = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(status, "deferred_target_releases"), 0);
  free_mapping(status);

  auto* taken = vm_owner_future_take(future_id);
  ASSERT_EQ(mapping_number(taken, "consumed"), 1);
  free_mapping(taken);

  vm_owner_clear_id(probe);
  destruct_object(probe);
}

TEST_F(DriverTest, TestVmObjectHandleReportsBasicResolveFailures) {
  VMObjectHandle invalid_handle;
  auto invalid_status = vm_object_handle_resolve_status(invalid_handle);
  ASSERT_EQ(invalid_status.object, nullptr);
  ASSERT_EQ(invalid_status.status, VMObjectHandleResolveStatus::kInvalidHandle);
  ASSERT_STREQ(vm_object_handle_resolve_status_name(invalid_status.status), "invalid_handle");

  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  vm_owner_set_id(obj, "owner/test/handle/basic");
  auto handle = vm_object_handle(obj);
  ASSERT_TRUE(handle.valid);

  auto missing_path_handle = handle;
  missing_path_handle.object_path.clear();
  auto missing_path_status = vm_object_handle_resolve_status(missing_path_handle);
  ASSERT_EQ(missing_path_status.object, nullptr);
  ASSERT_EQ(missing_path_status.status, VMObjectHandleResolveStatus::kMissingPath);
  ASSERT_STREQ(vm_object_handle_resolve_status_name(missing_path_status.status), "missing_path");

  auto object_id_mismatch_handle = handle;
  object_id_mismatch_handle.object_id++;
  auto object_id_mismatch_status = vm_object_handle_resolve_status(object_id_mismatch_handle);
  ASSERT_EQ(object_id_mismatch_status.object, nullptr);
  ASSERT_EQ(object_id_mismatch_status.status, VMObjectHandleResolveStatus::kObjectIdMismatch);
  ASSERT_STREQ(vm_object_handle_resolve_status_name(object_id_mismatch_status.status),
               "object_id_mismatch");
  ASSERT_TRUE(object_id_mismatch_status.diagnosed_via_owner_local_store);
  ASSERT_TRUE(object_id_mismatch_status.diagnosed_via_owner_local_path_index);
  ASSERT_FALSE(object_id_mismatch_status.diagnosed_via_owner_local_cross_shard);
  ASSERT_FALSE(object_id_mismatch_status.owner_local_object_pointer_index_found);
  ASSERT_FALSE(object_id_mismatch_status.global_live_object_found);
  ASSERT_TRUE(object_id_mismatch_status.global_live_object_source.empty());
  ASSERT_FALSE(object_id_mismatch_status.global_record_found);
  ASSERT_FALSE(object_id_mismatch_status.global_record_id_scan_bridge_used);
  ASSERT_FALSE(object_id_mismatch_status.global_record_id_scan_bridge_found);
  ASSERT_TRUE(object_id_mismatch_status.global_record_id_scan_bridge_source.empty());
  ASSERT_FALSE(object_id_mismatch_status.global_record_id_scan_bridge_skipped);
  ASSERT_TRUE(object_id_mismatch_status.global_record_id_scan_bridge_skip_reason.empty());
  ASSERT_FALSE(object_id_mismatch_status.global_record_pointer_bridge_used);
  ASSERT_FALSE(object_id_mismatch_status.global_record_pointer_bridge_found);
  ASSERT_TRUE(object_id_mismatch_status.global_record_pointer_bridge_source.empty());
  ASSERT_FALSE(object_id_mismatch_status.global_record_pointer_bridge_skipped);
  ASSERT_TRUE(object_id_mismatch_status.global_record_pointer_bridge_skip_reason.empty());
  ASSERT_FALSE(object_id_mismatch_status.diagnosed_via_global_index);

  auto object_not_found_handle = handle;
  object_not_found_handle.object_id += 1000000;
  object_not_found_handle.object_path += ".missing";
  auto object_not_found_status = vm_object_handle_resolve_status(object_not_found_handle);
  ASSERT_EQ(object_not_found_status.object, nullptr);
  ASSERT_EQ(object_not_found_status.status, VMObjectHandleResolveStatus::kObjectNotFound);
  ASSERT_STREQ(vm_object_handle_resolve_status_name(object_not_found_status.status),
               "object_not_found");
  ASSERT_FALSE(object_not_found_status.diagnosed_via_owner_local_store);
  ASSERT_FALSE(object_not_found_status.diagnosed_via_owner_local_path_index);
  ASSERT_FALSE(object_not_found_status.diagnosed_via_owner_local_cross_shard);
  ASSERT_FALSE(object_not_found_status.owner_local_object_pointer_index_found);
  ASSERT_FALSE(object_not_found_status.global_live_object_found);
  ASSERT_TRUE(object_not_found_status.global_live_object_source.empty());
  ASSERT_TRUE(object_not_found_status.global_live_object_bridge_retirement_ready);
  ASSERT_TRUE(object_not_found_status.global_live_object_fallback_skipped);
  ASSERT_EQ(object_not_found_status.global_live_object_fallback_reason,
            "global_live_object_bridge_retirement_ready");
  ASSERT_FALSE(object_not_found_status.global_record_found);
  ASSERT_FALSE(object_not_found_status.global_record_id_scan_bridge_used);
  ASSERT_FALSE(object_not_found_status.global_record_id_scan_bridge_found);
  ASSERT_TRUE(object_not_found_status.global_record_id_scan_bridge_source.empty());
  ASSERT_TRUE(object_not_found_status.global_record_id_scan_bridge_skipped);
  ASSERT_EQ(object_not_found_status.global_record_id_scan_bridge_skip_reason,
            "global_record_bridge_retirement_ready");
  ASSERT_FALSE(object_not_found_status.global_record_pointer_bridge_used);
  ASSERT_FALSE(object_not_found_status.global_record_pointer_bridge_found);
  ASSERT_TRUE(object_not_found_status.global_record_pointer_bridge_source.empty());
  ASSERT_FALSE(object_not_found_status.global_record_pointer_bridge_skipped);
  ASSERT_TRUE(object_not_found_status.global_record_pointer_bridge_skip_reason.empty());
  ASSERT_TRUE(object_not_found_status.global_record_bridge_retirement_ready);
  ASSERT_TRUE(object_not_found_status.global_record_fallback_skipped);
  ASSERT_EQ(object_not_found_status.global_record_fallback_reason,
            "global_record_bridge_retirement_ready");
  ASSERT_FALSE(object_not_found_status.diagnosed_via_global_index);
  ASSERT_FALSE(object_not_found_status.resolved_via_global_index);

  vm_owner_clear_id(obj);
  destruct_object(obj);
}

TEST_F(DriverTest, TestVmObjectStoreReportsPointerBridgeSkippedWhenRecordBridgeRetired) {

  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  vm_owner_set_id(obj, "owner/test/pointer-bridge/record-ready");
  vm_object_store_register(obj);
  auto handle = vm_object_handle(obj);
  ASSERT_TRUE(handle.valid);
  ASSERT_TRUE(vm_object_store_test_support_remove_live_object_ref_for_bridge_readiness(
      handle.owner_id.c_str(), handle.object_id));

  auto* store_status = vm_object_store_status();
  ASSERT_GE(mapping_number(store_status, "registered_objects"), 1);
  free_mapping(store_status);

  std::string bridge_path = "test/pointer_bridge_live_only";
  object_t live_only_object{};
  live_only_object.obname = bridge_path.c_str();
  live_only_object.flags = 0;
  ASSERT_TRUE(ObjectTable::instance().insert(bridge_path, &live_only_object));
  struct ObjectTableEntryGuard {
    std::string path;
    ~ObjectTableEntryGuard() {
      ObjectTable::instance().remove(path);
    }
  } bridge_entry{bridge_path};

  VMObjectHandle bridge_handle;
  bridge_handle.valid = true;
  bridge_handle.object_id = handle.object_id + 1000000;
  bridge_handle.owner_id = "owner/test/pointer-bridge/missing";
  bridge_handle.owner_epoch = 1;
  bridge_handle.object_path = bridge_path;

  auto handle_status = vm_object_handle_resolve_status(bridge_handle);
  ASSERT_EQ(handle_status.object, nullptr);
  ASSERT_EQ(handle_status.status, VMObjectHandleResolveStatus::kObjectNotFound);
  ASSERT_TRUE(handle_status.global_live_object_found);
  ASSERT_EQ(handle_status.global_live_object_source, "ObjectTable.global_live_object_bridge");
  ASSERT_FALSE(handle_status.global_live_object_bridge_retirement_ready);
  ASSERT_FALSE(handle_status.global_live_object_fallback_skipped);
  ASSERT_FALSE(handle_status.global_record_pointer_bridge_used);
  ASSERT_FALSE(handle_status.global_record_pointer_bridge_found);
  ASSERT_TRUE(handle_status.global_record_pointer_bridge_source.empty());
  ASSERT_TRUE(handle_status.global_record_pointer_bridge_skipped);
  ASSERT_EQ(handle_status.global_record_pointer_bridge_skip_reason,
            "global_record_bridge_retirement_ready");
  ASSERT_TRUE(handle_status.global_record_bridge_retirement_ready);
  ASSERT_TRUE(handle_status.global_record_fallback_skipped);
  ASSERT_EQ(handle_status.global_record_fallback_reason, "global_record_bridge_retirement_ready");
  ASSERT_FALSE(handle_status.global_record_found);
  ASSERT_FALSE(handle_status.diagnosed_via_global_index);
  ASSERT_FALSE(handle_status.resolved_via_global_index);

  // Worker-safe resolution is owner-local only: the same compatibility-only
  // object is rejected without touching either global bridge.
  auto owner_local_status = vm_object_handle_resolve_owner_local_status(bridge_handle);
  ASSERT_EQ(owner_local_status.object, nullptr);
  ASSERT_EQ(owner_local_status.status, VMObjectHandleResolveStatus::kObjectNotFound);
  ASSERT_FALSE(owner_local_status.global_live_object_found);
  ASSERT_TRUE(owner_local_status.global_live_object_source.empty());
  ASSERT_FALSE(owner_local_status.global_record_found);
  ASSERT_FALSE(owner_local_status.global_record_id_scan_bridge_used);
  ASSERT_FALSE(owner_local_status.global_record_pointer_bridge_used);
  ASSERT_FALSE(owner_local_status.diagnosed_via_global_index);
  ASSERT_FALSE(owner_local_status.resolved_via_global_index);

  auto* path_lookup = vm_object_store_owner_path_lookup_status("owner/test/pointer-bridge/missing",
                                                               bridge_path.c_str());
  ASSERT_EQ(mapping_number(path_lookup, "success"), 1);
  ASSERT_EQ(mapping_number(path_lookup, "found"), 0);
  ASSERT_EQ(mapping_number(path_lookup, "owner_local_global_live_object_found"), 1);
  ASSERT_STREQ(mapping_string(path_lookup, "owner_local_global_live_object_source"),
               "ObjectTable.global_live_object_bridge");
  ASSERT_EQ(mapping_number(path_lookup, "owner_local_global_live_object_fallback_skipped"), 0);
  ASSERT_STREQ(mapping_string(path_lookup, "owner_local_global_live_object_fallback_reason"), "");
  ASSERT_EQ(mapping_number(path_lookup, "owner_local_global_record_pointer_bridge_used"), 0);
  ASSERT_EQ(mapping_number(path_lookup, "owner_local_global_record_pointer_bridge_found"), 0);
  ASSERT_STREQ(mapping_string(path_lookup, "owner_local_global_record_pointer_bridge_source"), "");
  ASSERT_EQ(mapping_number(path_lookup, "owner_local_global_record_pointer_bridge_skipped"), 1);
  ASSERT_STREQ(mapping_string(path_lookup, "owner_local_global_record_pointer_bridge_skip_reason"),
               "global_record_bridge_retirement_ready");
  ASSERT_EQ(mapping_number(path_lookup, "owner_local_global_record_found"), 0);
  ASSERT_EQ(mapping_number(path_lookup, "owner_local_global_record_fallback_skipped"), 1);
  ASSERT_STREQ(mapping_string(path_lookup, "owner_local_global_record_fallback_reason"),
               "global_record_bridge_retirement_ready");
  ASSERT_EQ(mapping_number(path_lookup, "owner_local_global_record_scan_bridge_used"), 0);
  ASSERT_EQ(mapping_number(path_lookup, "owner_local_global_record_scan_bridge_found"), 0);
  ASSERT_STREQ(mapping_string(path_lookup, "owner_local_global_record_scan_bridge_source"), "");
  ASSERT_EQ(mapping_number(path_lookup, "owner_local_global_record_scan_bridge_skipped"), 1);
  ASSERT_STREQ(mapping_string(path_lookup, "owner_local_global_record_scan_bridge_skip_reason"),
               "global_record_bridge_retirement_ready");
  ASSERT_EQ(mapping_number(path_lookup, "global_record_bridge_retirement_ready"), 1);
  ASSERT_EQ(mapping_number(path_lookup, "global_live_object_bridge_retirement_ready"), 0);
  free_mapping(path_lookup);

  vm_owner_clear_id(obj);
  destruct_object(obj);
}

TEST_F(DriverTest, TestVmNameLookupPrefersOwnerLocalPathIndex) {
  struct MulticoreModeGuard {
    int saved_mode;
    ~MulticoreModeGuard() {
      CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
    }
  } mode_guard{CONFIG_INT(__RC_MULTICORE_MODE__)};
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;

  ScopedCurrentObjectAsMaster master_scope;
  auto* object = clone_object_for_test("single/void");
  ASSERT_NE(object, nullptr);
  vm_object_store_register(object);
  const std::string object_path = object->obname;

  EXPECT_EQ(vm_object_store_find_live_by_path(object_path.c_str()), object);
  const bool removed = ObjectTable::instance().remove(object_path);
  if (!removed) {
    ADD_FAILURE() << "test object was not present in ObjectTable";
    destruct_object_for_test(object);
    return;
  }

  // The canonical owner-local path index must remain usable when the legacy
  // global name entry is absent.
  EXPECT_EQ(vm_object_store_find_live_by_path(object_path.c_str()), object);
  EXPECT_EQ(find_object2(object_path.c_str()), object);
  EXPECT_EQ(find_object(object_path.c_str()), object);

  EXPECT_TRUE(ObjectTable::instance().insert(object_path, object));

  // Tracking disabled is an explicit compatibility fallback; the helper must
  // not return a pointer from stale owner-local state in that mode.
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_OFF;
  EXPECT_EQ(vm_object_store_find_live_by_path(object_path.c_str()), nullptr);
  EXPECT_EQ(find_object2(object_path.c_str()), object);
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;

  destruct_object_for_test(object);
  EXPECT_EQ(vm_object_store_find_live_by_path(object_path.c_str()), nullptr);
  EXPECT_EQ(find_object2(object_path.c_str()), nullptr);
}

TEST_F(DriverTest, TestVmObjectStoreRecordsOwnerMigrationTrace) {
  auto expect_no_lookup_global_live_object = [&](mapping_t* map) {
    ASSERT_EQ(mapping_number(map, "owner_local_global_live_object_found"), 0);
    ASSERT_STREQ(mapping_string(map, "owner_local_global_live_object_source"), "");
    ASSERT_EQ(mapping_number(map, "global_live_object_bridge_retirement_ready"), 1);
    ASSERT_EQ(mapping_number(map, "owner_local_global_live_object_fallback_skipped"), 0);
    ASSERT_STREQ(mapping_string(map, "owner_local_global_live_object_fallback_reason"), "");
  };
  auto expect_no_lookup_global_record_id_scan = [&](mapping_t* map) {
    ASSERT_EQ(mapping_number(map, "owner_local_global_record_id_scan_bridge_used"), 0);
    ASSERT_EQ(mapping_number(map, "owner_local_global_record_id_scan_bridge_found"), 0);
    ASSERT_STREQ(mapping_string(map, "owner_local_global_record_id_scan_bridge_source"), "");
    ASSERT_EQ(mapping_number(map, "owner_local_global_record_id_scan_bridge_skipped"), 0);
    ASSERT_STREQ(mapping_string(map, "owner_local_global_record_id_scan_bridge_skip_reason"), "");
  };
  auto expect_skipped_lookup_global_record_id_scan = [&](mapping_t* map) {
    ASSERT_EQ(mapping_number(map, "owner_local_global_record_id_scan_bridge_used"), 0);
    ASSERT_EQ(mapping_number(map, "owner_local_global_record_id_scan_bridge_found"), 0);
    ASSERT_STREQ(mapping_string(map, "owner_local_global_record_id_scan_bridge_source"), "");
    ASSERT_EQ(mapping_number(map, "owner_local_global_record_id_scan_bridge_skipped"), 1);
    ASSERT_STREQ(mapping_string(map, "owner_local_global_record_id_scan_bridge_skip_reason"),
                 "global_record_bridge_retirement_ready");
  };
  auto expect_no_lookup_global_record_pointer = [&](mapping_t* map) {
    ASSERT_EQ(mapping_number(map, "owner_local_global_record_pointer_bridge_used"), 0);
    ASSERT_EQ(mapping_number(map, "owner_local_global_record_pointer_bridge_found"), 0);
    ASSERT_STREQ(mapping_string(map, "owner_local_global_record_pointer_bridge_source"), "");
    ASSERT_EQ(mapping_number(map, "owner_local_global_record_pointer_bridge_skipped"), 0);
    ASSERT_STREQ(mapping_string(map, "owner_local_global_record_pointer_bridge_skip_reason"), "");
  };
  auto expect_no_lookup_global_record_scan = [&](mapping_t* map) {
    ASSERT_EQ(mapping_number(map, "owner_local_global_record_scan_bridge_used"), 0);
    ASSERT_EQ(mapping_number(map, "owner_local_global_record_scan_bridge_found"), 0);
    ASSERT_STREQ(mapping_string(map, "owner_local_global_record_scan_bridge_source"), "");
    ASSERT_EQ(mapping_number(map, "owner_local_global_record_scan_bridge_skipped"), 0);
    ASSERT_STREQ(mapping_string(map, "owner_local_global_record_scan_bridge_skip_reason"), "");
  };
  auto expect_skipped_lookup_global_record_scan = [&](mapping_t* map) {
    ASSERT_EQ(mapping_number(map, "owner_local_global_record_scan_bridge_used"), 0);
    ASSERT_EQ(mapping_number(map, "owner_local_global_record_scan_bridge_found"), 0);
    ASSERT_STREQ(mapping_string(map, "owner_local_global_record_scan_bridge_source"), "");
    ASSERT_EQ(mapping_number(map, "owner_local_global_record_scan_bridge_skipped"), 1);
    ASSERT_STREQ(mapping_string(map, "owner_local_global_record_scan_bridge_skip_reason"),
                 "global_record_bridge_retirement_ready");
  };

  ScopedCurrentObjectAsMaster master_scope;
  object_t* obj = clone_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  vm_owner_set_id(obj, "owner/test/migration/a");
  vm_object_store_register(obj);

  auto* before = vm_object_store_status();
  auto before_migrations = mapping_number(before, "migration_count");
  ASSERT_GE(mapping_number(before, "registered_objects"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_orphan_record_total"), 0);
  ASSERT_GE(mapping_number(before, "global_live_record_total"), 1);
  free_mapping(before);

  auto handle = vm_object_handle(obj);
  ASSERT_NE(handle.object_path.find('#'), std::string::npos);
  ASSERT_EQ(vm_object_store_owner_resolve("owner/test/migration/a", handle.object_id), obj);
  ASSERT_EQ(
      vm_object_store_owner_path_resolve("owner/test/migration/a", handle.object_path.c_str()),
      obj);
  auto* old_lookup_before =
      vm_object_store_owner_lookup_status("owner/test/migration/a", handle.object_id);
  ASSERT_EQ(mapping_number(old_lookup_before, "success"), 1);
  ASSERT_EQ(mapping_number(old_lookup_before, "record_found"), 1);
  ASSERT_EQ(mapping_number(old_lookup_before, "found"), 1);
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_directory_entry"), 1);
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_mismatch"), 0);
  ASSERT_EQ(mapping_number(old_lookup_before, "destructed"), 0);
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_record_found"), 1);
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_destructed_record_found"), 0);
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_record_destructed"), 0);
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_object_ref_found"), 1);
  ASSERT_STREQ(mapping_string(old_lookup_before, "owner_local_object_ref_source"),
               "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_object_ref_index_found"), 1);
  ASSERT_STREQ(mapping_string(old_lookup_before, "owner_local_object_ref_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_object_pointer_index_found"), 1);
  ASSERT_STREQ(mapping_string(old_lookup_before, "owner_local_object_pointer_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_resolve_found"), 1);
  ASSERT_STREQ(mapping_string(old_lookup_before, "owner_local_resolve_source"),
               "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_path_index_found"), 1);
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_destructed_path_index_found"), 0);
  ASSERT_STREQ(mapping_string(old_lookup_before, "owner_local_path_index_source"),
               "vm_object_shard.object_path_index");
  ASSERT_STREQ(mapping_string(old_lookup_before, "owner_local_record_source"),
               "vm_object_shard.local_records");
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_store_complete"), 1);
  ASSERT_STREQ(mapping_string(old_lookup_before, "owner_local_store_complete_blocker"), "");
  ASSERT_EQ(mapping_number(old_lookup_before, "global_index_bridge"), 0);
  expect_no_lookup_global_live_object(old_lookup_before);
  expect_no_lookup_global_record_id_scan(old_lookup_before);
  expect_no_lookup_global_record_pointer(old_lookup_before);
  expect_no_lookup_global_record_scan(old_lookup_before);
  ASSERT_STREQ(mapping_string(old_lookup_before, "record_owner_id"), "owner/test/migration/a");
  ASSERT_STREQ(mapping_string(old_lookup_before, "object_path"), handle.object_path.c_str());
  free_mapping(old_lookup_before);
  auto* old_path_lookup_before = vm_object_store_owner_path_lookup_status(
      "owner/test/migration/a", handle.object_path.c_str());
  ASSERT_EQ(mapping_number(old_path_lookup_before, "success"), 1);
  ASSERT_EQ(mapping_number(old_path_lookup_before, "record_found"), 1);
  ASSERT_EQ(mapping_number(old_path_lookup_before, "found"), 1);
  ASSERT_EQ(mapping_number(old_path_lookup_before, "object_id"),
            static_cast<long>(handle.object_id));
  ASSERT_EQ(mapping_number(old_path_lookup_before, "owner_local_object_ref_found"), 1);
  ASSERT_STREQ(mapping_string(old_path_lookup_before, "owner_local_object_ref_source"),
               "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(old_path_lookup_before, "owner_local_object_ref_index_found"), 1);
  ASSERT_STREQ(mapping_string(old_path_lookup_before, "owner_local_object_ref_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(old_path_lookup_before, "owner_local_object_pointer_index_found"), 1);
  ASSERT_STREQ(mapping_string(old_path_lookup_before, "owner_local_object_pointer_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(old_path_lookup_before, "owner_local_resolve_found"), 1);
  ASSERT_STREQ(mapping_string(old_path_lookup_before, "owner_local_resolve_source"),
               "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(old_path_lookup_before, "owner_local_path_index_found"), 1);
  ASSERT_EQ(mapping_number(old_path_lookup_before, "owner_local_destructed_path_index_found"), 0);
  ASSERT_EQ(mapping_number(old_path_lookup_before, "owner_local_canonical_record_ready"), 1);
  expect_no_lookup_global_live_object(old_path_lookup_before);
  expect_no_lookup_global_record_id_scan(old_path_lookup_before);
  expect_no_lookup_global_record_pointer(old_path_lookup_before);
  expect_no_lookup_global_record_scan(old_path_lookup_before);
  ASSERT_STREQ(mapping_string(old_path_lookup_before, "owner_local_path_index_source"),
               "vm_object_shard.object_path_index");
  free_mapping(old_path_lookup_before);

  vm_owner_set_id(obj, "owner/test/migration/b");
  auto migrated_handle_status = vm_object_handle_resolve_status(handle);
  ASSERT_EQ(migrated_handle_status.object, nullptr);
  ASSERT_STREQ(vm_object_handle_resolve_status_name(migrated_handle_status.status),
               "owner_mismatch");
  ASSERT_FALSE(migrated_handle_status.resolved_via_owner_local_store);
  ASSERT_TRUE(migrated_handle_status.diagnosed_via_owner_local_store);
  ASSERT_FALSE(migrated_handle_status.diagnosed_via_owner_local_path_index);
  ASSERT_TRUE(migrated_handle_status.diagnosed_via_owner_local_cross_shard);
  ASSERT_FALSE(migrated_handle_status.owner_local_object_pointer_index_found);
  ASSERT_FALSE(migrated_handle_status.global_live_object_found);
  ASSERT_FALSE(migrated_handle_status.global_record_found);
  ASSERT_FALSE(migrated_handle_status.diagnosed_via_global_index);
  ASSERT_FALSE(migrated_handle_status.resolved_via_global_index);

  auto* status = vm_object_store_status();
  ASSERT_GE(mapping_number(status, "migration_count"), before_migrations + 1);
  ASSERT_GE(mapping_number(status, "owner_local_record_total"), 1);
  ASSERT_GE(mapping_number(status, "owner_local_object_ref_total"), 1);
  ASSERT_GE(mapping_number(status, "owner_local_path_index_total"), 1);
  ASSERT_EQ(mapping_number(status, "owner_local_orphan_record_total"), 0);
  ASSERT_EQ(mapping_number(status, "global_record_total"),
            mapping_number(status, "registered_objects"));
  ASSERT_GE(mapping_number(status, "global_live_record_total"), 1);
  auto* migrations = find_string_in_mapping(status, "migrations");
  ASSERT_NE(migrations, nullptr);
  ASSERT_EQ(migrations->type, T_ARRAY);
  ASSERT_GE(migrations->u.arr->size, 1);
  auto* migration = migrations->u.arr->item[migrations->u.arr->size - 1].u.map;
  ASSERT_EQ(mapping_number(migration, "object_id"), static_cast<long>(handle.object_id));
  ASSERT_STREQ(mapping_string(migration, "from_owner_id"), "owner/test/migration/a");
  ASSERT_STREQ(mapping_string(migration, "to_owner_id"), "owner/test/migration/b");
  ASSERT_STREQ(mapping_string(migration, "object_path"), handle.object_path.c_str());
  free_mapping(status);

  auto* old_owner = vm_object_store_owner_status("owner/test/migration/a");
  ASSERT_EQ(mapping_number(old_owner, "objects"), 0);
  ASSERT_EQ(mapping_number(old_owner, "object_directory_count"), 0);
  ASSERT_EQ(mapping_number(old_owner, "owner_local_directory_count"), 0);
  ASSERT_EQ(mapping_number(old_owner, "owner_local_record_count"), 0);
  ASSERT_EQ(mapping_number(old_owner, "owner_local_destructed_record_count"), 0);
  ASSERT_EQ(mapping_number(old_owner, "owner_local_object_ref_count"), 0);
  ASSERT_EQ(mapping_number(old_owner, "owner_local_object_ref_index_count"), 0);
  ASSERT_EQ(mapping_number(old_owner, "owner_local_path_index_count"), 0);
  ASSERT_EQ(mapping_number(old_owner, "owner_local_destructed_path_index_count"), 0);
  free_mapping(old_owner);
  auto* old_lookup_after =
      vm_object_store_owner_lookup_status("owner/test/migration/a", handle.object_id);
  ASSERT_EQ(mapping_number(old_lookup_after, "success"), 1);
  ASSERT_EQ(mapping_number(old_lookup_after, "record_found"), 1);
  ASSERT_EQ(mapping_number(old_lookup_after, "found"), 0);
  ASSERT_EQ(mapping_number(old_lookup_after, "owner_local_directory_entry"), 0);
  ASSERT_EQ(mapping_number(old_lookup_after, "owner_local_record_found"), 0);
  ASSERT_EQ(mapping_number(old_lookup_after, "owner_local_destructed_record_found"), 0);
  ASSERT_EQ(mapping_number(old_lookup_after, "owner_local_record_destructed"), 0);
  ASSERT_EQ(mapping_number(old_lookup_after, "owner_local_cross_shard_record_found"), 1);
  ASSERT_STREQ(mapping_string(old_lookup_after, "owner_local_cross_shard_record_source"),
               "vm_object_shard.local_records");
  ASSERT_EQ(mapping_number(old_lookup_after, "owner_local_global_record_found"), 0);
  ASSERT_STREQ(mapping_string(old_lookup_after, "owner_local_global_record_source"), "");
  expect_no_lookup_global_live_object(old_lookup_after);
  expect_no_lookup_global_record_id_scan(old_lookup_after);
  expect_no_lookup_global_record_pointer(old_lookup_after);
  expect_no_lookup_global_record_scan(old_lookup_after);
  ASSERT_EQ(mapping_number(old_lookup_after, "owner_local_object_ref_found"), 0);
  ASSERT_STREQ(mapping_string(old_lookup_after, "owner_local_object_ref_source"), "");
  ASSERT_EQ(mapping_number(old_lookup_after, "owner_local_object_ref_index_found"), 0);
  ASSERT_STREQ(mapping_string(old_lookup_after, "owner_local_object_ref_index_source"), "");
  ASSERT_EQ(mapping_number(old_lookup_after, "owner_local_object_pointer_index_found"), 0);
  ASSERT_STREQ(mapping_string(old_lookup_after, "owner_local_object_pointer_index_source"), "");
  ASSERT_EQ(mapping_number(old_lookup_after, "owner_local_resolve_found"), 0);
  ASSERT_STREQ(mapping_string(old_lookup_after, "owner_local_resolve_source"), "");
  ASSERT_EQ(mapping_number(old_lookup_after, "owner_local_path_index_found"), 0);
  ASSERT_EQ(mapping_number(old_lookup_after, "owner_local_destructed_path_index_found"), 0);
  ASSERT_STREQ(mapping_string(old_lookup_after, "owner_local_record_source"), "");
  ASSERT_STREQ(mapping_string(old_lookup_after, "owner_local_path_index_source"), "");
  ASSERT_EQ(mapping_number(old_lookup_after, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(old_lookup_after, "owner_mismatch"), 1);
  ASSERT_STREQ(mapping_string(old_lookup_after, "record_owner_id"), "owner/test/migration/b");
  free_mapping(old_lookup_after);
  ASSERT_EQ(vm_object_store_owner_resolve("owner/test/migration/a", handle.object_id), nullptr);
  ASSERT_EQ(
      vm_object_store_owner_path_resolve("owner/test/migration/a", handle.object_path.c_str()),
      nullptr);
  auto* old_path_lookup_after = vm_object_store_owner_path_lookup_status(
      "owner/test/migration/a", handle.object_path.c_str());
  ASSERT_EQ(mapping_number(old_path_lookup_after, "success"), 1);
  ASSERT_EQ(mapping_number(old_path_lookup_after, "record_found"), 1);
  ASSERT_EQ(mapping_number(old_path_lookup_after, "found"), 0);
  ASSERT_EQ(mapping_number(old_path_lookup_after, "owner_local_cross_shard_record_found"), 1);
  ASSERT_STREQ(mapping_string(old_path_lookup_after, "owner_local_cross_shard_record_source"),
               "vm_object_shard.object_path_index");
  ASSERT_EQ(mapping_number(old_path_lookup_after, "owner_local_global_record_found"), 0);
  ASSERT_STREQ(mapping_string(old_path_lookup_after, "owner_local_global_record_source"), "");
  expect_no_lookup_global_live_object(old_path_lookup_after);
  expect_no_lookup_global_record_id_scan(old_path_lookup_after);
  expect_no_lookup_global_record_pointer(old_path_lookup_after);
  expect_no_lookup_global_record_scan(old_path_lookup_after);
  ASSERT_EQ(mapping_number(old_path_lookup_after, "owner_local_object_ref_found"), 0);
  ASSERT_STREQ(mapping_string(old_path_lookup_after, "owner_local_object_ref_source"), "");
  ASSERT_EQ(mapping_number(old_path_lookup_after, "owner_local_object_ref_index_found"), 0);
  ASSERT_STREQ(mapping_string(old_path_lookup_after, "owner_local_object_ref_index_source"), "");
  ASSERT_EQ(mapping_number(old_path_lookup_after, "owner_local_object_pointer_index_found"), 0);
  ASSERT_STREQ(mapping_string(old_path_lookup_after, "owner_local_object_pointer_index_source"),
               "");
  ASSERT_EQ(mapping_number(old_path_lookup_after, "owner_local_resolve_found"), 0);
  ASSERT_STREQ(mapping_string(old_path_lookup_after, "owner_local_resolve_source"), "");
  ASSERT_EQ(mapping_number(old_path_lookup_after, "owner_local_path_index_found"), 0);
  ASSERT_EQ(mapping_number(old_path_lookup_after, "owner_local_destructed_path_index_found"), 0);
  ASSERT_EQ(mapping_number(old_path_lookup_after, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(old_path_lookup_after, "owner_mismatch"), 1);
  ASSERT_STREQ(mapping_string(old_path_lookup_after, "record_owner_id"), "owner/test/migration/b");
  ASSERT_STREQ(mapping_string(old_path_lookup_after, "owner_local_path_index_source"), "");
  free_mapping(old_path_lookup_after);

  auto* new_owner = vm_object_store_owner_status("owner/test/migration/b");
  ASSERT_EQ(mapping_number(new_owner, "objects"), 1);
  ASSERT_EQ(mapping_number(new_owner, "object_directory_count"), 1);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_directory_count"), 1);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_record_count"), 1);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_destructed_record_count"), 0);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_object_ref_count"), 1);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_object_ref_index_count"), 1);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_path_index_count"), 1);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_destructed_path_index_count"), 0);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(new_owner, "uses_global_object_table"), 0);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_store_complete"), 1);
  auto* directory = find_string_in_mapping(new_owner, "object_directory");
  ASSERT_NE(directory, nullptr);
  ASSERT_EQ(directory->type, T_ARRAY);
  ASSERT_EQ(directory->u.arr->size, 1);
  ASSERT_EQ(mapping_number(new_owner, "object_directory_count"),
            mapping_number(new_owner, "owner_local_directory_count"));
  auto* directory_record = directory->u.arr->item[0].u.map;
  ASSERT_EQ(mapping_number(directory_record, "object_id"), static_cast<long>(handle.object_id));
  ASSERT_EQ(mapping_number(directory_record, "owner_epoch"),
            static_cast<long>(vm_owner_epoch(obj)));
  ASSERT_EQ(mapping_number(directory_record, "destructed"), 0);
  ASSERT_EQ(mapping_number(directory_record, "live"), 1);
  ASSERT_STREQ(mapping_string(directory_record, "owner_id"), "owner/test/migration/b");
  ASSERT_STREQ(mapping_string(directory_record, "object_path"), handle.object_path.c_str());
  free_mapping(new_owner);
  auto* new_lookup_after =
      vm_object_store_owner_lookup_status("owner/test/migration/b", handle.object_id);
  ASSERT_EQ(mapping_number(new_lookup_after, "success"), 1);
  ASSERT_EQ(mapping_number(new_lookup_after, "record_found"), 1);
  ASSERT_EQ(mapping_number(new_lookup_after, "found"), 1);
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_directory_entry"), 1);
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_record_found"), 1);
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_destructed_record_found"), 0);
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_record_destructed"), 0);
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_object_ref_found"), 1);
  ASSERT_STREQ(mapping_string(new_lookup_after, "owner_local_object_ref_source"),
               "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_object_ref_index_found"), 1);
  ASSERT_STREQ(mapping_string(new_lookup_after, "owner_local_object_ref_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_object_pointer_index_found"), 1);
  ASSERT_STREQ(mapping_string(new_lookup_after, "owner_local_object_pointer_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_resolve_found"), 1);
  ASSERT_STREQ(mapping_string(new_lookup_after, "owner_local_resolve_source"),
               "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_path_index_found"), 1);
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_destructed_path_index_found"), 0);
  expect_no_lookup_global_live_object(new_lookup_after);
  expect_no_lookup_global_record_id_scan(new_lookup_after);
  expect_no_lookup_global_record_pointer(new_lookup_after);
  expect_no_lookup_global_record_scan(new_lookup_after);
  ASSERT_STREQ(mapping_string(new_lookup_after, "owner_local_record_source"),
               "vm_object_shard.local_records");
  ASSERT_STREQ(mapping_string(new_lookup_after, "owner_local_path_index_source"),
               "vm_object_shard.object_path_index");
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_store_complete"), 1);
  ASSERT_STREQ(mapping_string(new_lookup_after, "owner_local_store_complete_blocker"), "");
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_mismatch"), 0);
  ASSERT_STREQ(mapping_string(new_lookup_after, "record_owner_id"), "owner/test/migration/b");
  ASSERT_STREQ(mapping_string(new_lookup_after, "object_path"), handle.object_path.c_str());
  free_mapping(new_lookup_after);
  ASSERT_EQ(vm_object_store_owner_resolve("owner/test/migration/b", handle.object_id), obj);
  ASSERT_EQ(
      vm_object_store_owner_path_resolve("owner/test/migration/b", handle.object_path.c_str()),
      obj);
  auto* new_path_lookup_after = vm_object_store_owner_path_lookup_status(
      "owner/test/migration/b", handle.object_path.c_str());
  ASSERT_EQ(mapping_number(new_path_lookup_after, "success"), 1);
  ASSERT_EQ(mapping_number(new_path_lookup_after, "record_found"), 1);
  ASSERT_EQ(mapping_number(new_path_lookup_after, "found"), 1);
  ASSERT_EQ(mapping_number(new_path_lookup_after, "object_id"),
            static_cast<long>(handle.object_id));
  ASSERT_EQ(mapping_number(new_path_lookup_after, "owner_local_object_ref_found"), 1);
  ASSERT_STREQ(mapping_string(new_path_lookup_after, "owner_local_object_ref_source"),
               "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(new_path_lookup_after, "owner_local_object_ref_index_found"), 1);
  ASSERT_STREQ(mapping_string(new_path_lookup_after, "owner_local_object_ref_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(new_path_lookup_after, "owner_local_object_pointer_index_found"), 1);
  ASSERT_STREQ(mapping_string(new_path_lookup_after, "owner_local_object_pointer_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(new_path_lookup_after, "owner_local_resolve_found"), 1);
  ASSERT_STREQ(mapping_string(new_path_lookup_after, "owner_local_resolve_source"),
               "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(new_path_lookup_after, "owner_local_path_index_found"), 1);
  ASSERT_EQ(mapping_number(new_path_lookup_after, "owner_local_destructed_path_index_found"), 0);
  ASSERT_EQ(mapping_number(new_path_lookup_after, "owner_local_canonical_record_ready"), 1);
  expect_no_lookup_global_live_object(new_path_lookup_after);
  expect_no_lookup_global_record_id_scan(new_path_lookup_after);
  expect_no_lookup_global_record_pointer(new_path_lookup_after);
  expect_no_lookup_global_record_scan(new_path_lookup_after);
  ASSERT_STREQ(mapping_string(new_path_lookup_after, "owner_local_path_index_source"),
               "vm_object_shard.object_path_index");
  ASSERT_STREQ(mapping_string(new_path_lookup_after, "record_owner_id"), "owner/test/migration/b");
  free_mapping(new_path_lookup_after);

  auto* missing_object_lookup =
      vm_object_store_owner_lookup_status("owner/test/migration/b", handle.object_id + 1000000);
  ASSERT_EQ(mapping_number(missing_object_lookup, "success"), 1);
  ASSERT_EQ(mapping_number(missing_object_lookup, "record_found"), 0);
  ASSERT_EQ(mapping_number(missing_object_lookup, "found"), 0);
  ASSERT_EQ(mapping_number(missing_object_lookup, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(missing_object_lookup, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(missing_object_lookup, "owner_local_store_complete"), 1);
  ASSERT_STREQ(mapping_string(missing_object_lookup, "owner_local_store_complete_blocker"), "");
  ASSERT_EQ(mapping_number(missing_object_lookup, "global_record_bridge_retirement_ready"), 1);
  ASSERT_EQ(mapping_number(missing_object_lookup, "owner_local_global_record_found"), 0);
  ASSERT_STREQ(mapping_string(missing_object_lookup, "owner_local_global_record_source"), "");
  ASSERT_EQ(mapping_number(missing_object_lookup, "owner_local_global_record_fallback_skipped"), 1);
  ASSERT_STREQ(mapping_string(missing_object_lookup, "owner_local_global_record_fallback_reason"),
               "global_record_bridge_retirement_ready");
  expect_skipped_lookup_global_record_id_scan(missing_object_lookup);
  expect_no_lookup_global_record_pointer(missing_object_lookup);
  expect_no_lookup_global_record_scan(missing_object_lookup);
  ASSERT_EQ(mapping_number(missing_object_lookup, "global_live_object_bridge_retirement_ready"), 1);
  ASSERT_EQ(mapping_number(missing_object_lookup, "owner_local_global_live_object_found"), 0);
  ASSERT_STREQ(mapping_string(missing_object_lookup, "owner_local_global_live_object_source"), "");
  ASSERT_EQ(
      mapping_number(missing_object_lookup, "owner_local_global_live_object_fallback_skipped"), 0);
  ASSERT_STREQ(
      mapping_string(missing_object_lookup, "owner_local_global_live_object_fallback_reason"), "");
  free_mapping(missing_object_lookup);

  std::string missing_path = handle.object_path + ".missing";
  auto* missing_path_lookup =
      vm_object_store_owner_path_lookup_status("owner/test/migration/b", missing_path.c_str());
  ASSERT_EQ(mapping_number(missing_path_lookup, "success"), 1);
  ASSERT_EQ(mapping_number(missing_path_lookup, "record_found"), 0);
  ASSERT_EQ(mapping_number(missing_path_lookup, "found"), 0);
  ASSERT_EQ(mapping_number(missing_path_lookup, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(missing_path_lookup, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(missing_path_lookup, "owner_local_store_complete"), 1);
  ASSERT_STREQ(mapping_string(missing_path_lookup, "owner_local_store_complete_blocker"), "");
  ASSERT_EQ(mapping_number(missing_path_lookup, "global_record_bridge_retirement_ready"), 1);
  ASSERT_EQ(mapping_number(missing_path_lookup, "owner_local_global_record_found"), 0);
  ASSERT_STREQ(mapping_string(missing_path_lookup, "owner_local_global_record_source"), "");
  ASSERT_EQ(mapping_number(missing_path_lookup, "owner_local_global_record_fallback_skipped"), 1);
  ASSERT_STREQ(mapping_string(missing_path_lookup, "owner_local_global_record_fallback_reason"),
               "global_record_bridge_retirement_ready");
  expect_no_lookup_global_record_id_scan(missing_path_lookup);
  expect_no_lookup_global_record_pointer(missing_path_lookup);
  expect_skipped_lookup_global_record_scan(missing_path_lookup);
  ASSERT_EQ(mapping_number(missing_path_lookup, "global_live_object_bridge_retirement_ready"), 1);
  ASSERT_EQ(mapping_number(missing_path_lookup, "owner_local_global_live_object_found"), 0);
  ASSERT_STREQ(mapping_string(missing_path_lookup, "owner_local_global_live_object_source"), "");
  ASSERT_EQ(mapping_number(missing_path_lookup, "owner_local_global_live_object_fallback_skipped"),
            1);
  ASSERT_STREQ(
      mapping_string(missing_path_lookup, "owner_local_global_live_object_fallback_reason"),
      "global_live_object_bridge_retirement_ready");
  free_mapping(missing_path_lookup);

  vm_owner_clear_id(obj);
  destruct_object(obj);
}

TEST_F(DriverTest, TestVmObjectStoreShardRemovesDestructedObject) {
  auto expect_owner_local_store_complete_contract = [&](mapping_t* /*map*/) {
    // The "store complete" contract block and its readiness keys were deleted
    // with the migration scaffolding; counts on the shard mapping carry the
    // same information, so this helper has nothing left to assert.
  };
  auto expect_no_lookup_global_live_object = [&](mapping_t* map) {
    ASSERT_EQ(mapping_number(map, "owner_local_global_live_object_found"), 0);
    ASSERT_STREQ(mapping_string(map, "owner_local_global_live_object_source"), "");
  };

  auto* missing_owner = vm_object_store_owner_status("owner/test/store/missing");
  ASSERT_EQ(mapping_number(missing_owner, "objects"), 0);
  free_mapping(missing_owner);

  object_t* obj = clone_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  vm_owner_set_id(obj, "owner/test/store/destruct");
  vm_object_store_register(obj);
  auto handle = vm_object_handle(obj);
  auto handle_resolve = vm_object_handle_resolve_status(handle);
  ASSERT_EQ(handle_resolve.object, obj);
  ASSERT_STREQ(vm_object_handle_resolve_status_name(handle_resolve.status), "current");
  ASSERT_TRUE(handle_resolve.resolved_via_owner_local_store);
  ASSERT_TRUE(handle_resolve.owner_local_fast_path_used);
  ASSERT_FALSE(handle_resolve.diagnosed_via_owner_local_store);
  ASSERT_FALSE(handle_resolve.diagnosed_via_owner_local_path_index);
  ASSERT_TRUE(handle_resolve.owner_local_object_pointer_index_found);
  ASSERT_FALSE(handle_resolve.global_live_object_found);
  ASSERT_FALSE(handle_resolve.global_record_found);
  ASSERT_FALSE(handle_resolve.resolved_via_global_index);
  ASSERT_EQ(vm_object_store_owner_resolve("owner/test/store/destruct", handle.object_id), obj);
  ASSERT_EQ(
      vm_object_store_owner_path_resolve("owner/test/store/destruct", handle.object_path.c_str()),
      obj);

  auto* before = vm_object_store_owner_status("owner/test/store/destruct");
  ASSERT_EQ(mapping_number(before, "objects"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_record_count"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_destructed_record_count"), 0);
  ASSERT_EQ(mapping_number(before, "owner_local_object_ref_count"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_object_ref_index_count"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_path_index_count"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_destructed_path_index_count"), 0);
  ASSERT_EQ(mapping_number(before, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_store_complete"), 1);
  expect_owner_local_store_complete_contract(before);
  auto before_destructed = mapping_number(before, "destructed");
  free_mapping(before);
  auto* lookup_before =
      vm_object_store_owner_lookup_status("owner/test/store/destruct", handle.object_id);
  ASSERT_EQ(mapping_number(lookup_before, "success"), 1);
  ASSERT_EQ(mapping_number(lookup_before, "record_found"), 1);
  ASSERT_EQ(mapping_number(lookup_before, "found"), 1);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_directory_entry"), 1);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_record_found"), 1);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_destructed_record_found"), 0);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_record_destructed"), 0);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_object_ref_found"), 1);
  ASSERT_STREQ(mapping_string(lookup_before, "owner_local_object_ref_source"),
               "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_object_ref_index_found"), 1);
  ASSERT_STREQ(mapping_string(lookup_before, "owner_local_object_ref_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_object_pointer_index_found"), 1);
  ASSERT_STREQ(mapping_string(lookup_before, "owner_local_object_pointer_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_resolve_found"), 1);
  ASSERT_STREQ(mapping_string(lookup_before, "owner_local_resolve_source"),
               "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_path_index_found"), 1);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_destructed_path_index_found"), 0);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_store_complete"), 1);
  ASSERT_STREQ(mapping_string(lookup_before, "owner_local_store_complete_blocker"), "");
  expect_no_lookup_global_live_object(lookup_before);
  ASSERT_STREQ(mapping_string(lookup_before, "owner_local_path_index_source"),
               "vm_object_shard.object_path_index");
  ASSERT_STREQ(mapping_string(lookup_before, "owner_local_record_source"),
               "vm_object_shard.local_records");
  ASSERT_EQ(mapping_number(lookup_before, "destructed"), 0);
  free_mapping(lookup_before);
  auto* path_lookup_before = vm_object_store_owner_path_lookup_status("owner/test/store/destruct",
                                                                      handle.object_path.c_str());
  ASSERT_EQ(mapping_number(path_lookup_before, "success"), 1);
  ASSERT_EQ(mapping_number(path_lookup_before, "record_found"), 1);
  ASSERT_EQ(mapping_number(path_lookup_before, "found"), 1);
  ASSERT_EQ(mapping_number(path_lookup_before, "object_id"), static_cast<long>(handle.object_id));
  ASSERT_EQ(mapping_number(path_lookup_before, "owner_local_object_ref_found"), 1);
  ASSERT_STREQ(mapping_string(path_lookup_before, "owner_local_object_ref_source"),
               "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(path_lookup_before, "owner_local_object_ref_index_found"), 1);
  ASSERT_STREQ(mapping_string(path_lookup_before, "owner_local_object_ref_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(path_lookup_before, "owner_local_object_pointer_index_found"), 1);
  ASSERT_STREQ(mapping_string(path_lookup_before, "owner_local_object_pointer_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(path_lookup_before, "owner_local_resolve_found"), 1);
  ASSERT_STREQ(mapping_string(path_lookup_before, "owner_local_resolve_source"),
               "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(path_lookup_before, "owner_local_path_index_found"), 1);
  ASSERT_EQ(mapping_number(path_lookup_before, "owner_local_destructed_path_index_found"), 0);
  ASSERT_EQ(mapping_number(path_lookup_before, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(path_lookup_before, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(path_lookup_before, "owner_local_store_complete"), 1);
  ASSERT_STREQ(mapping_string(path_lookup_before, "owner_local_store_complete_blocker"), "");
  expect_no_lookup_global_live_object(path_lookup_before);
  ASSERT_STREQ(mapping_string(path_lookup_before, "owner_local_path_index_source"),
               "vm_object_shard.object_path_index");
  free_mapping(path_lookup_before);

  destruct_object(obj);
  auto destructed_handle_status = vm_object_handle_resolve_status(handle);
  ASSERT_EQ(destructed_handle_status.object, nullptr);
  ASSERT_STREQ(vm_object_handle_resolve_status_name(destructed_handle_status.status),
               "record_destructed");
  ASSERT_FALSE(destructed_handle_status.resolved_via_owner_local_store);
  ASSERT_TRUE(destructed_handle_status.diagnosed_via_owner_local_store);
  ASSERT_FALSE(destructed_handle_status.diagnosed_via_owner_local_path_index);
  ASSERT_FALSE(destructed_handle_status.diagnosed_via_owner_local_cross_shard);
  ASSERT_FALSE(destructed_handle_status.owner_local_object_pointer_index_found);
  ASSERT_FALSE(destructed_handle_status.global_live_object_found);
  ASSERT_FALSE(destructed_handle_status.global_record_found);
  ASSERT_FALSE(destructed_handle_status.diagnosed_via_global_index);
  ASSERT_FALSE(destructed_handle_status.resolved_via_global_index);
  auto* store_after_destruct = vm_object_store_status();
  ASSERT_GE(mapping_number(store_after_destruct, "owner_local_destructed_record_total"), 1);
  ASSERT_EQ(mapping_number(store_after_destruct, "owner_local_object_ref_total"),
            mapping_number(store_after_destruct, "owner_local_object_ref_index_total"));
  ASSERT_GE(mapping_number(store_after_destruct, "owner_local_destructed_path_index_total"), 1);
  ASSERT_EQ(mapping_number(store_after_destruct, "owner_local_orphan_record_total"), 0);
  ASSERT_EQ(mapping_number(store_after_destruct, "global_record_total"),
            mapping_number(store_after_destruct, "registered_objects"));
  ASSERT_GE(mapping_number(store_after_destruct, "global_destructed_record_total"), 1);
  free_mapping(store_after_destruct);
  ASSERT_EQ(vm_object_store_owner_resolve("owner/test/store/destruct", handle.object_id), nullptr);
  ASSERT_EQ(
      vm_object_store_owner_path_resolve("owner/test/store/destruct", handle.object_path.c_str()),
      nullptr);

  auto* after = vm_object_store_owner_status("owner/test/store/destruct");
  ASSERT_EQ(mapping_number(after, "objects"), 0);
  ASSERT_EQ(mapping_number(after, "object_directory_count"), 0);
  ASSERT_EQ(mapping_number(after, "owner_local_directory_count"), 0);
  ASSERT_EQ(mapping_number(after, "owner_local_record_count"), 0);
  ASSERT_EQ(mapping_number(after, "owner_local_destructed_record_count"), 1);
  ASSERT_EQ(mapping_number(after, "owner_local_object_ref_count"), 0);
  ASSERT_EQ(mapping_number(after, "owner_local_object_ref_index_count"), 0);
  ASSERT_EQ(mapping_number(after, "owner_local_path_index_count"), 0);
  ASSERT_EQ(mapping_number(after, "owner_local_destructed_path_index_count"), 1);
  ASSERT_EQ(mapping_number(after, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(after, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(after, "owner_local_store_complete"), 1);
  expect_owner_local_store_complete_contract(after);
  ASSERT_EQ(mapping_number(after, "destructed"), before_destructed + 1);
  free_mapping(after);
  auto* lookup_after =
      vm_object_store_owner_lookup_status("owner/test/store/destruct", handle.object_id);
  ASSERT_EQ(mapping_number(lookup_after, "success"), 1);
  ASSERT_EQ(mapping_number(lookup_after, "record_found"), 1);
  ASSERT_EQ(mapping_number(lookup_after, "found"), 0);
  ASSERT_EQ(mapping_number(lookup_after, "owner_local_directory_entry"), 0);
  ASSERT_EQ(mapping_number(lookup_after, "owner_local_record_found"), 1);
  ASSERT_EQ(mapping_number(lookup_after, "owner_local_destructed_record_found"), 1);
  ASSERT_EQ(mapping_number(lookup_after, "owner_local_record_destructed"), 1);
  ASSERT_EQ(mapping_number(lookup_after, "owner_local_object_ref_found"), 0);
  ASSERT_STREQ(mapping_string(lookup_after, "owner_local_object_ref_source"), "");
  ASSERT_EQ(mapping_number(lookup_after, "owner_local_object_ref_index_found"), 0);
  ASSERT_STREQ(mapping_string(lookup_after, "owner_local_object_ref_index_source"), "");
  ASSERT_EQ(mapping_number(lookup_after, "owner_local_object_pointer_index_found"), 0);
  ASSERT_STREQ(mapping_string(lookup_after, "owner_local_object_pointer_index_source"), "");
  ASSERT_EQ(mapping_number(lookup_after, "owner_local_resolve_found"), 0);
  ASSERT_STREQ(mapping_string(lookup_after, "owner_local_resolve_source"), "");
  ASSERT_EQ(mapping_number(lookup_after, "owner_local_path_index_found"), 0);
  ASSERT_EQ(mapping_number(lookup_after, "owner_local_destructed_path_index_found"), 1);
  expect_no_lookup_global_live_object(lookup_after);
  ASSERT_STREQ(mapping_string(lookup_after, "owner_local_record_source"),
               "vm_object_shard.destructed_records");
  ASSERT_STREQ(mapping_string(lookup_after, "owner_local_path_index_source"),
               "vm_object_shard.destructed_path_index");
  ASSERT_EQ(mapping_number(lookup_after, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(lookup_after, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(lookup_after, "owner_local_store_complete"), 1);
  ASSERT_STREQ(mapping_string(lookup_after, "owner_local_store_complete_blocker"), "");
  ASSERT_EQ(mapping_number(lookup_after, "destructed"), 1);
  free_mapping(lookup_after);
  auto* path_lookup_after = vm_object_store_owner_path_lookup_status("owner/test/store/destruct",
                                                                     handle.object_path.c_str());
  ASSERT_EQ(mapping_number(path_lookup_after, "success"), 1);
  ASSERT_EQ(mapping_number(path_lookup_after, "record_found"), 1);
  ASSERT_EQ(mapping_number(path_lookup_after, "found"), 0);
  ASSERT_EQ(mapping_number(path_lookup_after, "object_id"), static_cast<long>(handle.object_id));
  ASSERT_EQ(mapping_number(path_lookup_after, "owner_local_object_ref_found"), 0);
  ASSERT_STREQ(mapping_string(path_lookup_after, "owner_local_object_ref_source"), "");
  ASSERT_EQ(mapping_number(path_lookup_after, "owner_local_object_ref_index_found"), 0);
  ASSERT_STREQ(mapping_string(path_lookup_after, "owner_local_object_ref_index_source"), "");
  ASSERT_EQ(mapping_number(path_lookup_after, "owner_local_object_pointer_index_found"), 0);
  ASSERT_STREQ(mapping_string(path_lookup_after, "owner_local_object_pointer_index_source"), "");
  ASSERT_EQ(mapping_number(path_lookup_after, "owner_local_resolve_found"), 0);
  ASSERT_STREQ(mapping_string(path_lookup_after, "owner_local_resolve_source"), "");
  ASSERT_EQ(mapping_number(path_lookup_after, "owner_local_path_index_found"), 0);
  ASSERT_EQ(mapping_number(path_lookup_after, "owner_local_destructed_path_index_found"), 1);
  expect_no_lookup_global_live_object(path_lookup_after);
  ASSERT_EQ(mapping_number(path_lookup_after, "owner_local_directory_entry"), 0);
  ASSERT_STREQ(mapping_string(path_lookup_after, "owner_local_record_source"),
               "vm_object_shard.destructed_records");
  ASSERT_STREQ(mapping_string(path_lookup_after, "owner_local_path_index_source"),
               "vm_object_shard.destructed_path_index");
  ASSERT_EQ(mapping_number(path_lookup_after, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(path_lookup_after, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(path_lookup_after, "owner_local_store_complete"), 1);
  ASSERT_STREQ(mapping_string(path_lookup_after, "owner_local_store_complete_blocker"), "");
  free_mapping(path_lookup_after);

  vm_owner_set_id(obj, "owner/test/store/destruct-after");
  ASSERT_EQ(vm_object_store_owner_resolve("owner/test/store/destruct-after", handle.object_id),
            nullptr);
  ASSERT_EQ(vm_object_store_owner_path_resolve("owner/test/store/destruct-after",
                                               handle.object_path.c_str()),
            nullptr);
  auto* moved_after_destruct = vm_object_store_owner_status("owner/test/store/destruct-after");
  ASSERT_EQ(mapping_number(moved_after_destruct, "objects"), 0);
  ASSERT_EQ(mapping_number(moved_after_destruct, "object_directory_count"), 0);
  ASSERT_EQ(mapping_number(moved_after_destruct, "owner_local_directory_count"), 0);
  ASSERT_EQ(mapping_number(moved_after_destruct, "owner_local_record_count"), 0);
  ASSERT_EQ(mapping_number(moved_after_destruct, "owner_local_destructed_record_count"), 1);
  ASSERT_EQ(mapping_number(moved_after_destruct, "owner_local_object_ref_count"), 0);
  ASSERT_EQ(mapping_number(moved_after_destruct, "owner_local_object_ref_index_count"), 0);
  ASSERT_EQ(mapping_number(moved_after_destruct, "owner_local_path_index_count"), 0);
  ASSERT_EQ(mapping_number(moved_after_destruct, "owner_local_destructed_path_index_count"), 1);
  ASSERT_EQ(mapping_number(moved_after_destruct, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(moved_after_destruct, "owner_local_store_complete"), 1);
  free_mapping(moved_after_destruct);
  auto* moved_lookup_after_destruct =
      vm_object_store_owner_lookup_status("owner/test/store/destruct-after", handle.object_id);
  ASSERT_EQ(mapping_number(moved_lookup_after_destruct, "success"), 1);
  ASSERT_EQ(mapping_number(moved_lookup_after_destruct, "record_found"), 1);
  ASSERT_EQ(mapping_number(moved_lookup_after_destruct, "found"), 0);
  ASSERT_EQ(mapping_number(moved_lookup_after_destruct, "owner_local_directory_entry"), 0);
  ASSERT_EQ(mapping_number(moved_lookup_after_destruct, "owner_local_record_found"), 1);
  ASSERT_EQ(mapping_number(moved_lookup_after_destruct, "owner_local_destructed_record_found"), 1);
  ASSERT_EQ(mapping_number(moved_lookup_after_destruct, "owner_local_record_destructed"), 1);
  ASSERT_EQ(mapping_number(moved_lookup_after_destruct, "owner_local_object_ref_found"), 0);
  ASSERT_STREQ(mapping_string(moved_lookup_after_destruct, "owner_local_object_ref_source"), "");
  ASSERT_EQ(mapping_number(moved_lookup_after_destruct, "owner_local_resolve_found"), 0);
  ASSERT_STREQ(mapping_string(moved_lookup_after_destruct, "owner_local_resolve_source"), "");
  ASSERT_EQ(mapping_number(moved_lookup_after_destruct, "owner_local_path_index_found"), 0);
  ASSERT_EQ(mapping_number(moved_lookup_after_destruct, "owner_local_destructed_path_index_found"),
            1);
  expect_no_lookup_global_live_object(moved_lookup_after_destruct);
  ASSERT_STREQ(mapping_string(moved_lookup_after_destruct, "owner_local_record_source"),
               "vm_object_shard.destructed_records");
  ASSERT_STREQ(mapping_string(moved_lookup_after_destruct, "owner_local_path_index_source"),
               "vm_object_shard.destructed_path_index");
  ASSERT_EQ(mapping_number(moved_lookup_after_destruct, "destructed"), 1);
  free_mapping(moved_lookup_after_destruct);
  auto* moved_path_lookup_after_destruct = vm_object_store_owner_path_lookup_status(
      "owner/test/store/destruct-after", handle.object_path.c_str());
  ASSERT_EQ(mapping_number(moved_path_lookup_after_destruct, "success"), 1);
  ASSERT_EQ(mapping_number(moved_path_lookup_after_destruct, "record_found"), 1);
  ASSERT_EQ(mapping_number(moved_path_lookup_after_destruct, "found"), 0);
  ASSERT_EQ(mapping_number(moved_path_lookup_after_destruct, "object_id"),
            static_cast<long>(handle.object_id));
  ASSERT_EQ(mapping_number(moved_path_lookup_after_destruct, "owner_local_object_ref_found"), 0);
  ASSERT_STREQ(mapping_string(moved_path_lookup_after_destruct, "owner_local_object_ref_source"),
               "");
  ASSERT_EQ(mapping_number(moved_path_lookup_after_destruct, "owner_local_path_index_found"), 0);
  ASSERT_EQ(
      mapping_number(moved_path_lookup_after_destruct, "owner_local_destructed_path_index_found"),
      1);
  expect_no_lookup_global_live_object(moved_path_lookup_after_destruct);
  ASSERT_STREQ(mapping_string(moved_path_lookup_after_destruct, "owner_local_record_source"),
               "vm_object_shard.destructed_records");
  ASSERT_STREQ(mapping_string(moved_path_lookup_after_destruct, "owner_local_path_index_source"),
               "vm_object_shard.destructed_path_index");
  free_mapping(moved_path_lookup_after_destruct);
}

TEST_F(DriverTest, TestVmOwnerGuardFailsFastOnMismatch) {
  ScopedCurrentObjectAsMaster master_scope;
  object_t* obj = find_object("single/master.c");
  ASSERT_NE(obj, nullptr);

  vm_owner_set_id(obj, "owner/test/guard");
  auto before_total = vm_owner_total_checks();
  auto before_mismatch = vm_owner_mismatch_checks();

  auto* result = vm_owner_guard(obj, "owner/test/guard");
  ASSERT_NE(result, nullptr);
  auto* success = find_string_in_mapping(result, "success");
  ASSERT_NE(success, nullptr);
  ASSERT_EQ(success->type, T_NUMBER);
  ASSERT_EQ(success->u.number, 1);
  free_mapping(result);
  ASSERT_EQ(vm_owner_total_checks(), before_total + 1);
  ASSERT_EQ(vm_owner_mismatch_checks(), before_mismatch);

  error_context_t econ{};
  save_context(&econ);
  try {
    vm_owner_guard(obj, "owner/test/other");
    pop_context(&econ);
    FAIL() << "vm_owner_guard should reject owner mismatch";
  } catch (...) {
    restore_context(&econ);
  }

  ASSERT_EQ(vm_owner_total_checks(), before_total + 2);
  ASSERT_EQ(vm_owner_mismatch_checks(), before_mismatch + 1);
  vm_owner_clear_id(obj);
}

TEST_F(DriverTest, TestVmWorkerRunsTasksInParallel) {
  auto result = vm_worker_benchmark(4, 80);
  ASSERT_GE(result.worker_count, 1);
  ASSERT_GE(result.max_parallel, std::min(2, result.worker_count));
  ASSERT_GT(result.checksum, 0u);
  ASSERT_LT(result.elapsed_ms, 260);
}

TEST_F(DriverTest, TestVmWorkerAsyncBenchmarkPollsResult) {
  auto task_id = vm_worker_submit_benchmark(4, 80);
  ASSERT_GT(task_id, 0u);

  VMWorkerTaskResult result;
  for (int i = 0; i < 100; i++) {
    result = vm_worker_poll_task(task_id);
    ASSERT_NE(result.state, VMWorkerTaskState::kUnknown);
    if (result.state == VMWorkerTaskState::kSucceeded) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(result.state, VMWorkerTaskState::kSucceeded);
  ASSERT_GE(result.bench.worker_count, 1);
  ASSERT_GE(result.bench.max_parallel, std::min(2, result.bench.worker_count));
  ASSERT_GT(result.bench.checksum, 0u);
  ASSERT_LT(result.bench.elapsed_ms, 300);
  ASSERT_EQ(vm_worker_poll_task(task_id).state, VMWorkerTaskState::kUnknown);
}

TEST_F(DriverTest, TestVmWorkerActorKeysSerializePerOwner) {
  constexpr int kActorBenchmarkMillis =
#ifdef _WIN32
      40;
#else
      80;
#endif
  auto result = vm_worker_actor_benchmark(4, 2, kActorBenchmarkMillis);
  ASSERT_EQ(result.owners, 4);
  ASSERT_EQ(result.tasks_per_owner, 2);
  ASSERT_EQ(result.total_tasks, 8);
  ASSERT_GE(result.worker_count, 1);
  ASSERT_GE(result.max_parallel, std::min(2, result.worker_count));
  ASSERT_EQ(result.max_owner_parallel, 1);
  ASSERT_GT(result.checksum, 0u);
  ASSERT_LT(result.elapsed_ms, 360);
}

TEST_F(DriverTest, TestVmWorkerSnapshotDigestUsesOwnerKey) {
  auto result = vm_worker_snapshot_digest("actor/test", "{\"hp\":100,\"room\":\"test\"}", 16);
  ASSERT_EQ(result.owner_key, "actor/test");
  ASSERT_GE(result.worker_count, 1);
  ASSERT_EQ(result.input_bytes, 24u);
  ASSERT_EQ(result.repeat, 16);
  ASSERT_GT(result.checksum, 0u);
}

TEST_F(DriverTest, TestVmWorkerAsyncSnapshotDigestPollsResult) {
  auto task_id =
      vm_worker_submit_snapshot_digest("actor/async", "{\"hp\":100,\"room\":\"test\"}", 16);
  ASSERT_GT(task_id, 0u);

  VMWorkerTaskResult result;
  for (int i = 0; i < 100; i++) {
    result = vm_worker_poll_task(task_id);
    ASSERT_NE(result.state, VMWorkerTaskState::kUnknown);
    if (result.state == VMWorkerTaskState::kSucceeded) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(result.state, VMWorkerTaskState::kSucceeded);
  ASSERT_EQ(result.type, "snapshot_digest");
  ASSERT_EQ(result.snapshot_digest.owner_key, "actor/async");
  ASSERT_GE(result.snapshot_digest.worker_count, 1);
  ASSERT_EQ(result.snapshot_digest.input_bytes, 24u);
  ASSERT_EQ(result.snapshot_digest.repeat, 16);
  ASSERT_GT(result.snapshot_digest.checksum, 0u);
  ASSERT_EQ(vm_worker_poll_task(task_id).state, VMWorkerTaskState::kUnknown);
}

TEST_F(DriverTest, TestVmWorkerActorScoreUsesSnapshotValues) {
  VMWorkerActorScoreInput input;
  input.hp = 80;
  input.max_hp = 100;
  input.mp = 50;
  input.max_mp = 100;
  input.ep = 100;
  input.max_ep = 100;

  auto result = vm_worker_actor_score("actor/score", input);
  ASSERT_EQ(result.owner_key, "actor/score");
  ASSERT_GE(result.worker_count, 1);
  ASSERT_EQ(result.hp_pct_bp, 8000);
  ASSERT_EQ(result.mp_pct_bp, 5000);
  ASSERT_EQ(result.ep_pct_bp, 10000);
  ASSERT_EQ(result.survival_score, 8000);
  ASSERT_EQ(result.resource_score, 7500);
  ASSERT_EQ(result.total_score, 7850);
  ASSERT_EQ(result.state, "strained");
}

TEST_F(DriverTest, TestVmWorkerAsyncActorScorePollsResult) {
  VMWorkerActorScoreInput input;
  input.hp = 80;
  input.max_hp = 100;
  input.mp = 50;
  input.max_mp = 100;
  input.ep = 100;
  input.max_ep = 100;

  auto task_id = vm_worker_submit_actor_score("actor/score-async", input);
  ASSERT_GT(task_id, 0u);

  VMWorkerTaskResult result;
  for (int i = 0; i < 100; i++) {
    result = vm_worker_poll_task(task_id);
    ASSERT_NE(result.state, VMWorkerTaskState::kUnknown);
    if (result.state == VMWorkerTaskState::kSucceeded) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(result.state, VMWorkerTaskState::kSucceeded);
  ASSERT_EQ(result.type, "actor_score");
  ASSERT_EQ(result.actor_score.owner_key, "actor/score-async");
  ASSERT_GE(result.actor_score.worker_count, 1);
  ASSERT_EQ(result.actor_score.hp_pct_bp, 8000);
  ASSERT_EQ(result.actor_score.mp_pct_bp, 5000);
  ASSERT_EQ(result.actor_score.ep_pct_bp, 10000);
  ASSERT_EQ(result.actor_score.survival_score, 8000);
  ASSERT_EQ(result.actor_score.resource_score, 7500);
  ASSERT_EQ(result.actor_score.total_score, 7850);
  ASSERT_EQ(result.actor_score.state, "strained");
  ASSERT_EQ(vm_worker_poll_task(task_id).state, VMWorkerTaskState::kUnknown);
}

TEST_F(DriverTest, TestVmWorkerCombatDamageBindsHashToSnapshotAndFields) {
  VMWorkerCombatDamageInput input;
  input.attack = 100;
  input.defense = 50;
  input.armor_break = 0;
  input.critical = 0;
  input.critical_resist = 0;
  input.variance_roll_bp = 500;
  input.critical_roll = 100;
  input.snapshot_hash = 424242;

  auto result = vm_worker_combat_damage("combat/test", input);
  ASSERT_EQ(result.owner_key, "combat/test");
  ASSERT_GE(result.worker_count, 1);
  ASSERT_EQ(result.armor_break_bp, 0);
  ASSERT_EQ(result.reduction_bp, 500);
  ASSERT_EQ(result.critical_rate, 5);
  ASSERT_EQ(result.critical_hit, 0);
  ASSERT_EQ(result.damage, 95);
  ASSERT_EQ(result.snapshot_hash, 424242u);
  ASSERT_NE(result.input_hash, 424242u);

  auto changed_attack = input;
  changed_attack.attack = 101;
  auto changed_attack_result = vm_worker_combat_damage("combat/test", changed_attack);
  ASSERT_NE(changed_attack_result.input_hash, result.input_hash);

  auto changed_snapshot = input;
  changed_snapshot.snapshot_hash = 424243;
  auto changed_snapshot_result = vm_worker_combat_damage("combat/test", changed_snapshot);
  ASSERT_EQ(changed_snapshot_result.snapshot_hash, 424243u);
  ASSERT_NE(changed_snapshot_result.input_hash, result.input_hash);
}

TEST_F(DriverTest, TestVmWorkerCombatDamageNormalizesExtremeInput) {
  VMWorkerCombatDamageInput input;
  input.snapshot_hash = std::numeric_limits<int>::max();
  input.attack = std::numeric_limits<int>::max();
  input.defense = std::numeric_limits<int>::max();
  input.armor_break = std::numeric_limits<int>::max();
  input.critical = std::numeric_limits<int>::max();
  input.critical_resist = std::numeric_limits<int>::max();
  input.reduction_min_bp = 9000;
  input.reduction_max_bp = 1000;
  input.damage_base = std::numeric_limits<int>::max();
  input.damage_skill_factor_bp = std::numeric_limits<int>::max();
  input.damage_random_min_bp = 20000;
  input.damage_random_max_bp = 1000;
  input.variance_roll_bp = std::numeric_limits<int>::max();
  input.critical_min = 90;
  input.critical_max = 10;
  input.critical_roll = std::numeric_limits<int>::min();
  input.critical_damage_factor_bp = std::numeric_limits<int>::max();

  auto result = vm_worker_combat_damage("combat/extreme", input);
  ASSERT_EQ(result.owner_key, "combat/extreme");
  ASSERT_EQ(result.snapshot_hash, static_cast<uint64_t>(std::numeric_limits<int>::max()));
  ASSERT_GE(result.damage, 0);
  ASSERT_GE(result.reduction_bp, 1000);
  ASSERT_LE(result.reduction_bp, 9000);
  ASSERT_GE(result.critical_rate, 10);
  ASSERT_LE(result.critical_rate, 90);
  ASSERT_GT(result.input_hash, 0u);
}

TEST_F(DriverTest, TestVmWorkerAsyncCombatDamagePollsResult) {
  VMWorkerCombatDamageInput input;
  input.attack = 100;
  input.defense = 50;
  input.variance_roll_bp = 500;
  input.critical_roll = 100;
  input.snapshot_hash = 31337;

  auto task_id = vm_worker_submit_combat_damage_v2("combat/async", input, 1000, 5000);
  ASSERT_GT(task_id, 0u);

  VMWorkerTaskResult result;
  for (int i = 0; i < 100; i++) {
    result = vm_worker_poll_task(task_id);
    ASSERT_NE(result.state, VMWorkerTaskState::kUnknown);
    if (result.state == VMWorkerTaskState::kSucceeded) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(result.state, VMWorkerTaskState::kSucceeded);
  ASSERT_EQ(result.type, "combat_damage");
  ASSERT_EQ(result.envelope.task_type, "combat_damage");
  ASSERT_EQ(result.envelope.owner_key, "combat/async");
  ASSERT_EQ(result.envelope.input_hash, result.combat_damage.input_hash);
  ASSERT_EQ(result.combat_damage.owner_key, "combat/async");
  ASSERT_EQ(result.combat_damage.damage, 95);
  ASSERT_EQ(result.combat_damage.critical_hit, 0);
  ASSERT_EQ(result.combat_damage.snapshot_hash, 31337u);
  ASSERT_NE(result.combat_damage.input_hash, 31337u);
}

TEST_F(DriverTest, TestVmWorkerComputeResultCompletesOwnerFutureThroughQueue) {

  VMWorkerActorScoreInput input;
  input.hp = 100;
  input.max_hp = 100;
  input.mp = 80;
  input.max_mp = 100;
  input.ep = 60;
  input.max_ep = 100;

  auto task_id = vm_worker_submit_actor_score_v2("actor/owner-future", input, 1000, 5000);
  ASSERT_GT(task_id, 0u);
  auto future_id = vm_worker_owner_future_id(task_id);
  ASSERT_GT(future_id, 0u);

  auto* pending = vm_owner_future_poll(future_id);
  ASSERT_STREQ(mapping_string(pending, "state"), "pending");
  ASSERT_EQ(mapping_number(pending, "target_task_id"), static_cast<long>(task_id));
  ASSERT_STREQ(mapping_string(pending, "message_type"), "actor_score");
  ASSERT_STREQ(mapping_string(pending, "payload_key"), "worker_compute");
  free_mapping(pending);

  VMWorkerTaskResult result;
  for (int i = 0; i < 100; i++) {
    result = vm_worker_poll_task(task_id);
    ASSERT_NE(result.state, VMWorkerTaskState::kUnknown);
    if (result.state == VMWorkerTaskState::kSucceeded) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(result.state, VMWorkerTaskState::kSucceeded);
  ASSERT_EQ(result.envelope.owner_future_id, future_id);
  auto* still_pending = vm_owner_future_poll(future_id);
  ASSERT_STREQ(mapping_string(still_pending, "state"), "pending");
  free_mapping(still_pending);

  auto* scheduled = vm_owner_drain_mailbox("actor/owner-future", 1);
  ASSERT_EQ(mapping_number(scheduled, "drained"), 1);
  auto* tasks = find_string_in_mapping(scheduled, "tasks");
  ASSERT_NE(tasks, nullptr);
  ASSERT_EQ(tasks ? tasks->type : T_INVALID, T_ARRAY);
  ASSERT_EQ(tasks->u.arr->size, 1);
  auto* task_map = tasks->u.arr->item[0].u.map;
  ASSERT_STREQ(mapping_string(task_map, "task_type"), "compute_result");
  ASSERT_STREQ(mapping_string(task_map, "task_key"), "actor_score");
  ASSERT_EQ(mapping_number(task_map, "future_target_task_id"), static_cast<long>(task_id));
  ASSERT_STREQ(mapping_string(task_map, "future_state"), "completed");
  free_mapping(scheduled);

  auto* completed = vm_owner_future_poll(future_id);
  ASSERT_STREQ(mapping_string(completed, "state"), "completed");
  ASSERT_STREQ(mapping_string(completed, "result_key"), "actor_score");
  ASSERT_EQ(mapping_number(completed, "payload_frozen"), 1);
  ASSERT_EQ(mapping_number(completed, "frozen_result"), 1);
  ASSERT_EQ(mapping_number(completed, "direct_cross_owner_write"), 0);
  auto* result_map = find_string_in_mapping(completed, "result");
  ASSERT_NE(result_map, nullptr);
  ASSERT_EQ(result_map ? result_map->type : T_INVALID, T_MAPPING);
  ASSERT_STREQ(mapping_string(result_map->u.map, "type"), "actor_score");
  ASSERT_STREQ(mapping_string(result_map->u.map, "owner_key"), "actor/owner-future");
  ASSERT_EQ(mapping_number(result_map->u.map, "hp_pct_bp"), 10000);
  ASSERT_EQ(mapping_number(result_map->u.map, "mp_pct_bp"), 8000);
  ASSERT_EQ(mapping_number(result_map->u.map, "ep_pct_bp"), 6000);
  ASSERT_EQ(mapping_number(result_map->u.map, "total_score"), 9100);
  ASSERT_STREQ(mapping_string(result_map->u.map, "state"), "stable");
  free_mapping(completed);
}

TEST_F(DriverTest, TestVmWorkerComputeResultCompletesOwnerFutureThroughOwnerExecutor) {

  const char* owner = "actor/owner-future-thread";
  vm_owner_thread_stop();
  free_mapping(vm_owner_drain_mailbox(owner, 0));

  auto* before = vm_owner_thread_status();
  auto before_completed = mapping_number(before, "thread_compute_result_completed");
  auto before_dispatched = mapping_number(before, "executor_safe_task_dispatched");
  auto before_claims = mapping_number(before, "executor_owner_claims");
  auto before_releases = mapping_number(before, "executor_owner_releases");
  free_mapping(before);

  auto* before_runtime = vm_owner_runtime_status();
  auto before_pending_futures = mapping_number(before_runtime, "pending_futures");
  free_mapping(before_runtime);

  VMWorkerActorScoreInput input;
  input.hp = 100;
  input.max_hp = 100;
  input.mp = 80;
  input.max_mp = 100;
  input.ep = 60;
  input.max_ep = 100;

  auto task_id = vm_worker_submit_actor_score_v2(owner, input, 1000, 5000);
  ASSERT_GT(task_id, 0u);
  auto future_id = vm_worker_owner_future_id(task_id);
  ASSERT_GT(future_id, 0u);

  auto* pending_runtime = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(pending_runtime, "pending_futures"), before_pending_futures + 1);
  free_mapping(pending_runtime);

  VMWorkerTaskResult result;
  for (int i = 0; i < 100; i++) {
    result = vm_worker_poll_task(task_id);
    ASSERT_NE(result.state, VMWorkerTaskState::kUnknown);
    if (result.state == VMWorkerTaskState::kSucceeded) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(result.state, VMWorkerTaskState::kSucceeded);
  ASSERT_EQ(result.envelope.owner_future_id, future_id);

  auto* queued = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(queued, "owner_queue_depth"), 1);
  ASSERT_EQ(mapping_number(queued, "owner_executor_safe_queue_depth"), 1);
  ASSERT_EQ(mapping_number(queued, "owner_main_required_queue_depth"), 0);
  ASSERT_GE(mapping_number(queued, "executor_safe_queue_depth"), 1);
  free_mapping(queued);

  auto* still_pending = vm_owner_future_poll(future_id);
  ASSERT_STREQ(mapping_string(still_pending, "state"), "pending");
  ASSERT_EQ(mapping_number(still_pending, "target_task_id"), static_cast<long>(task_id));
  free_mapping(still_pending);

  vm_owner_thread_start(1);
  for (int i = 0; i < 100; i++) {
    auto* polled = vm_owner_future_poll(future_id);
    auto completed = std::string(mapping_string(polled, "state")) == "completed";
    free_mapping(polled);
    auto* status = vm_owner_mailbox_status(owner);
    auto owner_depth = mapping_number(status, "owner_queue_depth");
    free_mapping(status);
    if (completed && owner_depth == 0) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  auto* completed = vm_owner_future_poll(future_id);
  ASSERT_STREQ(mapping_string(completed, "state"), "completed");
  ASSERT_STREQ(mapping_string(completed, "result_key"), "actor_score");
  ASSERT_EQ(mapping_number(completed, "requires_owner_message_completion"), 0);
  ASSERT_EQ(mapping_number(completed, "frozen_result"), 1);
  ASSERT_EQ(mapping_number(completed, "direct_cross_owner_write"), 0);
  auto* result_map = find_string_in_mapping(completed, "result");
  ASSERT_NE(result_map, nullptr);
  ASSERT_EQ(result_map ? result_map->type : T_INVALID, T_MAPPING);
  ASSERT_STREQ(mapping_string(result_map->u.map, "type"), "actor_score");
  ASSERT_STREQ(mapping_string(result_map->u.map, "owner_key"), owner);
  ASSERT_EQ(mapping_number(result_map->u.map, "hp_pct_bp"), 10000);
  ASSERT_EQ(mapping_number(result_map->u.map, "mp_pct_bp"), 8000);
  ASSERT_EQ(mapping_number(result_map->u.map, "ep_pct_bp"), 6000);
  ASSERT_EQ(mapping_number(result_map->u.map, "total_score"), 9100);
  ASSERT_STREQ(mapping_string(result_map->u.map, "state"), "stable");
  free_mapping(completed);

  auto* running = vm_owner_thread_status();
  ASSERT_GE(mapping_number(running, "thread_compute_result_completed"), before_completed + 1);
  ASSERT_GE(mapping_number(running, "executor_safe_task_dispatched"), before_dispatched + 1);
  ASSERT_GE(mapping_number(running, "executor_owner_claims"), before_claims + 1);
  ASSERT_EQ(mapping_number(running, "executor_owner_claims") - before_claims,
            mapping_number(running, "executor_owner_releases") - before_releases);
  ASSERT_EQ(mapping_number(running, "claimed_owners"), 0);
  ASSERT_EQ(mapping_number(running, "executor_safe_queue_depth"), 0);
  free_mapping(running);

  auto* trace = vm_owner_task_trace(32);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events ? events->type : T_INVALID, T_ARRAY);
  int compute_result_completed = 0;
  for (int i = 0; i < events->u.arr->size; i++) {
    auto* event = events->u.arr->item[i].u.map;
    if (mapping_number(event, "task_id") > 0 &&
        std::string(mapping_string(event, "task_type")) == "compute_result" &&
        std::string(mapping_string(event, "owner_id")) == owner &&
        std::string(mapping_string(event, "state")) == "thread_compute_result_completed") {
      compute_result_completed = 1;
    }
  }
  ASSERT_EQ(compute_result_completed, 1);
  free_mapping(trace);

  auto* completed_runtime = vm_owner_runtime_status();
  ASSERT_LE(mapping_number(completed_runtime, "pending_futures"), before_pending_futures);
  free_mapping(completed_runtime);
  vm_owner_thread_stop();
}

TEST_F(DriverTest, TestVmWorkerComputeResultRejectsCompletedWithoutFrozenFields) {

  const char* owner = "actor/owner-future-empty-result";
  const uint64_t worker_task_id = 987654321u;
  vm_owner_thread_stop();
  free_mapping(vm_owner_purge_mailbox(owner));

  auto future_id =
      vm_owner_register_compute_future(owner, worker_task_id, "empty_result", "worker_compute");
  ASSERT_GT(future_id, 0u);
  auto* pending = vm_owner_future_poll(future_id);
  ASSERT_EQ(mapping_number(pending, "success"), 1);
  ASSERT_STREQ(mapping_string(pending, "state"), "pending");
  ASSERT_EQ(mapping_number(pending, "target_task_id"), static_cast<long>(worker_task_id));
  ASSERT_EQ(mapping_number(pending, "frozen_result"), 0);
  free_mapping(pending);

  auto result_task_id = vm_owner_enqueue_compute_result(owner, worker_task_id, "empty_result",
                                                        "completed", "empty_result", "");
  ASSERT_GT(result_task_id, 0u);
  auto* queued = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(queued, "owner_queue_depth"), 1);
  ASSERT_EQ(mapping_number(queued, "owner_executor_safe_queue_depth"), 1);
  free_mapping(queued);

  auto* drained = vm_owner_drain_mailbox(owner, 1);
  ASSERT_EQ(mapping_number(drained, "drained"), 1);
  auto* tasks = find_string_in_mapping(drained, "tasks");
  ASSERT_NE(tasks, nullptr);
  ASSERT_EQ(tasks ? tasks->type : T_INVALID, T_ARRAY);
  ASSERT_EQ(tasks->u.arr->size, 1);
  auto* task_map = tasks->u.arr->item[0].u.map;
  ASSERT_STREQ(mapping_string(task_map, "task_type"), "compute_result");
  ASSERT_STREQ(mapping_string(task_map, "task_key"), "empty_result");
  ASSERT_STREQ(mapping_string(task_map, "future_state"), "completed");
  ASSERT_EQ(mapping_number(task_map, "future_target_task_id"), static_cast<long>(worker_task_id));
  free_mapping(drained);

  auto* failed = vm_owner_future_poll(future_id);
  ASSERT_EQ(mapping_number(failed, "success"), 1);
  ASSERT_STREQ(mapping_string(failed, "state"), "failed");
  ASSERT_STREQ(mapping_string(failed, "error"), "worker compute result must contain frozen data");
  ASSERT_EQ(mapping_number(failed, "requires_owner_message_completion"), 0);
  ASSERT_EQ(mapping_number(failed, "payload_frozen"), 1);
  ASSERT_EQ(mapping_number(failed, "frozen_result"), 0);
  free_mapping(failed);
}

TEST_F(DriverTest, TestVmWorkerV2EnvelopeKeepsResultUntilTtl) {
  auto task_id =
      vm_worker_submit_snapshot_digest_v2("actor/envelope", "{\"hp\":100}", 8, 1000, 5000);
  ASSERT_GT(task_id, 0u);

  VMWorkerTaskResult result;
  for (int i = 0; i < 100; i++) {
    result = vm_worker_poll_task(task_id);
    ASSERT_NE(result.state, VMWorkerTaskState::kUnknown);
    if (result.state == VMWorkerTaskState::kSucceeded) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(result.state, VMWorkerTaskState::kSucceeded);
  ASSERT_EQ(result.envelope.task_id, task_id);
  ASSERT_EQ(result.envelope.task_type, "snapshot_digest");
  ASSERT_EQ(result.envelope.owner_key, "actor/envelope");
  ASSERT_GT(result.envelope.input_hash, 0u);
  ASSERT_GT(result.envelope.submitted_at_ms, 0u);
  ASSERT_GE(result.envelope.completed_at_ms, result.envelope.submitted_at_ms);
  ASSERT_GT(result.envelope.expires_at_ms, result.envelope.completed_at_ms);
  ASSERT_EQ(result.envelope.timeout_ms, 1000);
  ASSERT_EQ(result.envelope.ttl_ms, 5000);
  ASSERT_EQ(vm_worker_poll_task(task_id).state, VMWorkerTaskState::kSucceeded);
}

TEST_F(DriverTest, TestVmWorkerComputeResultFutureCarriesBenchmarkResult) {

  free_mapping(vm_owner_drain_mailbox("global", 0));
  auto task_id = vm_worker_submit_benchmark_v2(2, 10, 1000, 5000);
  ASSERT_GT(task_id, 0u);
  auto future_id = vm_worker_owner_future_id(task_id);
  ASSERT_GT(future_id, 0u);

  VMWorkerTaskResult result;
  for (int i = 0; i < 100; i++) {
    result = vm_worker_poll_task(task_id);
    ASSERT_NE(result.state, VMWorkerTaskState::kUnknown);
    if (result.state == VMWorkerTaskState::kSucceeded) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(result.state, VMWorkerTaskState::kSucceeded);
  ASSERT_EQ(result.type, "bench");
  ASSERT_EQ(result.envelope.owner_future_id, future_id);
  ASSERT_EQ(result.bench.tasks, 2);
  ASSERT_GE(result.bench.worker_count, 1);
  ASSERT_GE(result.bench.max_parallel, 1);
  ASSERT_GT(result.bench.checksum, 0u);

  auto* scheduled = vm_owner_drain_mailbox("global", 1);
  ASSERT_EQ(mapping_number(scheduled, "drained"), 1);
  auto* tasks = find_string_in_mapping(scheduled, "tasks");
  ASSERT_NE(tasks, nullptr);
  ASSERT_EQ(tasks ? tasks->type : T_INVALID, T_ARRAY);
  ASSERT_EQ(tasks->u.arr->size, 1);
  auto* task_map = tasks->u.arr->item[0].u.map;
  ASSERT_STREQ(mapping_string(task_map, "task_type"), "compute_result");
  ASSERT_STREQ(mapping_string(task_map, "task_key"), "bench");
  ASSERT_EQ(mapping_number(task_map, "future_target_task_id"), static_cast<long>(task_id));
  ASSERT_STREQ(mapping_string(task_map, "future_state"), "completed");
  free_mapping(scheduled);

  auto* completed = vm_owner_future_poll(future_id);
  ASSERT_STREQ(mapping_string(completed, "state"), "completed");
  ASSERT_STREQ(mapping_string(completed, "result_key"), "bench");
  ASSERT_EQ(mapping_number(completed, "requires_owner_message_completion"), 0);
  ASSERT_EQ(mapping_number(completed, "frozen_result"), 1);
  ASSERT_EQ(mapping_number(completed, "direct_cross_owner_write"), 0);
  auto* result_map = find_string_in_mapping(completed, "result");
  ASSERT_NE(result_map, nullptr);
  ASSERT_EQ(result_map ? result_map->type : T_INVALID, T_MAPPING);
  ASSERT_STREQ(mapping_string(result_map->u.map, "type"), "bench");
  ASSERT_EQ(mapping_number(result_map->u.map, "tasks"), result.bench.tasks);
  ASSERT_EQ(mapping_number(result_map->u.map, "worker_count"), result.bench.worker_count);
  ASSERT_EQ(mapping_number(result_map->u.map, "max_parallel"), result.bench.max_parallel);
  ASSERT_EQ(mapping_number(result_map->u.map, "checksum"),
            static_cast<long>(result.bench.checksum));
  free_mapping(completed);
}

TEST_F(DriverTest, TestVmWorkerV2TimeoutFailsPendingTask) {

  free_mapping(vm_owner_drain_mailbox("global", 0));
  auto task_id = vm_worker_submit_benchmark_v2(64, 80, 1, 5000);
  ASSERT_GT(task_id, 0u);
  auto future_id = vm_worker_owner_future_id(task_id);
  ASSERT_GT(future_id, 0u);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  auto result = vm_worker_poll_task(task_id);
  ASSERT_EQ(result.state, VMWorkerTaskState::kFailed);
  ASSERT_EQ(result.error, "worker task timed out");
  ASSERT_EQ(result.envelope.task_id, task_id);
  ASSERT_EQ(result.envelope.owner_future_id, future_id);
  ASSERT_EQ(result.envelope.timeout_ms, 1);
  ASSERT_GT(result.envelope.completed_at_ms, 0u);

  auto* still_pending = vm_owner_future_poll(future_id);
  ASSERT_STREQ(mapping_string(still_pending, "state"), "pending");
  ASSERT_EQ(mapping_number(still_pending, "target_task_id"), static_cast<long>(task_id));
  free_mapping(still_pending);

  auto* scheduled = vm_owner_drain_mailbox("global", 1);
  ASSERT_EQ(mapping_number(scheduled, "drained"), 1);
  auto* tasks = find_string_in_mapping(scheduled, "tasks");
  ASSERT_NE(tasks, nullptr);
  ASSERT_EQ(tasks ? tasks->type : T_INVALID, T_ARRAY);
  ASSERT_EQ(tasks->u.arr->size, 1);
  auto* task_map = tasks->u.arr->item[0].u.map;
  ASSERT_STREQ(mapping_string(task_map, "task_type"), "compute_result");
  ASSERT_STREQ(mapping_string(task_map, "task_key"), "bench");
  ASSERT_EQ(mapping_number(task_map, "future_target_task_id"), static_cast<long>(task_id));
  ASSERT_STREQ(mapping_string(task_map, "future_state"), "failed");
  ASSERT_STREQ(mapping_string(task_map, "future_error"), "worker task timed out");
  free_mapping(scheduled);

  auto* failed = vm_owner_future_poll(future_id);
  ASSERT_STREQ(mapping_string(failed, "state"), "failed");
  ASSERT_STREQ(mapping_string(failed, "error"), "worker task timed out");
  ASSERT_EQ(mapping_number(failed, "requires_owner_message_completion"), 0);
  ASSERT_EQ(mapping_number(failed, "frozen_result"), 0);
  ASSERT_EQ(mapping_number(failed, "direct_cross_owner_write"), 0);
  free_mapping(failed);
}

TEST_F(DriverTest, TestVmWorkerComputeResultFutureCarriesSnapshotDigestResult) {

  const char* owner = "actor/future-snapshot";
  free_mapping(vm_owner_drain_mailbox(owner, 0));
  auto task_id = vm_worker_submit_snapshot_digest_v2(owner, "{\"hp\":100}", 8, 1000, 5000);
  ASSERT_GT(task_id, 0u);
  auto future_id = vm_worker_owner_future_id(task_id);
  ASSERT_GT(future_id, 0u);

  VMWorkerTaskResult result;
  for (int i = 0; i < 100; i++) {
    result = vm_worker_poll_task(task_id);
    ASSERT_NE(result.state, VMWorkerTaskState::kUnknown);
    if (result.state == VMWorkerTaskState::kSucceeded) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_EQ(result.state, VMWorkerTaskState::kSucceeded);

  free_mapping(vm_owner_drain_mailbox(owner, 1));
  auto* completed = vm_owner_future_poll(future_id);
  ASSERT_STREQ(mapping_string(completed, "state"), "completed");
  ASSERT_STREQ(mapping_string(completed, "result_key"), "snapshot_digest");
  auto* result_map = find_string_in_mapping(completed, "result");
  ASSERT_NE(result_map, nullptr);
  ASSERT_EQ(result_map ? result_map->type : T_INVALID, T_MAPPING);
  ASSERT_STREQ(mapping_string(result_map->u.map, "type"), "snapshot_digest");
  ASSERT_STREQ(mapping_string(result_map->u.map, "owner_key"), owner);
  ASSERT_EQ(mapping_number(result_map->u.map, "input_bytes"), 10);
  ASSERT_EQ(mapping_number(result_map->u.map, "repeat"), 8);
  ASSERT_NE(find_string_in_mapping(result_map->u.map, "checksum"), nullptr);
  free_mapping(completed);
}

TEST_F(DriverTest, TestVmWorkerComputeResultFutureCarriesCombatDamageResult) {

  const char* owner = "combat/future-damage";
  free_mapping(vm_owner_drain_mailbox(owner, 0));
  VMWorkerCombatDamageInput input;
  input.snapshot_hash = 31337;
  input.attack = 100;
  input.defense = 50;
  input.variance_roll_bp = 500;
  input.critical_roll = 100;

  auto task_id = vm_worker_submit_combat_damage_v2(owner, input, 1000, 5000);
  ASSERT_GT(task_id, 0u);
  auto future_id = vm_worker_owner_future_id(task_id);
  ASSERT_GT(future_id, 0u);

  VMWorkerTaskResult result;
  for (int i = 0; i < 100; i++) {
    result = vm_worker_poll_task(task_id);
    ASSERT_NE(result.state, VMWorkerTaskState::kUnknown);
    if (result.state == VMWorkerTaskState::kSucceeded) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_EQ(result.state, VMWorkerTaskState::kSucceeded);

  free_mapping(vm_owner_drain_mailbox(owner, 1));
  auto* completed = vm_owner_future_poll(future_id);
  ASSERT_STREQ(mapping_string(completed, "state"), "completed");
  ASSERT_STREQ(mapping_string(completed, "result_key"), "combat_damage");
  auto* result_map = find_string_in_mapping(completed, "result");
  ASSERT_NE(result_map, nullptr);
  ASSERT_EQ(result_map ? result_map->type : T_INVALID, T_MAPPING);
  ASSERT_STREQ(mapping_string(result_map->u.map, "type"), "combat_damage");
  ASSERT_STREQ(mapping_string(result_map->u.map, "owner_key"), owner);
  ASSERT_EQ(mapping_number(result_map->u.map, "damage"), 95);
  ASSERT_EQ(mapping_number(result_map->u.map, "critical_hit"), 0);
  ASSERT_EQ(mapping_number(result_map->u.map, "snapshot_hash"), 31337);
  free_mapping(completed);
}

TEST_F(DriverTest, TestVmWorkerPollTasksReturnsBatchResults) {
  VMWorkerActorScoreInput input;
  input.hp = 100;
  input.max_hp = 100;
  input.mp = 80;
  input.max_mp = 100;
  input.ep = 60;
  input.max_ep = 100;

  std::vector<uint64_t> task_ids;
  task_ids.push_back(vm_worker_submit_actor_score_v2("actor/batch-a", input, 1000, 5000));
  task_ids.push_back(vm_worker_submit_actor_score_v2("actor/batch-b", input, 1000, 5000));

  std::vector<VMWorkerTaskResult> results;
  for (int i = 0; i < 100; i++) {
    results = vm_worker_poll_tasks(task_ids);
    ASSERT_EQ(results.size(), 2u);
    if (results[0].state == VMWorkerTaskState::kSucceeded &&
        results[1].state == VMWorkerTaskState::kSucceeded) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(results[0].state, VMWorkerTaskState::kSucceeded);
  ASSERT_EQ(results[1].state, VMWorkerTaskState::kSucceeded);
  ASSERT_EQ(results[0].actor_score.owner_key, "actor/batch-a");
  ASSERT_EQ(results[1].actor_score.owner_key, "actor/batch-b");
}
