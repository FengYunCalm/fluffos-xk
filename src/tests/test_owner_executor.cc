#include "test_lpc_support.h"

TEST_F(DriverTest, TestVmOwnerExecutorCommandFrameRestoreDispatchesWithoutLpc) {
  const char* owner = "owner/test/executor/command-frame-restore";

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
  auto before_frame_restore =
      mapping_number(before, "executor_command_frame_restore_entry_executed");
  auto before_safe = mapping_number(before, "executor_safe_task_dispatched");
  auto before_execution_cleared = mapping_number(before, "thread_execution_cleared");
  auto before_eval_stack_owner_bound = mapping_number(before, "thread_eval_stack_owner_bound");
  auto before_eval_stack_cleared = mapping_number(before, "thread_eval_stack_cleared");
  auto before_eval_stack_leaks = mapping_number(before, "thread_eval_stack_leak_detected");
  auto before_control_stack_owner_bound =
      mapping_number(before, "thread_control_stack_owner_bound");
  auto before_control_stack_cleared = mapping_number(before, "thread_control_stack_cleared");
  auto before_control_stack_leaks = mapping_number(before, "thread_control_stack_leak_detected");
  auto before_value_stack_owner_bound = mapping_number(before, "thread_value_stack_owner_bound");
  auto before_value_stack_cleared = mapping_number(before, "thread_value_stack_cleared");
  auto before_value_stack_leaks = mapping_number(before, "thread_value_stack_leak_detected");
  auto before_apply_return_owner_bound = mapping_number(before, "thread_apply_return_owner_bound");
  auto before_apply_return_cleared = mapping_number(before, "thread_apply_return_cleared");
  auto before_apply_return_leaks = mapping_number(before, "thread_apply_return_leak_detected");
  auto before_context_leaks = mapping_number(before, "thread_context_leak_detected");
  free_mapping(before);

  auto task_id = vm_owner_enqueue_command_frame_restore(probe);
  ASSERT_GT(task_id, 0u);

  vm_owner_thread_start(1);
  for (int i = 0; i < 100; i++) {
    auto* status = vm_owner_thread_status();
    auto frame_restore_done =
        mapping_number(status, "executor_command_frame_restore_entry_executed");
    free_mapping(status);
    if (frame_restore_done >= before_frame_restore + 1) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  auto* running = vm_owner_thread_status();
  ASSERT_GE(mapping_number(running, "executor_command_frame_restore_entry_executed"),
            before_frame_restore + 1);
  ASSERT_GE(mapping_number(running, "executor_safe_task_dispatched"), before_safe + 1);
  ASSERT_GE(mapping_number(running, "thread_execution_cleared"), before_execution_cleared + 1);
  ASSERT_GE(mapping_number(running, "thread_eval_stack_owner_bound"),
            before_eval_stack_owner_bound + 1);
  ASSERT_GE(mapping_number(running, "thread_eval_stack_cleared"), before_eval_stack_cleared + 1);
  ASSERT_EQ(mapping_number(running, "thread_eval_stack_leak_detected"), before_eval_stack_leaks);
  ASSERT_GE(mapping_number(running, "thread_control_stack_owner_bound"),
            before_control_stack_owner_bound + 1);
  ASSERT_GE(mapping_number(running, "thread_control_stack_cleared"),
            before_control_stack_cleared + 1);
  ASSERT_EQ(mapping_number(running, "thread_control_stack_leak_detected"),
            before_control_stack_leaks);
  ASSERT_GE(mapping_number(running, "thread_value_stack_owner_bound"),
            before_value_stack_owner_bound + 1);
  ASSERT_GE(mapping_number(running, "thread_value_stack_cleared"), before_value_stack_cleared + 1);
  ASSERT_EQ(mapping_number(running, "thread_value_stack_leak_detected"), before_value_stack_leaks);
  ASSERT_GE(mapping_number(running, "thread_apply_return_owner_bound"),
            before_apply_return_owner_bound + 1);
  ASSERT_GE(mapping_number(running, "thread_apply_return_cleared"),
            before_apply_return_cleared + 1);
  ASSERT_EQ(mapping_number(running, "thread_apply_return_leak_detected"),
            before_apply_return_leaks);
  ASSERT_EQ(mapping_number(running, "thread_context_leak_detected"), before_context_leaks);
  ASSERT_EQ(mapping_number(running, "ordinary_lpc_default_closed"), 1);
  auto* vm_context_contract = mapping_entry(running, "vm_context_contract");
  ASSERT_EQ(mapping_number(vm_context_contract, "ordinary_lpc_ready"), 1);
  ASSERT_EQ(mapping_number(vm_context_contract, "eval_stack_owner_local"), 1);
  ASSERT_EQ(mapping_number(vm_context_contract, "control_stack_owner_local"), 1);
  ASSERT_EQ(mapping_number(vm_context_contract, "value_stack_owner_local"), 1);
  ASSERT_EQ(mapping_number(vm_context_contract, "apply_return_owner_local"), 1);
  ASSERT_STREQ(mapping_string(vm_context_contract, "ordinary_lpc_next_blocker"), "");
  free_mapping(running);

  auto* trace = vm_owner_task_trace(16);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  bool found_frame_restore_ready = false;
  for (int i = 0; i < events->u.arr->size; i++) {
    auto* event = events->u.arr->item[i].u.map;
    if (mapping_number(event, "task_id") == static_cast<long>(task_id) &&
        std::string(mapping_string(event, "state")) == "thread_command_frame_restore_ready") {
      found_frame_restore_ready = true;
      ASSERT_STREQ(mapping_string(event, "task_type"), "command_frame_restore");
      ASSERT_STREQ(mapping_string(event, "task_key"), "gateway-command-frame-restore");
      ASSERT_STREQ(mapping_string(event, "owner_id"), owner);
    }
  }
  ASSERT_TRUE(found_frame_restore_ready);
  free_mapping(trace);

  vm_owner_thread_stop();
  ASSERT_TRUE(vm_context_is_main_thread());
  destruct_object(probe);
}

TEST_F(DriverTest, TestVmOwnerPreparedCallbackCleanupSurvivesCallbackDestruction) {
  struct CleanupProbe {
    VMOwnerCallbackCleanupRecord* record;
    bool* called;
  };
  bool called = false;
  auto* record = new VMOwnerCallbackCleanupRecord();
  CleanupProbe probe{record, &called};
  record->prepare(
      "owner/test/prepared-cleanup", 1, "socket_callback", "cleanup",
      [](void* context) {
        auto* probe = static_cast<CleanupProbe*>(context);
        *probe->called = true;
        delete probe->record;
      },
      &probe);

  ASSERT_NE(vm_owner_enqueue_executor_callback_cleanup(record), 0u);
  ASSERT_GE(vm_owner_drain_main_tasks(16), 1);
  ASSERT_TRUE(called);
}

TEST_F(DriverTest, TestVmOwnerExecutorCallbackTaskBoundaryDispatchesAndDropsStaleTasks) {
  const char* owner = "owner/test/executor-callback";
  const char* moved_owner = "owner/test/executor-callback/moved";
  struct RuntimeGuard {
    int saved_mode;
    object_t* probe{nullptr};

    ~RuntimeGuard() {
      vm_owner_thread_stop();
      if (probe) {
        vm_owner_clear_id(probe);
        destruct_object(probe);
      }
      CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
    }
  } runtime_guard{CONFIG_INT(__RC_MULTICORE_MODE__)};
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;

  vm_owner_thread_stop();
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  runtime_guard.probe = probe;
  vm_owner_set_id(probe, owner);


  ASSERT_FALSE(vm_owner_executor_available());
  ASSERT_EQ(vm_owner_enqueue_executor_task(probe, "ordinary_lpc", "rejected", [] {}), 0u);

  auto* before = vm_owner_thread_status();
  auto before_queued = mapping_number(before, "executor_callback_queued");
  auto before_dispatched = mapping_number(before, "executor_callback_dispatched");
  auto before_dropped = mapping_number(before, "executor_callback_dropped");
  auto before_cleanup_queued = mapping_number(before, "executor_callback_main_cleanup_queued");
  auto before_cleanup = mapping_number(before, "executor_callback_main_cleanup_dispatched");
  free_mapping(before);

  std::atomic<int> first_saw_owner_scope{0};
  std::atomic<int> first_saw_execution_scope{0};
  std::atomic<int> second_ran{0};
  std::atomic<int> second_dropped{0};

  vm_owner_thread_start(1);
  ASSERT_TRUE(vm_owner_executor_available());
  ASSERT_EQ(vm_owner_enqueue_executor_task(probe, "legacy_lpc", "rejected", [] {}), 0u);

  auto first_task = vm_owner_enqueue_executor_task(probe, "heartbeat", "unit-first", [&] {
    first_saw_owner_scope.store(
        vm_context().owner.current_owner_id == owner && vm_context_is_main_thread() ? 1 : 0,
        std::memory_order_release);
    first_saw_execution_scope.store(vm_context().execution.current_object == probe ? 1 : 0,
                                    std::memory_order_release);
  });
  ASSERT_GT(first_task, 0u);
  ASSERT_GE(vm_owner_drain_main_tasks(8), 1);
  ASSERT_EQ(first_saw_owner_scope.load(std::memory_order_acquire), 1);
  ASSERT_EQ(first_saw_execution_scope.load(std::memory_order_acquire), 1);

  auto stale_task = vm_owner_enqueue_executor_task(
      probe, "call_out", "unit-stale",
      [&] {
        second_ran.store(1, std::memory_order_release);
      },
      [&] {
        second_dropped.store(vm_context_is_main_thread() ? 1 : -1, std::memory_order_release);
      });
  ASSERT_GT(stale_task, first_task);
  vm_owner_set_id(probe, moved_owner);
  ASSERT_GE(vm_owner_drain_main_tasks(8), 1);
  ASSERT_GE(vm_owner_drain_main_tasks(8), 1);
  ASSERT_EQ(second_ran.load(std::memory_order_acquire), 0);
  ASSERT_EQ(second_dropped.load(std::memory_order_acquire), 1);

  auto* after = vm_owner_thread_status();
  ASSERT_GE(mapping_number(after, "executor_callback_queued"), before_queued + 2);
  ASSERT_GE(mapping_number(after, "executor_callback_dispatched"), before_dispatched + 1);
  ASSERT_GE(mapping_number(after, "executor_callback_dropped"), before_dropped + 1);
  ASSERT_GE(mapping_number(after, "executor_callback_main_cleanup_queued"),
            before_cleanup_queued + 1);
  ASSERT_GE(mapping_number(after, "executor_callback_main_cleanup_dispatched"), before_cleanup + 1);
  ASSERT_EQ(mapping_number(after, "executor_callback_task_boundary_ready"), 1);
  ASSERT_EQ(mapping_number(after, "executor_callback_allowlist_ready"), 1);
  ASSERT_EQ(mapping_number(after, "executor_callback_allowlist_count"), 6);
  ASSERT_EQ(mapping_number(after, "owner_callback_diagnostics_ready"), 1);
  ASSERT_STREQ(mapping_string(after, "owner_callback_diagnostics_schema"),
               "owner_callback_diagnostics_v1");
  ASSERT_STREQ(mapping_string(after, "owner_callback_failure_code_schema"),
               "owner_callback_failure_code_v1");
  ASSERT_STREQ(mapping_string(after, "owner_callback_drop_reason_schema"),
               "owner_callback_drop_reason_v1");
  ASSERT_EQ(mapping_number(after, "owner_callback_allowlist_complete"), 1);
  ASSERT_STREQ(
      mapping_string(after, "owner_callback_supported_kinds"),
      "heartbeat,call_out,async_callback,dns_callback,socket_callback,gateway_command_execute,ed_callback");
  ASSERT_STREQ(mapping_string(after, "executor_callback_payload_policy"),
               "frozen_payload_or_owner_handle_only");
  free_mapping(after);

  auto* trace = vm_owner_task_trace(32);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  bool first_dispatched = false;
  bool stale_dropped = false;
  bool cleanup_dispatched = false;
  for (int i = 0; i < events->u.arr->size; i++) {
    auto* event = events->u.arr->item[i].u.map;
    auto state = std::string(mapping_string(event, "state"));
    if (mapping_number(event, "task_id") == static_cast<long>(first_task) &&
        state == "main_executor_callback_dispatched") {
      first_dispatched = true;
      ASSERT_STREQ(mapping_string(event, "task_type"), "heartbeat");
      ASSERT_STREQ(mapping_string(event, "owner_id"), owner);
      ASSERT_EQ(mapping_number(event, "trace_schema_version"), 2);
      ASSERT_STREQ(mapping_string(event, "drop_reason"), "none");
    }
    if (mapping_number(event, "task_id") == static_cast<long>(stale_task) &&
        state == "main_executor_callback_stale") {
      stale_dropped = true;
      ASSERT_STREQ(mapping_string(event, "task_type"), "call_out");
      ASSERT_STREQ(mapping_string(event, "owner_id"), owner);
      ASSERT_EQ(mapping_number(event, "target_handle_current"), 0);
      ASSERT_EQ(mapping_number(event, "trace_schema_version"), 2);
      ASSERT_STREQ(mapping_string(event, "diagnostic_schema"), "owner_callback_diagnostics_v1");
      ASSERT_STREQ(mapping_string(event, "failure_code_schema"), "owner_callback_failure_code_v1");
      ASSERT_STREQ(mapping_string(event, "drop_reason_schema"), "owner_callback_drop_reason_v1");
      ASSERT_STREQ(mapping_string(event, "failure_code"), "target_stale");
      ASSERT_STREQ(mapping_string(event, "drop_reason"), "target_stale");
    }
    if (mapping_number(event, "task_id") == static_cast<long>(stale_task) &&
        state == "executor_callback_main_cleanup_dispatched") {
      cleanup_dispatched = true;
    }
  }
  ASSERT_TRUE(first_dispatched);
  ASSERT_TRUE(stale_dropped);
  ASSERT_TRUE(cleanup_dispatched);
  free_mapping(trace);
}

TEST_F(DriverTest, TestVmOwnerThreadDrainsPendingOwnerMessages) {
  const char* owner = "owner/test/thread/pending-message";

  vm_owner_thread_stop();

  auto* submitted =
      vm_owner_submit_message("owner/test/thread/source", owner, "message", "payload/thread");
  ASSERT_EQ(mapping_number(submitted, "success"), 1);
  auto future_id = mapping_number(submitted, "future_id");
  free_mapping(submitted);

  auto* owner_status = vm_object_store_owner_status(owner);
  ASSERT_EQ(mapping_number(owner_status, "pending_messages"), 1);
  free_mapping(owner_status);

  vm_owner_thread_start(1);
  for (int i = 0; i < 100; i++) {
    auto* status = vm_owner_mailbox_status(owner);
    auto depth = mapping_number(status, "owner_queue_depth");
    free_mapping(status);
    if (depth == 0) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  owner_status = vm_object_store_owner_status(owner);
  ASSERT_EQ(mapping_number(owner_status, "pending_messages"), 0);
  free_mapping(owner_status);

  auto* future = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(future, "success"), 1);
  ASSERT_STREQ(mapping_string(future, "state"), "completed");
  free_mapping(future);

  vm_owner_thread_stop();
}

TEST_F(DriverTest, TestVmOwnerObjectMessageUsesOwnerExecutorRoute) {
  const char* owner = "owner/test/executor/target-route";
  const char* source_owner = "owner/test/executor/source";

  vm_owner_thread_stop();
  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  vm_owner_set_id(obj, owner);
  auto handle = vm_object_handle(obj);
  const auto time_of_ref_before = current_gametick() + 100000;
  obj->time_of_ref = time_of_ref_before;
  obj->flags |= O_RESET_STATE;
  const auto flags_before = obj->flags;

  auto* before = vm_owner_thread_status();
  auto before_probe = mapping_number(before, "executor_probe_executed");
  auto before_safe = mapping_number(before, "executor_safe_task_dispatched");
  auto before_skipped = mapping_number(before, "executor_main_required_skipped");
  auto before_main_queued = mapping_number(before, "main_queued");
  auto before_main_dispatched = mapping_number(before, "main_dispatched");
  free_mapping(before);

  auto* submitted = vm_owner_submit_object_message(source_owner, handle, "owner_lpc_probe",
                                                   "executor-target/payload");
  auto future_id = mapping_number(submitted, "future_id");
  ASSERT_EQ(mapping_number(submitted, "has_target_handle"), 1);
  ASSERT_EQ(mapping_number(submitted, "target_handle_current"), 1);
  ASSERT_EQ(mapping_number(submitted, "requires_owner_mailbox"), 1);
  ASSERT_EQ(mapping_number(submitted, "requires_owner_main_queue"), 0);
  ASSERT_EQ(mapping_number(submitted, "main_required"), 0);
  ASSERT_EQ(mapping_number(submitted, "queued_on_main"), 0);
  free_mapping(submitted);
  auto probe_task = vm_owner_enqueue_task(owner, "executor_probe", "safe-after-target-message");
  ASSERT_GT(probe_task, 0u);

  vm_owner_thread_start(2);
  for (int i = 0; i < 100; i++) {
    auto* status = vm_owner_thread_status();
    auto probe_done = mapping_number(status, "executor_probe_executed");
    free_mapping(status);
    auto* maybe_completed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
    auto completed_state = std::string(mapping_string(maybe_completed, "state"));
    free_mapping(maybe_completed);
    if (probe_done >= before_probe + 1 && completed_state == "completed") {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  auto* running = vm_owner_thread_status();
  ASSERT_GE(mapping_number(running, "executor_probe_executed"), before_probe + 1);
  ASSERT_GE(mapping_number(running, "executor_safe_task_dispatched"), before_safe + 1);
  ASSERT_EQ(mapping_number(running, "executor_main_required_skipped"), before_skipped);
  ASSERT_EQ(mapping_number(running, "executor_same_owner_claim_conflicts"), 0);
  ASSERT_EQ(mapping_number(running, "main_queued"), before_main_queued);
  ASSERT_EQ(mapping_number(running, "main_dispatched"), before_main_dispatched);
  free_mapping(running);

  auto* drained = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(drained, "owner_queue_depth"), 0);
  ASSERT_EQ(mapping_number(drained, "owner_main_queue_depth"), 0);
  free_mapping(drained);

  auto* completed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_STREQ(mapping_string(completed, "state"), "completed");
  ASSERT_EQ(mapping_number(completed, "requires_owner_message_completion"), 0);
  ASSERT_EQ(mapping_number(completed, "frozen_result"), 1);
  free_mapping(completed);
  ASSERT_EQ(obj->time_of_ref, time_of_ref_before);
  ASSERT_EQ(obj->flags, flags_before);
  auto* after = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(after, "main_dispatched"), before_main_dispatched);
  free_mapping(after);
  auto* completed_trace = vm_owner_message_trace(1);
  auto* completed_events = find_string_in_mapping(completed_trace, "events");
  ASSERT_NE(completed_events, nullptr);
  ASSERT_EQ(completed_events->type, T_ARRAY);
  ASSERT_EQ(completed_events->u.arr->size, 1);
  auto* completed_event = completed_events->u.arr->item[0].u.map;
  ASSERT_EQ(mapping_number(completed_event, "message_id"), future_id);
  ASSERT_STREQ(mapping_string(completed_event, "state"), "completed");
  ASSERT_STREQ(mapping_string(completed_event, "route"), "owner_mailbox");
  ASSERT_STREQ(mapping_string(completed_event, "result_key"), "owner_lpc_probe");
  ASSERT_STREQ(mapping_string(completed_event, "error"), "");
  ASSERT_EQ(mapping_number(completed_event, "pending"), 0);
  ASSERT_EQ(mapping_number(completed_event, "completed"), 1);
  ASSERT_EQ(mapping_number(completed_event, "failed"), 0);
  ASSERT_EQ(mapping_number(completed_event, "terminal"), 1);
  ASSERT_EQ(mapping_number(completed_event, "frozen_result"), 1);
  ASSERT_EQ(mapping_number(completed_event, "requires_owner_mailbox"), 1);
  ASSERT_EQ(mapping_number(completed_event, "requires_owner_main_queue"), 0);
  ASSERT_EQ(mapping_number(completed_event, "queued_on_main"), 0);
  free_mapping(completed_trace);
  vm_owner_thread_stop();
  vm_owner_clear_id(obj);
  destruct_object(obj);
}

TEST_F(DriverTest, TestVmOwnerExecutorDrainsTargetMessagesWithoutMainFallback) {
  const char* owner = "owner/test/executor/target-message-no-main-fallback";

  vm_owner_thread_stop();
  auto mapping_entry = [](mapping_t* map, const char* key) -> mapping_t* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_MAPPING);
    return value && value->type == T_MAPPING ? value->u.map : nullptr;
  };
  auto* before = vm_owner_thread_status();
  auto before_probe = mapping_number(before, "executor_probe_executed");
  auto before_safe = mapping_number(before, "executor_safe_task_dispatched");
  auto before_skipped = mapping_number(before, "executor_main_required_skipped");
  free_mapping(before);
  auto* before_runtime = vm_owner_runtime_status();
  auto before_normal_fallback = mapping_number(before_runtime, "normal_path_main_fallback_count");
  auto before_target_message_fallback =
      mapping_number(before_runtime, "target_owner_message_main_fallback");
  free_mapping(before_runtime);

  auto main_required_task =
      vm_owner_enqueue_test_main_required_message(owner, "blocked-object-message");
  auto probe_task = vm_owner_enqueue_task(owner, "executor_probe", "safe-after-main-required-head");
  ASSERT_GT(main_required_task, 0u);
  ASSERT_GT(probe_task, main_required_task);

  auto* queued = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(queued, "owner_queue_depth"), 2);
  ASSERT_EQ(mapping_number(queued, "owner_executor_safe_queue_depth"), 2);
  ASSERT_EQ(mapping_number(queued, "owner_main_required_queue_depth"), 0);
  free_mapping(queued);
  auto* queued_thread = vm_owner_thread_status();
  ASSERT_GE(mapping_number(queued_thread, "runnable_owner_count"), 1);
  auto* queued_fairness = mapping_entry(queued_thread, "executor_queue_fairness");
  ASSERT_GE(mapping_number(queued_fairness, "executor_ready_owner_count"), 1);
  ASSERT_EQ(mapping_number(queued_fairness, "mixed_backlog_owner_count"), 0);
  ASSERT_GE(mapping_number(queued_fairness, "max_executor_safe_backlog"), 2);
  ASSERT_EQ(mapping_number(queued_fairness, "max_main_required_backlog"), 0);
  ASSERT_EQ(mapping_number(queued_fairness, "owner_scheduler_backpressure_ready"), 1);
  ASSERT_GT(mapping_number(queued_fairness, "owner_scheduler_max_owner_queue_depth"), 0);
  ASSERT_GE(mapping_number(queued_fairness, "owner_scheduler_max_owner_backlog"), 2);
  ASSERT_EQ(mapping_number(queued_fairness, "owner_scheduler_backpressure_over_limit"), 0);
  ASSERT_EQ(mapping_number(queued_fairness, "owner_scheduler_fairness_guard_ready"), 1);
  free_mapping(queued_thread);

  vm_owner_thread_start(1);
  for (int i = 0; i < 100; i++) {
    auto* status = vm_owner_thread_status();
    auto probe_done = mapping_number(status, "executor_probe_executed");
    free_mapping(status);
    auto* mailbox = vm_owner_mailbox_status(owner);
    auto queue_depth = mapping_number(mailbox, "owner_queue_depth");
    free_mapping(mailbox);
    if (probe_done >= before_probe + 1 && queue_depth == 0) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  auto* running = vm_owner_thread_status();
  ASSERT_GE(mapping_number(running, "executor_probe_executed"), before_probe + 1);
  ASSERT_GE(mapping_number(running, "executor_safe_task_dispatched"), before_safe + 2);
  ASSERT_EQ(mapping_number(running, "executor_main_required_skipped"), before_skipped);
  ASSERT_EQ(mapping_number(running, "executor_same_owner_claim_conflicts"), 0);
  free_mapping(running);

  auto* after = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(after, "owner_queue_depth"), 0);
  ASSERT_EQ(mapping_number(after, "owner_executor_safe_queue_depth"), 0);
  ASSERT_EQ(mapping_number(after, "owner_main_required_queue_depth"), 0);
  free_mapping(after);
  auto* after_thread = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(after_thread, "runnable_owner_count"), 0);
  auto* after_fairness = mapping_entry(after_thread, "executor_queue_fairness");
  ASSERT_EQ(mapping_number(after_fairness, "main_required_only_owner_count"), 0);
  ASSERT_EQ(mapping_number(after_fairness, "mixed_backlog_owner_count"), 0);
  free_mapping(after_thread);
  auto* after_runtime = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(after_runtime, "normal_path_main_fallback_count"),
            before_normal_fallback);
  ASSERT_EQ(mapping_number(after_runtime, "target_owner_message_main_fallback"),
            before_target_message_fallback);
  ASSERT_EQ(mapping_number(after_runtime, "normal_path_main_fallback_ready"), 1);
  free_mapping(after_runtime);

  auto* trace = vm_owner_task_trace(16);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  int target_message_progressed = 0;
  int probe_completed = 0;
  for (int i = 0; i < events->u.arr->size; i++) {
    auto* event = events->u.arr->item[i].u.map;
    if (mapping_number(event, "task_id") == static_cast<long>(main_required_task) &&
        std::string(mapping_string(event, "state")) != "queued") {
      target_message_progressed = 1;
    }
    if (mapping_number(event, "task_id") == static_cast<long>(probe_task) &&
        std::string(mapping_string(event, "state")) == "executor_probe_completed") {
      probe_completed = 1;
    }
  }
  ASSERT_EQ(target_message_progressed, 1);
  ASSERT_EQ(probe_completed, 1);
  free_mapping(trace);

  auto* purged = vm_owner_purge_mailbox(owner);
  free_mapping(purged);
  auto* purged_status = vm_owner_thread_status();
  auto* purged_fairness = mapping_entry(purged_status, "executor_queue_fairness");
  ASSERT_EQ(mapping_number(purged_fairness, "main_required_only_owner_count"), 0);
  ASSERT_EQ(mapping_number(purged_fairness, "max_main_required_backlog"), 0);
  free_mapping(purged_status);
  vm_owner_thread_stop();
}

TEST_F(DriverTest, TestOwnerSchedulerBackpressureRejectsOverLimit) {
  const char* owner = "owner/test/scheduler/backpressure";

  vm_owner_thread_stop();

  auto* before = vm_owner_runtime_status();
  auto max_depth = mapping_number(before, "owner_scheduler_max_owner_queue_depth");
  auto before_rejected = mapping_number(before, "owner_executor_backpressure_rejected");
  free_mapping(before);
  ASSERT_GT(max_depth, 0);

  for (long i = 0; i < max_depth; i++) {
    auto task_id = vm_owner_enqueue_task(owner, "executor_probe", "backpressure-fill");
    ASSERT_GT(task_id, 0u);
  }

  auto rejected = vm_owner_enqueue_task(owner, "executor_probe", "backpressure-over-limit");
  ASSERT_EQ(rejected, 0u);

  auto* trace = vm_owner_task_trace(8);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  int rejected_queued = 0;
  int rejected_backpressure = 0;
  for (int i = 0; i < events->u.arr->size; ++i) {
    auto* event = events->u.arr->item[i].u.map;
    if (std::string(mapping_string(event, "task_key")) != "backpressure-over-limit") {
      continue;
    }
    const auto state = std::string(mapping_string(event, "state"));
    rejected_queued += state == "queued" ? 1 : 0;
    rejected_backpressure += state == "backpressure_rejected" ? 1 : 0;
  }
  EXPECT_EQ(rejected_queued, 0);
  EXPECT_EQ(rejected_backpressure, 1);
  free_mapping(trace);

  auto* mailbox = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(mailbox, "owner_queue_depth"), max_depth);
  free_mapping(mailbox);

  auto* after = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(after, "owner_scheduler_backpressure_ready"), 1);
  ASSERT_EQ(mapping_number(after, "owner_scheduler_max_owner_backlog"), max_depth);
  ASSERT_EQ(mapping_number(after, "owner_scheduler_backpressure_over_limit"), 0);
  ASSERT_EQ(mapping_number(after, "owner_scheduler_backpressure_high_watermark_exceeded"), 1);
  ASSERT_GT(mapping_number(after, "owner_executor_backpressure_rejected"), before_rejected);
  free_mapping(after);

  free_mapping(vm_owner_drain_mailbox(owner, 0));
}

TEST_F(DriverTest, TestOwnerSubmissionBackpressureClosesRegisteredFutures) {
  const char* owner = "owner/test/submission/backpressure";

  vm_owner_thread_stop();
  free_mapping(vm_owner_purge_mailbox(owner));
  auto* target = load_object_for_test("single/void");
  ASSERT_NE(target, nullptr);
  vm_owner_set_id(target, owner);
  struct TargetGuard {
    object_t* target;
    ~TargetGuard() {
      if (target) {
        vm_owner_clear_id(target);
        destruct_object(target);
      }
    }
  } target_guard{target};

  std::atomic<int> blocker_started{0};
  std::atomic<int> release_blocker{0};
  bool runtime_cleanup_needed = true;
  struct RuntimeGuard {
    const char* owner;
    std::atomic<int>* release_blocker;
    bool* cleanup_needed;
    ~RuntimeGuard() {
      if (!*cleanup_needed) {
        return;
      }
      release_blocker->store(1, std::memory_order_release);
      vm_owner_thread_stop();
      free_mapping(vm_owner_purge_mailbox(owner));
    }
  } runtime_guard{owner, &release_blocker, &runtime_cleanup_needed};


  vm_owner_thread_start(1);
  ASSERT_GT(vm_owner_enqueue_executor_task(
                target, "room_output_projection", "submission-backpressure-blocker",
                [&] {
                  blocker_started.store(1, std::memory_order_release);
                  while (release_blocker.load(std::memory_order_acquire) == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
                }),
            0u);
  for (int i = 0; i < 200 && blocker_started.load(std::memory_order_acquire) == 0; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  auto* runtime = vm_owner_runtime_status();
  const auto max_depth = mapping_number(runtime, "owner_scheduler_max_owner_queue_depth");
  free_mapping(runtime);
  ASSERT_GT(max_depth, 0);
  for (LPC_INT i = 0; i < max_depth; i++) {
    ASSERT_GT(vm_owner_enqueue_task(owner, "executor_probe", "submission-backpressure-fill"), 0u);
  }
  auto* full_mailbox = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(full_mailbox, "owner_queue_depth"), max_depth);
  free_mapping(full_mailbox);

  std::atomic<int> projector_runs{0};
  const auto string_submission = vm_owner_submit_frozen_string_task(
      target, "room_output_projection", "submission-backpressure-string",
      [&projector_runs](std::string* output) {
        projector_runs.fetch_add(1, std::memory_order_relaxed);
        *output = "must-not-run";
        return true;
      });
  ASSERT_FALSE(string_submission.queued);
  ASSERT_EQ(string_submission.task_id, 0u);
  ASSERT_GT(string_submission.future_id, 0u);

  auto* lpc_submission = vm_owner_lpc_task(target, owner, "owner_task_readonly");
  const auto lpc_future_id = static_cast<uint64_t>(mapping_number(lpc_submission, "future_id"));
  ASSERT_EQ(mapping_number(lpc_submission, "success"), 0);
  ASSERT_GT(lpc_future_id, 0u);
  free_mapping(lpc_submission);

  auto* ordinary_submission = vm_owner_ordinary_lpc_task(target, owner, "owner_task_player", 1);
  const auto ordinary_future_id =
      static_cast<uint64_t>(mapping_number(ordinary_submission, "future_id"));
  ASSERT_EQ(mapping_number(ordinary_submission, "success"), 0);
  ASSERT_STREQ(mapping_string(ordinary_submission, "state"), "failed");
  ASSERT_STREQ(mapping_string(ordinary_submission, "error"), "owner scheduler backpressure");
  ASSERT_GT(ordinary_future_id, 0u);
  free_mapping(ordinary_submission);

  constexpr uint64_t worker_task_id = 987654322u;
  const auto compute_future_id = vm_owner_register_compute_future(
      owner, worker_task_id, "bench", "submission-backpressure-compute");
  ASSERT_GT(compute_future_id, 0u);
  ASSERT_EQ(
      vm_owner_enqueue_compute_result(owner, worker_task_id, "bench", "completed", "bench", ""),
      0u);

  auto assert_failed_and_take = [&](uint64_t future_id) {
    auto* failed = vm_owner_future_poll(future_id);
    EXPECT_EQ(mapping_number(failed, "success"), 1);
    EXPECT_STREQ(mapping_string(failed, "state"), "failed");
    EXPECT_STREQ(mapping_string(failed, "error"), "owner scheduler backpressure");
    EXPECT_EQ(mapping_number(failed, "requires_owner_message_completion"), 0);
    free_mapping(failed);
    auto* taken = vm_owner_future_take(future_id);
    EXPECT_EQ(mapping_number(taken, "consumed"), 1);
    free_mapping(taken);
  };
  assert_failed_and_take(string_submission.future_id);
  assert_failed_and_take(lpc_future_id);
  assert_failed_and_take(ordinary_future_id);
  assert_failed_and_take(compute_future_id);
  ASSERT_EQ(projector_runs.load(std::memory_order_relaxed), 0);

  auto* purged = vm_owner_purge_mailbox(owner);
  ASSERT_EQ(mapping_number(purged, "purged"), max_depth);
  free_mapping(purged);
  release_blocker.store(1, std::memory_order_release);
  vm_owner_thread_stop();
  runtime_cleanup_needed = false;
}

TEST_F(DriverTest, TestVmOwnerRuntimeReportsExecutorTaskContract) {
  auto mapping_entry = [](mapping_t* map, const char* key) -> mapping_t* {
    auto* value = map ? find_string_in_mapping(map, key) : nullptr;
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_MAPPING);
    return value && value->type == T_MAPPING ? value->u.map : nullptr;
  };
  auto mapping_array = [](mapping_t* map, const char* key) -> array_t* {
    auto* value = map ? find_string_in_mapping(map, key) : nullptr;
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_ARRAY);
    return value && value->type == T_ARRAY ? value->u.arr : nullptr;
  };
  // The production-gate checklist is a document (docs/multicore-production-gate.md),
  // not runtime status: assert_contract below covers the actual runtime surface.

  auto assert_contract = [&](mapping_t* status) {
    ASSERT_STREQ(mapping_string(status, "executor_contract_version"), "owner_executor_v2");
    ASSERT_STREQ(mapping_string(status, "executor_model"), "owner_executor");
    ASSERT_STREQ(mapping_string(status, "executor_dispatch_model"), "descriptor_manifest");
    ASSERT_STREQ(mapping_string(status, "executor_lpc_model"), "default_closed_explicit_open");
    ASSERT_STREQ(mapping_string(status, "ordinary_lpc_default_policy"),
                 "default_closed_explicit_open");
    ASSERT_EQ(mapping_number(status, "ordinary_lpc_default_closed"), 1);
    ASSERT_EQ(mapping_number(status, "ordinary_lpc_activation_policy_ready"), 1);
    ASSERT_EQ(mapping_number(status, "ordinary_lpc_dispatch_path_ready"), 1);
    ASSERT_EQ(mapping_number(status, "ordinary_lpc_explicit_open_required"), 1);
    ASSERT_STREQ(mapping_string(status, "ordinary_lpc_activation_policy"),
                 "default_closed_explicit_open");
    ASSERT_STREQ(mapping_string(status, "ordinary_lpc_next_blocker"), "");
    ASSERT_EQ(mapping_number(status, "owner_runtime_split_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_runtime_split_model"),
                 "runtime_v4_modules_with_owner_runtime_coordinator");
    ASSERT_EQ(mapping_number(status, "owner_runtime_v4_hardening_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_runtime_benchmark_smoke_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_runtime_benchmark_schema"),
                 "owner_runtime_bench_v1");
    ASSERT_EQ(mapping_number(status, "owner_runtime_stress_profile_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_runtime_stress_entry"),
                 "tools/owner-runtime-v4-stress.sh");
    ASSERT_EQ(mapping_number(status, "lpc_modern_runtime_stress_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "lpc_modern_runtime_stress_entry"),
                 "tools/lpc-modern-runtime-stress.sh");
    ASSERT_EQ(mapping_number(status, "owner_runtime_layering_guard_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_runtime_coordinator_module_ready"), 1);
    ASSERT_EQ(mapping_number(status, "lpc_modern_profile_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "lpc_modern_profile_schema"), "lpc_modern_profile_v1");
    ASSERT_STREQ(mapping_string(status, "lpc_modern_profile_mode"), "opt_in_pragma");
    ASSERT_EQ(mapping_number(status, "lpc_vm_profile_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "lpc_vm_profile_schema"), "lpc_vm_profile_v1");
    ASSERT_EQ(mapping_number(status, "lpc_vm_profile_default_recording"), 0);
    ASSERT_STREQ(mapping_string(status, "lpc_vm_profile_recording_policy"),
                 "explicit_current_thread_only");
    ASSERT_EQ(mapping_number(status, "lpc_vm_benchmark_smoke_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "lpc_vm_benchmark_schema"), "lpc_vm_bench_v1");
    ASSERT_EQ(mapping_number(status, "lpc_vm_hot_path_profile_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "lpc_vm_hot_path_profile_model"),
                 "opcode_efun_call_other_function_pointer_parser_mapping_string_v1");
    ASSERT_EQ(mapping_number(status, "object_store_benchmark_smoke_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "object_store_benchmark_schema"), "object_store_bench_v1");
    ASSERT_EQ(mapping_number(status, "lpc_apply_dispatch_cache_probe_ready"), 1);
    ASSERT_EQ(mapping_number(status, "lpc_opcode_dispatch_profile_ready"), 1);
    ASSERT_EQ(mapping_number(status, "lpc_efun_dispatch_profile_ready"), 1);
    ASSERT_EQ(mapping_number(status, "lpc_call_other_profile_ready"), 1);
    ASSERT_EQ(mapping_number(status, "lpc_function_pointer_profile_ready"), 1);
    ASSERT_EQ(mapping_number(status, "lpc_parser_action_profile_ready"), 1);
    ASSERT_EQ(mapping_number(status, "lpc_mapping_string_profile_ready"), 1);
    ASSERT_EQ(mapping_number(status, "lpc_dispatch_cache_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "lpc_dispatch_cache_model"),
                 "apply_dispatch_thread_local_direct_cache_v1");
    ASSERT_EQ(mapping_number(status, "lpc_jit_experiment_default_off"), 1);
    ASSERT_EQ(mapping_number(status, "modern_lpc_pragma_ready"), 1);
    ASSERT_EQ(mapping_number(status, "strict_owner_pragma_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "strict_owner_policy"),
                 "strict_owner_owner_safe_payloads_v1");
    ASSERT_EQ(mapping_number(status, "lpc_source_encoding_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "lpc_source_encoding_schema"), "lpc_source_encoding_v1");
    ASSERT_STREQ(mapping_string(status, "vm_internal_string_encoding"), "utf-8");
    ASSERT_EQ(mapping_number(status, "session_encoding_contract_ready"), 1);
    ASSERT_EQ(mapping_number(status, "gateway_encoding_boundary_ready"), 1);
    ASSERT_EQ(mapping_number(status, "encoding_audit_ready"), 1);
    ASSERT_EQ(mapping_number(status, "legacy_lpc_default_closed"), 1);
    ASSERT_EQ(mapping_number(status, "owner_safe_future_api_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_safe_lpc_api_failure_schema_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_safe_lpc_api_failure_schema"),
                 "owner_safe_lpc_api_failure_v1");
    ASSERT_STREQ(mapping_string(status, "owner_safe_lpc_api_return_fields"),
                 "success,ok,code,error,reason,api,trace_id");
    ASSERT_EQ(mapping_number(status, "owner_async_api_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_await_poll_adapter_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_await_coroutine_runtime_ready"), 0);
    ASSERT_EQ(mapping_number(status, "freeze_snapshot_api_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "freeze_snapshot_model"), "validated_deep_copy");
    ASSERT_EQ(mapping_number(status, "lpc_value_object_profile_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "lpc_value_object_model"),
                 "frozen_snapshot_value_object_v1");
    ASSERT_EQ(mapping_number(status, "lpc_value_object_live_lifecycle_member"), 0);
    ASSERT_EQ(mapping_number(status, "lpc_value_object_cross_owner_payload_safe"), 1);
    ASSERT_EQ(mapping_number(status, "owner_snapshot_persistence_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_snapshot_persistence_model"),
                 "owner_snapshot_serialized_payload_v1");
    ASSERT_STREQ(mapping_string(status, "owner_snapshot_persistence_adapter"),
                 "main_thread_file_adapter");
    ASSERT_EQ(mapping_number(status, "owner_snapshot_direct_save_hot_path_audit_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_commit_api_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_commit_model"), "owner_commit_boundary_record");
    ASSERT_EQ(mapping_number(status, "owner_task_manifest_module_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_trace_store_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_future_store_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_scheduler_state_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_metrics_store_ready"), 1);
    ASSERT_EQ(mapping_number(status, "object_store_owner_fast_path_ready"), 1);
    ASSERT_EQ(mapping_number(status, "object_store_global_fallback_on_owner_fast_path"), 0);
    ASSERT_EQ(mapping_number(status, "object_handle_capability_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "object_handle_capability_model"),
                 "object_handle_capability_v1");
    ASSERT_EQ(mapping_number(status, "owner_scheduler_backpressure_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_scheduler_backpressure_strategy"),
                 "observe_then_reject_new_tasks");
    ASSERT_GT(mapping_number(status, "owner_scheduler_max_owner_queue_depth"), 0);
    ASSERT_GE(mapping_number(status, "owner_scheduler_max_owner_backlog"), 0);
    ASSERT_EQ(mapping_number(status, "owner_scheduler_backpressure_over_limit"), 0);
    ASSERT_EQ(mapping_number(status, "owner_scheduler_fairness_guard_ready"), 1);
    ASSERT_GE(mapping_number(status, "owner_executor_backpressure_rejected"), 0);
    ASSERT_EQ(mapping_number(status, "owner_task_manifest_v2_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_task_manifest_schema"), "owner_task_manifest_v2");
    ASSERT_EQ(mapping_number(status, "owner_executor_admission_gate_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_callback_admission_unified"), 1);
    ASSERT_EQ(mapping_number(status, "owner_callback_diagnostics_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_callback_diagnostics_schema"),
                 "owner_callback_diagnostics_v1");
    ASSERT_STREQ(mapping_string(status, "owner_callback_failure_code_schema"),
                 "owner_callback_failure_code_v1");
    ASSERT_STREQ(mapping_string(status, "owner_callback_drop_reason_schema"),
                 "owner_callback_drop_reason_v1");
    ASSERT_EQ(mapping_number(status, "owner_callback_allowlist_complete"), 1);
    ASSERT_STREQ(
        mapping_string(status, "owner_callback_supported_kinds"),
        "heartbeat,call_out,async_callback,dns_callback,socket_callback,gateway_command_execute,ed_callback");
    ASSERT_STREQ(mapping_string(status, "owner_executor_admission_policy"),
                 "owner_epoch_payload_allowlist_deadline_guard");
    ASSERT_GE(mapping_number(status, "owner_executor_admission_accepted"), 0);
    ASSERT_GE(mapping_number(status, "owner_executor_admission_rejected"), 0);
    ASSERT_GE(mapping_number(status, "owner_executor_admission_dropped"), 0);
    ASSERT_EQ(mapping_number(status, "owner_executor_payload_policy_v2_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_executor_trace_schema_v2_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_executor_trace_schema"), "owner_executor_trace_v2");
    ASSERT_EQ(mapping_number(status, "owner_executor_metrics_v2_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_executor_queue_depth_metrics_ready"), 1);
    ASSERT_GE(mapping_number(status, "owner_executor_queue_depth"), 0);
    ASSERT_GE(mapping_number(status, "owner_executor_runnable_queue_depth"), 0);
    ASSERT_GE(mapping_number(status, "owner_executor_safe_queue_depth"), 0);
    ASSERT_GE(mapping_number(status, "owner_executor_main_required_queue_depth"), 0);
    ASSERT_EQ(mapping_number(status, "owner_executor_future_timeout_cancel_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_executor_future_terminal_take_ready"), 1);
    ASSERT_GE(mapping_number(status, "owner_executor_future_timeout"), 0);
    ASSERT_GE(mapping_number(status, "owner_executor_future_cancelled"), 0);
    ASSERT_GE(mapping_number(status, "owner_executor_stale_drop"), 0);
    ASSERT_GE(mapping_number(status, "owner_executor_destructed_drop"), 0);
    ASSERT_GE(mapping_number(status, "owner_executor_epoch_mismatch_drop"), 0);
    ASSERT_GE(mapping_number(status, "owner_executor_context_cleanup_leaks"), 0);
    ASSERT_GE(mapping_number(status, "owner_executor_future_pending_backlog"), 0);
    ASSERT_EQ(mapping_number(status, "owner_executor_socket_release_trace_ready"), 1);
    ASSERT_EQ(mapping_number(status, "registered_owner_task_domains_ready"), 1);
    ASSERT_EQ(mapping_number(status, "registered_owner_task_domain_count"), 18);
    ASSERT_EQ(mapping_number(status, "owner_service_shard_registry_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_service_shard_registry_schema"),
                 "owner_service_shard_registry_v1");
    ASSERT_EQ(mapping_number(status, "owner_service_shard_domain_count"), 18);
    ASSERT_STREQ(mapping_string(status, "owner_service_shard_domains"),
                 "readonly,player,room,session,item,economy,combat,mail,reward,world,persistence,"
                 "team,guild,sect,quest,rank,crafting,life_skill");
    ASSERT_EQ(mapping_number(status, "owner_service_registry_lpc_domain_alignment_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_tick_group_scheduler_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_tick_group_scheduler_schema"),
                 "owner_tick_group_scheduler_v1");
    ASSERT_EQ(mapping_number(status, "owner_tick_group_count"), 6);
    ASSERT_STREQ(mapping_string(status, "owner_tick_groups"),
                 "gateway_command,heartbeat,callout,socket_async,service_tick,diagnostic");
    ASSERT_EQ(mapping_number(status, "owner_scheduler_tuning_config_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_scheduler_tuning_config_schema"),
                 "owner_scheduler_tuning_v1");
    ASSERT_STREQ(mapping_string(status, "owner_scheduler_tick_group_budget_source"),
                 "owner_service_registry");
    ASSERT_EQ(mapping_number(status, "owner_scheduler_priority_groups_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_scheduler_tick_group_backpressure_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_scheduler_starvation_guard_ready"), 1);
    ASSERT_EQ(mapping_number(status, "target_owner_message_executor_ready"), 1);
    ASSERT_EQ(mapping_number(status, "normal_path_main_fallback_count"), 0);
    ASSERT_EQ(mapping_number(status, "normal_path_main_fallback_ready"), 1);
    ASSERT_EQ(mapping_number(status, "main_fallback_policy_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "main_fallback_classification"), "explicit_policy");
    ASSERT_EQ(mapping_number(status, "session_fifo_contract_ready"), 1);
    ASSERT_GE(mapping_number(status, "gateway_command_pending_sessions"), 0);
    ASSERT_GE(mapping_number(status, "gateway_command_tasks_enqueued"), 0);
    ASSERT_GE(mapping_number(status, "gateway_command_tasks_finished"), 0);
    ASSERT_GE(mapping_number(status, "gateway_output_fifo_enqueued"), 0);
    ASSERT_GE(mapping_number(status, "gateway_output_fifo_flushed"), 0);
    ASSERT_GE(mapping_number(status, "gateway_raw_writes_sent"), 0);
    ASSERT_EQ(mapping_number(status, "gateway_io_adapter_only_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "gateway_io_boundary"), "main_thread_io_adapter");
    ASSERT_EQ(mapping_number(status, "gateway_low_overhead_latency_probe_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "gateway_latency_probe_source"), "gateway_status_internal");
    ASSERT_STREQ(
        mapping_string(status, "gateway_latency_probe_fields"),
        "receive_decode,receive_payload_copy,receive_enqueue_to_dispatch,receive_apply,"
        "command_enqueue_to_dispatch,command_execute,"
        "receive_main_queue_depth,deferred_main_drain_wait,"
        "reply_enqueue_to_dispatch,reply_execute,output_enqueue_to_dispatch,output_execute,main_drain");
    ASSERT_EQ(mapping_number(status, "callback_payload_strict_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_callback_payload_strict_diagnostics_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_callback_payload_policy_schema"),
                 "owner_callback_payload_policy_v1");
    ASSERT_STREQ(mapping_string(status, "owner_callback_payload_policy"),
                 "frozen_payload_or_owner_handle_only");
    ASSERT_STREQ(
        mapping_string(status, "owner_callback_failure_codes"),
        "owner_scheduler_backpressure,callback_not_allowlisted,callback_invalid_target,"
        "owner_epoch_mismatch,target_destructed,target_stale,admission_rejected,task_dropped");
    ASSERT_STREQ(
        mapping_string(status, "owner_callback_drop_reasons"),
        "none,owner_scheduler_backpressure,callback_not_allowlisted,callback_invalid_target,"
        "owner_epoch_mismatch,target_destructed,target_stale,admission_rejected,task_dropped");
    ASSERT_EQ(mapping_number(status, "owner_callback_human_reason_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_callback_failure_reason_schema"),
                 "owner_callback_failure_reason_v1");
    ASSERT_EQ(mapping_number(status, "service_shard_executor_ready"), 1);
    ASSERT_EQ(mapping_number(status, "domain_task_registry_mudlib_aligned"), 1);
    ASSERT_EQ(mapping_number(status, "keyed_service_shard_ready"), 1);
    ASSERT_EQ(mapping_number(status, "hot_path_service_owner_single_point"), 0);
    ASSERT_GT(mapping_number(status, "hot_path_service_shard_count"), 0);
    ASSERT_STREQ(mapping_string(status, "owner_service_shard_policy_model"),
                 "keyed_service_shard_for_hot_paths");
    ASSERT_EQ(mapping_number(status, "target_owner_message_main_fallback"), 0);
    ASSERT_EQ(mapping_number(status, "production_perfect_contract_ready"), 1);
    ASSERT_EQ(mapping_number(status, "facade_only_runtime_claims"), 0);
    ASSERT_EQ(mapping_number(status, "executor_callback_task_boundary_ready"), 1);
    ASSERT_EQ(mapping_number(status, "executor_callback_allowlist_ready"), 1);
    ASSERT_EQ(mapping_number(status, "executor_callback_main_adapter_ready"), 1);
    ASSERT_EQ(mapping_number(status, "executor_callback_allowlist_count"), 6);
    ASSERT_STREQ(mapping_string(status, "executor_callback_payload_policy"),
                 "frozen_payload_or_owner_handle_only");
    ASSERT_EQ(mapping_number(status, "heartbeat_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "heartbeat_owner_executor_task_type"), "heartbeat");
    ASSERT_STREQ(mapping_string(status, "heartbeat_owner_executor_route"),
                 "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(status, "heartbeat_owner_executor_fallback_route"), "");
    ASSERT_STREQ(mapping_string(status, "heartbeat_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_EQ(mapping_number(status, "heartbeat_owner_executor_fallback_main_ready"), 1);
    ASSERT_EQ(mapping_number(status, "heartbeat_current_object_thread_local"), 0);
    ASSERT_EQ(mapping_number(status, "callout_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "callout_owner_executor_task_type"), "call_out");
    ASSERT_STREQ(mapping_string(status, "callout_owner_executor_route"),
                 "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(status, "callout_owner_executor_fallback_route"), "");
    ASSERT_STREQ(mapping_string(status, "callout_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_EQ(mapping_number(status, "callout_owner_executor_expired_handle_detach_ready"), 1);
    ASSERT_EQ(mapping_number(status, "callout_owner_executor_cleanup_main_ready"), 1);
    ASSERT_EQ(mapping_number(status, "callout_owner_executor_drop_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(status, "callout_owner_executor_fallback_main_ready"), 1);
    ASSERT_EQ(mapping_number(status, "async_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "async_owner_executor_task_type"), "async_callback");
    ASSERT_STREQ(mapping_string(status, "async_owner_executor_route"),
                 "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(status, "async_owner_executor_fallback_route"), "");
    ASSERT_STREQ(mapping_string(status, "async_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_STREQ(mapping_string(status, "async_owner_executor_result_policy"),
                 "frozen_deep_copy_result");
    ASSERT_EQ(mapping_number(status, "async_owner_executor_cleanup_main_ready"), 1);
    ASSERT_EQ(mapping_number(status, "async_owner_executor_drop_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(status, "dns_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "dns_owner_executor_task_type"), "dns_callback");
    ASSERT_STREQ(mapping_string(status, "dns_owner_executor_route"),
                 "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(status, "dns_owner_executor_fallback_route"), "");
    ASSERT_STREQ(mapping_string(status, "dns_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_STREQ(mapping_string(status, "dns_owner_executor_result_policy"),
                 "frozen_deep_copy_result");
    ASSERT_EQ(mapping_number(status, "dns_owner_executor_owner_epoch_capture_ready"), 1);
    ASSERT_EQ(mapping_number(status, "dns_owner_executor_cleanup_main_ready"), 1);
    ASSERT_EQ(mapping_number(status, "dns_owner_executor_drop_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(status, "socket_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "socket_owner_executor_task_type"), "socket_callback");
    ASSERT_STREQ(mapping_string(status, "socket_owner_executor_route"),
                 "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(status, "socket_owner_executor_fallback_route"), "");
    ASSERT_STREQ(mapping_string(status, "socket_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_STREQ(mapping_string(status, "socket_owner_executor_result_policy"),
                 "frozen_deep_copy_args");
    ASSERT_EQ(mapping_number(status, "socket_owner_executor_cleanup_main_ready"), 1);
    ASSERT_EQ(mapping_number(status, "socket_owner_executor_drop_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(status, "socket_release_main_required"), 0);
    ASSERT_EQ(mapping_number(status, "socket_release_owner_safe_handshake_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "socket_release_owner_safe_handshake_policy"),
                 "synchronous_release_acquire_owner_epoch_guard");
    ASSERT_EQ(mapping_number(status, "socket_release_owner_epoch_guard_ready"), 1);
    ASSERT_EQ(mapping_number(status, "gateway_command_execute_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "gateway_command_execute_task_type"),
                 "gateway_command_execute");
    ASSERT_STREQ(mapping_string(status, "gateway_command_execute_route"),
                 "owner_main_queue_io_adapter");
    ASSERT_STREQ(mapping_string(status, "gateway_command_execute_fallback_route"), "");
    ASSERT_STREQ(mapping_string(status, "gateway_command_execute_policy"),
                 "main_thread_io_adapter_until_interactive_detached");
    ASSERT_STREQ(mapping_string(status, "gateway_command_execute_payload_policy"),
                 "owner_private_command_snapshot");
    ASSERT_EQ(mapping_number(status, "gateway_command_execute_reply_queue_main_ready"), 1);
    ASSERT_EQ(mapping_number(status, "gateway_command_execute_stale_drop_ready"), 1);
    ASSERT_EQ(mapping_number(status, "gateway_command_execute_context_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(status, "gateway_command_execute_session_revalidate_ready"), 1);
    ASSERT_GE(mapping_number(status, "executor_callback_main_cleanup_queued"), 0);
    auto* callback_task_contracts = mapping_array(status, "executor_callback_task_contracts");
    ASSERT_NE(callback_task_contracts, nullptr);
    ASSERT_EQ(callback_task_contracts->size, 6);
    auto* vm_context_contract = mapping_entry(status, "vm_context_contract");
    ASSERT_NE(vm_context_contract, nullptr);
    ASSERT_EQ(mapping_number(vm_context_contract, "contract_version"), 1);
    ASSERT_STREQ(mapping_string(vm_context_contract, "context_model"), "thread_local_vm_context");
    ASSERT_STREQ(mapping_string(vm_context_contract, "execution_state_model"),
                 "vm_context_execution_snapshot");
    ASSERT_STREQ(mapping_string(vm_context_contract, "owner_state_model"),
                 "vm_context_owner_scope");
    ASSERT_STREQ(mapping_string(vm_context_contract, "error_state_model"),
                 "vm_context_error_snapshot");
    ASSERT_STREQ(mapping_string(vm_context_contract, "object_store_model"),
                 "owner_local_object_store");
    ASSERT_STREQ(mapping_string(vm_context_contract, "object_store_off_main_policy"),
                 "owner_local_lookup_only");
    ASSERT_EQ(mapping_number(vm_context_contract, "ordinary_lpc_ready"), 1);
    ASSERT_STREQ(mapping_string(vm_context_contract, "ordinary_lpc_blocker"), "");
    ASSERT_EQ(mapping_number(vm_context_contract, "controlled_lpc_ready"), 1);
    ASSERT_STREQ(mapping_string(vm_context_contract, "controlled_lpc_policy"),
                 "descriptor_manifest_only");
    ASSERT_STREQ(mapping_string(vm_context_contract, "eval_stack_model"),
                 "thread_local_owner_execution_stack");
    ASSERT_EQ(mapping_number(vm_context_contract, "eval_stack_thread_local"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "eval_stack_owner_bound_on_executor"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "eval_stack_cleared_after_task"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "eval_stack_owner_local"), 1);
    ASSERT_STREQ(mapping_string(vm_context_contract, "control_stack_model"),
                 "thread_local_owner_control_stack");
    ASSERT_EQ(mapping_number(vm_context_contract, "control_stack_thread_local"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "control_stack_owner_bound_on_executor"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "control_stack_cleared_after_task"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "control_stack_owner_local"), 1);
    ASSERT_STREQ(mapping_string(vm_context_contract, "value_stack_model"),
                 "thread_local_owner_value_stack");
    ASSERT_EQ(mapping_number(vm_context_contract, "value_stack_thread_local"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "value_stack_lvalue_refs_cleared_after_task"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "value_stack_owner_bound_on_executor"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "value_stack_cleared_after_task"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "value_stack_owner_local"), 1);
    ASSERT_STREQ(mapping_string(vm_context_contract, "apply_return_model"),
                 "thread_local_owner_apply_return");
    ASSERT_EQ(mapping_number(vm_context_contract, "apply_return_thread_local"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "apply_return_owner_bound_on_executor"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "apply_return_cleared_after_task"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "apply_return_owner_local"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "sprintf_state_thread_local"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "sprintf_format_buffers_static_free"), 1);
    ASSERT_STREQ(mapping_string(vm_context_contract, "object_refs_model"),
                 "object_handle_boundary");
    ASSERT_EQ(mapping_number(vm_context_contract, "object_refs_owner_local"), 1);
    ASSERT_STREQ(mapping_string(vm_context_contract, "cross_owner_object_refs_policy"),
                 "object_handle_or_frozen_payload_only");
    ASSERT_EQ(mapping_number(vm_context_contract, "cross_owner_payload_rejects_objects"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "cross_owner_result_rejects_objects"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "owner_message_target_handle_guard"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "owner_executor_same_owner_object_refs_only"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "ordinary_lpc_object_store_gate_required"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "object_store_owner_local_complete"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "ordinary_lpc_activation_required"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "ordinary_lpc_activation_policy_ready"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "ordinary_lpc_dispatch_path_ready"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "ordinary_lpc_default_closed"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "ordinary_lpc_explicit_open_required"), 1);
    ASSERT_STREQ(mapping_string(vm_context_contract, "ordinary_lpc_dispatch_model"),
                 "generic_owner_lpc_dispatch");
    ASSERT_STREQ(mapping_string(vm_context_contract, "ordinary_lpc_activation_policy"),
                 "default_closed_explicit_open");
    ASSERT_STREQ(mapping_string(vm_context_contract, "ordinary_lpc_activation_rollout"),
                 "explicit_open_only_until_gateway_migration");
    ASSERT_STREQ(mapping_string(vm_context_contract, "ordinary_lpc_activation_rollback"),
                 "disable_explicit_open_submission");
    ASSERT_EQ(mapping_number(vm_context_contract, "error_state_contextualized"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "execution_state_contextualized"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "owner_scope_contextualized"), 1);
    ASSERT_EQ(mapping_number(vm_context_contract, "object_store_main_thread_only"), 0);
    ASSERT_GE(mapping_number(vm_context_contract, "object_store_sync_rejections"), 0);
    ASSERT_EQ(mapping_number(vm_context_contract, "off_main_object_store_sync_allowed"), 0);
    ASSERT_STREQ(mapping_string(vm_context_contract, "ordinary_lpc_readiness_gate_model"),
                 "all_gates_required_before_open");
    ASSERT_STREQ(mapping_string(vm_context_contract, "ordinary_lpc_next_blocker"), "");
    ASSERT_EQ(mapping_number(vm_context_contract, "ordinary_lpc_readiness_gate_count"), 13);
    ASSERT_EQ(mapping_number(vm_context_contract, "ordinary_lpc_satisfied_gate_count"), 13);
    ASSERT_EQ(mapping_number(vm_context_contract, "ordinary_lpc_blocked_gate_count"), 0);
    auto* readiness_gates = mapping_array(vm_context_contract, "ordinary_lpc_readiness_gates");
    ASSERT_NE(readiness_gates, nullptr);
    ASSERT_EQ(readiness_gates->size, 13);
    std::unordered_map<std::string, mapping_t*> gates_by_name;
    for (int i = 0; i < readiness_gates->size; i++) {
      ASSERT_EQ(readiness_gates->item[i].type, T_MAPPING);
      auto* gate = readiness_gates->item[i].u.map;
      gates_by_name[mapping_string(gate, "gate")] = gate;
      ASSERT_NE(find_string_in_mapping(gate, "model"), nullptr);
      ASSERT_NE(find_string_in_mapping(gate, "blocker"), nullptr);
      ASSERT_NE(find_string_in_mapping(gate, "next_action"), nullptr);
    }
    auto gate_entry = [&](const std::string& gate_name) -> mapping_t* {
      auto it = gates_by_name.find(gate_name);
      EXPECT_NE(it, gates_by_name.end());
      return it == gates_by_name.end() ? nullptr : it->second;
    };
    ASSERT_EQ(mapping_number(gate_entry("thread_local_vm_context"), "satisfied"), 1);
    ASSERT_EQ(mapping_number(gate_entry("execution_state_contextualized"), "satisfied"), 1);
    ASSERT_EQ(mapping_number(gate_entry("error_state_contextualized"), "satisfied"), 1);
    ASSERT_EQ(mapping_number(gate_entry("eval_stack_owner_local"), "satisfied"), 1);
    ASSERT_STREQ(mapping_string(gate_entry("eval_stack_owner_local"), "model"),
                 "thread_local_owner_execution_stack");
    ASSERT_STREQ(mapping_string(gate_entry("eval_stack_owner_local"), "blocker"), "");
    ASSERT_EQ(mapping_number(gate_entry("control_stack_owner_local"), "satisfied"), 1);
    ASSERT_STREQ(mapping_string(gate_entry("control_stack_owner_local"), "model"),
                 "thread_local_owner_control_stack");
    ASSERT_STREQ(mapping_string(gate_entry("control_stack_owner_local"), "blocker"), "");
    ASSERT_EQ(mapping_number(gate_entry("value_stack_owner_local"), "satisfied"), 1);
    ASSERT_STREQ(mapping_string(gate_entry("value_stack_owner_local"), "model"),
                 "thread_local_owner_value_stack");
    ASSERT_STREQ(mapping_string(gate_entry("value_stack_owner_local"), "blocker"), "");
    ASSERT_EQ(mapping_number(gate_entry("apply_return_owner_local"), "satisfied"), 1);
    ASSERT_STREQ(mapping_string(gate_entry("apply_return_owner_local"), "model"),
                 "thread_local_owner_apply_return");
    ASSERT_STREQ(mapping_string(gate_entry("apply_return_owner_local"), "blocker"), "");
    ASSERT_EQ(mapping_number(gate_entry("object_refs_owner_local"), "satisfied"), 1);
    ASSERT_STREQ(mapping_string(gate_entry("object_refs_owner_local"), "blocker"), "");
    ASSERT_STREQ(mapping_string(gate_entry("object_refs_owner_local"), "next_action"),
                 "keep_cross_owner_object_refs_handle_or_frozen_payload_only");
    ASSERT_EQ(mapping_number(gate_entry("object_store_owner_local_complete"), "satisfied"), 1);
    ASSERT_STREQ(mapping_string(gate_entry("object_store_owner_local_complete"), "blocker"), "");
    ASSERT_STREQ(mapping_string(gate_entry("object_store_owner_local_complete"), "next_action"),
                 "keep_owner_local_store_canonical_without_global_fallback");
    ASSERT_EQ(mapping_number(gate_entry("ordinary_lpc_activation_policy"), "satisfied"), 1);
    ASSERT_STREQ(mapping_string(gate_entry("ordinary_lpc_activation_policy"), "blocker"), "");
    ASSERT_STREQ(mapping_string(gate_entry("ordinary_lpc_activation_policy"), "next_action"),
                 "keep_default_closed_until_dispatch_path_ready");
    ASSERT_EQ(mapping_number(gate_entry("ordinary_lpc_dispatch_path"), "satisfied"), 1);
    ASSERT_STREQ(mapping_string(gate_entry("ordinary_lpc_dispatch_path"), "blocker"), "");
    ASSERT_STREQ(mapping_string(gate_entry("ordinary_lpc_dispatch_path"), "next_action"),
                 "keep_generic_dispatch_explicit_open_and_frozen_result_guarded");

    auto* frozen_payload_contract = mapping_entry(status, "frozen_payload_contract");
    ASSERT_NE(frozen_payload_contract, nullptr);
    ASSERT_EQ(mapping_number(frozen_payload_contract, "contract_version"), 1);
    ASSERT_STREQ(mapping_string(frozen_payload_contract, "validator"), "vm_frozen_value_safe");
    ASSERT_EQ(mapping_number(frozen_payload_contract, "deep_copy"), 1);
    ASSERT_EQ(mapping_number(frozen_payload_contract, "max_depth"), 8);
    ASSERT_EQ(mapping_number(frozen_payload_contract, "mapping_keys_must_be_strings"), 1);
    ASSERT_EQ(mapping_number(frozen_payload_contract, "top_level_owner_payload_must_be_mapping"),
              1);
    ASSERT_EQ(mapping_number(frozen_payload_contract, "object_allowed"), 0);
    ASSERT_EQ(mapping_number(frozen_payload_contract, "function_allowed"), 0);
    ASSERT_EQ(mapping_number(frozen_payload_contract, "buffer_allowed"), 0);
    ASSERT_EQ(mapping_number(frozen_payload_contract, "class_allowed"), 0);
    auto* allowed_types = mapping_array(frozen_payload_contract, "allowed_types");
    ASSERT_NE(allowed_types, nullptr);
    ASSERT_EQ(allowed_types->size, 5);
    auto* rejected_types = mapping_array(frozen_payload_contract, "rejected_types");
    ASSERT_NE(rejected_types, nullptr);
    ASSERT_EQ(rejected_types->size, 4);
    std::unordered_set<std::string> rejected_type_names;
    for (int i = 0; i < rejected_types->size; i++) {
      ASSERT_EQ(rejected_types->item[i].type, T_STRING);
      rejected_type_names.insert(rejected_types->item[i].u.string);
    }
    ASSERT_NE(rejected_type_names.find("object"), rejected_type_names.end());
    ASSERT_NE(rejected_type_names.find("function"), rejected_type_names.end());
    ASSERT_NE(rejected_type_names.find("buffer"), rejected_type_names.end());
    ASSERT_NE(rejected_type_names.find("class"), rejected_type_names.end());
    auto* frozen_paths = mapping_array(frozen_payload_contract, "paths");
    ASSERT_NE(frozen_paths, nullptr);
    ASSERT_EQ(frozen_paths->size, 5);
    std::unordered_map<std::string, mapping_t*> frozen_paths_by_name;
    for (int i = 0; i < frozen_paths->size; i++) {
      ASSERT_EQ(frozen_paths->item[i].type, T_MAPPING);
      auto* path = frozen_paths->item[i].u.map;
      frozen_paths_by_name.emplace(mapping_string(path, "path"), path);
      ASSERT_EQ(mapping_number(path, "uses_shared_validator"), 1);
    }
    auto frozen_path = [&](const char* path_name) -> mapping_t* {
      auto it = frozen_paths_by_name.find(path_name);
      EXPECT_NE(it, frozen_paths_by_name.end());
      return it == frozen_paths_by_name.end() ? nullptr : it->second;
    };
    ASSERT_EQ(mapping_number(frozen_path("owner_send"), "top_level_mapping_required"), 1);
    ASSERT_EQ(mapping_number(frozen_path("owner_send"), "frozen_result_required"), 0);
    ASSERT_STREQ(mapping_string(frozen_path("owner_call_async"), "result_policy"),
                 "frozen_result_required");
    ASSERT_EQ(mapping_number(frozen_path("owner_call_async"), "frozen_result_required"), 1);
    ASSERT_STREQ(mapping_string(frozen_path("owner_publish_snapshot"), "result_policy"),
                 "snapshot_only");
    ASSERT_EQ(mapping_number(frozen_path("worker_snapshot"), "top_level_mapping_required"), 0);
    ASSERT_STREQ(mapping_string(frozen_path("worker_snapshot"), "result_policy"),
                 "owner_future_frozen_result_required");
    ASSERT_EQ(mapping_number(frozen_path("domain_task"), "top_level_mapping_required"), 1);
    ASSERT_STREQ(mapping_string(frozen_path("domain_task"), "input_policy"), "domain_task_payload");
    ASSERT_STREQ(mapping_string(frozen_path("domain_task"), "result_policy"),
                 "owner_future_frozen_result_required");
    ASSERT_EQ(mapping_number(frozen_path("domain_task"), "frozen_result_required"), 1);

    auto* boundary_contract = mapping_entry(status, "owner_executor_boundary_contract");
    ASSERT_NE(boundary_contract, nullptr);
    ASSERT_EQ(mapping_number(boundary_contract, "contract_version"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "boundary_model"), "owner_executor_boundary_v1");
    ASSERT_STREQ(mapping_string(boundary_contract, "class_name"), "OwnerExecutor");
    ASSERT_EQ(mapping_number(boundary_contract, "class_extracted"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "module_extracted"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "compilation_unit_extracted"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "depends_on_owner_cc_internal_state"), 0);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_split_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_runtime_split_model"),
                 "runtime_v4_modules_with_owner_runtime_coordinator");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_v4_hardening_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_benchmark_smoke_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_runtime_benchmark_schema"),
                 "owner_runtime_bench_v1");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_stress_profile_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_runtime_stress_entry"),
                 "tools/owner-runtime-v4-stress.sh");
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_modern_runtime_stress_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "lpc_modern_runtime_stress_entry"),
                 "tools/lpc-modern-runtime-stress.sh");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_layering_guard_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_coordinator_module_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_runtime_store_owner"),
                 "OwnerRuntimeCoordinator");
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_modern_profile_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "lpc_modern_profile_schema"),
                 "lpc_modern_profile_v1");
    ASSERT_STREQ(mapping_string(boundary_contract, "lpc_modern_profile_mode"), "opt_in_pragma");
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_vm_profile_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "lpc_vm_profile_schema"), "lpc_vm_profile_v1");
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_vm_profile_default_recording"), 0);
    ASSERT_STREQ(mapping_string(boundary_contract, "lpc_vm_profile_recording_policy"),
                 "explicit_current_thread_only");
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_vm_benchmark_smoke_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "lpc_vm_benchmark_schema"), "lpc_vm_bench_v1");
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_vm_hot_path_profile_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "lpc_vm_hot_path_profile_model"),
                 "opcode_efun_call_other_function_pointer_parser_mapping_string_v1");
    ASSERT_EQ(mapping_number(boundary_contract, "object_store_benchmark_smoke_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "object_store_benchmark_schema"),
                 "object_store_bench_v1");
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_apply_dispatch_cache_probe_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_opcode_dispatch_profile_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_efun_dispatch_profile_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_call_other_profile_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_function_pointer_profile_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_parser_action_profile_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_mapping_string_profile_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_dispatch_cache_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "lpc_dispatch_cache_model"),
                 "apply_dispatch_thread_local_direct_cache_v1");
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_jit_experiment_default_off"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "modern_lpc_pragma_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "strict_owner_pragma_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "strict_owner_policy"),
                 "strict_owner_owner_safe_payloads_v1");
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_source_encoding_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "lpc_source_encoding_schema"),
                 "lpc_source_encoding_v1");
    ASSERT_STREQ(mapping_string(boundary_contract, "vm_internal_string_encoding"), "utf-8");
    ASSERT_EQ(mapping_number(boundary_contract, "session_encoding_contract_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "gateway_encoding_boundary_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "encoding_audit_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "legacy_lpc_default_closed"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_safe_future_api_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_safe_lpc_api_failure_schema_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_safe_lpc_api_failure_schema"),
                 "owner_safe_lpc_api_failure_v1");
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_safe_lpc_api_return_fields"),
                 "success,ok,code,error,reason,api,trace_id");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_async_api_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_await_poll_adapter_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_await_coroutine_runtime_ready"), 0);
    ASSERT_EQ(mapping_number(boundary_contract, "freeze_snapshot_api_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "freeze_snapshot_model"), "validated_deep_copy");
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_value_object_profile_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "lpc_value_object_model"),
                 "frozen_snapshot_value_object_v1");
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_value_object_live_lifecycle_member"), 0);
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_value_object_cross_owner_payload_safe"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_snapshot_persistence_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_snapshot_persistence_model"),
                 "owner_snapshot_serialized_payload_v1");
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_snapshot_persistence_adapter"),
                 "main_thread_file_adapter");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_snapshot_direct_save_hot_path_audit_ready"),
              1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_commit_api_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_commit_model"),
                 "owner_commit_boundary_record");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_task_manifest_module_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_trace_store_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_future_store_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_scheduler_state_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_metrics_store_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "object_store_owner_fast_path_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "object_store_global_fallback_on_owner_fast_path"),
              0);
    ASSERT_EQ(mapping_number(boundary_contract, "object_handle_capability_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "object_handle_capability_model"),
                 "object_handle_capability_v1");
    ASSERT_STREQ(mapping_string(boundary_contract, "object_handle_permission_intent_default"),
                 "owner_runtime");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_scheduler_backpressure_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_scheduler_backpressure_strategy"),
                 "observe_then_reject_new_tasks");
    ASSERT_GT(mapping_number(boundary_contract, "owner_scheduler_max_owner_queue_depth"), 0);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_scheduler_fairness_guard_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_future_timeout_cancel_drop_cleanup_ready"),
              1);
    ASSERT_EQ(mapping_number(boundary_contract, "dependency_manifest_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "runtime_dependency_contract_version"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "dependency_domains"),
                 "owner_scheduler_state,owner_task_manifest,owner_trace_store,owner_future_store,"
                 "owner_runtime_metrics,task_dispatch,vm_context");
    ASSERT_EQ(mapping_number(boundary_contract, "future_completion_dependency"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_facade_required"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_facade_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_runtime_facade_model"),
                 "owner_executor_runtime_facade_v1");
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_runtime_facade_domains"),
                 "scheduler_state,mailbox_state,future_completion");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_facade_scheduler_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_facade_future_completion_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "compilation_unit_blocker"), "");
    ASSERT_EQ(mapping_number(boundary_contract, "claim_release_boundary_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "budget_boundary_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "thread_context_boundary_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "dispatch_manifest_boundary_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "same_owner_serial_required"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "main_required_tasks_excluded"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "target_handle_messages_main_required"), 0);
    ASSERT_EQ(mapping_number(boundary_contract, "target_handle_messages_executor_safe"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "compute_result_executor_safe"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "executor_callback_task_boundary_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "executor_callback_allowlist_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_callback_admission_unified"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_callback_diagnostics_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_callback_diagnostics_schema"),
                 "owner_callback_diagnostics_v1");
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_callback_failure_code_schema"),
                 "owner_callback_failure_code_v1");
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_callback_drop_reason_schema"),
                 "owner_callback_drop_reason_v1");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_callback_allowlist_complete"), 1);
    ASSERT_STREQ(
        mapping_string(boundary_contract, "owner_callback_supported_kinds"),
        "heartbeat,call_out,async_callback,dns_callback,socket_callback,gateway_command_execute,ed_callback");
    ASSERT_EQ(mapping_number(boundary_contract, "executor_callback_cleanup_main_required"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "executor_callback_main_adapter_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "executor_callback_allowlist_count"), 6);
    ASSERT_STREQ(mapping_string(boundary_contract, "executor_callback_allowlist"),
                 "heartbeat,call_out,async_callback,dns_callback,socket_callback,ed_callback");
    ASSERT_EQ(mapping_number(boundary_contract, "heartbeat_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "heartbeat_owner_executor_task_type"),
                 "heartbeat");
    ASSERT_STREQ(mapping_string(boundary_contract, "heartbeat_owner_executor_route"),
                 "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(boundary_contract, "heartbeat_owner_executor_fallback_route"), "");
    ASSERT_STREQ(mapping_string(boundary_contract, "heartbeat_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_EQ(mapping_number(boundary_contract, "heartbeat_owner_executor_fallback_main_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "heartbeat_current_object_thread_local"), 0);
    ASSERT_EQ(mapping_number(boundary_contract, "callout_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "callout_owner_executor_task_type"), "call_out");
    ASSERT_STREQ(mapping_string(boundary_contract, "callout_owner_executor_route"),
                 "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(boundary_contract, "callout_owner_executor_fallback_route"), "");
    ASSERT_STREQ(mapping_string(boundary_contract, "callout_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_EQ(
        mapping_number(boundary_contract, "callout_owner_executor_expired_handle_detach_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "callout_owner_executor_cleanup_main_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "callout_owner_executor_drop_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "callout_owner_executor_fallback_main_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "async_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "async_owner_executor_task_type"),
                 "async_callback");
    ASSERT_STREQ(mapping_string(boundary_contract, "async_owner_executor_route"),
                 "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(boundary_contract, "async_owner_executor_fallback_route"), "");
    ASSERT_STREQ(mapping_string(boundary_contract, "async_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_STREQ(mapping_string(boundary_contract, "async_owner_executor_result_policy"),
                 "frozen_deep_copy_result");
    ASSERT_EQ(mapping_number(boundary_contract, "async_owner_executor_cleanup_main_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "async_owner_executor_drop_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "dns_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "dns_owner_executor_task_type"), "dns_callback");
    ASSERT_STREQ(mapping_string(boundary_contract, "dns_owner_executor_route"),
                 "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(boundary_contract, "dns_owner_executor_fallback_route"), "");
    ASSERT_STREQ(mapping_string(boundary_contract, "dns_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_STREQ(mapping_string(boundary_contract, "dns_owner_executor_result_policy"),
                 "frozen_deep_copy_result");
    ASSERT_EQ(mapping_number(boundary_contract, "dns_owner_executor_owner_epoch_capture_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "dns_owner_executor_cleanup_main_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "dns_owner_executor_drop_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "socket_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "socket_owner_executor_task_type"),
                 "socket_callback");
    ASSERT_STREQ(mapping_string(boundary_contract, "socket_owner_executor_route"),
                 "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(boundary_contract, "socket_owner_executor_fallback_route"), "");
    ASSERT_STREQ(mapping_string(boundary_contract, "socket_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_STREQ(mapping_string(boundary_contract, "socket_owner_executor_result_policy"),
                 "frozen_deep_copy_args");
    ASSERT_EQ(mapping_number(boundary_contract, "socket_owner_executor_cleanup_main_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "socket_owner_executor_drop_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "socket_release_main_required"), 0);
    ASSERT_EQ(mapping_number(boundary_contract, "socket_release_owner_safe_handshake_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "socket_release_owner_safe_handshake_policy"),
                 "synchronous_release_acquire_owner_epoch_guard");
    ASSERT_EQ(mapping_number(boundary_contract, "socket_release_owner_epoch_guard_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "gateway_command_rejected"), 0);
    ASSERT_EQ(mapping_number(boundary_contract, "gateway_command_executor_activation_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "gateway_command_execute_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "gateway_command_execute_task_type"),
                 "gateway_command_execute");
    ASSERT_STREQ(mapping_string(boundary_contract, "gateway_command_execute_route"),
                 "owner_main_queue_io_adapter");
    ASSERT_STREQ(mapping_string(boundary_contract, "gateway_command_execute_fallback_route"), "");
    ASSERT_STREQ(mapping_string(boundary_contract, "gateway_command_execute_policy"),
                 "main_thread_io_adapter_until_interactive_detached");
    ASSERT_STREQ(mapping_string(boundary_contract, "gateway_command_execute_payload_policy"),
                 "owner_private_command_snapshot");
    ASSERT_EQ(mapping_number(boundary_contract, "gateway_command_execute_reply_queue_main_ready"),
              1);
    ASSERT_EQ(mapping_number(boundary_contract, "gateway_command_execute_stale_drop_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "gateway_command_execute_context_cleanup_ready"),
              1);
    ASSERT_EQ(mapping_number(boundary_contract, "gateway_command_execute_session_revalidate_ready"),
              1);
    ASSERT_EQ(mapping_number(boundary_contract, "ordinary_lpc_default_closed"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "ordinary_lpc_explicit_open_required"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "ordinary_lpc_policy"),
                 "explicit_open_same_owner_only");
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_surface_expanded"), 0);
    ASSERT_EQ(mapping_number(boundary_contract, "registered_owner_task_domains_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_service_shard_registry_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_service_shard_registry_schema"),
                 "owner_service_shard_registry_v1");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_service_shard_domain_count"), 18);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_service_shard_domains"),
                 "readonly,player,room,session,item,economy,combat,mail,reward,world,persistence,"
                 "team,guild,sect,quest,rank,crafting,life_skill");
    ASSERT_EQ(
        mapping_number(boundary_contract, "owner_service_registry_lpc_domain_alignment_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_tick_group_scheduler_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_tick_group_scheduler_schema"),
                 "owner_tick_group_scheduler_v1");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_tick_group_count"), 6);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_tick_groups"),
                 "gateway_command,heartbeat,callout,socket_async,service_tick,diagnostic");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_scheduler_tuning_config_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_scheduler_tuning_config_schema"),
                 "owner_scheduler_tuning_v1");
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_scheduler_tick_group_budget_source"),
                 "owner_service_registry");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_scheduler_priority_groups_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_scheduler_tick_group_backpressure_ready"),
              1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_scheduler_starvation_guard_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "target_owner_message_executor_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "normal_path_main_fallback_count"), 0);
    ASSERT_EQ(mapping_number(boundary_contract, "normal_path_main_fallback_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "main_fallback_policy_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "main_fallback_classification"),
                 "explicit_policy");
    ASSERT_EQ(mapping_number(boundary_contract, "session_fifo_contract_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "gateway_io_adapter_only_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "gateway_io_boundary"),
                 "main_thread_io_adapter");
    ASSERT_EQ(mapping_number(boundary_contract, "gateway_low_overhead_latency_probe_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "gateway_latency_probe_source"),
                 "gateway_status_internal");
    ASSERT_STREQ(
        mapping_string(boundary_contract, "gateway_latency_probe_fields"),
        "receive_decode,receive_payload_copy,receive_enqueue_to_dispatch,receive_apply,"
        "command_enqueue_to_dispatch,command_execute,"
        "receive_main_queue_depth,deferred_main_drain_wait,"
        "reply_enqueue_to_dispatch,reply_execute,output_enqueue_to_dispatch,output_execute,main_drain");
    ASSERT_EQ(mapping_number(boundary_contract, "callback_payload_strict_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_callback_payload_strict_diagnostics_ready"),
              1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_callback_payload_policy_schema"),
                 "owner_callback_payload_policy_v1");
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_callback_payload_policy"),
                 "frozen_payload_or_owner_handle_only");
    ASSERT_STREQ(
        mapping_string(boundary_contract, "owner_callback_failure_codes"),
        "owner_scheduler_backpressure,callback_not_allowlisted,callback_invalid_target,"
        "owner_epoch_mismatch,target_destructed,target_stale,admission_rejected,task_dropped");
    ASSERT_STREQ(
        mapping_string(boundary_contract, "owner_callback_drop_reasons"),
        "none,owner_scheduler_backpressure,callback_not_allowlisted,callback_invalid_target,"
        "owner_epoch_mismatch,target_destructed,target_stale,admission_rejected,task_dropped");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_callback_human_reason_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_callback_failure_reason_schema"),
                 "owner_callback_failure_reason_v1");
    ASSERT_EQ(mapping_number(boundary_contract, "service_shard_executor_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "domain_task_registry_mudlib_aligned"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "keyed_service_shard_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "hot_path_service_owner_single_point"), 0);
    ASSERT_GT(mapping_number(boundary_contract, "hot_path_service_shard_count"), 0);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_service_shard_policy_model"),
                 "keyed_service_shard_for_hot_paths");
    ASSERT_EQ(mapping_number(boundary_contract, "target_owner_message_main_fallback"), 0);
    ASSERT_EQ(mapping_number(boundary_contract, "production_perfect_contract_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "facade_only_runtime_claims"), 0);

    auto* gateway_contract = mapping_entry(status, "gateway_owner_task_contract");
    ASSERT_NE(gateway_contract, nullptr);
    ASSERT_EQ(mapping_number(gateway_contract, "contract_version"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "input_model"),
                 "owner_executor_with_main_fallback");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_payload_model"),
                 "gateway_command_buffer_metadata_v1");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_input_source"),
                 "interactive_text_buffer");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_text_snapshot_policy"),
                 "owner_private_redacted_from_trace");
    ASSERT_EQ(mapping_number(gateway_contract, "command_text_snapshot_ready"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "command_input_callback_state_policy"),
                 "redacted_input_to_get_char_state_v1");
    ASSERT_EQ(mapping_number(gateway_contract, "command_input_callback_snapshot_ready"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "command_input_callback_frame_model"),
                 "owner_command_frame_input_callback_detach_v1");
    ASSERT_EQ(mapping_number(gateway_contract, "command_input_callback_frame_detach_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "command_input_callback_frame_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "command_input_callback_apply_frame_model"),
                 "owner_command_frame_input_callback_apply");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_input_callback_apply_frame_task_type"),
                 "interactive_input_callback");
    ASSERT_EQ(mapping_number(gateway_contract, "command_input_callback_apply_frame_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "command_input_callback_apply_frame_executor_ready"),
              1);
    ASSERT_STREQ(mapping_string(gateway_contract, "command_input_callback_mode_delta_model"),
                 "owner_command_frame_input_callback_mode_delta");
    ASSERT_EQ(mapping_number(gateway_contract, "command_input_callback_mode_delta_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "command_input_callback_mode_delta_executor_ready"),
              1);
    ASSERT_STREQ(mapping_string(gateway_contract, "command_input_callback_blocker"), "");
    ASSERT_STREQ(mapping_string(gateway_contract, "process_input_apply_frame_model"),
                 "owner_command_frame_process_input_apply");
    ASSERT_STREQ(mapping_string(gateway_contract, "process_input_apply_frame_task_type"),
                 "interactive_command_parser");
    ASSERT_EQ(mapping_number(gateway_contract, "process_input_apply_frame_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "process_input_apply_frame_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "process_input_add_action_parser_frame_model"),
                 "owner_command_parser_context_v1");
    ASSERT_EQ(mapping_number(gateway_contract, "process_input_add_action_parser_frame_ready"), 1);
    ASSERT_EQ(
        mapping_number(gateway_contract, "process_input_add_action_parser_frame_executor_ready"),
        1);
    ASSERT_STREQ(mapping_string(gateway_contract, "process_input_add_action_parser_blocker"), "");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_executor_blocker"),
                 "interactive_command_requires_main_thread_io_adapter");
    ASSERT_EQ(mapping_number(gateway_contract, "gateway_command_execute_ready"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "gateway_command_execute_task_type"),
                 "gateway_command_execute");
    ASSERT_STREQ(mapping_string(gateway_contract, "gateway_command_execute_route"),
                 "owner_main_queue_io_adapter");
    ASSERT_STREQ(mapping_string(gateway_contract, "gateway_command_execute_fallback_route"), "");
    ASSERT_STREQ(mapping_string(gateway_contract, "gateway_command_execute_policy"),
                 "main_thread_io_adapter_until_interactive_detached");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_consume_model"),
                 "owner_owned_snapshot_main_thread_consume");
    ASSERT_EQ(mapping_number(gateway_contract, "command_consume_snapshot_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "command_consume_executor_ready"), 0);
    ASSERT_STREQ(mapping_string(gateway_contract, "command_consume_blocker"),
                 "interactive_command_requires_main_thread_io_adapter");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_reply_queue_model"),
                 "main_reply_queue_after_owner_command");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_reply_queue_task_type"),
                 "command_reply");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_reply_queue_task_key"),
                 "prompt_telnet_reschedule_io");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_reply_queue_side_effects"),
                 "prompt_telnet_reschedule_io");
    ASSERT_EQ(mapping_number(gateway_contract, "command_reply_queue_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "command_reply_queue_main_required"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "command_reply_write_prompt_apply_frame_model"),
                 "owner_command_frame_write_prompt_apply");
    ASSERT_STREQ(
        mapping_string(gateway_contract, "command_reply_write_prompt_apply_frame_task_type"),
        "command_reply");
    ASSERT_EQ(mapping_number(gateway_contract, "command_reply_write_prompt_apply_frame_ready"), 1);
    ASSERT_EQ(
        mapping_number(gateway_contract, "command_reply_write_prompt_apply_frame_executor_ready"),
        0);
    ASSERT_STREQ(mapping_string(gateway_contract, "command_mode_delta_model"),
                 "owner_command_frame_mode_delta");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_mode_delta_localecho_restore_boundary"),
                 "main_reply_queue_after_command_consume");
    ASSERT_EQ(mapping_number(gateway_contract, "command_mode_delta_localecho_restore_ready"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "interactive_mode_localecho_restore_model"),
                 "owner_command_frame_localecho_restore");
    ASSERT_STREQ(mapping_string(gateway_contract, "interactive_mode_localecho_restore_task_type"),
                 "interactive_mode_flags");
    ASSERT_EQ(mapping_number(gateway_contract, "interactive_mode_localecho_restore_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "interactive_mode_localecho_restore_executor_ready"),
              1);
    ASSERT_STREQ(mapping_string(gateway_contract, "command_mode_delta_terminal_mode_task_type"),
                 "command_mode_delta");
    ASSERT_STREQ(
        mapping_string(gateway_contract, "command_mode_delta_terminal_mode_task_keys"),
        "get_char_linemode_restore,single_char_escape_linemode,single_char_escape_charmode_restore");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_mode_delta_terminal_mode_boundary"),
                 "main_mode_delta_queue_after_command_consume");
    ASSERT_EQ(mapping_number(gateway_contract, "command_mode_delta_terminal_mode_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "command_mode_delta_ready"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "interactive_mode_mxp_tag_filter_model"),
                 "owner_command_frame_mxp_tag_filter");
    ASSERT_STREQ(mapping_string(gateway_contract, "interactive_mode_mxp_tag_filter_task_type"),
                 "interactive_mode_flags");
    ASSERT_EQ(mapping_number(gateway_contract, "interactive_mode_mxp_tag_filter_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "interactive_mode_mxp_tag_filter_executor_ready"),
              1);
    ASSERT_STREQ(mapping_string(gateway_contract, "interactive_mode_ed_command_model"),
                 "owner_command_frame_ed_command");
    ASSERT_STREQ(mapping_string(gateway_contract, "interactive_mode_ed_command_task_type"),
                 "interactive_mode_flags");
    ASSERT_EQ(mapping_number(gateway_contract, "interactive_mode_ed_command_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "interactive_mode_ed_command_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "raw_input_trace_policy"),
                 "no_raw_command_text_in_trace");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_execution_frame_model"),
                 "gateway_command_execution_frame_v1");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_execution_frame_policy"),
                 "owner_scope_current_interactive_command_giver");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_execution_frame_restore_policy"),
                 "main_thread_vmcontext_scope");
    ASSERT_EQ(mapping_number(gateway_contract, "command_execution_frame_restore_ready"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "command_execution_frame_restore_blocker"), "");
    ASSERT_EQ(mapping_number(gateway_contract, "command_execution_frame_executor_ready"), 0);
    ASSERT_STREQ(mapping_string(gateway_contract, "command_stale_guard"),
                 "owner_epoch_target_handle_guard");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_stale_trace_state"), "main_stale");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_stale_target_status"),
                 "owner_epoch_mismatch");
    ASSERT_EQ(mapping_number(gateway_contract, "gateway_command_execute_stale_drop_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "gateway_command_execute_context_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "gateway_command_execute_session_revalidate_ready"),
              1);
    ASSERT_EQ(mapping_number(gateway_contract, "gateway_command_execute_reply_queue_main_ready"),
              1);
    ASSERT_STREQ(mapping_string(gateway_contract, "command_executor_readiness_gate_model"),
                 "all_gates_required_before_owner_executor");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_executor_next_gate"), "");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_executor_next_blocker"), "");
    ASSERT_GE(mapping_number(status, "thread_gateway_command_guarded"), 0);
    ASSERT_GE(mapping_number(status, "thread_gateway_command_rejected"), 0);
    ASSERT_EQ(mapping_number(gateway_contract, "command_executor_readiness_gate_count"), 7);
    ASSERT_EQ(mapping_number(gateway_contract, "command_executor_satisfied_gate_count"), 7);
    ASSERT_EQ(mapping_number(gateway_contract, "command_executor_blocked_gate_count"), 0);
    ASSERT_STREQ(mapping_string(gateway_contract, "command_side_effect_readiness_gate_model"),
                 "all_side_effect_gates_required_before_activation");
    ASSERT_EQ(mapping_number(gateway_contract, "command_side_effect_readiness_gate_count"), 5);
    ASSERT_EQ(mapping_number(gateway_contract, "command_side_effect_satisfied_gate_count"), 5);
    ASSERT_EQ(mapping_number(gateway_contract, "command_side_effect_blocked_gate_count"), 0);
    ASSERT_EQ(mapping_number(gateway_contract, "command_side_effect_snapshot_gate_count"), 5);
    ASSERT_EQ(mapping_number(gateway_contract, "command_side_effect_snapshot_ready_count"), 5);
    ASSERT_EQ(mapping_number(gateway_contract, "command_side_effect_observability_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "command_side_effect_activation_ready"), 1);
    auto* command_executor_gates =
        mapping_array(gateway_contract, "command_executor_readiness_gates");
    ASSERT_NE(command_executor_gates, nullptr);
    ASSERT_EQ(command_executor_gates->size, 7);
    std::unordered_map<std::string, mapping_t*> command_executor_gates_by_name;
    for (int i = 0; i < command_executor_gates->size; i++) {
      ASSERT_EQ(command_executor_gates->item[i].type, T_MAPPING);
      auto* gate = command_executor_gates->item[i].u.map;
      command_executor_gates_by_name[mapping_string(gate, "gate")] = gate;
      ASSERT_NE(mapping_string(gate, "model"), nullptr);
      ASSERT_NE(mapping_string(gate, "blocker"), nullptr);
      ASSERT_NE(mapping_string(gate, "next_action"), nullptr);
    }
    auto command_executor_gate = [&](const std::string& gate_name) -> mapping_t* {
      auto it = command_executor_gates_by_name.find(gate_name);
      EXPECT_NE(it, command_executor_gates_by_name.end());
      return it == command_executor_gates_by_name.end() ? nullptr : it->second;
    };
    ASSERT_EQ(mapping_number(command_executor_gate("owner_epoch_target_handle_guard"), "satisfied"),
              1);
    ASSERT_EQ(mapping_number(command_executor_gate("owner_owned_command_snapshot"), "satisfied"),
              1);
    ASSERT_STREQ(mapping_string(command_executor_gate("owner_owned_command_snapshot"), "blocker"),
                 "");
    ASSERT_EQ(mapping_number(command_executor_gate("owner_owned_command_consume"), "satisfied"), 1);
    ASSERT_STREQ(mapping_string(command_executor_gate("owner_owned_command_consume"), "blocker"),
                 "");
    ASSERT_EQ(
        mapping_number(command_executor_gate("owner_executor_command_consume_entry"), "satisfied"),
        1);
    ASSERT_STREQ(
        mapping_string(command_executor_gate("owner_executor_command_consume_entry"), "blocker"),
        "");
    ASSERT_EQ(mapping_number(command_executor_gate("owner_executor_frame_restore"), "satisfied"),
              1);
    ASSERT_STREQ(mapping_string(command_executor_gate("owner_executor_frame_restore"), "blocker"),
                 "");
    ASSERT_EQ(mapping_number(command_executor_gate("ordinary_lpc_ready"), "satisfied"), 1);
    ASSERT_STREQ(mapping_string(command_executor_gate("ordinary_lpc_ready"), "blocker"), "");
    ASSERT_EQ(
        mapping_number(command_executor_gate("gateway_command_executor_activation"), "satisfied"),
        1);
    ASSERT_STREQ(
        mapping_string(command_executor_gate("gateway_command_executor_activation"), "blocker"),
        "interactive_command_requires_main_thread_io_adapter");
    auto* command_side_effect_gates =
        mapping_array(gateway_contract, "command_side_effect_readiness_gates");
    ASSERT_NE(command_side_effect_gates, nullptr);
    ASSERT_EQ(command_side_effect_gates->size, 5);
    std::unordered_map<std::string, mapping_t*> command_side_effect_gates_by_name;
    for (int i = 0; i < command_side_effect_gates->size; i++) {
      ASSERT_EQ(command_side_effect_gates->item[i].type, T_MAPPING);
      auto* gate = command_side_effect_gates->item[i].u.map;
      command_side_effect_gates_by_name[mapping_string(gate, "gate")] = gate;
      ASSERT_NE(mapping_string(gate, "model"), nullptr);
      ASSERT_NE(mapping_string(gate, "blocker"), nullptr);
      ASSERT_NE(mapping_string(gate, "next_action"), nullptr);
      ASSERT_NE(mapping_string(gate, "state_owner"), nullptr);
      ASSERT_NE(mapping_string(gate, "migration_boundary"), nullptr);
      ASSERT_NE(mapping_string(gate, "side_effect_class"), nullptr);
      ASSERT_NE(mapping_string(gate, "snapshot_policy"), nullptr);
      ASSERT_EQ(mapping_number(gate, "snapshot_ready"), 1);
      ASSERT_EQ(mapping_number(gate, "state_redacted"), 1);
      ASSERT_GE(mapping_number(gate, "blocks_activation"), 0);
    }
    auto command_side_effect_gate = [&](const std::string& gate_name) -> mapping_t* {
      auto it = command_side_effect_gates_by_name.find(gate_name);
      EXPECT_NE(it, command_side_effect_gates_by_name.end());
      return it == command_side_effect_gates_by_name.end() ? nullptr : it->second;
    };
    ASSERT_EQ(mapping_number(command_side_effect_gate("interactive_buffer_consume"), "satisfied"),
              1);
    ASSERT_EQ(
        mapping_number(command_side_effect_gate("interactive_buffer_consume"), "blocks_activation"),
        0);
    ASSERT_STREQ(mapping_string(command_side_effect_gate("interactive_buffer_consume"), "blocker"),
                 "");
    ASSERT_STREQ(
        mapping_string(command_side_effect_gate("interactive_buffer_consume"), "state_owner"),
        "owner_command_snapshot");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("interactive_buffer_consume"),
                                "migration_boundary"),
                 "main_thread_consume_before_executor_activation");
    ASSERT_STREQ(
        mapping_string(command_side_effect_gate("interactive_buffer_consume"), "side_effect_class"),
        "input_buffer_consume");
    ASSERT_STREQ(
        mapping_string(command_side_effect_gate("interactive_buffer_consume"), "snapshot_policy"),
        "owner_private_command_text_snapshot_v1");
    ASSERT_EQ(mapping_number(command_side_effect_gate("input_to_get_char_state"), "satisfied"), 1);
    ASSERT_EQ(
        mapping_number(command_side_effect_gate("input_to_get_char_state"), "blocks_activation"),
        0);
    ASSERT_STREQ(mapping_string(command_side_effect_gate("input_to_get_char_state"), "blocker"),
                 "");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("input_to_get_char_state"), "state_owner"),
                 "owner_command_frame");
    ASSERT_STREQ(
        mapping_string(command_side_effect_gate("input_to_get_char_state"), "migration_boundary"),
        "owner_command_frame_input_callback_executor");
    ASSERT_STREQ(
        mapping_string(command_side_effect_gate("input_to_get_char_state"), "side_effect_class"),
        "input_callback_state");
    ASSERT_STREQ(
        mapping_string(command_side_effect_gate("input_to_get_char_state"), "snapshot_policy"),
        "redacted_input_to_get_char_state_v1");
    ASSERT_EQ(
        mapping_number(command_side_effect_gate("process_input_add_action_parser"), "satisfied"),
        1);
    ASSERT_EQ(mapping_number(command_side_effect_gate("process_input_add_action_parser"),
                             "blocks_activation"),
              0);
    ASSERT_STREQ(
        mapping_string(command_side_effect_gate("process_input_add_action_parser"), "blocker"), "");
    ASSERT_STREQ(
        mapping_string(command_side_effect_gate("process_input_add_action_parser"), "state_owner"),
        "owner_command_frame");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("process_input_add_action_parser"),
                                "migration_boundary"),
                 "owner_command_parser_context_executor");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("process_input_add_action_parser"),
                                "side_effect_class"),
                 "parser_command_giver_state");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("process_input_add_action_parser"),
                                "snapshot_policy"),
                 "redacted_process_input_add_action_parser_state_v1");
    ASSERT_EQ(mapping_number(command_side_effect_gate("prompt_telnet_reschedule_io"), "satisfied"),
              1);
    ASSERT_EQ(mapping_number(command_side_effect_gate("prompt_telnet_reschedule_io"),
                             "blocks_activation"),
              0);
    ASSERT_STREQ(mapping_string(command_side_effect_gate("prompt_telnet_reschedule_io"), "blocker"),
                 "");
    ASSERT_STREQ(
        mapping_string(command_side_effect_gate("prompt_telnet_reschedule_io"), "state_owner"),
        "main_reply_queue_and_network_io");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("prompt_telnet_reschedule_io"),
                                "migration_boundary"),
                 "main_reply_queue_after_owner_command");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("prompt_telnet_reschedule_io"),
                                "side_effect_class"),
                 "prompt_telnet_reschedule_io");
    ASSERT_STREQ(
        mapping_string(command_side_effect_gate("prompt_telnet_reschedule_io"), "snapshot_policy"),
        "redacted_prompt_telnet_reschedule_io_v1");
    ASSERT_EQ(mapping_number(command_side_effect_gate("interactive_mode_flags"), "satisfied"), 1);
    ASSERT_EQ(
        mapping_number(command_side_effect_gate("interactive_mode_flags"), "blocks_activation"), 0);
    ASSERT_STREQ(mapping_string(command_side_effect_gate("interactive_mode_flags"), "blocker"), "");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("interactive_mode_flags"), "state_owner"),
                 "owner_command_frame");
    ASSERT_STREQ(
        mapping_string(command_side_effect_gate("interactive_mode_flags"), "migration_boundary"),
        "owner_command_frame_mode_delta_executor");
    ASSERT_STREQ(
        mapping_string(command_side_effect_gate("interactive_mode_flags"), "side_effect_class"),
        "echo_mxp_ed_mode_flags");
    ASSERT_STREQ(
        mapping_string(command_side_effect_gate("interactive_mode_flags"), "snapshot_policy"),
        "redacted_interactive_mode_flags_v1");
    ASSERT_EQ(mapping_number(gateway_contract, "ordinary_lpc_ready_required"), 0);
    ASSERT_EQ(mapping_number(gateway_contract, "main_required"), 0);
    ASSERT_EQ(mapping_number(gateway_contract, "fallback_main_required"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "next_blocker"),
                 "current_evidence_not_bound_to_this_binary");
    ASSERT_STREQ(mapping_string(gateway_contract, "next_blocker_chain"),
                 "production_gate_evidence_required");
    auto* gateway_tasks = mapping_array(gateway_contract, "tasks");
    ASSERT_NE(gateway_tasks, nullptr);
    ASSERT_EQ(gateway_tasks->size, 4);
    std::unordered_map<std::string, mapping_t*> gateway_tasks_by_key;
    for (int i = 0; i < gateway_tasks->size; i++) {
      ASSERT_EQ(gateway_tasks->item[i].type, T_MAPPING);
      auto* task = gateway_tasks->item[i].u.map;
      auto task_key = std::string(mapping_string(task, "task_key"));
      gateway_tasks_by_key.emplace(task_key, task);
      ASSERT_STREQ(mapping_string(task, "task_type"), "gateway");
      if (task_key == "process_user_command") {
        ASSERT_EQ(mapping_number(task, "main_required"), 1);
        ASSERT_EQ(mapping_number(task, "executor_safe"), 0);
      } else {
        ASSERT_EQ(mapping_number(task, "main_required"), 1);
        ASSERT_EQ(mapping_number(task, "executor_safe"), 0);
      }
      ASSERT_EQ(mapping_number(task, "requires_owner_scope"), 1);
      ASSERT_EQ(mapping_number(task, "requires_current_interactive"), 1);
      ASSERT_EQ(mapping_number(task, "requires_command_giver"), 1);
      ASSERT_EQ(mapping_number(task, "ordinary_lpc_ready_required"), 0);
      ASSERT_EQ(mapping_number(task, "command_serial_per_owner"), 1);
      ASSERT_NE(mapping_string(task, "payload_key"), nullptr);
      ASSERT_NE(mapping_string(task, "input_payload_policy"), nullptr);
      ASSERT_NE(mapping_string(task, "command_consume_model"), nullptr);
      ASSERT_GE(mapping_number(task, "command_consume_snapshot_ready"), 0);
      ASSERT_LE(mapping_number(task, "command_consume_snapshot_ready"), 1);
      ASSERT_GE(mapping_number(task, "command_consume_executor_ready"), 0);
      ASSERT_LE(mapping_number(task, "command_consume_executor_ready"), 1);
      ASSERT_NE(mapping_string(task, "command_consume_blocker"), nullptr);
      ASSERT_NE(mapping_string(task, "execution_frame_model"), nullptr);
      ASSERT_NE(mapping_string(task, "execution_frame_policy"), nullptr);
      ASSERT_NE(mapping_string(task, "execution_frame_restore_policy"), nullptr);
      ASSERT_GE(mapping_number(task, "execution_frame_restore_ready"), 0);
      ASSERT_LE(mapping_number(task, "execution_frame_restore_ready"), 1);
      ASSERT_NE(mapping_string(task, "execution_frame_restore_blocker"), nullptr);
      ASSERT_GE(mapping_number(task, "execution_frame_executor_ready"), 0);
      ASSERT_LE(mapping_number(task, "execution_frame_executor_ready"), 1);
    }
    auto gateway_task = [&](const char* task_key) -> mapping_t* {
      auto it = gateway_tasks_by_key.find(task_key);
      EXPECT_NE(it, gateway_tasks_by_key.end());
      return it == gateway_tasks_by_key.end() ? nullptr : it->second;
    };
    auto assert_gateway_task = [&](const char* task_key, const char* executor_mode,
                                   const char* route, long requires_main_queue,
                                   const char* owner_scope_model, const char* stale_policy) {
      auto* task = gateway_task(task_key);
      ASSERT_NE(task, nullptr);
      ASSERT_STREQ(mapping_string(task, "executor_mode"), executor_mode);
      ASSERT_STREQ(mapping_string(task, "route"), route);
      ASSERT_EQ(mapping_number(task, "requires_owner_main_queue"), requires_main_queue);
      ASSERT_STREQ(mapping_string(task, "owner_scope_model"), owner_scope_model);
      ASSERT_STREQ(mapping_string(task, "stale_policy"), stale_policy);
    };
    assert_gateway_task("gateway_receive", "main_required", "owner_main_queue", 1,
                        "owner_scope_and_current_interactive", "owner_epoch_target_guard");
    assert_gateway_task("process_user_command", "main_required", "owner_main_queue", 1,
                        "owner_scope_current_interactive_command_giver",
                        "owner_epoch_target_guard");
    ASSERT_STREQ(mapping_string(gateway_task("process_user_command"), "payload_key"),
                 "gateway_command_input");
    ASSERT_STREQ(mapping_string(gateway_task("process_user_command"), "input_payload_policy"),
                 "buffer_metadata_no_raw_command_text");
    ASSERT_STREQ(mapping_string(gateway_task("process_user_command"), "command_consume_model"),
                 "owner_owned_snapshot_main_thread_consume");
    ASSERT_EQ(
        mapping_number(gateway_task("process_user_command"), "command_consume_snapshot_ready"), 1);
    ASSERT_EQ(
        mapping_number(gateway_task("process_user_command"), "command_consume_executor_ready"), 0);
    ASSERT_STREQ(mapping_string(gateway_task("process_user_command"), "command_consume_blocker"),
                 "interactive_command_requires_main_thread_io_adapter");
    ASSERT_STREQ(mapping_string(gateway_task("process_user_command"), "execution_frame_model"),
                 "gateway_command_execution_frame_v1");
    ASSERT_STREQ(mapping_string(gateway_task("process_user_command"), "execution_frame_policy"),
                 "owner_scope_current_interactive_command_giver");
    ASSERT_STREQ(
        mapping_string(gateway_task("process_user_command"), "execution_frame_restore_policy"),
        "main_thread_vmcontext_scope");
    ASSERT_EQ(mapping_number(gateway_task("process_user_command"), "execution_frame_restore_ready"),
              1);
    ASSERT_STREQ(
        mapping_string(gateway_task("process_user_command"), "execution_frame_restore_blocker"),
        "");
    ASSERT_EQ(
        mapping_number(gateway_task("process_user_command"), "execution_frame_executor_ready"), 0);
    ASSERT_EQ(mapping_number(gateway_task("process_user_command"), "requires_target_handle"), 1);
    ASSERT_EQ(mapping_number(gateway_task("process_user_command"), "requires_frozen_payload"), 1);
    assert_gateway_task("gateway_logon", "main_required", "direct_main_owner_scope", 0,
                        "owner_scope_and_current_interactive", "session_owner_resolve_after_exec");
    assert_gateway_task("gateway_disconnected", "main_required", "direct_main_owner_scope", 0,
                        "owner_scope_and_current_interactive", "session_owner_resolve_after_exec");

    auto* contract = mapping_entry(status, "executor_task_contract");
    ASSERT_NE(contract, nullptr);
    auto* lpc_contracts = mapping_array(status, "executor_lpc_task_contracts");
    ASSERT_NE(lpc_contracts, nullptr);
    ASSERT_EQ(lpc_contracts->size, 18);
    ASSERT_EQ(lpc_contracts->item[0].type, T_MAPPING);
    auto* readonly_contract = lpc_contracts->item[0].u.map;
    ASSERT_STREQ(mapping_string(readonly_contract, "method"), "owner_task_readonly");
    ASSERT_STREQ(mapping_string(readonly_contract, "executor_mode"), "executor_safe_allowlist");
    ASSERT_STREQ(mapping_string(readonly_contract, "route"), "owner_executor");
    ASSERT_STREQ(mapping_string(readonly_contract, "result_policy"), "frozen_result_required");
    ASSERT_EQ(mapping_number(readonly_contract, "executor_safe"), 1);
    ASSERT_EQ(mapping_number(readonly_contract, "main_required"), 0);
    ASSERT_EQ(mapping_number(readonly_contract, "rejected"), 0);
    ASSERT_EQ(mapping_number(readonly_contract, "requires_target"), 1);
    ASSERT_EQ(mapping_number(readonly_contract, "requires_owner_thread"), 1);
    ASSERT_EQ(mapping_number(readonly_contract, "requires_owner_message_completion"), 1);
    ASSERT_EQ(mapping_number(readonly_contract, "frozen_result_required"), 1);
    ASSERT_EQ(mapping_number(readonly_contract, "direct_cross_owner_write"), 0);
    bool found_top_level_player_domain = false;
    bool found_top_level_economy_domain = false;
    for (int i = 0; i < lpc_contracts->size; i++) {
      ASSERT_EQ(lpc_contracts->item[i].type, T_MAPPING);
      auto method = std::string(mapping_string(lpc_contracts->item[i].u.map, "method"));
      found_top_level_player_domain =
          found_top_level_player_domain || method == "owner_task_player";
      found_top_level_economy_domain =
          found_top_level_economy_domain || method == "owner_task_economy";
    }
    ASSERT_TRUE(found_top_level_player_domain);
    ASSERT_TRUE(found_top_level_economy_domain);

    auto* dispatch_contracts = mapping_array(status, "executor_task_dispatch_contracts");
    ASSERT_NE(dispatch_contracts, nullptr);
    ASSERT_EQ(dispatch_contracts->size, 20);
    std::unordered_map<std::string, mapping_t*> dispatch_by_type;
    for (int i = 0; i < dispatch_contracts->size; i++) {
      ASSERT_EQ(dispatch_contracts->item[i].type, T_MAPPING);
      auto* entry = dispatch_contracts->item[i].u.map;
      auto task_type = std::string(mapping_string(entry, "task_type"));
      dispatch_by_type.emplace(task_type, entry);
      auto callback_main_adapter = (task_type == "heartbeat" || task_type == "call_out" ||
                                    task_type == "async_callback" || task_type == "dns_callback" ||
                                    task_type == "socket_callback" || task_type == "ed_callback");
      ASSERT_EQ(mapping_number(entry, "requires_owner_mailbox"), callback_main_adapter ? 0 : 1);
      ASSERT_EQ(mapping_number(entry, "requires_owner_main_queue"),
                (task_type == "gateway_command_execute" || callback_main_adapter) ? 1 : 0);
      ASSERT_EQ(mapping_number(entry, "manifest_version"), 2);
      ASSERT_STREQ(mapping_string(entry, "manifest_schema"), "owner_task_manifest_v2");
      ASSERT_STREQ(mapping_string(entry, "admission_policy"),
                   "owner_epoch_payload_allowlist_deadline_guard");
      ASSERT_STREQ(mapping_string(entry, "trace_schema"), "owner_executor_trace_v2");
      ASSERT_EQ(mapping_number(entry, "deadline_required"), 0);
      ASSERT_EQ(mapping_number(entry, "ordinary_lpc_default_closed"), 1);
      ASSERT_NE(mapping_string(entry, "payload_policy"), nullptr);
      ASSERT_NE(mapping_string(entry, "cleanup_policy"), nullptr);
      ASSERT_NE(mapping_string(entry, "reply_future_policy"), nullptr);
      ASSERT_NE(mapping_string(entry, "tick_group"), nullptr);
      ASSERT_GT(mapping_number(entry, "scheduler_priority"), 0);
      ASSERT_GT(mapping_number(entry, "scheduler_budget"), 0);
      ASSERT_GT(mapping_number(entry, "scheduler_max_queue_depth"), 0);
      ASSERT_STREQ(mapping_string(entry, "backpressure_policy"), "observe_then_reject_new_tasks");
    }
    auto dispatch_entry = [&](const char* task_type) -> mapping_t* {
      auto it = dispatch_by_type.find(task_type);
      EXPECT_NE(it, dispatch_by_type.end());
      return it == dispatch_by_type.end() ? nullptr : it->second;
    };
    auto assert_dispatch = [&](const char* task_type, const char* contract_key,
                               const char* dispatch_kind, const char* executor_mode,
                               long executor_runnable, long executor_safe, long rejected,
                               const char* route = "owner_executor", long main_required = 0) {
      auto* entry = dispatch_entry(task_type);
      ASSERT_NE(entry, nullptr);
      ASSERT_STREQ(mapping_string(entry, "contract_key"), contract_key);
      ASSERT_STREQ(mapping_string(entry, "dispatch_kind"), dispatch_kind);
      ASSERT_STREQ(mapping_string(entry, "task_kind"), dispatch_kind);
      ASSERT_STREQ(mapping_string(entry, "executor_mode"), executor_mode);
      ASSERT_STREQ(mapping_string(entry, "route"), route);
      ASSERT_EQ(mapping_number(entry, "executor_runnable"), executor_runnable);
      ASSERT_EQ(mapping_number(entry, "executor_safe"), executor_safe);
      ASSERT_EQ(mapping_number(entry, "main_required"), main_required);
      ASSERT_EQ(mapping_number(entry, "rejected"), rejected);
    };
    assert_dispatch("executor_probe", "executor_probe", "executor_probe", "executor_safe", 1, 1, 0);
    assert_dispatch("lpc_probe", "lpc_probe", "lpc_probe", "executor_safe", 1, 1, 0);
    assert_dispatch("lpc_canary", "lpc_canary", "lpc_canary", "executor_safe", 1, 1, 0);
    assert_dispatch("lpc_task", "lpc_task_allowlist", "lpc_task", "executor_safe_allowlist", 1, 1,
                    0);
    assert_dispatch("ordinary_lpc", "ordinary_lpc_dispatch", "ordinary_lpc",
                    "executor_safe_explicit_open", 1, 1, 0);
    assert_dispatch("owner_message", "owner_message_mailbox", "owner_message", "executor_safe", 1,
                    1, 0);
    assert_dispatch("command_consume", "owner_executor_command_consumer", "command_consume",
                    "executor_safe", 1, 1, 0);
    assert_dispatch("command_frame_restore", "owner_executor_command_frame_restore",
                    "command_frame_restore", "executor_safe", 1, 1, 0);
    assert_dispatch("gateway_command", "gateway_command_executor_activation", "gateway_command",
                    "executor_safe", 1, 1, 0);
    assert_dispatch("heartbeat", "owner_executor_callback", "executor_callback",
                    "main_required_callback", 0, 0, 0, "owner_main_queue_callback_adapter", 1);
    assert_dispatch("call_out", "owner_executor_callback", "executor_callback",
                    "main_required_callback", 0, 0, 0, "owner_main_queue_callback_adapter", 1);
    assert_dispatch("async_callback", "owner_executor_callback", "executor_callback",
                    "main_required_callback", 0, 0, 0, "owner_main_queue_callback_adapter", 1);
    assert_dispatch("dns_callback", "owner_executor_callback", "executor_callback",
                    "main_required_callback", 0, 0, 0, "owner_main_queue_callback_adapter", 1);
    assert_dispatch("socket_callback", "owner_executor_callback", "executor_callback",
                    "main_required_callback", 0, 0, 0, "owner_main_queue_callback_adapter", 1);
    assert_dispatch("gateway_command_execute", "gateway_command_main_queue", "main_thread",
                    "main_required", 1, 0, 0, "owner_main_queue", 1);
    assert_dispatch("ed_callback", "owner_executor_callback", "executor_callback",
                    "main_required_callback", 0, 0, 0, "owner_main_queue_callback_adapter", 1);
    assert_dispatch("room_output_projection", "room_output_projection", "executor_callback",
                    "executor_safe", 1, 1, 0);
    assert_dispatch("compute_result", "compute_result", "compute_result", "executor_safe", 1, 1, 0);
    assert_dispatch("lpc", "lpc", "reject_lpc", "rejected", 1, 0, 1);
    assert_dispatch("owner_state", "owner_state", "guard_owner_state", "rejected", 1, 0, 1);
    ASSERT_STREQ(mapping_string(dispatch_entry("gateway_command_execute"), "tick_group"),
                 "gateway_command");
    ASSERT_STREQ(mapping_string(dispatch_entry("heartbeat"), "tick_group"), "heartbeat");
    ASSERT_STREQ(mapping_string(dispatch_entry("call_out"), "tick_group"), "callout");
    ASSERT_STREQ(mapping_string(dispatch_entry("socket_callback"), "tick_group"), "socket_async");
    ASSERT_STREQ(mapping_string(dispatch_entry("executor_probe"), "tick_group"), "diagnostic");

    auto* compute = mapping_entry(contract, "compute_result");
    ASSERT_STREQ(mapping_string(compute, "executor_mode"), "executor_safe");
    ASSERT_STREQ(mapping_string(compute, "route"), "owner_executor");
    ASSERT_EQ(mapping_number(compute, "executor_safe"), 1);
    ASSERT_EQ(mapping_number(compute, "main_required"), 0);
    ASSERT_EQ(mapping_number(compute, "rejected"), 0);

    auto* command_consume = mapping_entry(contract, "owner_executor_command_consumer");
    ASSERT_STREQ(mapping_string(command_consume, "executor_mode"), "executor_safe");
    ASSERT_STREQ(mapping_string(command_consume, "route"), "owner_executor");
    ASSERT_EQ(mapping_number(command_consume, "executor_safe"), 1);
    ASSERT_EQ(mapping_number(command_consume, "main_required"), 0);
    ASSERT_EQ(mapping_number(command_consume, "rejected"), 0);

    auto* command_frame_restore = mapping_entry(contract, "owner_executor_command_frame_restore");
    ASSERT_STREQ(mapping_string(command_frame_restore, "executor_mode"), "executor_safe");
    ASSERT_STREQ(mapping_string(command_frame_restore, "route"), "owner_executor");
    ASSERT_EQ(mapping_number(command_frame_restore, "executor_safe"), 1);
    ASSERT_EQ(mapping_number(command_frame_restore, "main_required"), 0);
    ASSERT_EQ(mapping_number(command_frame_restore, "rejected"), 0);

    auto* gateway_command = mapping_entry(contract, "gateway_command_executor_activation");
    ASSERT_STREQ(mapping_string(gateway_command, "executor_mode"), "executor_safe");
    ASSERT_STREQ(mapping_string(gateway_command, "route"), "owner_executor");
    ASSERT_EQ(mapping_number(gateway_command, "executor_safe"), 1);
    ASSERT_EQ(mapping_number(gateway_command, "main_required"), 0);
    ASSERT_EQ(mapping_number(gateway_command, "rejected"), 0);
    ASSERT_EQ(mapping_number(gateway_command, "side_effect_snapshot_gate_count"), 5);
    ASSERT_EQ(mapping_number(gateway_command, "side_effect_snapshot_ready_count"), 5);
    ASSERT_EQ(mapping_number(gateway_command, "side_effect_observability_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_command, "side_effect_activation_ready"), 1);
    ASSERT_STREQ(mapping_string(gateway_command, "activation_blocker"),
                 "interactive_command_requires_main_thread_io_adapter");

    auto* callback_allowlist = mapping_entry(contract, "owner_executor_callback_allowlist");
    ASSERT_STREQ(mapping_string(callback_allowlist, "executor_mode"), "main_required_callback");
    ASSERT_STREQ(mapping_string(callback_allowlist, "route"), "owner_main_queue_callback_adapter");
    ASSERT_EQ(mapping_number(callback_allowlist, "executor_safe"), 0);
    ASSERT_EQ(mapping_number(callback_allowlist, "main_required"), 1);
    ASSERT_EQ(mapping_number(callback_allowlist, "rejected"), 0);
    auto* callback_contracts = mapping_array(callback_allowlist, "contracts");
    ASSERT_NE(callback_contracts, nullptr);
    ASSERT_EQ(callback_contracts->size, 6);

    auto* mailbox_message = mapping_entry(contract, "owner_message_mailbox");
    ASSERT_EQ(mapping_number(mailbox_message, "executor_safe"), 1);
    ASSERT_EQ(mapping_number(mailbox_message, "main_required"), 0);
    ASSERT_EQ(mapping_number(mailbox_message, "requires_owner_mailbox"), 1);
    ASSERT_EQ(mapping_number(mailbox_message, "requires_owner_main_queue"), 0);
    ASSERT_STREQ(mapping_string(mailbox_message, "route"), "owner_executor");

    auto* target_message = mapping_entry(contract, "owner_message_target_handle");
    ASSERT_STREQ(mapping_string(target_message, "executor_mode"), "executor_safe");
    ASSERT_STREQ(mapping_string(target_message, "route"), "owner_executor");
    ASSERT_EQ(mapping_number(target_message, "executor_safe"), 1);
    ASSERT_EQ(mapping_number(target_message, "main_required"), 0);
    ASSERT_EQ(mapping_number(target_message, "requires_owner_mailbox"), 1);
    ASSERT_EQ(mapping_number(target_message, "requires_owner_main_queue"), 0);
    ASSERT_EQ(mapping_number(target_message, "rejected"), 0);

    auto* allowlist = mapping_entry(contract, "lpc_task_allowlist");
    ASSERT_STREQ(mapping_string(allowlist, "executor_mode"), "executor_safe_allowlist");
    ASSERT_EQ(mapping_number(allowlist, "executor_safe"), 1);
    ASSERT_EQ(mapping_number(allowlist, "rejected"), 0);
    auto* nested_lpc_contracts = mapping_array(allowlist, "contracts");
    ASSERT_NE(nested_lpc_contracts, nullptr);
    ASSERT_EQ(nested_lpc_contracts->size, 18);
    ASSERT_EQ(nested_lpc_contracts->item[0].type, T_MAPPING);
    ASSERT_STREQ(mapping_string(nested_lpc_contracts->item[0].u.map, "method"),
                 "owner_task_readonly");
    bool found_player_domain = false;
    bool found_economy_domain = false;
    for (int i = 0; i < nested_lpc_contracts->size; i++) {
      ASSERT_EQ(nested_lpc_contracts->item[i].type, T_MAPPING);
      auto method = std::string(mapping_string(nested_lpc_contracts->item[i].u.map, "method"));
      found_player_domain = found_player_domain || method == "owner_task_player";
      found_economy_domain = found_economy_domain || method == "owner_task_economy";
    }
    ASSERT_TRUE(found_player_domain);
    ASSERT_TRUE(found_economy_domain);

    auto* ordinary_lpc = mapping_entry(contract, "ordinary_lpc");
    ASSERT_STREQ(mapping_string(ordinary_lpc, "executor_mode"), "executor_safe_explicit_open");
    ASSERT_STREQ(mapping_string(ordinary_lpc, "route"), "owner_executor");
    ASSERT_EQ(mapping_number(ordinary_lpc, "executor_safe"), 1);
    ASSERT_EQ(mapping_number(ordinary_lpc, "main_required"), 0);
    ASSERT_EQ(mapping_number(ordinary_lpc, "rejected"), 0);
    ASSERT_STREQ(mapping_string(ordinary_lpc, "dispatch_model"), "generic_owner_lpc_dispatch");
    ASSERT_STREQ(mapping_string(ordinary_lpc, "activation_policy"), "default_closed_explicit_open");
    ASSERT_EQ(mapping_number(ordinary_lpc, "default_closed"), 1);
    ASSERT_EQ(mapping_number(ordinary_lpc, "explicit_open_required"), 1);
    ASSERT_EQ(mapping_number(ordinary_lpc, "requires_target"), 1);
    ASSERT_EQ(mapping_number(ordinary_lpc, "requires_owner_thread"), 1);
    ASSERT_EQ(mapping_number(ordinary_lpc, "requires_owner_message_completion"), 1);
    ASSERT_EQ(mapping_number(ordinary_lpc, "frozen_result_required"), 1);
    ASSERT_EQ(mapping_number(ordinary_lpc, "direct_cross_owner_write"), 0);

    auto* legacy_lpc = mapping_entry(contract, "lpc");
    ASSERT_STREQ(mapping_string(legacy_lpc, "executor_mode"), "rejected");
    ASSERT_STREQ(mapping_string(legacy_lpc, "route"), "owner_executor");
    ASSERT_EQ(mapping_number(legacy_lpc, "executor_safe"), 0);
    ASSERT_EQ(mapping_number(legacy_lpc, "main_required"), 0);
    ASSERT_EQ(mapping_number(legacy_lpc, "rejected"), 1);
  };

  auto* runtime_status = vm_owner_runtime_status();
  assert_contract(runtime_status);
  ASSERT_EQ(mapping_number(runtime_status, "ordinary_lpc_default_closed"), 1);
  free_mapping(runtime_status);

  auto* thread_status = vm_owner_thread_status();
  assert_contract(thread_status);
  ASSERT_EQ(mapping_number(thread_status, "ordinary_lpc_default_closed"), 1);
  free_mapping(thread_status);

  auto* before_future_status = vm_owner_runtime_status();
  auto before_cancelled = mapping_number(before_future_status, "owner_executor_future_cancelled");
  auto before_timeout = mapping_number(before_future_status, "owner_executor_future_timeout");
  auto before_failed = mapping_number(before_future_status, "futures_failed");
  free_mapping(before_future_status);

  auto cancel_future_id = vm_owner_register_compute_future("owner/test/future-cancel", 9101,
                                                           "manifest_cancel", "v2_cancel");
  auto timeout_future_id = vm_owner_register_compute_future("owner/test/future-timeout", 9102,
                                                            "manifest_timeout", "v2_timeout");
  ASSERT_GT(cancel_future_id, 0u);
  ASSERT_GT(timeout_future_id, 0u);

  auto* pending_cancel = vm_owner_future_poll(cancel_future_id);
  ASSERT_STREQ(mapping_string(pending_cancel, "state"), "pending");
  ASSERT_GT(mapping_number(pending_cancel, "created_at_ms"), 0);
  ASSERT_STREQ(mapping_string(pending_cancel, "future_policy"), "owner_future_timeout_cancel_v2");
  ASSERT_EQ(mapping_number(pending_cancel, "cancelled"), 0);
  ASSERT_EQ(mapping_number(pending_cancel, "timed_out"), 0);
  free_mapping(pending_cancel);

  auto* cancelled = vm_owner_future_cancel(cancel_future_id, "unit cancel");
  ASSERT_STREQ(mapping_string(cancelled, "state"), "failed");
  ASSERT_STREQ(mapping_string(cancelled, "error"), "unit cancel");
  ASSERT_EQ(mapping_number(cancelled, "cancelled"), 1);
  ASSERT_EQ(mapping_number(cancelled, "timed_out"), 0);
  ASSERT_EQ(mapping_number(cancelled, "terminal_cleanup_required"), 0);
  ASSERT_STREQ(mapping_string(cancelled, "future_policy"), "owner_future_timeout_cancel_v2");
  free_mapping(cancelled);

  auto* timed_out = vm_owner_future_timeout(timeout_future_id, "unit timeout");
  ASSERT_STREQ(mapping_string(timed_out, "state"), "failed");
  ASSERT_STREQ(mapping_string(timed_out, "error"), "unit timeout");
  ASSERT_EQ(mapping_number(timed_out, "cancelled"), 0);
  ASSERT_EQ(mapping_number(timed_out, "timed_out"), 1);
  ASSERT_EQ(mapping_number(timed_out, "terminal_cleanup_required"), 0);
  ASSERT_STREQ(mapping_string(timed_out, "future_policy"), "owner_future_timeout_cancel_v2");
  free_mapping(timed_out);

  auto* after_future_status = vm_owner_runtime_status();
  ASSERT_GE(mapping_number(after_future_status, "owner_executor_future_cancelled"),
            before_cancelled + 1);
  ASSERT_GE(mapping_number(after_future_status, "owner_executor_future_timeout"),
            before_timeout + 1);
  ASSERT_GE(mapping_number(after_future_status, "futures_failed"), before_failed + 2);
  ASSERT_EQ(mapping_number(after_future_status, "owner_executor_future_pending_backlog"),
            mapping_number(after_future_status, "pending_futures"));
  free_mapping(after_future_status);
}

TEST_F(DriverTest, TestOwnerRuntimeLayeringGuardKeepsStoresOutOfOwnerCc) {
  const auto owner_cc = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto coordinator_cc =
      read_source_file_for_test("../src/vm/internal/owner_runtime_coordinator.cc");
  ASSERT_FALSE(owner_cc.empty());
  ASSERT_FALSE(coordinator_cc.empty());

  const std::vector<std::string> forbidden_owner_cc_storage = {
      "OwnerRuntimeMetrics owner_runtime_metrics;",
      "OwnerFutureStore owner_future_store;",
      "OwnerSchedulerState owner_scheduler_state;",
      "OwnerTraceStore owner_trace_store;",
      "std::mutex owner_runtime_mutex;",
      "std::condition_variable owner_runtime_cv;",
      "std::vector<std::thread> owner_threads;",
  };
  for (const auto& declaration : forbidden_owner_cc_storage) {
    ASSERT_EQ(owner_cc.find(declaration), std::string::npos)
        << "owner.cc must not own runtime store: " << declaration;
  }

  ASSERT_NE(coordinator_cc.find("OwnerRuntimeCoordinator &owner_runtime_coordinator()"),
            std::string::npos);
  ASSERT_NE(coordinator_cc.find("OwnerRuntimeMetrics &owner_runtime_metrics_instance()"),
            std::string::npos);
  ASSERT_NE(coordinator_cc.find("OwnerFutureStore &owner_future_store_instance()"),
            std::string::npos);
  ASSERT_NE(coordinator_cc.find("OwnerSchedulerState &owner_scheduler_state_instance()"),
            std::string::npos);
  ASSERT_NE(coordinator_cc.find("OwnerTraceStore &owner_trace_store_instance()"),
            std::string::npos);
}

TEST_F(DriverTest, TestVmOwnerExecutorBudgetYieldsAndRequeuesSameOwnerBacklog) {
  const char* owner = "owner/test/executor/budget-yield";

  vm_owner_thread_stop();
  // Reset the global yield-observation baseline under the runtime lock so
  // the test does not inherit observations from earlier tests.
  vm_owner_test_support_reset_budget_yield_observations();
  test_set_env("FLUFFOS_OWNER_EXECUTOR_PROBE_DELAY_MS", "5");
  struct ProbeDelayGuard {
    ~ProbeDelayGuard() {
      test_unset_env("FLUFFOS_OWNER_EXECUTOR_PROBE_DELAY_MS");
    }
  } probe_delay_guard;

  auto mapping_array = [](mapping_t* map, const char* key) -> array_t* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_ARRAY);
    return value && value->type == T_ARRAY ? value->u.arr : nullptr;
  };

  auto* before = vm_owner_thread_status();
  auto before_budget_yields = mapping_number(before, "executor_budget_yields");
  auto before_probe = mapping_number(before, "executor_probe_executed");
  auto budget = mapping_number(before, "executor_task_budget");
  ASSERT_GT(budget, 0);
  free_mapping(before);

  auto task_count = budget * 3 + 1;
  for (long i = 0; i < task_count; i++) {
    auto task_id = vm_owner_enqueue_task(owner, "executor_probe", "budget-yield-probe");
    ASSERT_GT(task_id, 0u);
  }

  auto* queued = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(queued, "owner_queue_depth"), task_count);
  ASSERT_EQ(mapping_number(queued, "owner_executor_safe_queue_depth"), task_count);
  free_mapping(queued);

  vm_owner_thread_start(1);
  int observed_budget_yield = 0;
  for (int i = 0; i < 200; i++) {
    auto* status = vm_owner_thread_status();
    auto budget_yields = mapping_number(status, "executor_budget_yields");
    auto probe_done = mapping_number(status, "executor_probe_executed");
    auto last_yield_backlog = mapping_number(status, "executor_last_budget_yield_backlog");
    auto last_yield_safe_backlog =
        mapping_number(status, "executor_last_budget_yield_safe_backlog");
    auto last_yield_owner = std::string(mapping_string(status, "executor_last_budget_yield_owner"));
    free_mapping(status);
    auto* mailbox = vm_owner_mailbox_status(owner);
    auto owner_depth = mapping_number(mailbox, "owner_queue_depth");
    auto safe_depth = mapping_number(mailbox, "owner_executor_safe_queue_depth");
    free_mapping(mailbox);
    if (budget_yields >= before_budget_yields + 1 && probe_done >= before_probe + budget &&
        !last_yield_owner.empty() && last_yield_backlog > 0 && last_yield_safe_backlog > 0 &&
        owner_depth > 0 && safe_depth > 0) {
      // All yield-observation fields (counter, owner, backlog) must come
      // from the same snapshot; assert them together instead of capturing
      // a failure after the fact.
      ASSERT_EQ(last_yield_owner, owner);
      ASSERT_GT(last_yield_backlog, 0);
      ASSERT_GT(last_yield_safe_backlog, 0);
      observed_budget_yield = 1;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  ASSERT_EQ(observed_budget_yield, 1);

  for (int i = 0; i < 200; i++) {
    auto* mailbox = vm_owner_mailbox_status(owner);
    auto owner_depth = mapping_number(mailbox, "owner_queue_depth");
    free_mapping(mailbox);
    auto* status = vm_owner_thread_status();
    auto probe_done = mapping_number(status, "executor_probe_executed");
    free_mapping(status);
    if (owner_depth == 0 && probe_done >= before_probe + task_count) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  auto* drained = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(drained, "owner_queue_depth"), 0);
  ASSERT_EQ(mapping_number(drained, "owner_executor_safe_queue_depth"), 0);
  free_mapping(drained);

  auto* running = vm_owner_thread_status();
  ASSERT_GE(mapping_number(running, "executor_budget_yields"), before_budget_yields + 1);
  ASSERT_GE(mapping_number(running, "executor_probe_executed"), before_probe + task_count);
  ASSERT_STREQ(mapping_string(running, "executor_last_budget_yield_owner"), owner);
  ASSERT_EQ(mapping_number(running, "executor_same_owner_claim_conflicts"), 0);
  ASSERT_EQ(mapping_number(running, "claimed_owners"), 0);
  free_mapping(running);

  auto* runtime = vm_owner_runtime_status();
  ASSERT_STREQ(mapping_string(runtime, "executor_last_budget_yield_owner"), owner);
  ASSERT_GE(mapping_number(runtime, "executor_last_budget_yield_backlog"), 0);
  ASSERT_GE(mapping_number(runtime, "executor_last_budget_yield_safe_backlog"), 0);
  free_mapping(runtime);

  auto* executor_trace = vm_owner_executor_trace(32);
  ASSERT_EQ(mapping_number(executor_trace, "success"), 1);
  ASSERT_STREQ(mapping_string(executor_trace, "trace_kind"), "owner_executor_trace");
  ASSERT_STREQ(mapping_string(executor_trace, "trace_model"), "owner_executor_scheduler_trace");
  ASSERT_STREQ(mapping_string(executor_trace, "trace_schema"), "owner_executor_trace_v2");
  ASSERT_STREQ(mapping_string(executor_trace, "owner_task_manifest_schema"),
               "owner_task_manifest_v2");
  ASSERT_STREQ(mapping_string(executor_trace, "admission_policy"),
               "owner_epoch_payload_allowlist_deadline_guard");
  ASSERT_STREQ(mapping_string(executor_trace, "executor_contract_version"), "owner_executor_v2");
  ASSERT_STREQ(mapping_string(executor_trace, "executor_model"), "owner_executor");
  ASSERT_GT(mapping_number(executor_trace, "returned"), 0);
  auto* events = mapping_array(executor_trace, "events");
  ASSERT_NE(events, nullptr);
  int saw_claimed = 0;
  int saw_budget_yield = 0;
  int saw_released = 0;
  for (int i = 0; i < events->size; i++) {
    auto* event = events->item[i].u.map;
    if (std::string(mapping_string(event, "owner_id")) != owner) {
      continue;
    }
    ASSERT_STREQ(mapping_string(event, "trace_model"), "owner_executor_scheduler_event");
    ASSERT_STREQ(mapping_string(event, "trace_schema"), "owner_executor_trace_v2");
    ASSERT_STREQ(mapping_string(event, "owner_task_manifest_schema"), "owner_task_manifest_v2");
    ASSERT_STREQ(mapping_string(event, "admission_policy"),
                 "owner_epoch_payload_allowlist_deadline_guard");
    ASSERT_STREQ(mapping_string(event, "executor_contract_version"), "owner_executor_v2");
    ASSERT_STREQ(mapping_string(event, "executor_model"), "owner_executor");
    ASSERT_STREQ(mapping_string(event, "executor_dispatch_model"), "descriptor_manifest");
    auto event_name = std::string(mapping_string(event, "event"));
    if (event_name == "owner_claimed") {
      saw_claimed = 1;
      ASSERT_GE(mapping_number(event, "claimed_owners"), 1);
    } else if (event_name == "budget_yield") {
      saw_budget_yield = 1;
      ASSERT_GT(mapping_number(event, "backlog"), 0);
      ASSERT_GT(mapping_number(event, "safe_backlog"), 0);
    } else if (event_name == "owner_released") {
      saw_released = 1;
      ASSERT_EQ(mapping_number(event, "claimed_owners"), 0);
    }
    ASSERT_GE(mapping_number(event, "runnable_owners"), 0);
    ASSERT_GE(mapping_number(event, "main_required_backlog"), 0);
    ASSERT_GE(mapping_number(event, "active_claims"), 0);
  }
  ASSERT_EQ(saw_claimed, 1);
  ASSERT_EQ(saw_budget_yield, 1);
  ASSERT_EQ(saw_released, 1);
  free_mapping(executor_trace);

  vm_owner_thread_stop();
}

TEST_F(DriverTest, TestVmOwnerExecutorDoesNotYieldWhenExactBudgetDrainsBacklog) {
  const char* owner = "owner/test/executor/exact-budget";

  vm_owner_thread_stop();
  auto mapping_array = [](mapping_t* map, const char* key) -> array_t* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_ARRAY);
    return value && value->type == T_ARRAY ? value->u.arr : nullptr;
  };

  auto* before = vm_owner_thread_status();
  auto before_budget_yields = mapping_number(before, "executor_budget_yields");
  auto before_probe = mapping_number(before, "executor_probe_executed");
  auto before_last_yield_backlog = mapping_number(before, "executor_last_budget_yield_backlog");
  auto before_last_yield_safe_backlog =
      mapping_number(before, "executor_last_budget_yield_safe_backlog");
  auto before_last_yield_owner =
      std::string(mapping_string(before, "executor_last_budget_yield_owner"));
  auto budget = mapping_number(before, "executor_task_budget");
  ASSERT_GT(budget, 0);
  free_mapping(before);

  for (long i = 0; i < budget; i++) {
    auto task_id = vm_owner_enqueue_task(owner, "executor_probe", "exact-budget-probe");
    ASSERT_GT(task_id, 0u);
  }

  auto* queued = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(queued, "owner_queue_depth"), budget);
  ASSERT_EQ(mapping_number(queued, "owner_executor_safe_queue_depth"), budget);
  free_mapping(queued);

  vm_owner_thread_start(1);
  for (int i = 0; i < 200; i++) {
    auto* mailbox = vm_owner_mailbox_status(owner);
    auto owner_depth = mapping_number(mailbox, "owner_queue_depth");
    free_mapping(mailbox);
    auto* status = vm_owner_thread_status();
    auto probe_done = mapping_number(status, "executor_probe_executed");
    free_mapping(status);
    if (owner_depth == 0 && probe_done >= before_probe + budget) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  auto* drained = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(drained, "owner_queue_depth"), 0);
  ASSERT_EQ(mapping_number(drained, "owner_executor_safe_queue_depth"), 0);
  free_mapping(drained);

  auto* running = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(running, "executor_budget_yields"), before_budget_yields);
  ASSERT_GE(mapping_number(running, "executor_probe_executed"), before_probe + budget);
  ASSERT_STREQ(mapping_string(running, "executor_last_budget_yield_owner"),
               before_last_yield_owner.c_str());
  ASSERT_EQ(mapping_number(running, "executor_last_budget_yield_backlog"),
            before_last_yield_backlog);
  ASSERT_EQ(mapping_number(running, "executor_last_budget_yield_safe_backlog"),
            before_last_yield_safe_backlog);
  ASSERT_EQ(mapping_number(running, "executor_same_owner_claim_conflicts"), 0);
  ASSERT_EQ(mapping_number(running, "claimed_owners"), 0);
  free_mapping(running);

  auto* executor_trace = vm_owner_executor_trace(64);
  ASSERT_EQ(mapping_number(executor_trace, "success"), 1);
  auto* events = mapping_array(executor_trace, "events");
  ASSERT_NE(events, nullptr);
  int saw_budget_yield = 0;
  for (int i = 0; i < events->size; i++) {
    auto* event = events->item[i].u.map;
    if (std::string(mapping_string(event, "owner_id")) == owner &&
        std::string(mapping_string(event, "event")) == "budget_yield") {
      saw_budget_yield = 1;
    }
  }
  ASSERT_EQ(saw_budget_yield, 0);
  free_mapping(executor_trace);

  vm_owner_thread_stop();
}

// F04: when a task throws inside the worker, the pending future for that
// task must be terminalized as failed (idempotent, stable error code) and
// the worker must survive to drain the rest of the mailbox.
TEST_F(DriverTest, TestVmOwnerExecutorExceptionTerminalizesPendingFuture) {
  const char* owner = "owner/test/executor/exception-future";

  vm_owner_thread_stop();
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);
  struct ProbeGuard {
    object_t* probe;
    ~ProbeGuard() {
      vm_owner_clear_id(probe);
      destruct_object(probe);
    }
  } probe_guard{probe};


  auto* before = vm_owner_thread_status();
  auto before_std = mapping_number(before, "executor_task_exceptions");
  auto before_unknown = mapping_number(before, "executor_task_unknown_exceptions");
  free_mapping(before);

  vm_owner_thread_start(1);

  // The worker callback blocks on a gate so the test can register the
  // pending future for the exact internal task id before the task throws.
  std::atomic<int> callback_started{0};
  std::atomic<int> release_callback{0};
  std::atomic<int> throw_mode{0};  // 0=std, 1=unknown
  const auto callback_task_id = vm_owner_enqueue_executor_task(
      probe, "room_output_projection", "exception-callback",
      [&] {
        callback_started.store(1, std::memory_order_release);
        while (release_callback.load(std::memory_order_acquire) == 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (throw_mode.load(std::memory_order_relaxed) == 1) {
          throw 42;  // unknown exception
        }
        throw std::runtime_error("injected executor failure");
      },
      nullptr);
  ASSERT_GT(callback_task_id, 0u);
  for (int i = 0; i < 200 && callback_started.load(std::memory_order_acquire) == 0; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  ASSERT_EQ(callback_started.load(std::memory_order_acquire), 1);

  // Register a pending future for the exact internal task id; the task is
  // already inside the worker, so the finalizer must find and terminalize it.
  const auto future_id =
      vm_owner_register_compute_future(owner, callback_task_id, "bench", "exception/future");
  ASSERT_GT(future_id, 0u);
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_PENDING);

  // Release the gate: the callback throws, the worker catch block must
  // terminalize the pending future with the stable error code.
  release_callback.store(1, std::memory_order_release);
  for (int i = 0; i < 200; i++) {
    auto state = vm_owner_future_state(future_id);
    if (state != VM_OWNER_FUTURE_PENDING) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_FAILED);
  auto* polled = vm_owner_future_poll(future_id);
  ASSERT_STREQ(mapping_string(polled, "error"), "executor_std_exception");
  free_mapping(polled);
  auto* taken = vm_owner_future_take(future_id);
  ASSERT_EQ(mapping_number(taken, "consumed"), 1);
  free_mapping(taken);

  // Worker must survive and the unknown-exception counter must not move.
  auto* after_std = vm_owner_thread_status();
  ASSERT_GE(mapping_number(after_std, "executor_task_exceptions"), before_std + 1);
  ASSERT_EQ(mapping_number(after_std, "executor_task_unknown_exceptions"), before_unknown);
  free_mapping(after_std);

  vm_owner_thread_stop();
}

TEST_F(DriverTest, TestVmOwnerExecutorRunsDifferentOwnersInParallel) {
  const char* owner_a = "owner/test/executor/parallel-a";
  const char* owner_b = "owner/test/executor/parallel-b";

  vm_owner_thread_stop();

  auto* before = vm_owner_thread_status();
  auto before_probe = mapping_number(before, "executor_probe_executed");
  auto before_claims = mapping_number(before, "executor_owner_claims");
  auto before_releases = mapping_number(before, "executor_owner_releases");
  auto before_max_parallel = mapping_number(before, "executor_max_parallel_owners");
  auto before_max_owner_parallel = mapping_number(before, "executor_max_owner_parallel");
  free_mapping(before);

  auto task_a = vm_owner_enqueue_task(owner_a, "executor_probe", "parallel-a");
  auto task_b = vm_owner_enqueue_task(owner_b, "executor_probe", "parallel-b");
  ASSERT_GT(task_a, 0u);
  ASSERT_GT(task_b, task_a);

  test_set_env("FLUFFOS_OWNER_EXECUTOR_PROBE_DELAY_MS", "80");
  vm_owner_thread_start(2);
  for (int i = 0; i < 200; i++) {
    auto* status = vm_owner_thread_status();
    auto probe_done = mapping_number(status, "executor_probe_executed");
    auto releases_done = mapping_number(status, "executor_owner_releases");
    auto active_owners = mapping_number(status, "active_owners");
    free_mapping(status);
    if (probe_done >= before_probe + 2 && releases_done >= before_releases + 2 &&
        active_owners == 0) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  test_unset_env("FLUFFOS_OWNER_EXECUTOR_PROBE_DELAY_MS");

  auto* running = vm_owner_thread_status();
  ASSERT_GE(mapping_number(running, "executor_probe_executed"), before_probe + 2);
  ASSERT_GE(mapping_number(running, "executor_owner_claims"), before_claims + 2);
  ASSERT_EQ(mapping_number(running, "executor_owner_claims") - before_claims,
            mapping_number(running, "executor_owner_releases") - before_releases);
  ASSERT_GE(mapping_number(running, "executor_max_parallel_owners"),
            std::max<long>(2, before_max_parallel));
  ASSERT_GE(mapping_number(running, "executor_max_owner_parallel"),
            std::max<long>(1, before_max_owner_parallel));
  ASSERT_LE(mapping_number(running, "executor_max_owner_parallel"), 1);
  ASSERT_EQ(mapping_number(running, "executor_same_owner_claim_conflicts"), 0);
  ASSERT_EQ(mapping_number(running, "active_owners"), 0);
  free_mapping(running);

  vm_owner_thread_stop();
}

TEST_F(DriverTest, TestVmOwnerStartCannotCrossAnInProgressStop) {
  constexpr const char* kOwner = "owner/test/executor/stop-start-guard";
  vm_owner_thread_stop();

  auto* before = vm_owner_thread_status();
  const auto before_starts = mapping_number(before, "thread_starts");
  const auto before_probe = mapping_number(before, "executor_probe_executed");
  const auto before_claims = mapping_number(before, "executor_owner_claims");
  const auto before_releases = mapping_number(before, "executor_owner_releases");
  free_mapping(before);

  // Keep the claimed worker inside the stop-owned join window long enough
  // for heavily instrumented TSan builds to observe and exercise the guard.
  test_set_env("FLUFFOS_OWNER_EXECUTOR_PROBE_DELAY_MS", "3000");
  ASSERT_GT(vm_owner_enqueue_task(kOwner, "executor_probe", "stop-start-guard-a"), 0u);
  ASSERT_GT(vm_owner_enqueue_task(kOwner, "executor_probe", "stop-start-guard-b"), 0u);
  vm_owner_thread_start(1);

  bool saw_active_claim = false;
  for (int i = 0; i < 200; ++i) {
    auto* status = vm_owner_thread_status();
    saw_active_claim = mapping_number(status, "active_owners") == 1 &&
                       mapping_number(status, "executor_active_claims") == 1;
    free_mapping(status);
    if (saw_active_claim) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_TRUE(saw_active_claim);

  std::thread stopper([] {
    vm_owner_thread_stop();
  });
  bool saw_stop_owned_threads = false;
  for (int i = 0; i < 200; ++i) {
    auto* status = vm_owner_thread_status();
    saw_stop_owned_threads =
        mapping_number(status, "stopping") == 1 && mapping_number(status, "enabled") == 0;
    free_mapping(status);
    if (saw_stop_owned_threads) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  // This start must be rejected while the first stop owns the swapped thread
  // handles. The second stop must likewise leave the first stop's latch intact.
  vm_owner_thread_start(1);
  auto* during = vm_owner_thread_status();
  const auto during_starts = mapping_number(during, "thread_starts");
  const auto during_enabled = mapping_number(during, "enabled");
  const auto during_stopping = mapping_number(during, "stopping");
  free_mapping(during);
  vm_owner_thread_stop();
  stopper.join();
  test_unset_env("FLUFFOS_OWNER_EXECUTOR_PROBE_DELAY_MS");
  vm_owner_thread_stop();

  ASSERT_TRUE(saw_stop_owned_threads);
  ASSERT_EQ(during_starts, before_starts + 1);
  ASSERT_EQ(during_enabled, 0);
  ASSERT_EQ(during_stopping, 1);

  auto* stopped = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(stopped, "stopping"), 0);
  ASSERT_EQ(mapping_number(stopped, "active_owners"), 0);
  ASSERT_EQ(mapping_number(stopped, "executor_active_claims"), 0);
  ASSERT_EQ(mapping_number(stopped, "executor_owner_claims") - before_claims,
            mapping_number(stopped, "executor_owner_releases") - before_releases);
  ASSERT_GE(mapping_number(stopped, "executor_probe_executed"), before_probe + 2);
  free_mapping(stopped);

  ASSERT_GT(vm_owner_enqueue_task(kOwner, "executor_probe", "stop-start-restart"), 0u);
  vm_owner_thread_start(1);
  for (int i = 0; i < 200; ++i) {
    auto* status = vm_owner_thread_status();
    const auto restarted = mapping_number(status, "executor_probe_executed") >= before_probe + 3;
    free_mapping(status);
    if (restarted) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  vm_owner_thread_stop();
  auto* restarted = vm_owner_thread_status();
  ASSERT_GE(mapping_number(restarted, "executor_probe_executed"), before_probe + 3);
  ASSERT_EQ(mapping_number(restarted, "active_owners"), 0);
  ASSERT_EQ(mapping_number(restarted, "executor_active_claims"), 0);
  ASSERT_EQ(mapping_number(restarted, "executor_owner_claims") - before_claims,
            mapping_number(restarted, "executor_owner_releases") - before_releases);
  free_mapping(restarted);
}

TEST_F(DriverTest, TestVmOwnerRuntimePerformanceProbesRecordDiagnostics) {
  const char* same_owner = "owner/test/runtime-v3/probe/same";
  const char* owner_a = "owner/test/runtime-v3/probe/a";
  const char* owner_b = "owner/test/runtime-v3/probe/b";
  const char* callback_owner = "owner/test/runtime-v3/probe/callback";
  const char* moved_owner = "owner/test/runtime-v3/probe/callback/moved";
  const long same_owner_tasks = 16;
  const long different_owner_tasks = 12;
  const long object_resolve_iterations = 64;

  struct RuntimeProbeGuard {
    object_t* callback_probe{nullptr};
    object_t* resolve_probe{nullptr};
    ~RuntimeProbeGuard() {
      vm_owner_thread_stop();
      test_unset_env("FLUFFOS_OWNER_EXECUTOR_PROBE_DELAY_MS");
      if (callback_probe) {
        vm_owner_clear_id(callback_probe);
        destruct_object(callback_probe);
      }
      if (resolve_probe) {
        vm_owner_clear_id(resolve_probe);
        destruct_object(resolve_probe);
      }
    }
  } guard;

  vm_owner_thread_stop();
  auto wait_for_owner_probe = [&](const char* owner, long expected_probe_count) {
    for (int i = 0; i < 200; i++) {
      auto* mailbox = vm_owner_mailbox_status(owner);
      auto depth = mapping_number(mailbox, "owner_queue_depth");
      free_mapping(mailbox);
      auto* status = vm_owner_thread_status();
      auto probe_done = mapping_number(status, "executor_probe_executed");
      free_mapping(status);
      if (depth == 0 && probe_done >= expected_probe_count) {
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  };

  auto* before = vm_owner_thread_status();
  auto before_probe = mapping_number(before, "executor_probe_executed");
  auto before_claims = mapping_number(before, "executor_owner_claims");
  auto before_releases = mapping_number(before, "executor_owner_releases");
  auto before_conflicts = mapping_number(before, "executor_same_owner_claim_conflicts");
  auto before_max_parallel = mapping_number(before, "executor_max_parallel_owners");
  free_mapping(before);

  auto* before_runtime = vm_owner_runtime_status();
  auto before_normal_fallback = mapping_number(before_runtime, "normal_path_main_fallback_count");
  auto before_future_cancelled = mapping_number(before_runtime, "owner_executor_future_cancelled");
  auto before_future_timeout = mapping_number(before_runtime, "owner_executor_future_timeout");
  auto before_admission_accepted =
      mapping_number(before_runtime, "owner_executor_admission_accepted");
  auto before_admission_rejected =
      mapping_number(before_runtime, "owner_executor_admission_rejected");
  auto before_admission_dropped =
      mapping_number(before_runtime, "owner_executor_admission_dropped");
  ASSERT_EQ(mapping_number(before_runtime, "owner_executor_context_cleanup_leaks"), 0);
  ASSERT_EQ(before_normal_fallback, 0);
  free_mapping(before_runtime);

  auto same_start = std::chrono::steady_clock::now();
  for (long i = 0; i < same_owner_tasks; i++) {
    auto task_id = vm_owner_enqueue_task(same_owner, "executor_probe", "runtime-v3-same-owner");
    ASSERT_GT(task_id, 0u);
  }
  auto* same_queued = vm_owner_mailbox_status(same_owner);
  ASSERT_EQ(mapping_number(same_queued, "owner_queue_depth"), same_owner_tasks);
  ASSERT_EQ(mapping_number(same_queued, "owner_executor_safe_queue_depth"), same_owner_tasks);
  RecordProperty("same_owner_queue_depth", mapping_number(same_queued, "owner_queue_depth"));
  free_mapping(same_queued);

  vm_owner_thread_start(1);
  wait_for_owner_probe(same_owner, before_probe + same_owner_tasks);
  vm_owner_thread_stop();
  auto same_elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - same_start)
                             .count();

  auto* after_same = vm_owner_thread_status();
  ASSERT_GE(mapping_number(after_same, "executor_probe_executed"), before_probe + same_owner_tasks);
  ASSERT_EQ(mapping_number(after_same, "executor_same_owner_claim_conflicts"), before_conflicts);
  ASSERT_EQ(mapping_number(after_same, "active_owners"), 0);
  ASSERT_EQ(mapping_number(after_same, "claimed_owners"), 0);
  RecordProperty("same_owner_tasks", same_owner_tasks);
  RecordProperty("same_owner_completed",
                 mapping_number(after_same, "executor_probe_executed") - before_probe);
  RecordProperty("same_owner_elapsed_us", static_cast<long>(same_elapsed_us));
  free_mapping(after_same);

  auto* before_parallel = vm_owner_thread_status();
  auto before_parallel_probe = mapping_number(before_parallel, "executor_probe_executed");
  auto before_parallel_claims = mapping_number(before_parallel, "executor_owner_claims");
  auto before_parallel_releases = mapping_number(before_parallel, "executor_owner_releases");
  free_mapping(before_parallel);

  test_set_env("FLUFFOS_OWNER_EXECUTOR_PROBE_DELAY_MS", "20");
  auto parallel_start = std::chrono::steady_clock::now();
  for (long i = 0; i < different_owner_tasks; i++) {
    ASSERT_GT(vm_owner_enqueue_task(owner_a, "executor_probe", "runtime-v3-owner-a"), 0u);
    ASSERT_GT(vm_owner_enqueue_task(owner_b, "executor_probe", "runtime-v3-owner-b"), 0u);
  }
  vm_owner_thread_start(2);
  wait_for_owner_probe(owner_a, before_parallel_probe + different_owner_tasks * 2);
  wait_for_owner_probe(owner_b, before_parallel_probe + different_owner_tasks * 2);
  vm_owner_thread_stop();
  test_unset_env("FLUFFOS_OWNER_EXECUTOR_PROBE_DELAY_MS");
  auto parallel_elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::steady_clock::now() - parallel_start)
                                 .count();

  auto* after_parallel = vm_owner_thread_status();
  auto parallel_completed =
      mapping_number(after_parallel, "executor_probe_executed") - before_parallel_probe;
  ASSERT_GE(parallel_completed, different_owner_tasks * 2);
  ASSERT_EQ(mapping_number(after_parallel, "executor_owner_claims") - before_parallel_claims,
            mapping_number(after_parallel, "executor_owner_releases") - before_parallel_releases);
  ASSERT_EQ(mapping_number(after_parallel, "executor_same_owner_claim_conflicts"),
            before_conflicts);
  ASSERT_EQ(mapping_number(after_parallel, "active_owners"), 0);
  ASSERT_EQ(mapping_number(after_parallel, "claimed_owners"), 0);
  ASSERT_GE(mapping_number(after_parallel, "executor_max_parallel_owners"),
            std::max<long>(2, before_max_parallel));
  ASSERT_LE(mapping_number(after_parallel, "executor_max_owner_parallel"), 1);
  RecordProperty("different_owner_tasks", different_owner_tasks * 2);
  RecordProperty("different_owner_completed", parallel_completed);
  RecordProperty("different_owner_elapsed_us", static_cast<long>(parallel_elapsed_us));
  RecordProperty("different_owner_max_parallel",
                 mapping_number(after_parallel, "executor_max_parallel_owners"));
  RecordProperty("owner_claim_delta",
                 mapping_number(after_parallel, "executor_owner_claims") - before_claims);
  RecordProperty("owner_release_delta",
                 mapping_number(after_parallel, "executor_owner_releases") - before_releases);
  free_mapping(after_parallel);

  auto cancel_future_id = vm_owner_register_compute_future(
      "owner/test/runtime-v3/future-cancel", 9201, "runtime_v3_cancel", "probe_cancel");
  auto timeout_future_id = vm_owner_register_compute_future(
      "owner/test/runtime-v3/future-timeout", 9202, "runtime_v3_timeout", "probe_timeout");
  ASSERT_GT(cancel_future_id, 0u);
  ASSERT_GT(timeout_future_id, 0u);
  auto* pending_future = vm_owner_future_poll(cancel_future_id);
  ASSERT_STREQ(mapping_string(pending_future, "state"), "pending");
  free_mapping(pending_future);
  auto* cancelled = vm_owner_future_cancel(cancel_future_id, "runtime v3 probe cancel");
  ASSERT_STREQ(mapping_string(cancelled, "state"), "failed");
  ASSERT_EQ(mapping_number(cancelled, "cancelled"), 1);
  free_mapping(cancelled);
  auto* timed_out = vm_owner_future_timeout(timeout_future_id, "runtime v3 probe timeout");
  ASSERT_STREQ(mapping_string(timed_out, "state"), "failed");
  ASSERT_EQ(mapping_number(timed_out, "timed_out"), 1);
  free_mapping(timed_out);

  guard.resolve_probe = clone_object_for_test("single/void");
  ASSERT_NE(guard.resolve_probe, nullptr);
  vm_owner_set_id(guard.resolve_probe, "owner/test/runtime-v3/resolve");
  vm_object_store_register(guard.resolve_probe);
  auto handle = vm_object_handle(guard.resolve_probe);
  auto resolve_start = std::chrono::steady_clock::now();
  for (long i = 0; i < object_resolve_iterations; i++) {
    auto handle_resolve = vm_object_handle_resolve_status(handle);
    ASSERT_EQ(handle_resolve.object, guard.resolve_probe);
    ASSERT_STREQ(vm_object_handle_resolve_status_name(handle_resolve.status), "current");
    ASSERT_TRUE(handle_resolve.owner_local_fast_path_used);
    ASSERT_TRUE(handle_resolve.resolved_via_owner_local_store);
    ASSERT_FALSE(handle_resolve.resolved_via_global_index);
  }
  auto resolve_elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now() - resolve_start)
                                .count();
  RecordProperty("object_resolve_iterations", object_resolve_iterations);
  RecordProperty("object_resolve_elapsed_us", static_cast<long>(resolve_elapsed_us));

  guard.callback_probe = load_object_for_test("single/void");
  ASSERT_NE(guard.callback_probe, nullptr);
  vm_owner_set_id(guard.callback_probe, callback_owner);
  ASSERT_FALSE(vm_owner_executor_available());
  ASSERT_EQ(vm_owner_enqueue_executor_task(guard.callback_probe, "ordinary_lpc",
                                           "runtime-v3-rejected", [] {}),
            0u);

  std::atomic<int> adapter_ran{0};
  std::atomic<int> stale_ran{0};
  std::atomic<int> stale_drop_cleanup{0};
  vm_owner_thread_start(1);
  ASSERT_TRUE(vm_owner_executor_available());
  ASSERT_EQ(vm_owner_enqueue_executor_task(guard.callback_probe, "legacy_lpc",
                                           "runtime-v3-legacy-rejected", [] {}),
            0u);
  auto adapter_task = vm_owner_enqueue_executor_task(
      guard.callback_probe, "heartbeat", "runtime-v3-main-adapter", [&] {
        adapter_ran.store(vm_context_is_main_thread() ? 1 : -1, std::memory_order_release);
      });
  ASSERT_GT(adapter_task, 0u);
  ASSERT_EQ(vm_owner_drain_main_tasks(16), 1);
  ASSERT_EQ(adapter_ran.load(std::memory_order_acquire), 1);

  auto stale_task = vm_owner_enqueue_executor_task(
      guard.callback_probe, "call_out", "runtime-v3-stale",
      [&] {
        stale_ran.store(1, std::memory_order_release);
      },
      [&] {
        stale_drop_cleanup.store(vm_context_is_main_thread() ? 1 : -1, std::memory_order_release);
      });
  ASSERT_GT(stale_task, adapter_task);
  vm_owner_set_id(guard.callback_probe, moved_owner);
  ASSERT_EQ(vm_owner_drain_main_tasks(16), 1);
  ASSERT_EQ(vm_owner_drain_main_tasks(16), 1);
  ASSERT_EQ(stale_ran.load(std::memory_order_acquire), 0);
  ASSERT_EQ(stale_drop_cleanup.load(std::memory_order_acquire), 1);
  vm_owner_thread_stop();

  auto* final_runtime = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(final_runtime, "normal_path_main_fallback_count"),
            before_normal_fallback);
  ASSERT_EQ(mapping_number(final_runtime, "normal_path_main_fallback_count"), 0);
  ASSERT_EQ(mapping_number(final_runtime, "owner_executor_context_cleanup_leaks"), 0);
  ASSERT_GE(mapping_number(final_runtime, "owner_executor_future_cancelled"),
            before_future_cancelled + 1);
  ASSERT_GE(mapping_number(final_runtime, "owner_executor_future_timeout"),
            before_future_timeout + 1);
  ASSERT_GE(mapping_number(final_runtime, "owner_executor_admission_accepted"),
            before_admission_accepted + 2);
  ASSERT_GE(mapping_number(final_runtime, "owner_executor_admission_rejected"),
            before_admission_rejected + 1);
  ASSERT_GE(mapping_number(final_runtime, "owner_executor_admission_dropped"),
            before_admission_dropped + 1);
  RecordProperty(
      "future_cancel_delta",
      mapping_number(final_runtime, "owner_executor_future_cancelled") - before_future_cancelled);
  RecordProperty(
      "future_timeout_delta",
      mapping_number(final_runtime, "owner_executor_future_timeout") - before_future_timeout);
  RecordProperty("admission_accept_delta",
                 mapping_number(final_runtime, "owner_executor_admission_accepted") -
                     before_admission_accepted);
  RecordProperty("admission_reject_delta",
                 mapping_number(final_runtime, "owner_executor_admission_rejected") -
                     before_admission_rejected);
  RecordProperty(
      "admission_drop_delta",
      mapping_number(final_runtime, "owner_executor_admission_dropped") - before_admission_dropped);
  RecordProperty("normal_path_main_fallback_count",
                 mapping_number(final_runtime, "normal_path_main_fallback_count"));
  free_mapping(final_runtime);

  auto* final_thread = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(final_thread, "executor_same_owner_claim_conflicts"), before_conflicts);
  ASSERT_EQ(mapping_number(final_thread, "active_owners"), 0);
  ASSERT_EQ(mapping_number(final_thread, "claimed_owners"), 0);
  RecordProperty("same_owner_claim_conflicts",
                 mapping_number(final_thread, "executor_same_owner_claim_conflicts"));
  RecordProperty("executor_context_cleanup_leaks",
                 mapping_number(final_thread, "owner_executor_context_cleanup_leaks"));
  free_mapping(final_thread);
}

TEST_F(DriverTest, TestVmOwnerThreadRejectsLpcAndKeepsMessageSpecs) {
  const char* owner = "owner/test/thread/safe-experiment";

  vm_owner_thread_stop();

  auto* before = vm_owner_thread_status();
  auto before_runnable = mapping_number(before, "executor_runnable_task_dispatched");
  auto before_safe = mapping_number(before, "executor_safe_task_dispatched");
  free_mapping(before);

  auto lpc_task = vm_owner_enqueue_task(owner, "lpc", "off-main-dummy");
  auto state_task = vm_owner_enqueue_task(owner, "owner_state", "single-owner-state");
  auto gateway_command_task =
      vm_owner_enqueue_task(owner, "gateway_command", "player-command-activation");
  auto message_task = vm_owner_enqueue_task(owner, "owner_message", "cross-owner-message");
  ASSERT_GT(lpc_task, 0u);
  ASSERT_GT(state_task, lpc_task);
  ASSERT_GT(gateway_command_task, state_task);
  ASSERT_GT(message_task, gateway_command_task);

  auto* queued = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(queued, "owner_queue_depth"), 4);
  ASSERT_EQ(mapping_number(queued, "owner_executor_runnable_queue_depth"), 4);
  ASSERT_EQ(mapping_number(queued, "owner_executor_safe_queue_depth"), 2);
  ASSERT_EQ(mapping_number(queued, "owner_main_required_queue_depth"), 0);
  ASSERT_GE(mapping_number(queued, "executor_runnable_queue_depth"), 4);
  ASSERT_GE(mapping_number(queued, "executor_safe_queue_depth"), 2);
  free_mapping(queued);
  auto* queued_thread = vm_owner_thread_status();
  ASSERT_GE(mapping_number(queued_thread, "executor_runnable_queue_depth"), 4);
  auto* queued_fairness = find_string_in_mapping(queued_thread, "executor_queue_fairness");
  ASSERT_NE(queued_fairness, nullptr);
  ASSERT_EQ(queued_fairness->type, T_MAPPING);
  ASSERT_GE(mapping_number(queued_fairness->u.map, "executor_runnable_owner_count"), 1);
  ASSERT_GE(mapping_number(queued_fairness->u.map, "max_executor_runnable_backlog"), 4);
  ASSERT_GE(mapping_number(queued_fairness->u.map, "max_executor_safe_backlog"), 1);
  free_mapping(queued_thread);

  vm_owner_thread_start(1);
  for (int i = 0; i < 100; i++) {
    auto* status = vm_owner_mailbox_status(owner);
    auto depth = mapping_number(status, "owner_queue_depth");
    free_mapping(status);
    if (depth == 0) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  auto* running = vm_owner_thread_status();
  ASSERT_GE(mapping_number(running, "thread_lpc_rejected"), 1);
  ASSERT_GE(mapping_number(running, "thread_owner_state_guarded"), 1);
  ASSERT_GE(mapping_number(running, "thread_gateway_command_guarded"), 1);
  ASSERT_GE(mapping_number(running, "thread_message_dispatched"), 1);
  ASSERT_EQ(mapping_number(running, "executor_runnable_task_dispatched"), before_runnable + 4);
  ASSERT_EQ(mapping_number(running, "executor_safe_task_dispatched"), before_safe + 2);
  free_mapping(running);

  auto* trace = vm_owner_task_trace(16);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  int lpc_rejected = 0;
  int state_guarded = 0;
  int gateway_command_guarded = 0;
  int message_dispatched = 0;
  for (int i = 0; i < events->u.arr->size; i++) {
    auto* event = events->u.arr->item[i].u.map;
    if (mapping_number(event, "task_id") == static_cast<long>(lpc_task) &&
        std::string(mapping_string(event, "state")) == "thread_lpc_rejected") {
      lpc_rejected = 1;
    }
    if (mapping_number(event, "task_id") == static_cast<long>(state_task) &&
        std::string(mapping_string(event, "state")) == "thread_owner_state_guarded") {
      state_guarded = 1;
    }
    if (mapping_number(event, "task_id") == static_cast<long>(gateway_command_task) &&
        std::string(mapping_string(event, "state")) == "thread_gateway_command_executor_guarded") {
      gateway_command_guarded = 1;
    }
    if (mapping_number(event, "task_id") == static_cast<long>(message_task) &&
        std::string(mapping_string(event, "state")) == "thread_message_dispatched") {
      message_dispatched = 1;
    }
  }
  ASSERT_EQ(lpc_rejected, 1);
  ASSERT_EQ(state_guarded, 1);
  ASSERT_EQ(gateway_command_guarded, 1);
  ASSERT_EQ(message_dispatched, 1);
  free_mapping(trace);

  vm_owner_thread_stop();
}

TEST_F(DriverTest, TestVmOwnerThreadGuardsControlledLpcProbeOffMain) {
  const char* owner = "owner/test/thread/lpc-probe";
  ASSERT_TRUE(vm_context_is_main_thread());

  vm_owner_thread_stop();
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);


  auto* submitted = vm_owner_lpc_probe(probe, owner, "owner_lpc_probe");
  auto task_id = mapping_number(submitted, "task_id");
  ASSERT_EQ(mapping_number(submitted, "success"), 1);
  ASSERT_EQ(mapping_number(submitted, "requires_owner_thread"), 1);
  ASSERT_EQ(mapping_number(submitted, "direct_cross_owner_write"), 0);
  ASSERT_STREQ(mapping_string(submitted, "task_type"), "lpc_probe");
  ASSERT_STREQ(mapping_string(submitted, "method"), "owner_lpc_probe");
  free_mapping(submitted);

  vm_owner_thread_start(1);
  for (int i = 0; i < 100; i++) {
    auto* status = vm_owner_mailbox_status(owner);
    auto depth = mapping_number(status, "owner_queue_depth");
    free_mapping(status);
    if (depth == 0) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  auto* running = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(running, "thread_lpc_probe_executed"), 0);
  ASSERT_GE(mapping_number(running, "thread_lpc_probe_guarded"), 1);
  ASSERT_EQ(mapping_number(running, "thread_lpc_probe_failed"), 0);
  ASSERT_GE(mapping_number(running, "thread_context_bound"), 1);
  ASSERT_GE(mapping_number(running, "thread_object_store_isolated"), 1);
  free_mapping(running);

  auto* trace = vm_owner_task_trace(16);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  int lpc_guarded = 0;
  for (int i = 0; i < events->u.arr->size; i++) {
    auto* event = events->u.arr->item[i].u.map;
    if (mapping_number(event, "task_id") == task_id &&
        std::string(mapping_string(event, "state")) == "thread_lpc_probe_guarded") {
      lpc_guarded = 1;
    }
  }
  ASSERT_EQ(lpc_guarded, 1);
  free_mapping(trace);

  vm_owner_thread_stop();
  destruct_object(probe);
}

TEST_F(DriverTest, TestVmOwnerThreadRunsRestrictedLpcCanaryOffMainDeferredRelease) {
  const char* owner = "owner/test/thread/lpc-canary";
  ASSERT_TRUE(vm_context_is_main_thread());

  vm_owner_thread_stop();
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);
  auto owner_epoch = vm_owner_epoch(probe);
  const auto time_of_ref_before = current_gametick() + 100000;
  probe->time_of_ref = time_of_ref_before;

  auto mapping_entry = [](mapping_t* map, const char* key) -> mapping_t* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_MAPPING);
    return value && value->type == T_MAPPING ? value->u.map : nullptr;
  };

  auto* before = vm_owner_thread_status();
  auto before_executed = mapping_number(before, "thread_lpc_canary_executed");
  // T1 evidence: the canary body also asserts that get_os_env()/set_os_env()
  // are stably rejected on the worker, and this records the process-level
  // variable the rejected set_os_env("FLUFFOS_XK_TEST_ENV", "worker") would
  // have written if it had reached setenv().
  const char* os_env_before = std::getenv("FLUFFOS_XK_TEST_ENV");
  const std::string os_env_before_value = os_env_before ? os_env_before : "";
  auto before_succeeded = mapping_number(before, "thread_lpc_canary_succeeded");
  auto before_failed = mapping_number(before, "thread_lpc_canary_failed");
  auto before_rejected = mapping_number(before, "thread_lpc_canary_rejected");
  auto before_owner_cleared = mapping_number(before, "thread_owner_cleared");
  auto before_execution_cleared = mapping_number(before, "thread_execution_cleared");
  auto before_canary_flag_cleared = mapping_number(before, "thread_lpc_canary_flag_cleared");
  auto before_context_leaks = mapping_number(before, "thread_context_leak_detected");
  free_mapping(before);

  ScopedLpcMapping submitted_mapping(vm_owner_lpc_canary(probe, owner, "owner_lpc_canary"));
  auto* submitted = submitted_mapping.get();
  auto task_id = mapping_number(submitted, "task_id");
  ASSERT_EQ(mapping_number(submitted, "success"), 1);
  ASSERT_EQ(mapping_number(submitted, "requires_owner_thread"), 1);
  ASSERT_EQ(mapping_number(submitted, "direct_cross_owner_write"), 0);
  ASSERT_EQ(mapping_number(submitted, "owner_epoch"), static_cast<long>(owner_epoch));
  ASSERT_STREQ(mapping_string(submitted, "task_type"), "lpc_canary");
  ASSERT_STREQ(mapping_string(submitted, "method"), "owner_lpc_canary");

  vm_owner_thread_start(1);
  for (int i = 0; i < 100; i++) {
    auto* status = vm_owner_mailbox_status(owner);
    auto depth = mapping_number(status, "owner_queue_depth");
    free_mapping(status);
    if (depth == 0) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  // An empty mailbox only means the worker dequeued the task, not that it
  // finished the LPC canary (the body makes two efun calls), so wait on the
  // completion counter with a bound before asserting on the run counters.
  for (int i = 0; i < 400; i++) {
    auto* poll = vm_owner_thread_status();
    auto executed = mapping_number(poll, "thread_lpc_canary_executed");
    free_mapping(poll);
    if (executed >= before_executed + 1) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  ScopedLpcMapping running_mapping(vm_owner_thread_status());
  auto* running = running_mapping.get();
  ASSERT_GE(mapping_number(running, "thread_lpc_canary_executed"), before_executed + 1);
  // The canary body (testsuite/single/void.c owner_lpc_canary()) also probes
  // the T1 contract from the worker: get_os_env()/set_os_env() must be
  // rejected with "requires the main thread" there. A probe failure makes the
  // canary return 0, so this counter is the C++-level evidence for T1.
  ASSERT_GE(mapping_number(running, "thread_lpc_canary_succeeded"), before_succeeded + 1);
  {
    const char* os_env_after = std::getenv("FLUFFOS_XK_TEST_ENV");
    EXPECT_EQ(os_env_after ? os_env_after : "", os_env_before_value)
        << "the worker-side set_os_env() rejection must not reach setenv()";
  }
  ASSERT_EQ(mapping_number(running, "thread_lpc_canary_failed"), before_failed);
  ASSERT_EQ(mapping_number(running, "thread_lpc_canary_rejected"), before_rejected);
  ASSERT_GE(mapping_number(running, "thread_owner_cleared"), before_owner_cleared + 1);
  ASSERT_GE(mapping_number(running, "thread_execution_cleared"), before_execution_cleared + 1);
  ASSERT_GE(mapping_number(running, "thread_lpc_canary_flag_cleared"),
            before_canary_flag_cleared + 1);
  ASSERT_EQ(mapping_number(running, "thread_context_leak_detected"), before_context_leaks);
  ASSERT_GE(mapping_number(running, "thread_context_bound"), 1);
  ASSERT_GE(mapping_number(running, "thread_object_store_isolated"), 1);

  ScopedLpcMapping trace_mapping(vm_owner_task_trace(16));
  auto* trace = trace_mapping.get();
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  int canary_succeeded = 0;
  for (int i = 0; i < events->u.arr->size; i++) {
    auto* event = events->u.arr->item[i].u.map;
    if (mapping_number(event, "task_id") == task_id &&
        std::string(mapping_string(event, "state")) == "thread_lpc_canary_succeeded") {
      canary_succeeded = 1;
    }
  }
  ASSERT_EQ(canary_succeeded, 1);
  ASSERT_EQ(probe->time_of_ref, time_of_ref_before);

  vm_owner_thread_stop();
  auto* stopped = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(stopped, "deferred_target_releases"), 0);
  free_mapping(stopped);
  ASSERT_TRUE(vm_context_is_main_thread());
  destruct_object(probe);
}

