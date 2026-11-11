#include "test_lpc_support.h"

TEST_F(DriverTest, TestVmExecutionScopeRestoresGlobalState) {
  ScopedCurrentObjectAsMaster master_scope;
  command_giver = nullptr;
  current_interactive = nullptr;
  previous_ob = nullptr;
  current_prog = nullptr;
  caller_type = 0;
  vm_context_set_call_origin(vm_context(), 0);
  vm_context_set_inherit_offsets(vm_context(), 0, 0);
  vm_context_set_stack_temporary_depth(vm_context(), 0);
  vm_context_sync_execution(vm_context());

  object_t* obj = find_object("single/master.c");
  ASSERT_NE(obj, nullptr);

  VMExecutionState scoped = vm_context_capture_execution();
  scoped.current_object = obj;
  scoped.command_giver = obj;
  scoped.current_interactive = obj;
  scoped.previous_ob = obj;
  scoped.current_prog = obj->prog;
  scoped.caller_type = 42;
  scoped.call_origin = ORIGIN_EFUN;
  scoped.function_index_offset = 7;
  scoped.variable_index_offset = 11;
  scoped.stack_in_use_as_temporary = 2;

  {
    VMExecutionScope scope(vm_context(), scoped);
    ASSERT_EQ(current_object, obj);
    ASSERT_EQ(command_giver, obj);
    ASSERT_EQ(current_interactive, obj);
    ASSERT_EQ(previous_ob, obj);
    ASSERT_EQ(current_prog, obj->prog);
    ASSERT_EQ(caller_type, 42);
    ASSERT_EQ(call_origin, ORIGIN_EFUN);
    ASSERT_EQ(function_index_offset, 7);
    ASSERT_EQ(variable_index_offset, 11);
#ifdef DEBUG
    ASSERT_EQ(stack_in_use_as_temporary, 2);
#endif
    ASSERT_EQ(vm_context().execution.current_object, obj);
    ASSERT_EQ(vm_context().execution.command_giver, obj);
    ASSERT_EQ(vm_context().execution.current_interactive, obj);
    ASSERT_EQ(vm_context().execution.previous_ob, obj);
    ASSERT_EQ(vm_context().execution.current_prog, obj->prog);
    ASSERT_EQ(vm_context().execution.caller_type, 42);
    ASSERT_EQ(vm_context().execution.call_origin, ORIGIN_EFUN);
    ASSERT_EQ(vm_context().execution.function_index_offset, 7);
    ASSERT_EQ(vm_context().execution.variable_index_offset, 11);
    ASSERT_EQ(vm_context().execution.stack_in_use_as_temporary, 2);
  }

  ASSERT_EQ(current_object, master_ob);
  ASSERT_EQ(command_giver, nullptr);
  ASSERT_EQ(current_interactive, nullptr);
  ASSERT_EQ(previous_ob, nullptr);
  ASSERT_EQ(current_prog, nullptr);
  ASSERT_EQ(caller_type, 0);
  ASSERT_EQ(call_origin, 0);
  ASSERT_EQ(function_index_offset, 0);
  ASSERT_EQ(variable_index_offset, 0);
#ifdef DEBUG
  ASSERT_EQ(stack_in_use_as_temporary, 0);
#endif
  ASSERT_EQ(vm_context().execution.current_object, master_ob);
  ASSERT_EQ(vm_context().execution.command_giver, nullptr);
  ASSERT_EQ(vm_context().execution.current_interactive, nullptr);
  ASSERT_EQ(vm_context().execution.previous_ob, nullptr);
  ASSERT_EQ(vm_context().execution.current_prog, nullptr);
  ASSERT_EQ(vm_context().execution.caller_type, 0);
  ASSERT_EQ(vm_context().execution.call_origin, 0);
  ASSERT_EQ(vm_context().execution.function_index_offset, 0);
  ASSERT_EQ(vm_context().execution.variable_index_offset, 0);
  ASSERT_EQ(vm_context().execution.stack_in_use_as_temporary, 0);
}

TEST_F(DriverTest, TestVmContextResetClearsThreadExecutionState) {
  object_t* obj = find_object("single/master.c");
  ASSERT_NE(obj, nullptr);

  vm_context_set_current_object(vm_context(), obj);
  vm_context_set_command_giver(vm_context(), obj);
  vm_context_set_current_interactive(vm_context(), obj);
  vm_context_set_previous_object(vm_context(), obj);
  vm_context_set_current_program(vm_context(), obj->prog);
  vm_context_set_caller_type(vm_context(), ORIGIN_DRIVER);
  vm_context_set_call_origin(vm_context(), ORIGIN_DRIVER);
  vm_context_set_inherit_offsets(vm_context(), 3, 5);
  vm_context_set_stack_temporary_depth(vm_context(), 7);

  vm_context_reset_execution(vm_context());
  auto execution = vm_context_capture_execution();

  ASSERT_EQ(execution.current_object, nullptr);
  ASSERT_EQ(execution.command_giver, nullptr);
  ASSERT_EQ(execution.current_interactive, nullptr);
  ASSERT_EQ(execution.previous_ob, nullptr);
  ASSERT_EQ(execution.current_prog, nullptr);
  ASSERT_EQ(execution.caller_type, 0);
  ASSERT_EQ(execution.call_origin, 0);
  ASSERT_EQ(execution.function_index_offset, 0);
  ASSERT_EQ(execution.variable_index_offset, 0);
  ASSERT_EQ(execution.stack_in_use_as_temporary, 0);
}

TEST_F(DriverTest, TestDetachedVmContextSettersDoNotClobberThreadState) {
  object_t* first = find_object("single/master.c");
  object_t* second = find_object("single/simul_efun.c");
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);

  ScopedCurrentObjectAsMaster master_scope;
  command_giver = nullptr;
  current_interactive = nullptr;
  previous_ob = nullptr;
  current_prog = nullptr;
  caller_type = 0;
  call_origin = 0;
  function_index_offset = 0;
  variable_index_offset = 0;
#ifdef DEBUG
  stack_in_use_as_temporary = 0;
#endif
  current_error_context = nullptr;
  too_deep_error = 0;
  max_eval_error = 0;
  vm_context_sync_execution(vm_context());

  VMContext detached;
  error_context_t error_context{};
  vm_context_set_current_object(detached, first);
  vm_context_set_command_giver(detached, first);
  vm_context_set_current_interactive(detached, first);
  vm_context_set_previous_object(detached, second);
  vm_context_set_current_program(detached, first->prog);
  vm_context_set_caller_type(detached, ORIGIN_DRIVER);
  vm_context_set_call_origin(detached, ORIGIN_EFUN);
  vm_context_set_inherit_offsets(detached, 9, 13);
  vm_context_set_stack_temporary_depth(detached, 4);
  vm_context_set_current_error_context(detached, &error_context);
  vm_context_set_error_flags(detached, 1, 1);

  ASSERT_EQ(current_object, master_ob);
  ASSERT_EQ(command_giver, nullptr);
  ASSERT_EQ(current_interactive, nullptr);
  ASSERT_EQ(previous_ob, nullptr);
  ASSERT_EQ(current_prog, nullptr);
  ASSERT_EQ(caller_type, 0);
  ASSERT_EQ(call_origin, 0);
  ASSERT_EQ(function_index_offset, 0);
  ASSERT_EQ(variable_index_offset, 0);
#ifdef DEBUG
  ASSERT_EQ(stack_in_use_as_temporary, 0);
#endif
  ASSERT_EQ(current_error_context, nullptr);
  ASSERT_EQ(too_deep_error, 0);
  ASSERT_EQ(max_eval_error, 0);

  ASSERT_EQ(detached.execution.current_object, first);
  ASSERT_EQ(detached.execution.command_giver, first);
  ASSERT_EQ(detached.execution.current_interactive, first);
  ASSERT_EQ(detached.execution.previous_ob, second);
  ASSERT_EQ(detached.execution.current_prog, first->prog);
  ASSERT_EQ(detached.execution.caller_type, ORIGIN_DRIVER);
  ASSERT_EQ(detached.execution.call_origin, ORIGIN_EFUN);
  ASSERT_EQ(detached.execution.function_index_offset, 9);
  ASSERT_EQ(detached.execution.variable_index_offset, 13);
  ASSERT_EQ(detached.execution.stack_in_use_as_temporary, 4);
  ASSERT_EQ(detached.error.current_error_context, &error_context);
  ASSERT_EQ(detached.error.too_deep_error, 1);
  ASSERT_EQ(detached.error.max_eval_error, 1);

  VMExecutionState applied;
  applied.current_object = second;
  applied.current_prog = second->prog;
  applied.previous_ob = first;
  applied.caller_type = ORIGIN_CALL_OTHER;
  vm_context_apply_execution(detached, applied);
  ASSERT_EQ(current_object, master_ob);
  ASSERT_EQ(current_prog, nullptr);
  ASSERT_EQ(previous_ob, nullptr);
  ASSERT_EQ(caller_type, 0);
  ASSERT_EQ(detached.execution.current_object, second);
  ASSERT_EQ(detached.execution.current_prog, second->prog);
  ASSERT_EQ(detached.execution.previous_ob, first);
  ASSERT_EQ(detached.execution.caller_type, ORIGIN_CALL_OTHER);

  detached.execution.current_object = first;
  vm_context_sync_execution(detached);
  ASSERT_EQ(detached.execution.current_object, first);

  vm_context_reset_execution(detached);
  ASSERT_EQ(current_object, master_ob);
  ASSERT_EQ(detached.execution.current_object, nullptr);
  ASSERT_EQ(detached.execution.current_prog, nullptr);
}

TEST_F(DriverTest, TestVmCurrentInteractiveScopeRestoresState) {
  object_t* obj = find_object("single/master.c");
  ASSERT_NE(obj, nullptr);

  current_interactive = nullptr;
  vm_context_sync_execution(vm_context());

  {
    VMCurrentInteractiveScope scope(vm_context(), obj);
    ASSERT_EQ(current_interactive, obj);
    ASSERT_EQ(vm_context().execution.current_interactive, obj);
  }

  ASSERT_EQ(current_interactive, nullptr);
  ASSERT_EQ(vm_context().execution.current_interactive, nullptr);
}

TEST_F(DriverTest, TestVmCommandGiverStackSyncsContext) {
  object_t* first = find_object("single/master.c");
  object_t* second = find_object("single/simul_efun.c");
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);

  set_command_giver(nullptr);
  ASSERT_EQ(command_giver, nullptr);
  ASSERT_EQ(vm_context().execution.command_giver, nullptr);

  save_command_giver(first);
  ASSERT_EQ(command_giver, first);
  ASSERT_EQ(vm_context().execution.command_giver, first);

  save_command_giver(second);
  ASSERT_EQ(command_giver, second);
  ASSERT_EQ(vm_context().execution.command_giver, second);

  restore_command_giver();
  ASSERT_EQ(command_giver, first);
  ASSERT_EQ(vm_context().execution.command_giver, first);

  restore_command_giver();
  ASSERT_EQ(command_giver, nullptr);
  ASSERT_EQ(vm_context().execution.command_giver, nullptr);
}

TEST_F(DriverTest, TestVmExecutionFrameSettersSyncContext) {
  object_t* first = find_object("single/master.c");
  object_t* second = find_object("single/simul_efun.c");
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  ASSERT_NE(first->prog, nullptr);
  ASSERT_NE(second->prog, nullptr);

  vm_context_set_execution_frame(vm_context(), first, first->prog, second, ORIGIN_DRIVER);
  ASSERT_EQ(current_object, first);
  ASSERT_EQ(current_prog, first->prog);
  ASSERT_EQ(previous_ob, second);
  ASSERT_EQ(caller_type, ORIGIN_DRIVER);
  ASSERT_EQ(vm_context().execution.current_object, first);
  ASSERT_EQ(vm_context().execution.current_prog, first->prog);
  ASSERT_EQ(vm_context().execution.previous_ob, second);
  ASSERT_EQ(vm_context().execution.caller_type, ORIGIN_DRIVER);

  vm_context_set_current_object(vm_context(), second);
  vm_context_set_current_program(vm_context(), second->prog);
  vm_context_set_previous_object(vm_context(), first);
  vm_context_set_caller_type(vm_context(), ORIGIN_LOCAL);
  vm_context_set_call_origin(vm_context(), ORIGIN_CALL_OTHER);
  vm_context_set_inherit_offsets(vm_context(), 3, 5);
  vm_context_set_stack_temporary_depth(vm_context(), 2);
  ASSERT_EQ(current_object, second);
  ASSERT_EQ(current_prog, second->prog);
  ASSERT_EQ(previous_ob, first);
  ASSERT_EQ(caller_type, ORIGIN_LOCAL);
  ASSERT_EQ(call_origin, ORIGIN_CALL_OTHER);
  ASSERT_EQ(function_index_offset, 3);
  ASSERT_EQ(variable_index_offset, 5);
#ifdef DEBUG
  ASSERT_EQ(stack_in_use_as_temporary, 2);
#endif
  ASSERT_EQ(vm_context().execution.current_object, second);
  ASSERT_EQ(vm_context().execution.current_prog, second->prog);
  ASSERT_EQ(vm_context().execution.previous_ob, first);
  ASSERT_EQ(vm_context().execution.caller_type, ORIGIN_LOCAL);
  ASSERT_EQ(vm_context().execution.call_origin, ORIGIN_CALL_OTHER);
  ASSERT_EQ(vm_context().execution.function_index_offset, 3);
  ASSERT_EQ(vm_context().execution.variable_index_offset, 5);
  ASSERT_EQ(vm_context().execution.stack_in_use_as_temporary, 2);

  vm_context_adjust_stack_temporary_depth(vm_context(), -1);
#ifdef DEBUG
  ASSERT_EQ(stack_in_use_as_temporary, 1);
#endif
  ASSERT_EQ(vm_context().execution.stack_in_use_as_temporary, 1);

  vm_context_set_execution_frame(vm_context(), master_ob, nullptr, nullptr, 0);
  vm_context_set_call_origin(vm_context(), 0);
  vm_context_set_inherit_offsets(vm_context(), 0, 0);
  vm_context_set_stack_temporary_depth(vm_context(), 0);
}

TEST_F(DriverTest, TestVmErrorContextStackSyncsContext) {
  vm_context_set_current_error_context(vm_context(), nullptr);
  ASSERT_EQ(current_error_context, nullptr);
  ASSERT_EQ(vm_context().error.current_error_context, nullptr);

  error_context_t first{};
  error_context_t second{};
  save_context(&first);
  ASSERT_EQ(current_error_context, &first);
  ASSERT_EQ(vm_context().error.current_error_context, &first);
  ASSERT_EQ(first.save_context, nullptr);

  save_context(&second);
  ASSERT_EQ(current_error_context, &second);
  ASSERT_EQ(vm_context().error.current_error_context, &second);
  ASSERT_EQ(second.save_context, &first);

  pop_context(&second);
  ASSERT_EQ(current_error_context, &first);
  ASSERT_EQ(vm_context().error.current_error_context, &first);

  pop_context(&first);
  ASSERT_EQ(current_error_context, nullptr);
  ASSERT_EQ(vm_context().error.current_error_context, nullptr);
}

TEST_F(DriverTest, TestVmErrorFlagsSyncContext) {
  vm_context_set_error_flags(vm_context(), 0, 0);
  vm_context_set_error_depths(vm_context(), 0, 0);
  ASSERT_EQ(too_deep_error, 0);
  ASSERT_EQ(max_eval_error, 0);
  ASSERT_EQ(vm_context().error.too_deep_error, 0);
  ASSERT_EQ(vm_context().error.max_eval_error, 0);
  ASSERT_EQ(vm_context().error.error_depth, 0);
  ASSERT_EQ(vm_context().error.mudlib_error_depth, 0);

  vm_context_set_too_deep_error(vm_context(), 1);
  ASSERT_EQ(too_deep_error, 1);
  ASSERT_EQ(max_eval_error, 0);
  ASSERT_EQ(vm_context().error.too_deep_error, 1);
  ASSERT_EQ(vm_context().error.max_eval_error, 0);

  vm_context_set_max_eval_error(vm_context(), 1);
  ASSERT_EQ(too_deep_error, 1);
  ASSERT_EQ(max_eval_error, 1);
  ASSERT_EQ(vm_context().error.too_deep_error, 1);
  ASSERT_EQ(vm_context().error.max_eval_error, 1);

  vm_context_adjust_error_depth(vm_context(), 1);
  vm_context_adjust_mudlib_error_depth(vm_context(), 2);
  ASSERT_EQ(vm_context().error.error_depth, 1);
  ASSERT_EQ(vm_context().error.mudlib_error_depth, 2);

  clear_state();
  ASSERT_EQ(too_deep_error, 0);
  ASSERT_EQ(max_eval_error, 0);
  ASSERT_EQ(vm_context().error.too_deep_error, 0);
  ASSERT_EQ(vm_context().error.max_eval_error, 0);
  ASSERT_EQ(vm_context().error.error_depth, 0);
  ASSERT_EQ(vm_context().error.mudlib_error_depth, 0);
}

TEST_F(DriverTest, TestVmObjectLifecycleStateSyncsContext) {
  object_t* obj = find_object("single/master.c");
  ASSERT_NE(obj, nullptr);

  vm_context_set_load_object_depth(vm_context(), 0);
  vm_context_set_restricted_destruct_object(vm_context(), nullptr);
  ASSERT_EQ(vm_context().object_store.load_object_depth, 0);
  ASSERT_EQ(vm_context().object_store.restricted_destruct_object, nullptr);

  vm_context_adjust_load_object_depth(vm_context(), 2);
  ASSERT_EQ(vm_context().object_store.load_object_depth, 2);

  vm_context_adjust_load_object_depth(vm_context(), -1);
  ASSERT_EQ(vm_context().object_store.load_object_depth, 1);

  vm_context_set_restricted_destruct_object(vm_context(), obj);
  ASSERT_EQ(vm_context().object_store.restricted_destruct_object, obj);

  clear_state();
  ASSERT_EQ(vm_context().object_store.load_object_depth, 0);
  ASSERT_EQ(vm_context().object_store.restricted_destruct_object, nullptr);
}

TEST_F(DriverTest, TestVmContextThreadScopeBindsThreadLocalContext) {
  auto* main_context = &vm_context();
  ASSERT_EQ(main_context, &vm_main_context());
  ASSERT_FALSE(vm_context_is_owner_controlled_lpc());

  bool worker_bound = false;
  bool worker_owner_controlled = false;
  uint64_t worker_gametick = 0;
  std::thread worker([&] {
    VMContext worker_context;
    VMContextThreadScope scope(worker_context);
    vm_context_set_current_gametick(vm_context(), 777);
    vm_context().owner.controlled_lpc_active = true;
    worker_bound = &vm_context() == &worker_context && &vm_context() != main_context;
    worker_owner_controlled = vm_context_is_owner_controlled_lpc();
    worker_gametick = worker_context.current_gametick;
  });
  worker.join();

  ASSERT_TRUE(worker_bound);
  ASSERT_TRUE(worker_owner_controlled);
  ASSERT_EQ(worker_gametick, 777u);
  ASSERT_EQ(&vm_context(), main_context);
}

TEST_F(DriverTest, TestEvalLimitExpiryFlagIsThreadLocal) {
  outoftime = 1;
  bool worker_default = true;
  std::thread worker([&] {
    worker_default = outoftime != 0;
    outoftime = 0;
  });
  worker.join();

  ASSERT_FALSE(worker_default);
  ASSERT_EQ(outoftime, 1);
  outoftime = 0;
}

#ifdef __linux__
TEST_F(DriverTest, TestEvalLimitTimersAreIndependentAcrossThreads) {
  constexpr uint64_t kMainDeadlineMicros = 3'600'000'000ULL;
  constexpr uint64_t kWorkerDeadlineMicros = 7'200'000'000ULL;
  constexpr uint64_t kSchedulingToleranceMicros = 100'000'000ULL;

  set_eval(kMainDeadlineMicros);
  uint64_t worker_remaining = 0;
  std::thread worker([&] {
    set_eval(kWorkerDeadlineMicros);
    worker_remaining = static_cast<uint64_t>(get_eval());
  });
  worker.join();
  const auto main_remaining = static_cast<uint64_t>(get_eval());
  set_eval(max_eval_cost);

  ASSERT_GT(worker_remaining, kWorkerDeadlineMicros - kSchedulingToleranceMicros);
  ASSERT_LE(worker_remaining, kWorkerDeadlineMicros);
  ASSERT_GT(main_remaining, kMainDeadlineMicros - kSchedulingToleranceMicros);
  ASSERT_LE(main_remaining, kMainDeadlineMicros);
}

TEST_F(DriverTest, TestEvalLimitSignalExpiresOnlyTheTargetThread) {
  constexpr uint64_t kMainDeadlineMicros = 3'600'000'000ULL;
  constexpr uint64_t kWorkerDeadlineMicros = 25'000ULL;

  outoftime = 0;
  set_eval(kMainDeadlineMicros);
  bool worker_expired = false;
  std::thread worker([&] {
    set_eval(kWorkerDeadlineMicros);
    const auto wait_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!outoftime && std::chrono::steady_clock::now() < wait_deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    worker_expired = outoftime != 0;
  });
  worker.join();
  const bool main_expired = outoftime != 0;
  set_eval(max_eval_cost);
  outoftime = 0;

  ASSERT_TRUE(worker_expired);
  ASSERT_FALSE(main_expired);
}
#endif

TEST_F(DriverTest, TestVmContextObjectStoreRemainsMainThreadOwned) {
  auto* main_context = &vm_context();
  vm_context_sync_object_store(*main_context);
  ASSERT_TRUE(main_context->object_store.main_thread_owned);
  ASSERT_EQ(main_context->object_store.objects, obj_list);

  auto before_rejections = vm_context_object_store_sync_rejections();
  bool worker_store_rejected = false;
  uint64_t worker_rejections = 0;
  std::thread worker([&] {
    VMContext worker_context;
    VMContextThreadScope scope(worker_context);
    vm_context_sync_object_store(vm_context());
    worker_store_rejected = !worker_context.object_store.main_thread_owned &&
                            worker_context.object_store.objects == nullptr;
    worker_rejections = worker_context.object_store.sync_rejections;
  });
  worker.join();

  ASSERT_TRUE(worker_store_rejected);
  ASSERT_EQ(worker_rejections, 1u);
  ASSERT_EQ(vm_context_object_store_sync_rejections(), before_rejections + 1);
  ASSERT_TRUE(main_context->object_store.main_thread_owned);
  ASSERT_EQ(main_context->object_store.objects, obj_list);
}

TEST_F(DriverTest, TestRemoveDestructedObjectsBoundedDrainsIncrementally) {
  remove_destructed_objects();
  ASSERT_EQ(vm_destructed_object_backlog_size(), 0u);
  ASSERT_NE(load_object_for_test("single/void"), nullptr);

  std::vector<object_t*> objects;
  for (int i = 0; i < 3; i++) {
    auto* object = clone_object_for_test("single/void");
    ASSERT_NE(object, nullptr);
    objects.push_back(object);
  }
  for (auto* object : objects) {
    destruct_object_for_test(object);
  }

  ASSERT_EQ(vm_destructed_object_backlog_size(), 3u);
  auto cleanup_total_before = vm_destructed_object_cleanup_total();

  ASSERT_EQ(remove_destructed_objects_bounded(2), 2u);
  ASSERT_EQ(vm_destructed_object_backlog_size(), 1u);
  ASSERT_EQ(vm_context().object_store.destructed_objects, obj_list_destruct);

  ASSERT_EQ(remove_destructed_objects_bounded(2), 1u);
  ASSERT_EQ(vm_destructed_object_backlog_size(), 0u);
  ASSERT_EQ(vm_destructed_object_cleanup_total(), cleanup_total_before + 3);
  ASSERT_GE(vm_destructed_object_cleanup_batches(), 2u);
  ASSERT_EQ(vm_destructed_object_cleanup_last_removed(), 1u);

  auto* runtime = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(runtime, "destructed_object_backlog"), 0);
  ASSERT_GE(mapping_number(runtime, "destructed_object_cleanup_total"), 3);
  ASSERT_EQ(mapping_number(runtime, "destructed_object_incremental_cleanup_ready"), 1);
  free_mapping(runtime);
}

TEST_F(DriverTest, TestVmOwnerScopeBindsAndRestoresCurrentOwner) {
  auto* context = &vm_context();
  vm_context_set_current_owner(*context, "owner/test/original", 7);

  {
    VMOwnerScope scope(*context, "owner/test/scoped", 8);
    ASSERT_EQ(context->owner.current_owner_id, "owner/test/scoped");
    ASSERT_EQ(context->owner.current_owner_epoch, 8u);
  }

  ASSERT_EQ(context->owner.current_owner_id, "owner/test/original");
  ASSERT_EQ(context->owner.current_owner_epoch, 7u);
  vm_context_set_current_owner(*context, "", 0);
}

TEST_F(DriverTest, TestVmOwnerMetadataDefaultsAndChecks) {
  ScopedCurrentObjectAsMaster master_scope;
  object_t* obj = find_object("single/master.c");
  ASSERT_NE(obj, nullptr);

  ASSERT_STREQ(vm_owner_default_id(), vm_owner_id(obj));
  auto default_epoch = vm_owner_epoch(obj);
  vm_owner_set_id(obj, "owner/test");
  ASSERT_STREQ("owner/test", vm_owner_id(obj));
  ASSERT_EQ(vm_owner_epoch(obj), default_epoch + 1);
  ASSERT_TRUE(vm_owner_matches(obj, "owner/test"));

  auto before_total = vm_owner_total_checks();
  auto before_mismatch = vm_owner_mismatch_checks();
  vm_owner_record_check(obj, "owner/other", false);
  ASSERT_EQ(vm_owner_total_checks(), before_total + 1);
  ASSERT_EQ(vm_owner_mismatch_checks(), before_mismatch + 1);

  vm_owner_set_id(obj, vm_owner_default_id());
  ASSERT_STREQ(vm_owner_default_id(), vm_owner_id(obj));
  ASSERT_FALSE(vm_owner_has_explicit_id(obj));
  ASSERT_EQ(vm_owner_epoch(obj), default_epoch + 2);
}

TEST_F(DriverTest, TestVmOwnerQueryObjectSnapshotOnlyForCrossOwnerTargets) {
  object_t* source = find_object("single/master.c");
  object_t* target = find_object("single/simul_efun.c");
  ASSERT_NE(source, nullptr);
  ASSERT_NE(target, nullptr);


  current_object = source;
  vm_owner_set_id(source, "owner/test/snapshot/source");
  vm_owner_set_id(target, "owner/test/snapshot/source");
  ASSERT_EQ(vm_owner_query_object_snapshot(target, vm_owner_id(source)), nullptr);

  vm_owner_clear_id(target);
  ASSERT_EQ(vm_owner_query_object_snapshot(target, vm_owner_id(source)), nullptr);

  vm_owner_set_id(target, "owner/test/snapshot/target");
  auto* snapshot = vm_owner_query_object_snapshot(target, vm_owner_id(source));
  ASSERT_NE(snapshot, nullptr);
  ASSERT_STREQ(mapping_string(snapshot, "object_name"), target->obname);
  ASSERT_STREQ(mapping_string(snapshot, "owner_id"), "owner/test/snapshot/target");
  ASSERT_EQ(mapping_number(snapshot, "living"), 0);
  ASSERT_EQ(mapping_number(snapshot, "living_flag"), 0);
  ASSERT_EQ(mapping_number(snapshot, "has_is_npc"), 0);
  ASSERT_EQ(mapping_number(snapshot, "has_is_player"), 0);
  ASSERT_EQ(mapping_number(snapshot, "has_is_character"), 0);
  free_mapping(snapshot);

  vm_owner_clear_id(source);
  vm_owner_clear_id(target);
}

TEST_F(DriverTest, TestLoadedSingletonUsesDefaultOwnerInsideOwnerScope) {
  if (auto* existing = find_object2("single/owner_singleton.c")) {
    destruct_object_for_test(existing);
  }

  VMOwnerScope scope(vm_context(), "owner/test/player", 1);
  ScopedCurrentObjectAsMaster master_scope;
  auto* obj = load_object_for_test("single/owner_singleton.c");

  ASSERT_NE(obj, nullptr);
  ASSERT_STREQ(vm_owner_default_id(), vm_owner_id(obj));
  destruct_object_for_test(obj);
}

TEST_F(DriverTest, TestCommandSingletonUsesDefaultOwnerInsidePlayerOwnerScope) {
  if (auto* existing = find_object2("command/refs.c")) {
    destruct_object_for_test(existing);
  }
  ScopedCurrentObjectAsMaster master_scope;
  auto* player = clone_object_for_test("single/owner_singleton");
  ASSERT_NE(player, nullptr);
  vm_owner_set_id(player, "owner/test/command/player");

  VMOwnerScope scope(vm_context(), vm_owner_id(player), vm_owner_epoch(player));
  current_object = player;
  auto* command = load_object_for_test("command/refs.c");

  ASSERT_NE(command, nullptr);
  ASSERT_STREQ(vm_owner_default_id(), vm_owner_id(command));
  ASSERT_NE(vm_owner_id(command), vm_owner_id(player));

  destruct_object_for_test(command);
  destruct_object_for_test(player);
}

TEST_F(DriverTest, TestStdServiceUsesDefaultOwnerInsidePlayerOwnerScope) {
  if (auto* existing = find_object2("std/database.c")) {
    destruct_object_for_test(existing);
  }
  ScopedCurrentObjectAsMaster master_scope;
  auto* player = clone_object_for_test("single/owner_singleton");
  ASSERT_NE(player, nullptr);
  vm_owner_set_id(player, "owner/test/std-service/player");

  VMOwnerScope scope(vm_context(), vm_owner_id(player), vm_owner_epoch(player));
  current_object = player;
  auto* service = load_object_for_test("std/database.c");

  ASSERT_NE(service, nullptr);
  ASSERT_STREQ(vm_owner_default_id(), vm_owner_id(service));
  ASSERT_NE(vm_owner_id(service), vm_owner_id(player));

  destruct_object_for_test(service);
  destruct_object_for_test(player);
}

TEST_F(DriverTest, TestSharedStdServicesUseDefaultOwnerInsidePlayerOwnerScope) {
  const char* service_paths[] = {"std/http.c", "std/present_clone.c", "std/telnet.c"};
  for (auto* path : service_paths) {
    if (auto* existing = find_object2(path)) {
      destruct_object_for_test(existing);
    }
  }

  ScopedCurrentObjectAsMaster master_scope;
  auto* player = clone_object_for_test("single/owner_singleton");
  ASSERT_NE(player, nullptr);
  vm_owner_set_id(player, "owner/test/shared-service/player");

  VMOwnerScope scope(vm_context(), vm_owner_id(player), vm_owner_epoch(player));
  current_object = player;

  for (auto* path : service_paths) {
    auto* service = load_object_for_test(path);
    ASSERT_NE(service, nullptr) << path;
    ASSERT_STREQ(vm_owner_default_id(), vm_owner_id(service)) << path;
    ASSERT_NE(vm_owner_id(service), vm_owner_id(player)) << path;
  }

  for (auto* path : service_paths) {
    if (auto* service = find_object2(path)) {
      destruct_object_for_test(service);
    }
  }
  destruct_object_for_test(player);
}

TEST_F(DriverTest, TestStdHelperServicesUseDefaultOwnerInsidePlayerOwnerScope) {
  const char* helper_paths[] = {
      "std/base64.c",  "std/break_string.c", "std/element_of_weighted.c", "std/reduce.c",
      "std/highest.c", "std/lowest.c",       "std/number_string.c",       "std/sum.c"};
  for (auto* path : helper_paths) {
    if (auto* existing = find_object2(path)) {
      destruct_object_for_test(existing);
    }
  }

  ScopedCurrentObjectAsMaster master_scope;
  auto* player = clone_object_for_test("single/owner_singleton");
  ASSERT_NE(player, nullptr);
  vm_owner_set_id(player, "owner/test/std-helper/player");

  VMOwnerScope scope(vm_context(), vm_owner_id(player), vm_owner_epoch(player));
  current_object = player;

  for (auto* path : helper_paths) {
    auto* service = load_object_for_test(path);
    ASSERT_NE(service, nullptr) << path;
    ASSERT_STREQ(vm_owner_default_id(), vm_owner_id(service)) << path;
    ASSERT_NE(vm_owner_id(service), vm_owner_id(player)) << path;

    auto handle = vm_object_handle(service);
    ASSERT_TRUE(handle.valid) << path;
    ASSERT_FALSE(handle.object_path.empty()) << path;
    ASSERT_EQ(vm_object_store_owner_resolve(vm_owner_default_id(), handle.object_id), service)
        << path;
    ASSERT_EQ(vm_object_store_owner_resolve(vm_owner_id(player), handle.object_id), nullptr)
        << path;
    ASSERT_EQ(vm_object_store_owner_path_resolve(vm_owner_default_id(), handle.object_path.c_str()),
              service)
        << path;
    ASSERT_EQ(vm_object_store_owner_path_resolve(vm_owner_id(player), handle.object_path.c_str()),
              nullptr)
        << path;
  }

  for (auto* path : helper_paths) {
    if (auto* service = find_object2(path)) {
      destruct_object_for_test(service);
    }
  }
  destruct_object_for_test(player);
}

TEST_F(DriverTest, TestSimulEfunSingletonKeepsDefaultOwnerInsidePlayerOwnerScope) {
  ScopedCurrentObjectAsMaster master_scope;
  auto* player = clone_object_for_test("single/owner_singleton");
  ASSERT_NE(player, nullptr);
  vm_owner_set_id(player, "owner/test/simul-efun/player");

  VMOwnerScope scope(vm_context(), vm_owner_id(player), vm_owner_epoch(player));
  current_object = player;

  const char* singleton_paths[] = {"single/simul_efun.c", "std/all_environment.c", "std/json.c"};
  for (auto* path : singleton_paths) {
    auto* service = load_object_for_test(path);
    ASSERT_NE(service, nullptr) << path;
    ASSERT_STREQ(vm_owner_default_id(), vm_owner_id(service)) << path;
    ASSERT_NE(vm_owner_id(service), vm_owner_id(player)) << path;
  }

  destruct_object_for_test(player);
}

TEST_F(DriverTest, TestVirtualObjectUsesDefaultOwnerAndUpdatesStorePath) {
  if (auto* existing = find_object2("test/virtual")) {
    destruct_object_for_test(existing);
  }
  if (auto* source = find_object2("single/void.c")) {
    destruct_object_for_test(source);
  }


  ScopedCurrentObjectAsMaster master_scope;
  auto* player = clone_object_for_test("single/owner_singleton");
  ASSERT_NE(player, nullptr);
  vm_owner_set_id(player, "owner/test/virtual/player");

  VMOwnerScope scope(vm_context(), vm_owner_id(player), vm_owner_epoch(player));
  current_object = player;
  auto* virtual_object = load_object_for_test("test/virtual");
  ASSERT_NE(virtual_object, nullptr);
  ASSERT_TRUE((virtual_object->flags & O_VIRTUAL) != 0);
  ASSERT_STREQ("test/virtual", virtual_object->obname);
  ASSERT_STREQ(vm_owner_default_id(), vm_owner_id(virtual_object));
  ASSERT_NE(vm_owner_id(virtual_object), vm_owner_id(player));

  auto handle = vm_object_handle(virtual_object);
  ASSERT_TRUE(handle.valid);
  auto handle_resolve = vm_object_handle_resolve_status(handle);
  ASSERT_EQ(handle_resolve.object, virtual_object);
  ASSERT_STREQ(vm_object_handle_resolve_status_name(handle_resolve.status), "current");
  ASSERT_TRUE(handle_resolve.resolved_via_owner_local_store);
  ASSERT_TRUE(handle_resolve.owner_local_fast_path_used);
  ASSERT_FALSE(handle_resolve.global_live_object_found);
  ASSERT_FALSE(handle_resolve.resolved_via_global_index);
  ASSERT_EQ(vm_object_handle_resolve(handle), virtual_object);
  ASSERT_STREQ(handle.object_path.c_str(), "test/virtual");
  ASSERT_EQ(vm_object_store_owner_resolve(vm_owner_default_id(), handle.object_id), virtual_object);
  ASSERT_EQ(vm_object_store_owner_path_resolve(vm_owner_default_id(), "test/virtual"),
            virtual_object);
  auto* handle_status = vm_object_handle_status(virtual_object);
  ASSERT_EQ(mapping_number(handle_status, "current"), 1);
  ASSERT_STREQ(mapping_string(handle_status, "resolve_status"), "current");
  ASSERT_EQ(mapping_number(handle_status, "resolved_via_owner_local_store"), 1);
  ASSERT_EQ(mapping_number(handle_status, "owner_local_fast_path_used"), 1);
  ASSERT_STREQ(mapping_string(handle_status, "owner_local_fast_path_lock_model"),
               "shared_mutex_read_lock");
  ASSERT_EQ(mapping_number(handle_status, "owner_local_fast_path_global_fallback"), 0);
  ASSERT_EQ(mapping_number(handle_status, "diagnosed_via_owner_local_store"), 0);
  ASSERT_EQ(mapping_number(handle_status, "diagnosed_via_owner_local_path_index"), 0);
  ASSERT_EQ(mapping_number(handle_status, "owner_local_object_pointer_index_found"), 1);
  ASSERT_STREQ(mapping_string(handle_status, "owner_local_object_pointer_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(handle_status, "global_live_object_found"), 0);
  ASSERT_STREQ(mapping_string(handle_status, "global_live_object_source"), "");
  auto handle_live_object_bridge_ready =
      mapping_number(handle_status, "global_live_object_bridge_retirement_ready");
  ASSERT_TRUE(handle_live_object_bridge_ready == 0 || handle_live_object_bridge_ready == 1);
  ASSERT_EQ(mapping_number(handle_status, "global_live_object_fallback_skipped"), 0);
  ASSERT_STREQ(mapping_string(handle_status, "global_live_object_fallback_reason"), "");
  ASSERT_EQ(mapping_number(handle_status, "global_record_found"), 0);
  ASSERT_STREQ(mapping_string(handle_status, "global_record_source"), "");
  ASSERT_EQ(mapping_number(handle_status, "global_record_id_scan_bridge_used"), 0);
  ASSERT_EQ(mapping_number(handle_status, "global_record_id_scan_bridge_found"), 0);
  ASSERT_STREQ(mapping_string(handle_status, "global_record_id_scan_bridge_source"), "");
  ASSERT_EQ(mapping_number(handle_status, "global_record_id_scan_bridge_skipped"), 0);
  ASSERT_STREQ(mapping_string(handle_status, "global_record_id_scan_bridge_skip_reason"), "");
  ASSERT_EQ(mapping_number(handle_status, "global_record_pointer_bridge_used"), 0);
  ASSERT_EQ(mapping_number(handle_status, "global_record_pointer_bridge_found"), 0);
  ASSERT_STREQ(mapping_string(handle_status, "global_record_pointer_bridge_source"), "");
  ASSERT_EQ(mapping_number(handle_status, "global_record_pointer_bridge_skipped"), 0);
  ASSERT_STREQ(mapping_string(handle_status, "global_record_pointer_bridge_skip_reason"), "");
  auto handle_record_bridge_ready =
      mapping_number(handle_status, "global_record_bridge_retirement_ready");
  ASSERT_TRUE(handle_record_bridge_ready == 0 || handle_record_bridge_ready == 1);
  ASSERT_EQ(mapping_number(handle_status, "global_record_fallback_skipped"), 0);
  ASSERT_STREQ(mapping_string(handle_status, "global_record_fallback_reason"), "");
  ASSERT_EQ(mapping_number(handle_status, "resolved_via_global_index"), 0);
  free_mapping(handle_status);

  auto* owner_status = vm_object_store_owner_status(vm_owner_default_id());
  auto* directory = find_string_in_mapping(owner_status, "object_directory");
  ASSERT_NE(directory, nullptr);
  ASSERT_EQ(directory->type, T_ARRAY);
  bool found_virtual_record = false;
  for (int i = 0; i < directory->u.arr->size; i++) {
    auto* record = directory->u.arr->item[i].u.map;
    if (std::string(mapping_string(record, "object_path")) == "test/virtual") {
      found_virtual_record = true;
      ASSERT_EQ(mapping_number(record, "object_id"), static_cast<long>(handle.object_id));
      ASSERT_STREQ(mapping_string(record, "owner_id"), vm_owner_default_id());
      ASSERT_EQ(mapping_number(record, "owner_epoch"),
                static_cast<long>(vm_owner_epoch(virtual_object)));
      ASSERT_EQ(mapping_number(record, "destructed"), 0);
      ASSERT_EQ(mapping_number(record, "live"), 1);
      break;
    }
  }
  ASSERT_TRUE(found_virtual_record);
  free_mapping(owner_status);
  auto* virtual_lookup =
      vm_object_store_owner_lookup_status(vm_owner_default_id(), handle.object_id);
  ASSERT_EQ(mapping_number(virtual_lookup, "success"), 1);
  ASSERT_EQ(mapping_number(virtual_lookup, "found"), 1);
  ASSERT_EQ(mapping_number(virtual_lookup, "owner_local_object_ref_found"), 1);
  ASSERT_STREQ(mapping_string(virtual_lookup, "owner_local_object_ref_source"),
               "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(virtual_lookup, "owner_local_object_ref_index_found"), 1);
  ASSERT_STREQ(mapping_string(virtual_lookup, "owner_local_object_ref_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(virtual_lookup, "owner_local_object_pointer_index_found"), 1);
  ASSERT_STREQ(mapping_string(virtual_lookup, "owner_local_object_pointer_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(virtual_lookup, "owner_local_resolve_found"), 1);
  ASSERT_STREQ(mapping_string(virtual_lookup, "owner_local_resolve_source"),
               "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(virtual_lookup, "owner_local_path_index_found"), 1);
  ASSERT_EQ(mapping_number(virtual_lookup, "owner_local_destructed_path_index_found"), 0);
  ASSERT_STREQ(mapping_string(virtual_lookup, "owner_local_path_index_source"),
               "vm_object_shard.object_path_index");
  ASSERT_STREQ(mapping_string(virtual_lookup, "object_path"), "test/virtual");
  free_mapping(virtual_lookup);
  auto* virtual_path_lookup =
      vm_object_store_owner_path_lookup_status(vm_owner_default_id(), "test/virtual");
  ASSERT_EQ(mapping_number(virtual_path_lookup, "success"), 1);
  ASSERT_EQ(mapping_number(virtual_path_lookup, "record_found"), 1);
  ASSERT_EQ(mapping_number(virtual_path_lookup, "found"), 1);
  ASSERT_EQ(mapping_number(virtual_path_lookup, "object_id"), static_cast<long>(handle.object_id));
  ASSERT_EQ(mapping_number(virtual_path_lookup, "owner_local_object_ref_found"), 1);
  ASSERT_STREQ(mapping_string(virtual_path_lookup, "owner_local_object_ref_source"),
               "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(virtual_path_lookup, "owner_local_object_ref_index_found"), 1);
  ASSERT_STREQ(mapping_string(virtual_path_lookup, "owner_local_object_ref_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(virtual_path_lookup, "owner_local_object_pointer_index_found"), 1);
  ASSERT_STREQ(mapping_string(virtual_path_lookup, "owner_local_object_pointer_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(virtual_path_lookup, "owner_local_resolve_found"), 1);
  ASSERT_STREQ(mapping_string(virtual_path_lookup, "owner_local_resolve_source"),
               "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(virtual_path_lookup, "owner_local_path_index_found"), 1);
  ASSERT_EQ(mapping_number(virtual_path_lookup, "owner_local_destructed_path_index_found"), 0);
  ASSERT_STREQ(mapping_string(virtual_path_lookup, "owner_local_path_index_source"),
               "vm_object_shard.object_path_index");
  ASSERT_STREQ(mapping_string(virtual_path_lookup, "record_owner_id"), vm_owner_default_id());
  free_mapping(virtual_path_lookup);

  destruct_object_for_test(virtual_object);
  destruct_object_for_test(player);
}

TEST_F(DriverTest, TestCloneOwnerUsesCurrentObjectNotAmbientScope) {
  if (auto* existing = find_object2("single/owner_singleton.c")) {
    destruct_object_for_test(existing);
  }
  auto* prototype = load_object_for_test("single/owner_singleton.c");
  ASSERT_NE(prototype, nullptr);

  VMOwnerScope scope(vm_context(), "owner/test/player", 1);
  ScopedCurrentObjectAsMaster master_scope;
  auto* shared_clone = clone_object_for_test("single/owner_singleton");
  ASSERT_NE(shared_clone, nullptr);
  ASSERT_STREQ(vm_owner_default_id(), vm_owner_id(shared_clone));
  destruct_object_for_test(shared_clone);

  vm_owner_set_id(prototype, "owner/test/factory");
  current_object = prototype;
  auto* owned_clone = clone_object_for_test("single/owner_singleton");
  ASSERT_NE(owned_clone, nullptr);
  ASSERT_STREQ("owner/test/factory", vm_owner_id(owned_clone));
  destruct_object_for_test(owned_clone);
  destruct_object_for_test(prototype);
}

TEST_F(DriverTest, TestMoveObjectOwnerInheritanceRespectsExplicitOwner) {
  auto* dest = find_object("single/simul_efun.c");
  ASSERT_NE(dest, nullptr);

  ScopedCurrentObjectAsMaster master_scope;
  auto* inherited_item = clone_object_for_test("single/owner_singleton");
  ASSERT_NE(inherited_item, nullptr);
  vm_owner_clear_id(inherited_item);
  auto inherited_epoch = vm_owner_epoch(inherited_item);
  ASSERT_FALSE(vm_owner_has_explicit_id(inherited_item));

  vm_owner_set_id(dest, "owner/test/move/inherit-dest");
  move_object(inherited_item, dest);
  ASSERT_TRUE(vm_owner_has_explicit_id(inherited_item));
  ASSERT_STREQ("owner/test/move/inherit-dest", vm_owner_id(inherited_item));
  ASSERT_GT(vm_owner_epoch(inherited_item), inherited_epoch);
  destruct_object_for_test(inherited_item);

  auto* explicit_item = clone_object_for_test("single/owner_singleton");
  ASSERT_NE(explicit_item, nullptr);
  vm_owner_set_id(explicit_item, "owner/test/move/explicit-item");
  auto explicit_epoch = vm_owner_epoch(explicit_item);
  vm_owner_set_id(dest, "owner/test/move/explicit-dest");
  move_object(explicit_item, dest);
  ASSERT_TRUE(vm_owner_has_explicit_id(explicit_item));
  ASSERT_STREQ("owner/test/move/explicit-item", vm_owner_id(explicit_item));
  ASSERT_EQ(vm_owner_epoch(explicit_item), explicit_epoch);
  destruct_object_for_test(explicit_item);

  vm_owner_clear_id(dest);
}

TEST_F(DriverTest, TestInteractiveExecPreservesNewObjectOwner) {
  ScopedCurrentObjectAsMaster master_scope;
  auto* old_user = clone_object_for_test("single/owner_singleton");
  auto* new_user = clone_object_for_test("single/owner_singleton");
  ASSERT_NE(old_user, nullptr);
  ASSERT_NE(new_user, nullptr);

  add_ref(old_user, "TestInteractiveExecPreservesNewObjectOwner");

  vm_owner_set_id(old_user, "owner/test/interactive/login");
  vm_owner_set_id(new_user, "owner/test/interactive/exec-user");
  auto old_epoch = vm_owner_epoch(old_user);
  auto new_epoch = vm_owner_epoch(new_user);

  auto* ip = user_add();
  ASSERT_NE(ip, nullptr);
  ip->ob = old_user;
  ip->fd = -1;
  old_user->interactive = ip;
  old_user->flags |= O_ONCE_INTERACTIVE;
  set_command_giver(old_user);

  ASSERT_EQ(replace_interactive(new_user, old_user), 1);
  ASSERT_EQ(new_user->interactive, ip);
  ASSERT_EQ(ip->ob, new_user);
  ASSERT_EQ(old_user->interactive, nullptr);
  ASSERT_EQ(command_giver, new_user);
  ASSERT_STREQ("owner/test/interactive/login", vm_owner_id(old_user));
  ASSERT_EQ(vm_owner_epoch(old_user), old_epoch);
  ASSERT_STREQ("owner/test/interactive/exec-user", vm_owner_id(new_user));
  ASSERT_EQ(vm_owner_epoch(new_user), new_epoch);

  set_command_giver(nullptr);
  remove_interactive(new_user, 1);
  ASSERT_EQ(new_user->interactive, nullptr);
  destruct_object_for_test(old_user);
  destruct_object_for_test(new_user);
}

TEST_F(DriverTest, TestVmOwnerMailboxDrainsOwnerFifo) {
  const char* owner_id = "owner/test/mailbox";
  auto first_id = vm_owner_enqueue_task(owner_id, "command", "first");
  auto second_id = vm_owner_enqueue_task(owner_id, "command", "second");
  ASSERT_LT(first_id, second_id);


  auto* status = vm_owner_mailbox_status(owner_id);
  ASSERT_EQ(mapping_number(status, "owner_queue_depth"), 2);
  free_mapping(status);

  auto* first_drain = vm_owner_drain_mailbox(owner_id, 1);
  ASSERT_EQ(mapping_number(first_drain, "drained"), 1);
  ASSERT_EQ(mapping_number(first_drain, "remaining"), 1);
  auto* first_tasks = find_string_in_mapping(first_drain, "tasks");
  ASSERT_NE(first_tasks, nullptr);
  ASSERT_EQ(first_tasks->type, T_ARRAY);
  ASSERT_EQ(first_tasks->u.arr->size, 1);
  ASSERT_EQ(first_tasks->u.arr->item[0].type, T_MAPPING);
  ASSERT_EQ(mapping_number(first_tasks->u.arr->item[0].u.map, "task_id"),
            static_cast<long>(first_id));
  ASSERT_EQ(mapping_number(first_tasks->u.arr->item[0].u.map, "owner_epoch"), 0);
  ASSERT_STREQ(mapping_string(first_tasks->u.arr->item[0].u.map, "task_key"), "first");
  free_mapping(first_drain);

  auto* second_drain = vm_owner_drain_mailbox(owner_id, 0);
  ASSERT_EQ(mapping_number(second_drain, "drained"), 1);
  ASSERT_EQ(mapping_number(second_drain, "remaining"), 0);
  auto* second_tasks = find_string_in_mapping(second_drain, "tasks");
  ASSERT_NE(second_tasks, nullptr);
  ASSERT_EQ(second_tasks->type, T_ARRAY);
  ASSERT_EQ(second_tasks->u.arr->size, 1);
  ASSERT_EQ(second_tasks->u.arr->item[0].type, T_MAPPING);
  ASSERT_EQ(mapping_number(second_tasks->u.arr->item[0].u.map, "task_id"),
            static_cast<long>(second_id));
  ASSERT_STREQ(mapping_string(second_tasks->u.arr->item[0].u.map, "task_key"), "second");
  free_mapping(second_drain);
}

TEST_F(DriverTest, TestVmOwnerEpochRejectsStaleTask) {
  ScopedCurrentObjectAsMaster master_scope;
  object_t* obj = find_object("single/master.c");
  ASSERT_NE(obj, nullptr);

  vm_owner_set_id(obj, "owner/test/epoch-a");
  auto epoch_a = vm_owner_epoch(obj);
  auto stale_task = vm_owner_enqueue_task_epoch("owner/test/epoch-a", "command", "stale", epoch_a);
  vm_owner_set_id(obj, "owner/test/epoch-b");
  auto epoch_b = vm_owner_epoch(obj);
  ASSERT_GT(epoch_b, epoch_a);

  auto* scheduled = vm_owner_schedule(1);
  auto* tasks = find_string_in_mapping(scheduled, "tasks");
  ASSERT_NE(tasks, nullptr);
  ASSERT_EQ(tasks->type, T_ARRAY);
  ASSERT_EQ(tasks->u.arr->size, 1);
  ASSERT_EQ(mapping_number(tasks->u.arr->item[0].u.map, "task_id"), static_cast<long>(stale_task));
  ASSERT_EQ(mapping_number(tasks->u.arr->item[0].u.map, "owner_epoch"), static_cast<long>(epoch_a));
  free_mapping(scheduled);

  error_context_t econ{};
  save_context(&econ);
  try {
    vm_owner_guard_epoch(obj, "owner/test/epoch-a", epoch_a);
    pop_context(&econ);
    FAIL() << "vm_owner_guard_epoch should reject stale owner task";
  } catch (...) {
    restore_context(&econ);
  }

  auto* guarded = vm_owner_guard_epoch(obj, "owner/test/epoch-b", epoch_b);
  ASSERT_NE(guarded, nullptr);
  ASSERT_EQ(mapping_number(guarded, "owner_epoch"), static_cast<long>(epoch_b));
  free_mapping(guarded);
  vm_owner_clear_id(obj);
}

TEST_F(DriverTest, TestVmOwnerPurgeRemovesOwnerQueueBeforeSchedule) {
  const char* owner = "owner/test/purge";
  const char* other = "owner/test/purge-other";
  vm_owner_enqueue_task(owner, "command", "first");
  vm_owner_enqueue_task(owner, "command", "second");
  auto other_task = vm_owner_enqueue_task(other, "command", "other");


  auto* purged = vm_owner_purge_mailbox(owner);
  ASSERT_EQ(mapping_number(purged, "purged"), 2);
  ASSERT_EQ(mapping_number(purged, "remaining"), 0);
  free_mapping(purged);

  auto* status = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(status, "owner_queue_depth"), 0);
  free_mapping(status);

  auto* scheduled = vm_owner_schedule(1);
  auto* tasks = find_string_in_mapping(scheduled, "tasks");
  ASSERT_NE(tasks, nullptr);
  ASSERT_EQ(tasks->type, T_ARRAY);
  ASSERT_EQ(tasks->u.arr->size, 1);
  ASSERT_EQ(mapping_number(tasks->u.arr->item[0].u.map, "task_id"), static_cast<long>(other_task));
  ASSERT_STREQ(mapping_string(tasks->u.arr->item[0].u.map, "owner_id"), other);
  free_mapping(scheduled);
}

TEST_F(DriverTest, TestVmOwnerTaskTraceRecordsObservedAndDispatchedEvents) {
  const char* owner = "owner/test/trace";
  auto trace_id = vm_owner_record_task_trace(owner, "command", "look", 7, "observed");
  auto task_id = vm_owner_enqueue_task_epoch(owner, "command", "inventory", 7);
  ASSERT_GT(trace_id, 0u);
  ASSERT_GT(task_id, 0u);

  auto* scheduled = vm_owner_schedule(1);
  free_mapping(scheduled);


  auto* trace = vm_owner_task_trace(8);
  ASSERT_STREQ(mapping_string(trace, "trace_kind"), "owner_task_trace");
  ASSERT_STREQ(mapping_string(trace, "trace_model"), "owner_task_lifecycle_trace");
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  ASSERT_GE(events->u.arr->size, 3);
  auto find_trace_event = [&](const char* state, const char* task_key) -> mapping_t* {
    for (int i = 0; i < events->u.arr->size; i++) {
      auto* event = events->u.arr->item[i].u.map;
      if (std::string(mapping_string(event, "owner_id")) == owner &&
          std::string(mapping_string(event, "state")) == state &&
          std::string(mapping_string(event, "task_key")) == task_key) {
        return event;
      }
    }
    return nullptr;
  };
  auto* observed = find_trace_event("observed", "look");
  auto* queued = find_trace_event("queued", "inventory");
  auto* dispatched = find_trace_event("dispatched", "inventory");
  ASSERT_NE(observed, nullptr);
  ASSERT_NE(queued, nullptr);
  ASSERT_NE(dispatched, nullptr);
  ASSERT_STREQ(mapping_string(observed, "trace_model"), "owner_task_lifecycle_event");
  ASSERT_STREQ(mapping_string(queued, "trace_model"), "owner_task_lifecycle_event");
  ASSERT_STREQ(mapping_string(dispatched, "trace_model"), "owner_task_lifecycle_event");
  ASSERT_EQ(mapping_number(dispatched, "task_id"), static_cast<long>(task_id));
  free_mapping(trace);
}

TEST_F(DriverTest, TestVmOwnerMainQueueDispatchesWithOwnerScope) {
  ScopedCurrentObjectAsMaster master_scope;
  object_t* obj = clone_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  vm_owner_set_id(obj, "owner/test/main-queue");

  bool ran = false;
  std::string seen_owner;
  long active_owners_during_callback = 0;
  auto* before = vm_owner_thread_status();
  auto before_claims = mapping_number(before, "main_owner_claims");
  auto before_releases = mapping_number(before, "main_owner_releases");
  free_mapping(before);
  auto* before_runtime = vm_owner_runtime_status();
  auto before_normal_fallback = mapping_number(before_runtime, "normal_path_main_fallback_count");
  auto before_io_adapter = mapping_number(before_runtime, "main_io_adapter_count");
  free_mapping(before_runtime);

  auto task_id = vm_owner_enqueue_main_task(
      obj, "unit_main", "dispatch",
      [&] {
        ran = true;
        seen_owner = vm_context().owner.current_owner_id;
        auto* running = vm_owner_thread_status();
        active_owners_during_callback = mapping_number(running, "main_active_owners");
        free_mapping(running);
      },
      nullptr, VM_OWNER_MAIN_TASK_IO_ADAPTER);
  ASSERT_GT(task_id, 0u);
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);
  ASSERT_TRUE(ran);
  ASSERT_EQ(seen_owner, "owner/test/main-queue");
  ASSERT_EQ(active_owners_during_callback, 1);

  auto* after = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(after, "main_active_owners"), 0);
  ASSERT_EQ(mapping_number(after, "main_owner_claims"), before_claims + 1);
  ASSERT_EQ(mapping_number(after, "main_owner_releases"), before_releases + 1);
  free_mapping(after);
  auto* after_runtime = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(after_runtime, "normal_path_main_fallback_count"),
            before_normal_fallback);
  ASSERT_EQ(mapping_number(after_runtime, "main_io_adapter_count"), before_io_adapter + 1);
  ASSERT_EQ(mapping_number(after_runtime, "main_fallback_policy_ready"), 1);
  free_mapping(after_runtime);

  auto* trace = vm_owner_task_trace(2);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  ASSERT_EQ(events->u.arr->size, 2);
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "state"), "main_queued");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "main_task_policy"), "io_adapter");
  ASSERT_STREQ(mapping_string(events->u.arr->item[1].u.map, "state"), "main_dispatched");
  ASSERT_STREQ(mapping_string(events->u.arr->item[1].u.map, "main_task_policy"), "io_adapter");
  free_mapping(trace);

  vm_owner_clear_id(obj);
  destruct_object(obj);
}

TEST_F(DriverTest, TestVmOwnerMainDrainRecoversAfterCallbackException) {
  ScopedCurrentObjectAsMaster master_scope;
  ASSERT_EQ(vm_owner_drain_main_tasks(1024), 0);
  object_t* first = clone_object_for_test("single/void");
  object_t* second = clone_object_for_test("single/void");
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  vm_owner_set_id(first, "owner/test/main-exception/first");
  vm_owner_set_id(second, "owner/test/main-exception/second");

  const auto first_ref_before = first->ref;
  bool second_ran = false;
  ASSERT_GT(vm_owner_enqueue_main_task(first, "unit_main", "throws",
                                       [] {
                                         throw "owner main callback test";
                                       }),
            0u);
  ASSERT_GT(vm_owner_enqueue_main_task(second, "unit_main", "after-throw",
                                       [&] {
                                         second_ran = true;
                                       }),
            0u);
  ASSERT_EQ(first->ref, first_ref_before + 1);

  bool caught = false;
  try {
    vm_owner_drain_main_tasks(1);
  } catch (const char* message) {
    caught = true;
    EXPECT_STREQ(message, "owner main callback test");
  }
  ASSERT_TRUE(caught);
  EXPECT_EQ(first->ref, first_ref_before);

  auto* after_failure = vm_owner_thread_status();
  EXPECT_EQ(mapping_number(after_failure, "main_active_owners"), 0);
  free_mapping(after_failure);
  EXPECT_EQ(vm_owner_drain_main_tasks(1), 1);
  EXPECT_TRUE(second_ran);
  EXPECT_EQ(vm_owner_main_queue_total_depth(), 0);

  auto* after_recovery = vm_owner_thread_status();
  EXPECT_EQ(mapping_number(after_recovery, "main_active_owners"), 0);
  free_mapping(after_recovery);

  vm_owner_clear_id(first);
  vm_owner_clear_id(second);
  destruct_object(first);
  destruct_object(second);
}

TEST_F(DriverTest, TestVmOwnerMainDrainWallBudgetYieldsAfterFirstTask) {
  ScopedCurrentObjectAsMaster master_scope;
  ASSERT_EQ(vm_owner_drain_main_tasks(1024), 0);
  object_t* first = clone_object_for_test("single/void");
  object_t* second = clone_object_for_test("single/void");
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  vm_owner_set_id(first, "owner/test/main-wall-budget/first");
  vm_owner_set_id(second, "owner/test/main-wall-budget/second");

  int first_ran = 0;
  int second_ran = 0;
  ASSERT_GT(vm_owner_enqueue_main_task(first, "unit_main", "wall-budget-first",
                                       [&] {
                                         first_ran++;
                                         auto deadline = std::chrono::steady_clock::now() +
                                                         std::chrono::milliseconds(3);
                                         while (std::chrono::steady_clock::now() < deadline) {
                                         }
                                       }),
            0u);
  ASSERT_GT(vm_owner_enqueue_main_task(second, "unit_main", "wall-budget-second",
                                       [&] {
                                         second_ran++;
                                       }),
            0u);

  auto result = vm_owner_drain_main_tasks_with_budget(64, 500000);
  ASSERT_EQ(result.dispatched, 1);
  ASSERT_EQ(result.remaining_main_tasks, 1);
  ASSERT_EQ(result.remaining_cleanup_tasks, 0);
  ASSERT_GE(result.elapsed_ns, 500000u);
  ASSERT_GE(result.max_main_task_elapsed_ns, 500000u);
  ASSERT_EQ(result.main_tasks_exceeding_wall_budget, 1);
  ASSERT_TRUE(result.wall_budget_yielded);
  ASSERT_FALSE(result.task_budget_yielded);
  ASSERT_EQ(first_ran, 1);
  ASSERT_EQ(second_ran, 0);

  ASSERT_EQ(vm_owner_drain_main_tasks(64), 1);
  ASSERT_EQ(first_ran, 1);
  ASSERT_EQ(second_ran, 1);
  ASSERT_EQ(vm_owner_main_queue_total_depth(), 0);

  vm_owner_clear_id(first);
  vm_owner_clear_id(second);
  destruct_object(first);
  destruct_object(second);
}

TEST_F(DriverTest, TestVmOwnerMainQueueDropsStaleOwnerEpoch) {
  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  vm_owner_set_id(obj, "owner/test/main-stale-old");

  bool ran = false;
  auto task_id = vm_owner_enqueue_main_task(obj, "unit_main", "stale", [&] {
    ran = true;
  });
  ASSERT_GT(task_id, 0u);
  vm_owner_set_id(obj, "owner/test/main-stale-new");
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);
  ASSERT_FALSE(ran);

  auto* trace = vm_owner_task_trace(2);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  ASSERT_EQ(events->u.arr->size, 2);
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "state"), "main_queued");
  ASSERT_STREQ(mapping_string(events->u.arr->item[1].u.map, "state"), "main_stale");
  free_mapping(trace);

  vm_owner_clear_id(obj);
  destruct_object(obj);
}

TEST_F(DriverTest, TestVmOwnerMainQueueRunsDropCallbackForStaleTask) {
  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  vm_owner_set_id(obj, "owner/test/main-drop-old");

  bool ran = false;
  bool dropped = false;
  auto task_id = vm_owner_enqueue_main_task(
      obj, "unit_main", "drop",
      [&] {
        ran = true;
      },
      [&] {
        dropped = true;
      });
  ASSERT_GT(task_id, 0u);
  vm_owner_set_id(obj, "owner/test/main-drop-new");
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);
  ASSERT_FALSE(ran);
  ASSERT_TRUE(dropped);

  vm_owner_clear_id(obj);
  destruct_object(obj);
}

TEST_F(DriverTest, TestVmOwnerHeartbeatTraceRecordsScheduledEvent) {
  ScopedCurrentObjectAsMaster master_scope;
  object_t* obj = find_object("single/master.c");
  ASSERT_NE(obj, nullptr);
  vm_owner_set_id(obj, "owner/test/heartbeat");

  ASSERT_EQ(set_heart_beat(obj, 1), 1);

  auto* trace = vm_owner_task_trace(1);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  ASSERT_EQ(events->u.arr->size, 1);
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "task_type"), "heartbeat");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "state"), "scheduled");
  free_mapping(trace);

  auto* owner_status = vm_object_store_owner_status("owner/test/heartbeat");
  ASSERT_EQ(mapping_number(owner_status, "active_heartbeats"), 1);
  ASSERT_EQ(mapping_number(owner_status, "runnable_tasks"), 1);
  free_mapping(owner_status);

  set_heart_beat(obj, 0);
  owner_status = vm_object_store_owner_status("owner/test/heartbeat");
  ASSERT_EQ(mapping_number(owner_status, "active_heartbeats"), 0);
  ASSERT_EQ(mapping_number(owner_status, "runnable_tasks"), 0);
  free_mapping(owner_status);
  vm_owner_clear_id(obj);
}

TEST_F(DriverTest, TestVmObjectStoreTracksPendingCallouts) {

  object_t* obj = find_object("single/master.c");
  ASSERT_NE(obj, nullptr);
  vm_owner_set_id(obj, "owner/test/callout-store");
  vm_object_store_record_callout(obj, 12345);

  auto* owner_status = vm_object_store_owner_status("owner/test/callout-store");
  ASSERT_EQ(mapping_number(owner_status, "pending_callouts"), 1);
  ASSERT_EQ(mapping_number(owner_status, "runnable_tasks"), 1);
  free_mapping(owner_status);

  vm_object_store_remove_callout("owner/test/callout-store", 12345);
  owner_status = vm_object_store_owner_status("owner/test/callout-store");
  ASSERT_EQ(mapping_number(owner_status, "pending_callouts"), 0);
  ASSERT_EQ(mapping_number(owner_status, "runnable_tasks"), 0);
  free_mapping(owner_status);
  vm_owner_clear_id(obj);
}

TEST_F(DriverTest, TestVmObjectStoreTracksPendingOwnerMessages) {

  auto assert_pending_messages = [&](const char* owner_id, long expected) {
    auto* owner_status = vm_object_store_owner_status(owner_id);
    ASSERT_EQ(mapping_number(owner_status, "pending_messages"), expected);
    ASSERT_EQ(mapping_number(owner_status, "runnable_tasks"), expected);
    free_mapping(owner_status);
  };

  auto* drained = vm_owner_submit_message("owner/test/message/source", "owner/test/message/drain",
                                          "message", "payload/drain");
  ASSERT_EQ(mapping_number(drained, "success"), 1);
  free_mapping(drained);
  assert_pending_messages("owner/test/message/drain", 1);
  auto* drain_result = vm_owner_drain_mailbox("owner/test/message/drain", 1);
  ASSERT_EQ(mapping_number(drain_result, "drained"), 1);
  free_mapping(drain_result);
  assert_pending_messages("owner/test/message/drain", 0);

  auto* scheduled = vm_owner_submit_message(
      "owner/test/message/source", "owner/test/message/schedule", "message", "payload/schedule");
  ASSERT_EQ(mapping_number(scheduled, "success"), 1);
  free_mapping(scheduled);
  assert_pending_messages("owner/test/message/schedule", 1);
  auto* schedule_result = vm_owner_schedule(1);
  ASSERT_EQ(mapping_number(schedule_result, "dispatched"), 1);
  free_mapping(schedule_result);
  assert_pending_messages("owner/test/message/schedule", 0);

  auto* purged = vm_owner_submit_message("owner/test/message/source", "owner/test/message/purge",
                                         "message", "payload/purge");
  ASSERT_EQ(mapping_number(purged, "success"), 1);
  free_mapping(purged);
  assert_pending_messages("owner/test/message/purge", 1);
  auto* purge_result = vm_owner_purge_mailbox("owner/test/message/purge");
  ASSERT_EQ(mapping_number(purge_result, "purged"), 1);
  free_mapping(purge_result);
  assert_pending_messages("owner/test/message/purge", 0);
}

TEST_F(DriverTest, TestVmOwnerHeartbeatStaleOwnerSkipsExecution) {
  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  vm_owner_set_id(obj, "owner/test/heartbeat-stale-old");

  auto call_number = [](const char* method, object_t* target) -> long {
    auto* ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
    EXPECT_EQ(ret ? ret->type : T_INVALID, T_NUMBER);
    return ret && ret->type == T_NUMBER ? ret->u.number : -1;
  };

  ASSERT_EQ(call_number("get_heartbeat_called", obj), 0);
  ASSERT_EQ(set_heart_beat(obj, 1), 1);
  vm_owner_set_id(obj, "owner/test/heartbeat-stale-new");

  call_heart_beat();
  clear_tick_events();

  ASSERT_EQ(call_number("get_heartbeat_called", obj), 0);
  auto* trace = vm_owner_task_trace(4);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  bool saw_stale = false;
  for (int i = 0; i < events->u.arr->size; i++) {
    auto* event = events->u.arr->item[i].u.map;
    if (std::string(mapping_string(event, "task_type")) == "heartbeat" &&
        std::string(mapping_string(event, "state")) == "stale") {
      saw_stale = true;
      ASSERT_STREQ(mapping_string(event, "owner_id"), "owner/test/heartbeat-stale-old");
    }
  }
  ASSERT_TRUE(saw_stale);
  free_mapping(trace);

  set_heart_beat(obj, 0);
  vm_owner_clear_id(obj);
  destruct_object(obj);
}

TEST_F(DriverTest, TestVmOwnerHeartbeatDispatchesThroughOwnerExecutor) {
  const char* owner = "owner/test/heartbeat-executor";
  struct RuntimeGuard {
    int saved_mode;
    object_t* obj{nullptr};

    ~RuntimeGuard() {
      vm_owner_thread_stop();
      if (obj) {
        set_heart_beat(obj, 0);
        vm_owner_clear_id(obj);
        destruct_object(obj);
      }
      clear_heartbeats();
      clear_tick_events();
      CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
    }
  } runtime_guard{CONFIG_INT(__RC_MULTICORE_MODE__)};
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;
  vm_owner_thread_stop();

  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  runtime_guard.obj = obj;
  vm_owner_set_id(obj, owner);

  auto call_number = [](const char* method, object_t* target) -> long {
    auto* ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
    EXPECT_EQ(ret ? ret->type : T_INVALID, T_NUMBER);
    return ret && ret->type == T_NUMBER ? ret->u.number : -1;
  };
  auto trace_has_state = [&](const char* state) {
    auto* trace = vm_owner_task_trace(64);
    auto* events = find_string_in_mapping(trace, "events");
    bool found = false;
    EXPECT_NE(events, nullptr);
    EXPECT_EQ(events ? events->type : T_INVALID, T_ARRAY);
    if (events && events->type == T_ARRAY) {
      for (int i = 0; i < events->u.arr->size; i++) {
        auto* event = events->u.arr->item[i].u.map;
        if (std::string(mapping_string(event, "task_type")) == "heartbeat" &&
            std::string(mapping_string(event, "owner_id")) == owner &&
            std::string(mapping_string(event, "state")) == state) {
          found = true;
          break;
        }
      }
    }
    free_mapping(trace);
    return found;
  };

  ASSERT_EQ(call_number("get_heartbeat_called", obj), 0);
  ASSERT_EQ(set_heart_beat(obj, 1), 1);
  auto* before = vm_owner_thread_status();
  auto before_callback_queued = mapping_number(before, "executor_callback_queued");
  auto before_callback_dispatched = mapping_number(before, "executor_callback_dispatched");
  auto before_main_queued = mapping_number(before, "main_queued");
  free_mapping(before);

  vm_owner_thread_start(1);
  ASSERT_TRUE(vm_owner_executor_available());
  call_heart_beat();
  clear_tick_events();

  for (int i = 0; i < 100 && !trace_has_state("main_executor_callback_completed"); i++) {
    vm_owner_drain_main_tasks(8);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_TRUE(trace_has_state("main_executor_callback_dispatched"));
  ASSERT_TRUE(trace_has_state("main_executor_callback_completed"));
  ASSERT_EQ(call_number("get_heartbeat_called", obj), 1);

  auto* after = vm_owner_thread_status();
  ASSERT_GE(mapping_number(after, "executor_callback_queued"), before_callback_queued + 1);
  ASSERT_GE(mapping_number(after, "executor_callback_dispatched"), before_callback_dispatched + 1);
  ASSERT_GE(mapping_number(after, "main_queued"), before_main_queued + 1);
  ASSERT_EQ(mapping_number(after, "heartbeat_owner_executor_ready"), 1);
  ASSERT_STREQ(mapping_string(after, "heartbeat_owner_executor_task_type"), "heartbeat");
  ASSERT_STREQ(mapping_string(after, "heartbeat_owner_executor_route"),
               "owner_main_queue_callback_adapter");
  ASSERT_STREQ(mapping_string(after, "heartbeat_owner_executor_fallback_route"), "");
  ASSERT_STREQ(mapping_string(after, "heartbeat_owner_executor_policy"),
               "main_thread_callback_adapter_after_owner_admission");
  ASSERT_EQ(mapping_number(after, "heartbeat_owner_executor_fallback_main_ready"), 1);
  ASSERT_EQ(mapping_number(after, "heartbeat_current_object_thread_local"), 0);
  free_mapping(after);
}

TEST_F(DriverTest, TestVmOwnerHeartbeatExecutorDropsStaleOwnerEpoch) {
  const char* owner = "owner/test/heartbeat-executor-stale";
  const char* moved_owner = "owner/test/heartbeat-executor-stale/moved";
  struct RuntimeGuard {
    int saved_mode;
    object_t* obj{nullptr};

    ~RuntimeGuard() {
      vm_owner_thread_stop();
      if (obj) {
        set_heart_beat(obj, 0);
        vm_owner_clear_id(obj);
        destruct_object(obj);
      }
      clear_heartbeats();
      clear_tick_events();
      CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
    }
  } runtime_guard{CONFIG_INT(__RC_MULTICORE_MODE__)};
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;
  vm_owner_thread_stop();

  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  runtime_guard.obj = obj;
  vm_owner_set_id(obj, owner);

  auto call_number = [](const char* method, object_t* target) -> long {
    auto* ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
    EXPECT_EQ(ret ? ret->type : T_INVALID, T_NUMBER);
    return ret && ret->type == T_NUMBER ? ret->u.number : -1;
  };
  auto trace_has_heartbeat_stale = [&] {
    auto* trace = vm_owner_task_trace(96);
    auto* events = find_string_in_mapping(trace, "events");
    bool found = false;
    EXPECT_NE(events, nullptr);
    EXPECT_EQ(events ? events->type : T_INVALID, T_ARRAY);
    if (events && events->type == T_ARRAY) {
      for (int i = 0; i < events->u.arr->size; i++) {
        auto* event = events->u.arr->item[i].u.map;
        if (std::string(mapping_string(event, "task_type")) == "heartbeat" &&
            std::string(mapping_string(event, "owner_id")) == owner &&
            std::string(mapping_string(event, "state")) == "main_executor_callback_stale") {
          found = true;
          break;
        }
      }
    }
    free_mapping(trace);
    return found;
  };

  vm_owner_thread_start(1);
  ASSERT_TRUE(vm_owner_executor_available());

  ASSERT_EQ(call_number("get_heartbeat_called", obj), 0);
  auto task_id = vm_owner_enqueue_executor_task(obj, "heartbeat", "heart_beat", [] {});
  ASSERT_GT(task_id, 0u);
  vm_owner_set_id(obj, moved_owner);
  ASSERT_GE(vm_owner_drain_main_tasks(8), 1);

  for (int i = 0; i < 100 && !trace_has_heartbeat_stale(); i++) {
    vm_owner_drain_main_tasks(8);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_TRUE(trace_has_heartbeat_stale());
  ASSERT_EQ(call_number("get_heartbeat_called", obj), 0);
}

TEST_F(DriverTest, TestVmOwnerCalloutDispatchesThroughOwnerExecutor) {
  const char* owner = "owner/test/callout-executor";
  struct RuntimeGuard {
    int saved_mode;
    object_t* obj{nullptr};

    ~RuntimeGuard() {
      vm_owner_thread_stop();
      vm_owner_drain_main_tasks(64);
      clear_call_outs();
      clear_tick_events();
      if (obj) {
        vm_owner_clear_id(obj);
        destruct_object(obj);
      }
      CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
    }
  } runtime_guard{CONFIG_INT(__RC_MULTICORE_MODE__)};
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;
  vm_owner_thread_stop();

  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  runtime_guard.obj = obj;
  vm_owner_set_id(obj, owner);

  auto call_number = [](const char* method, object_t* target) -> long {
    auto* ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
    EXPECT_EQ(ret ? ret->type : T_INVALID, T_NUMBER);
    return ret && ret->type == T_NUMBER ? ret->u.number : -1;
  };
  auto trace_has_callout_state = [&](const char* state) {
    auto* trace = vm_owner_task_trace(96);
    auto* events = find_string_in_mapping(trace, "events");
    bool found = false;
    EXPECT_NE(events, nullptr);
    EXPECT_EQ(events ? events->type : T_INVALID, T_ARRAY);
    if (events && events->type == T_ARRAY) {
      for (int i = 0; i < events->u.arr->size; i++) {
        auto* event = events->u.arr->item[i].u.map;
        if (std::string(mapping_string(event, "task_type")) == "call_out" &&
            std::string(mapping_string(event, "owner_id")) == owner &&
            std::string(mapping_string(event, "state")) == state) {
          found = true;
          break;
        }
      }
    }
    free_mapping(trace);
    return found;
  };

  ASSERT_EQ(call_number("get_callout_called", obj), 0);
  ASSERT_EQ(call_number("get_callout_off_main", obj), 0);
  auto* before = vm_owner_thread_status();
  auto before_callback_queued = mapping_number(before, "executor_callback_queued");
  auto before_callback_dispatched = mapping_number(before, "executor_callback_dispatched");
  auto before_cleanup_queued = mapping_number(before, "executor_callback_main_cleanup_queued");
  auto before_cleanup_dispatched =
      mapping_number(before, "executor_callback_main_cleanup_dispatched");
  auto before_main_queued = mapping_number(before, "main_queued");
  free_mapping(before);

  vm_owner_thread_start(1);
  ASSERT_TRUE(vm_owner_executor_available());
  auto handle = call_number("start_callout_probe", obj);
  ASSERT_GT(handle, 0);
  ASSERT_TRUE(vm_call_out_test_support_run_handle(static_cast<LPC_INT>(handle)));

  for (int i = 0; i < 100 && !trace_has_callout_state("main_executor_callback_completed"); i++) {
    vm_owner_drain_main_tasks(64);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_TRUE(trace_has_callout_state("main_executor_callback_dispatched"));
  ASSERT_TRUE(trace_has_callout_state("main_executor_callback_completed"));
  ASSERT_EQ(call_number("get_callout_called", obj), 1);
  ASSERT_EQ(call_number("get_callout_off_main", obj), 0);

  auto* after = vm_owner_thread_status();
  ASSERT_GE(mapping_number(after, "executor_callback_queued"), before_callback_queued + 1);
  ASSERT_GE(mapping_number(after, "executor_callback_dispatched"), before_callback_dispatched + 1);
  ASSERT_EQ(mapping_number(after, "executor_callback_main_cleanup_queued"), before_cleanup_queued);
  ASSERT_EQ(mapping_number(after, "executor_callback_main_cleanup_dispatched"),
            before_cleanup_dispatched);
  ASSERT_GE(mapping_number(after, "main_queued"), before_main_queued + 1);
  ASSERT_EQ(mapping_number(after, "callout_owner_executor_ready"), 1);
  ASSERT_STREQ(mapping_string(after, "callout_owner_executor_task_type"), "call_out");
  ASSERT_STREQ(mapping_string(after, "callout_owner_executor_route"),
               "owner_main_queue_callback_adapter");
  ASSERT_STREQ(mapping_string(after, "callout_owner_executor_fallback_route"), "");
  ASSERT_STREQ(mapping_string(after, "callout_owner_executor_policy"),
               "main_thread_callback_adapter_after_owner_admission");
  ASSERT_EQ(mapping_number(after, "callout_owner_executor_expired_handle_detach_ready"), 1);
  ASSERT_EQ(mapping_number(after, "callout_owner_executor_cleanup_main_ready"), 1);
  ASSERT_EQ(mapping_number(after, "callout_owner_executor_drop_cleanup_ready"), 1);
  ASSERT_EQ(mapping_number(after, "callout_owner_executor_fallback_main_ready"), 1);
  free_mapping(after);
}

TEST_F(DriverTest, TestVmOwnerCalloutExecutorDropsStaleOwnerEpoch) {
  const char* owner = "owner/test/callout-executor-stale";
  const char* moved_owner = "owner/test/callout-executor-stale/moved";
  struct RuntimeGuard {
    int saved_mode;
    object_t* obj{nullptr};

    ~RuntimeGuard() {
      vm_owner_thread_stop();
      vm_owner_drain_main_tasks(64);
      clear_call_outs();
      clear_tick_events();
      if (obj) {
        vm_owner_clear_id(obj);
        destruct_object(obj);
      }
      CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
    }
  } runtime_guard{CONFIG_INT(__RC_MULTICORE_MODE__)};
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;
  vm_owner_thread_stop();

  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  runtime_guard.obj = obj;
  vm_owner_set_id(obj, owner);

  auto call_number = [](const char* method, object_t* target) -> long {
    auto* ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
    EXPECT_EQ(ret ? ret->type : T_INVALID, T_NUMBER);
    return ret && ret->type == T_NUMBER ? ret->u.number : -1;
  };
  auto trace_has_callout_stale = [&] {
    auto* trace = vm_owner_task_trace(128);
    auto* events = find_string_in_mapping(trace, "events");
    bool found = false;
    EXPECT_NE(events, nullptr);
    EXPECT_EQ(events ? events->type : T_INVALID, T_ARRAY);
    if (events && events->type == T_ARRAY) {
      for (int i = 0; i < events->u.arr->size; i++) {
        auto* event = events->u.arr->item[i].u.map;
        if (std::string(mapping_string(event, "task_type")) == "call_out" &&
            std::string(mapping_string(event, "owner_id")) == owner &&
            std::string(mapping_string(event, "state")) == "main_executor_callback_stale") {
          found = true;
          break;
        }
      }
    }
    free_mapping(trace);
    return found;
  };

  vm_owner_thread_start(1);
  ASSERT_TRUE(vm_owner_executor_available());

  ASSERT_EQ(call_number("get_callout_called", obj), 0);
  auto handle = call_number("start_callout_probe", obj);
  ASSERT_GT(handle, 0);
  ASSERT_TRUE(vm_call_out_test_support_run_handle(static_cast<LPC_INT>(handle)));
  vm_owner_set_id(obj, moved_owner);
  ASSERT_GE(vm_owner_drain_main_tasks(64), 1);

  for (int i = 0; i < 100 && !trace_has_callout_stale(); i++) {
    vm_owner_drain_main_tasks(64);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_TRUE(trace_has_callout_stale());
  ASSERT_GE(vm_owner_drain_main_tasks(64), 1);
  ASSERT_EQ(call_number("get_callout_called", obj), 0);
}

TEST_F(DriverTest, TestVmOwnerAsyncDnsCallbacksDispatchThroughOwnerExecutor) {
  const char* owner = "owner/test/async-dns-executor";
  struct RuntimeGuard {
    int saved_mode;
    object_t* obj{nullptr};

    ~RuntimeGuard() {
      vm_owner_thread_stop();
      vm_owner_drain_main_tasks(64);
      if (obj) {
        vm_owner_clear_id(obj);
        destruct_object(obj);
      }
      CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
    }
  } runtime_guard{CONFIG_INT(__RC_MULTICORE_MODE__)};
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;
  vm_owner_thread_stop();

  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  runtime_guard.obj = obj;
  vm_owner_set_id(obj, owner);

  auto call_number = [](const char* method, object_t* target) -> long {
    auto* ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
    EXPECT_EQ(ret ? ret->type : T_INVALID, T_NUMBER);
    return ret && ret->type == T_NUMBER ? ret->u.number : -1;
  };
  auto call_string = [](const char* method, object_t* target) -> std::string {
    auto* ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
    if (ret && ret->type == T_NUMBER) {
      return "";
    }
    EXPECT_EQ(ret ? ret->type : T_INVALID, T_STRING);
    return ret && ret->type == T_STRING ? ret->u.string : "";
  };
  auto trace_has_state = [&](const char* task_type, const char* state) {
    auto* trace = vm_owner_task_trace(160);
    auto* events = find_string_in_mapping(trace, "events");
    bool found = false;
    EXPECT_NE(events, nullptr);
    EXPECT_EQ(events ? events->type : T_INVALID, T_ARRAY);
    if (events && events->type == T_ARRAY) {
      for (int i = 0; i < events->u.arr->size; i++) {
        auto* event = events->u.arr->item[i].u.map;
        if (std::string(mapping_string(event, "task_type")) == task_type &&
            std::string(mapping_string(event, "owner_id")) == owner &&
            std::string(mapping_string(event, "state")) == state) {
          found = true;
          break;
        }
      }
    }
    free_mapping(trace);
    return found;
  };

  safe_apply("reset_async_callback_probe", obj, 0, ORIGIN_DRIVER);
  safe_apply("reset_dns_callback_probe", obj, 0, ORIGIN_DRIVER);
  ASSERT_EQ(call_number("get_async_callback_called", obj), 0);
  ASSERT_EQ(call_number("get_dns_callback_called", obj), 0);
  auto* before = vm_owner_thread_status();
  auto before_callback_queued = mapping_number(before, "executor_callback_queued");
  auto before_callback_dispatched = mapping_number(before, "executor_callback_dispatched");
  auto before_cleanup_queued = mapping_number(before, "executor_callback_main_cleanup_queued");
  auto before_cleanup_dispatched =
      mapping_number(before, "executor_callback_main_cleanup_dispatched");
  auto before_main_queued = mapping_number(before, "main_queued");
  free_mapping(before);

  vm_owner_thread_start(1);
  ASSERT_TRUE(vm_owner_executor_available());
  ASSERT_TRUE(vm_async_test_support_dispatch_read_callback(obj, "async_callback_probe",
                                                           "async owner result"));
  ASSERT_TRUE(vm_dns_test_support_dispatch_callback(obj, "dns_callback_probe", 4242));

  for (int i = 0; i < 100 && !trace_has_state("async_callback", "main_executor_callback_completed");
       i++) {
    vm_owner_drain_main_tasks(64);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  for (int i = 0; i < 100 && !trace_has_state("dns_callback", "main_executor_callback_completed");
       i++) {
    vm_owner_drain_main_tasks(64);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_TRUE(trace_has_state("async_callback", "main_executor_callback_dispatched"));
  ASSERT_TRUE(trace_has_state("async_callback", "main_executor_callback_completed"));
  ASSERT_TRUE(trace_has_state("dns_callback", "main_executor_callback_dispatched"));
  ASSERT_TRUE(trace_has_state("dns_callback", "main_executor_callback_completed"));
  ASSERT_EQ(call_number("get_async_callback_called", obj), 1);
  ASSERT_EQ(call_number("get_async_callback_off_main", obj), 0);
  ASSERT_EQ(call_string("get_async_callback_value", obj), "async owner result");
  ASSERT_EQ(call_number("get_dns_callback_called", obj), 1);
  ASSERT_EQ(call_number("get_dns_callback_off_main", obj), 0);
  ASSERT_EQ(call_number("get_dns_callback_key", obj), 4242);

  auto* after = vm_owner_thread_status();
  ASSERT_GE(mapping_number(after, "executor_callback_queued"), before_callback_queued + 2);
  ASSERT_GE(mapping_number(after, "executor_callback_dispatched"), before_callback_dispatched + 2);
  ASSERT_EQ(mapping_number(after, "executor_callback_main_cleanup_queued"), before_cleanup_queued);
  ASSERT_EQ(mapping_number(after, "executor_callback_main_cleanup_dispatched"),
            before_cleanup_dispatched);
  ASSERT_GE(mapping_number(after, "main_queued"), before_main_queued + 2);
  ASSERT_EQ(mapping_number(after, "async_owner_executor_ready"), 1);
  ASSERT_STREQ(mapping_string(after, "async_owner_executor_task_type"), "async_callback");
  ASSERT_STREQ(mapping_string(after, "async_owner_executor_route"),
               "owner_main_queue_callback_adapter");
  ASSERT_STREQ(mapping_string(after, "async_owner_executor_fallback_route"), "");
  ASSERT_STREQ(mapping_string(after, "async_owner_executor_policy"),
               "main_thread_callback_adapter_after_owner_admission");
  ASSERT_STREQ(mapping_string(after, "async_owner_executor_result_policy"),
               "frozen_deep_copy_result");
  ASSERT_EQ(mapping_number(after, "async_owner_executor_cleanup_main_ready"), 1);
  ASSERT_EQ(mapping_number(after, "async_owner_executor_drop_cleanup_ready"), 1);
  ASSERT_EQ(mapping_number(after, "dns_owner_executor_ready"), 1);
  ASSERT_STREQ(mapping_string(after, "dns_owner_executor_task_type"), "dns_callback");
  ASSERT_STREQ(mapping_string(after, "dns_owner_executor_route"),
               "owner_main_queue_callback_adapter");
  ASSERT_STREQ(mapping_string(after, "dns_owner_executor_fallback_route"), "");
  ASSERT_STREQ(mapping_string(after, "dns_owner_executor_policy"),
               "main_thread_callback_adapter_after_owner_admission");
  ASSERT_STREQ(mapping_string(after, "dns_owner_executor_result_policy"),
               "frozen_deep_copy_result");
  ASSERT_EQ(mapping_number(after, "dns_owner_executor_owner_epoch_capture_ready"), 1);
  ASSERT_EQ(mapping_number(after, "dns_owner_executor_cleanup_main_ready"), 1);
  ASSERT_EQ(mapping_number(after, "dns_owner_executor_drop_cleanup_ready"), 1);
  free_mapping(after);
}

TEST_F(DriverTest, TestVmOwnerAsyncCallbackExecutorDropsStaleOwnerEpoch) {
  const char* owner = "owner/test/async-executor-stale";
  const char* moved_owner = "owner/test/async-executor-stale/moved";
  struct RuntimeGuard {
    int saved_mode;
    object_t* obj{nullptr};

    ~RuntimeGuard() {
      vm_owner_thread_stop();
      vm_owner_drain_main_tasks(64);
      if (obj) {
        vm_owner_clear_id(obj);
        destruct_object(obj);
      }
      CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
    }
  } runtime_guard{CONFIG_INT(__RC_MULTICORE_MODE__)};
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;
  vm_owner_thread_stop();

  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  runtime_guard.obj = obj;
  vm_owner_set_id(obj, owner);

  auto call_number = [](const char* method, object_t* target) -> long {
    auto* ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
    EXPECT_EQ(ret ? ret->type : T_INVALID, T_NUMBER);
    return ret && ret->type == T_NUMBER ? ret->u.number : -1;
  };
  auto trace_has_async_stale = [&] {
    auto* trace = vm_owner_task_trace(128);
    auto* events = find_string_in_mapping(trace, "events");
    bool found = false;
    EXPECT_NE(events, nullptr);
    EXPECT_EQ(events ? events->type : T_INVALID, T_ARRAY);
    if (events && events->type == T_ARRAY) {
      for (int i = 0; i < events->u.arr->size; i++) {
        auto* event = events->u.arr->item[i].u.map;
        if (std::string(mapping_string(event, "task_type")) == "async_callback" &&
            std::string(mapping_string(event, "owner_id")) == owner &&
            std::string(mapping_string(event, "state")) == "main_executor_callback_stale") {
          found = true;
          break;
        }
      }
    }
    free_mapping(trace);
    return found;
  };

  vm_owner_thread_start(1);
  ASSERT_TRUE(vm_owner_executor_available());

  safe_apply("reset_async_callback_probe", obj, 0, ORIGIN_DRIVER);
  ASSERT_TRUE(vm_async_test_support_dispatch_read_callback(obj, "async_callback_probe",
                                                           "stale async result"));
  vm_owner_set_id(obj, moved_owner);
  ASSERT_GE(vm_owner_drain_main_tasks(64), 1);

  for (int i = 0; i < 100 && !trace_has_async_stale(); i++) {
    vm_owner_drain_main_tasks(64);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_TRUE(trace_has_async_stale());
  ASSERT_GE(vm_owner_drain_main_tasks(64), 1);
  ASSERT_EQ(call_number("get_async_callback_called", obj), 0);
}

TEST_F(DriverTest, TestVmOwnerSocketCallbacksDispatchThroughOwnerExecutor) {
  const char* owner = "owner/test/socket-executor";
  struct RuntimeGuard {
    int saved_mode;
    object_t* obj{nullptr};

    ~RuntimeGuard() {
      vm_owner_thread_stop();
      vm_owner_drain_main_tasks(64);
      if (obj) {
        vm_owner_clear_id(obj);
        destruct_object(obj);
      }
      CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
    }
  } runtime_guard{CONFIG_INT(__RC_MULTICORE_MODE__)};
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;
  vm_owner_thread_stop();

  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  runtime_guard.obj = obj;
  vm_owner_set_id(obj, owner);

  auto call_number = [](const char* method, object_t* target) -> long {
    auto* ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
    EXPECT_EQ(ret ? ret->type : T_INVALID, T_NUMBER);
    return ret && ret->type == T_NUMBER ? ret->u.number : -1;
  };
  auto trace_has_state = [&](const char* state) {
    auto* trace = vm_owner_task_trace(160);
    auto* events = find_string_in_mapping(trace, "events");
    bool found = false;
    EXPECT_NE(events, nullptr);
    EXPECT_EQ(events ? events->type : T_INVALID, T_ARRAY);
    if (events && events->type == T_ARRAY) {
      for (int i = 0; i < events->u.arr->size; i++) {
        auto* event = events->u.arr->item[i].u.map;
        if (std::string(mapping_string(event, "task_type")) == "socket_callback" &&
            std::string(mapping_string(event, "owner_id")) == owner &&
            std::string(mapping_string(event, "state")) == state) {
          if (std::string(state) == "main_executor_callback_dispatched") {
            EXPECT_EQ(mapping_number(event, "manifest_version"), 2);
            EXPECT_STREQ(mapping_string(event, "manifest_schema"), "owner_task_manifest_v2");
            EXPECT_STREQ(mapping_string(event, "task_kind"), "executor_callback");
            EXPECT_STREQ(mapping_string(event, "payload_policy"),
                         "frozen_payload_or_owner_handle_only");
            EXPECT_STREQ(mapping_string(event, "cleanup_policy"), "main_thread_drop_cleanup");
            EXPECT_STREQ(mapping_string(event, "reply_future_policy"),
                         "main_reply_or_cleanup_queue");
            EXPECT_STREQ(mapping_string(event, "admission_policy"),
                         "owner_epoch_payload_allowlist_deadline_guard");
            EXPECT_STREQ(mapping_string(event, "admission_state"), "accepted");
            EXPECT_STREQ(mapping_string(event, "trace_schema"), "owner_executor_trace_v2");
          }
          found = true;
          break;
        }
      }
    }
    free_mapping(trace);
    return found;
  };

  safe_apply("reset_socket_callback_probe", obj, 0, ORIGIN_DRIVER);
  ASSERT_EQ(call_number("get_socket_callback_called", obj), 0);
  auto* before = vm_owner_thread_status();
  auto before_callback_queued = mapping_number(before, "executor_callback_queued");
  auto before_callback_dispatched = mapping_number(before, "executor_callback_dispatched");
  auto before_cleanup_queued = mapping_number(before, "executor_callback_main_cleanup_queued");
  auto before_cleanup_dispatched =
      mapping_number(before, "executor_callback_main_cleanup_dispatched");
  auto before_main_queued = mapping_number(before, "main_queued");
  free_mapping(before);

  vm_owner_thread_start(1);
  ASSERT_TRUE(vm_owner_executor_available());
  ASSERT_TRUE(vm_socket_test_support_dispatch_callback(obj, "socket_callback_probe", 9876));

  for (int i = 0; i < 100 && !trace_has_state("main_executor_callback_completed"); i++) {
    vm_owner_drain_main_tasks(64);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_TRUE(trace_has_state("main_executor_callback_dispatched"));
  ASSERT_TRUE(trace_has_state("main_executor_callback_completed"));
  ASSERT_EQ(call_number("get_socket_callback_called", obj), 1);
  ASSERT_EQ(call_number("get_socket_callback_off_main", obj), 0);
  ASSERT_EQ(call_number("get_socket_callback_fd", obj), 9876);

  auto* after = vm_owner_thread_status();
  ASSERT_GE(mapping_number(after, "executor_callback_queued"), before_callback_queued + 1);
  ASSERT_GE(mapping_number(after, "executor_callback_dispatched"), before_callback_dispatched + 1);
  ASSERT_EQ(mapping_number(after, "executor_callback_main_cleanup_queued"), before_cleanup_queued);
  ASSERT_EQ(mapping_number(after, "executor_callback_main_cleanup_dispatched"),
            before_cleanup_dispatched);
  ASSERT_GE(mapping_number(after, "main_queued"), before_main_queued + 1);
  ASSERT_EQ(mapping_number(after, "socket_owner_executor_ready"), 1);
  ASSERT_STREQ(mapping_string(after, "socket_owner_executor_task_type"), "socket_callback");
  ASSERT_STREQ(mapping_string(after, "socket_owner_executor_route"),
               "owner_main_queue_callback_adapter");
  ASSERT_STREQ(mapping_string(after, "socket_owner_executor_fallback_route"), "");
  ASSERT_STREQ(mapping_string(after, "socket_owner_executor_policy"),
               "main_thread_callback_adapter_after_owner_admission");
  ASSERT_STREQ(mapping_string(after, "socket_owner_executor_result_policy"),
               "frozen_deep_copy_args");
  ASSERT_EQ(mapping_number(after, "socket_owner_executor_cleanup_main_ready"), 1);
  ASSERT_EQ(mapping_number(after, "socket_owner_executor_drop_cleanup_ready"), 1);
  ASSERT_EQ(mapping_number(after, "socket_release_main_required"), 0);
  ASSERT_EQ(mapping_number(after, "socket_release_owner_safe_handshake_ready"), 1);
  ASSERT_STREQ(mapping_string(after, "socket_release_owner_safe_handshake_policy"),
               "synchronous_release_acquire_owner_epoch_guard");
  ASSERT_EQ(mapping_number(after, "socket_release_owner_epoch_guard_ready"), 1);
  free_mapping(after);
}

TEST_F(DriverTest, TestVmOwnerSocketCallbackExecutorDropsStaleOwnerEpoch) {
  const char* owner = "owner/test/socket-executor-stale";
  const char* moved_owner = "owner/test/socket-executor-stale/moved";
  struct RuntimeGuard {
    int saved_mode;
    object_t* obj{nullptr};

    ~RuntimeGuard() {
      vm_owner_thread_stop();
      vm_owner_drain_main_tasks(64);
      if (obj) {
        vm_owner_clear_id(obj);
        destruct_object(obj);
      }
      CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
    }
  } runtime_guard{CONFIG_INT(__RC_MULTICORE_MODE__)};
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;
  vm_owner_thread_stop();

  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  runtime_guard.obj = obj;
  vm_owner_set_id(obj, owner);

  auto call_number = [](const char* method, object_t* target) -> long {
    auto* ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
    EXPECT_EQ(ret ? ret->type : T_INVALID, T_NUMBER);
    return ret && ret->type == T_NUMBER ? ret->u.number : -1;
  };
  auto trace_has_socket_stale = [&] {
    auto* trace = vm_owner_task_trace(160);
    auto* events = find_string_in_mapping(trace, "events");
    bool found = false;
    EXPECT_NE(events, nullptr);
    EXPECT_EQ(events ? events->type : T_INVALID, T_ARRAY);
    if (events && events->type == T_ARRAY) {
      for (int i = 0; i < events->u.arr->size; i++) {
        auto* event = events->u.arr->item[i].u.map;
        if (std::string(mapping_string(event, "task_type")) == "socket_callback" &&
            std::string(mapping_string(event, "owner_id")) == owner &&
            std::string(mapping_string(event, "state")) == "main_executor_callback_stale") {
          found = true;
          break;
        }
      }
    }
    free_mapping(trace);
    return found;
  };

  auto* before = vm_owner_thread_status();
  auto before_callback_dropped = mapping_number(before, "executor_callback_dropped");
  auto before_admission_dropped = mapping_number(before, "owner_executor_admission_dropped");
  auto before_stale_drop = mapping_number(before, "owner_executor_stale_drop");
  free_mapping(before);

  vm_owner_thread_start(1);
  ASSERT_TRUE(vm_owner_executor_available());

  safe_apply("reset_socket_callback_probe", obj, 0, ORIGIN_DRIVER);
  ASSERT_TRUE(vm_socket_test_support_dispatch_callback(obj, "socket_callback_probe", 1234));
  vm_owner_set_id(obj, moved_owner);
  ASSERT_GE(vm_owner_drain_main_tasks(64), 1);

  for (int i = 0; i < 100 && !trace_has_socket_stale(); i++) {
    vm_owner_drain_main_tasks(64);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_TRUE(trace_has_socket_stale());
  ASSERT_GE(vm_owner_drain_main_tasks(64), 1);
  ASSERT_EQ(call_number("get_socket_callback_called", obj), 0);

  auto* after = vm_owner_thread_status();
  ASSERT_GE(mapping_number(after, "executor_callback_dropped"), before_callback_dropped + 1);
  ASSERT_GE(mapping_number(after, "owner_executor_admission_dropped"),
            before_admission_dropped + 1);
  ASSERT_GE(mapping_number(after, "owner_executor_stale_drop"), before_stale_drop + 1);
  free_mapping(after);
}

TEST_F(DriverTest, TestVmOwnerAccessTraceRecordsCrossOwnerAccess) {
  ScopedCurrentObjectAsMaster master_scope;
  object_t* source = find_object("single/master.c");
  object_t* target = find_object("single/simul_efun.c");
  ASSERT_NE(source, nullptr);
  ASSERT_NE(target, nullptr);
  vm_owner_set_id(source, "owner/test/access/source");
  vm_owner_set_id(target, "owner/test/access/target");

  auto access_id = vm_owner_record_access(source, target, "unit-test");
  ASSERT_GT(access_id, 0u);

  auto* trace = vm_owner_access_trace(1);
  ASSERT_STREQ(mapping_string(trace, "trace_kind"), "owner_access_trace");
  ASSERT_STREQ(mapping_string(trace, "trace_model"), "cross_owner_access_policy_trace");
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  ASSERT_EQ(events->u.arr->size, 1);
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "trace_model"),
               "cross_owner_access_policy_event");
  ASSERT_EQ(mapping_number(events->u.arr->item[0].u.map, "cross_owner"), 1);
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"),
               "owner/test/access/source");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"),
               "owner/test/access/target");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "operation"), "unit-test");
  free_mapping(trace);

  vm_owner_clear_id(source);
  vm_owner_clear_id(target);
}

TEST_F(DriverTest, TestVmOwnerCrossOwnerAccessTraceSkipsSameOwner) {
  ScopedCurrentObjectAsMaster master_scope;
  object_t* source = find_object("single/master.c");
  object_t* target = find_object("single/simul_efun.c");
  ASSERT_NE(source, nullptr);
  ASSERT_NE(target, nullptr);


  auto* before = vm_owner_access_trace(0);
  auto before_total = mapping_number(before, "total_traced");
  free_mapping(before);

  vm_owner_set_id(source, "owner/test/access/shared");
  vm_owner_set_id(target, "owner/test/access/shared");
  ASSERT_EQ(vm_owner_record_cross_owner_access(source, target, "environment"), 0u);

  auto* same_owner = vm_owner_access_trace(0);
  ASSERT_EQ(mapping_number(same_owner, "total_traced"), before_total);
  free_mapping(same_owner);

  vm_owner_set_id(target, "owner/test/access/target");
  auto access_id = vm_owner_record_cross_owner_access(source, target, "environment");
  ASSERT_GT(access_id, 0u);

  auto* trace = vm_owner_access_trace(1);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  ASSERT_EQ(events->u.arr->size, 1);
  ASSERT_EQ(mapping_number(events->u.arr->item[0].u.map, "cross_owner"), 1);
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "operation"), "environment");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"),
               "owner/test/access/shared");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"),
               "owner/test/access/target");
  free_mapping(trace);

  vm_owner_clear_id(source);
  vm_owner_clear_id(target);
}

TEST_F(DriverTest, TestVmOwnerCrossOwnerAccessTraceClassifiesPolicyModes) {
  ScopedCurrentObjectAsMaster master_scope;
  object_t* source = find_object("single/master.c");
  object_t* target = find_object("single/simul_efun.c");
  ASSERT_NE(source, nullptr);
  ASSERT_NE(target, nullptr);


  vm_owner_set_id(source, "owner/test/access-policy/source");
  vm_owner_set_id(target, "owner/test/access-policy/target");
  ASSERT_GT(vm_owner_record_access(source, target, "environment"), 0u);
  ASSERT_GT(vm_owner_record_access(source, target, "call_other"), 0u);
  ASSERT_GT(vm_owner_record_access(source, target, "present"), 0u);
  ASSERT_GT(vm_owner_record_access(source, target, "unknown_access"), 0u);

  auto* trace = vm_owner_access_trace(4);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  ASSERT_EQ(events->u.arr->size, 4);

  auto* snapshot_event = events->u.arr->item[0].u.map;
  ASSERT_STREQ(mapping_string(snapshot_event, "operation"), "environment");
  ASSERT_STREQ(mapping_string(snapshot_event, "access_mode"), "snapshot");
  ASSERT_EQ(mapping_number(snapshot_event, "snapshot_only"), 1);
  ASSERT_EQ(mapping_number(snapshot_event, "direct_cross_owner_write"), 0);

  auto* message_event = events->u.arr->item[1].u.map;
  ASSERT_STREQ(mapping_string(message_event, "operation"), "call_other");
  ASSERT_STREQ(mapping_string(message_event, "access_mode"), "message");
  ASSERT_EQ(mapping_number(message_event, "message_only_cross_owner"), 1);
  ASSERT_EQ(mapping_number(message_event, "direct_cross_owner_write"), 0);

  auto* present_event = events->u.arr->item[2].u.map;
  ASSERT_STREQ(mapping_string(present_event, "operation"), "present");
  ASSERT_STREQ(mapping_string(present_event, "access_mode"), "message");
  ASSERT_EQ(mapping_number(present_event, "message_only_cross_owner"), 1);
  ASSERT_EQ(mapping_number(present_event, "direct_cross_owner_write"), 0);

  auto* rejected_event = events->u.arr->item[3].u.map;
  ASSERT_STREQ(mapping_string(rejected_event, "operation"), "unknown_access");
  ASSERT_STREQ(mapping_string(rejected_event, "access_mode"), "reject");
  ASSERT_EQ(mapping_number(rejected_event, "rejected_by_default"), 1);
  ASSERT_EQ(mapping_number(trace, "direct_cross_owner_write"), 0);
  free_mapping(trace);

  vm_owner_clear_id(source);
  vm_owner_clear_id(target);
}

TEST_F(DriverTest, TestVmMulticoreModeControlsCrossOwnerBlocking) {
  object_t* source = find_object("single/master.c");
  object_t* target = find_object("single/simul_efun.c");
  ASSERT_NE(source, nullptr);
  ASSERT_NE(target, nullptr);
  vm_owner_set_id(source, "owner/test/mode/source");
  vm_owner_set_id(target, "owner/test/mode/target");

  auto saved_mode = CONFIG_INT(__RC_MULTICORE_MODE__);
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_OFF;
  ASSERT_STREQ(vm_multicore_mode_name(vm_multicore_mode()), "off");
  ASSERT_FALSE(vm_multicore_audit_enabled());
  ASSERT_FALSE(vm_owner_cross_owner_access_blocked(source, target, "call_other"));

  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;
  ASSERT_STREQ(vm_multicore_mode_name(vm_multicore_mode()), "audit");
  ASSERT_TRUE(vm_multicore_audit_enabled());
  ASSERT_FALSE(vm_multicore_enforced());
  ASSERT_FALSE(vm_owner_cross_owner_access_blocked(source, target, "call_other"));

  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_ENFORCED;
  ASSERT_STREQ(vm_multicore_mode_name(vm_multicore_mode()), "enforced");
  ASSERT_TRUE(vm_multicore_enforced());
  ASSERT_FALSE(vm_owner_cross_owner_access_blocked(source, target, "environment"));
  ASSERT_TRUE(vm_owner_cross_owner_access_blocked(source, target, "call_other"));
  ASSERT_TRUE(vm_owner_cross_owner_access_blocked(source, target, "present"));
  ASSERT_TRUE(vm_owner_cross_owner_access_blocked(source, target, "parser"));
  ASSERT_TRUE(vm_owner_cross_owner_access_blocked(source, target, "unknown_access"));

  CONFIG_INT(__RC_MULTICORE_MODE__) = 999;
  ASSERT_STREQ(vm_multicore_mode_name(vm_multicore_mode()), "audit");
  ASSERT_FALSE(vm_multicore_enforced());

  CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
  vm_owner_clear_id(source);
  vm_owner_clear_id(target);
}

TEST_F(DriverTest, TestVmOwnerAccessFastBypassOnlySkipsDefaultModeDiagnostics) {
  object_t* source = find_object("single/master.c");
  object_t* target = find_object("single/simul_efun.c");
  ASSERT_NE(source, nullptr);
  ASSERT_NE(target, nullptr);
  vm_owner_set_id(source, "owner/test/fast-bypass/source");
  vm_owner_set_id(target, "owner/test/fast-bypass/target");


  auto saved_mode = CONFIG_INT(__RC_MULTICORE_MODE__);
  auto* before = vm_owner_access_trace(0);
  auto before_total = mapping_number(before, "total_traced");
  free_mapping(before);
  auto* before_task_trace = vm_owner_task_trace(0);
  auto before_task_total = mapping_number(before_task_trace, "total_traced");
  free_mapping(before_task_trace);

  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_OFF;
  ASSERT_TRUE(vm_owner_access_fast_bypass(source, target));
  ASSERT_EQ(vm_owner_record_cross_owner_access(source, target, "call_other"), 0u);
  ASSERT_EQ(vm_owner_record_task_trace("owner/test/fast-bypass/source", "interactive", "look", 1,
                                       "off_mode_probe"),
            0u);
  auto* off_trace = vm_owner_access_trace(0);
  ASSERT_EQ(mapping_number(off_trace, "total_traced"), before_total);
  free_mapping(off_trace);
  auto* off_task_trace = vm_owner_task_trace(0);
  ASSERT_EQ(mapping_number(off_task_trace, "total_traced"), before_task_total);
  free_mapping(off_task_trace);

  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;
  ASSERT_FALSE(vm_owner_access_fast_bypass(source, target));
  ASSERT_GT(vm_owner_record_cross_owner_access(source, target, "call_other"), 0u);
  ASSERT_GT(vm_owner_record_task_trace("owner/test/fast-bypass/source", "interactive", "look", 1,
                                       "audit_mode_probe"),
            0u);
  ASSERT_FALSE(vm_owner_cross_owner_access_blocked(source, target, "call_other"));

  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_ENFORCED;
  ASSERT_FALSE(vm_owner_access_fast_bypass(source, target));
  ASSERT_TRUE(vm_owner_cross_owner_access_blocked(source, target, "call_other"));
  ASSERT_TRUE(vm_owner_access_fast_bypass(source, source));

  CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
  vm_owner_clear_id(source);
  vm_owner_clear_id(target);
}

TEST_F(DriverTest, TestCurrentOwnerScopeControlsCrossOwnerBlocking) {
  object_t* source = find_object("single/master.c");
  object_t* target = find_object("single/simul_efun.c");
  ASSERT_NE(source, nullptr);
  ASSERT_NE(target, nullptr);

  auto saved_mode = CONFIG_INT(__RC_MULTICORE_MODE__);
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_ENFORCED;

  vm_owner_set_id(source, "owner/test/scope/source-object");
  vm_owner_set_id(target, "owner/test/scope/target");


  ASSERT_TRUE(vm_owner_cross_owner_access_blocked(source, target, "call_other"));
  {
    VMOwnerScope scope(vm_context(), "owner/test/scope/target", vm_owner_epoch(target));
    ASSERT_FALSE(vm_owner_cross_owner_access_blocked(source, target, "call_other"));
    ASSERT_EQ(vm_owner_record_cross_owner_access(source, target, "call_other"), 0u);
  }
  {
    VMOwnerScope scope(vm_context(), "owner/test/scope/other", 1);
    ASSERT_TRUE(vm_owner_cross_owner_access_blocked(source, target, "call_other"));
  }
  vm_owner_clear_id(target);
  ASSERT_FALSE(vm_owner_cross_owner_access_blocked(source, target, "call_other"));
  vm_owner_set_id(target, "owner/test/scope/target");
  vm_owner_clear_id(source);
  ASSERT_FALSE(vm_owner_cross_owner_access_blocked(source, target, "call_other"));
  {
    VMOwnerScope scope(vm_context(), "owner/test/scope/other", 1);
    ASSERT_FALSE(vm_owner_cross_owner_access_blocked(source, target, "call_other"));
  }
  vm_owner_set_id(source, "owner/test/scope/source-object");
  {
    VMOwnerScope scope(vm_context(), vm_owner_default_id(), 0);
    vm_owner_set_id(source, "owner/test/scope/target");
    set_command_giver(source);
    ASSERT_FALSE(vm_owner_cross_owner_access_blocked(source, target, "call_other"));
    ASSERT_EQ(vm_owner_record_cross_owner_access(source, target, "call_other"), 0u);
    ASSERT_GT(vm_owner_record_access(source, target, "call_other"), 0u);
    auto* trace = vm_owner_access_trace(1);
    auto* events = find_string_in_mapping(trace, "events");
    ASSERT_NE(events, nullptr);
    ASSERT_EQ(events->type, T_ARRAY);
    ASSERT_EQ(events->u.arr->size, 1);
    ASSERT_EQ(mapping_number(events->u.arr->item[0].u.map, "cross_owner"), 0);
    ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"),
                 "owner/test/scope/target");
    ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"),
                 "owner/test/scope/target");
    free_mapping(trace);

    vm_owner_set_id(source, "owner/test/scope/source-object");
    set_command_giver(source);
    ASSERT_TRUE(vm_owner_cross_owner_access_blocked(source, target, "call_other"));

    set_command_giver(target);
    ASSERT_FALSE(vm_owner_cross_owner_access_blocked(source, target, "call_other"));
    ASSERT_GT(vm_owner_record_access(source, target, "call_other"), 0u);
    trace = vm_owner_access_trace(1);
    events = find_string_in_mapping(trace, "events");
    ASSERT_NE(events, nullptr);
    ASSERT_EQ(events->type, T_ARRAY);
    ASSERT_EQ(events->u.arr->size, 1);
    ASSERT_EQ(mapping_number(events->u.arr->item[0].u.map, "cross_owner"), 0);
    ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"),
                 "owner/test/scope/target");
    ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"),
                 "owner/test/scope/target");
    free_mapping(trace);
    set_command_giver(nullptr);
  }
  set_command_giver(target);
  ASSERT_TRUE(vm_owner_cross_owner_access_blocked(source, target, "call_other"));
  set_command_giver(nullptr);

  CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
  vm_owner_clear_id(source);
  vm_owner_clear_id(target);
}

TEST_F(DriverTest, TestAllInventoryRecordsCrossOwnerAccessTrace) {
  object_t* source = find_object("single/master.c");
  object_t* target = find_object("single/simul_efun.c");
  ASSERT_NE(source, nullptr);
  ASSERT_NE(target, nullptr);


  auto* before = vm_owner_access_trace(0);
  auto before_total = mapping_number(before, "total_traced");
  free_mapping(before);

  current_object = source;
  vm_owner_set_id(source, "owner/test/inventory/shared");
  vm_owner_set_id(target, "owner/test/inventory/shared");
  ASSERT_EQ(all_inventory(target, 0), &the_null_array);

  auto* same_owner = vm_owner_access_trace(0);
  ASSERT_EQ(mapping_number(same_owner, "total_traced"), before_total);
  free_mapping(same_owner);

  vm_owner_set_id(target, "owner/test/inventory/target");
  ASSERT_EQ(all_inventory(target, 0), &the_null_array);

  auto* trace = vm_owner_access_trace(1);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  ASSERT_EQ(events->u.arr->size, 1);
  ASSERT_EQ(mapping_number(events->u.arr->item[0].u.map, "cross_owner"), 1);
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "operation"), "all_inventory");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"),
               "owner/test/inventory/shared");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"),
               "owner/test/inventory/target");
  free_mapping(trace);

  vm_owner_clear_id(source);
  vm_owner_clear_id(target);
}

TEST_F(DriverTest, TestPresentRecordsCrossOwnerAccessTrace) {
  object_t* source = find_object("single/master.c");
  object_t* target = find_object("single/simul_efun.c");
  ASSERT_NE(source, nullptr);
  ASSERT_NE(target, nullptr);


  auto* before = vm_owner_access_trace(0);
  auto before_total = mapping_number(before, "total_traced");
  free_mapping(before);

  svalue_t needle;
  needle.type = T_STRING;
  needle.subtype = STRING_CONSTANT;
  needle.u.string = "nonexistent";
  current_object = source;
  vm_owner_set_id(source, "owner/test/present/shared");
  vm_owner_set_id(target, "owner/test/present/shared");
  ASSERT_EQ(object_present(&needle, target), nullptr);

  auto* same_owner = vm_owner_access_trace(0);
  ASSERT_EQ(mapping_number(same_owner, "total_traced"), before_total);
  free_mapping(same_owner);

  vm_owner_set_id(target, "owner/test/present/target");
  ASSERT_EQ(object_present(&needle, target), nullptr);

  auto* trace = vm_owner_access_trace(1);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  ASSERT_EQ(events->u.arr->size, 1);
  ASSERT_EQ(mapping_number(events->u.arr->item[0].u.map, "cross_owner"), 1);
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "operation"), "present");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"),
               "owner/test/present/shared");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"),
               "owner/test/present/target");
  free_mapping(trace);

  vm_owner_clear_id(source);
  vm_owner_clear_id(target);
}

TEST_F(DriverTest, TestPresentEnforcedModeBlocksCrossOwnerIdSearch) {
  object_t* source = find_object("single/master.c");
  object_t* target = find_object("single/simul_efun.c");
  ASSERT_NE(source, nullptr);
  ASSERT_NE(target, nullptr);

  current_object = source;
  vm_owner_set_id(source, "owner/test/present/enforced/source");
  vm_owner_set_id(target, "owner/test/present/enforced/target");

  auto saved_mode = CONFIG_INT(__RC_MULTICORE_MODE__);
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_ENFORCED;

  svalue_t needle;
  needle.type = T_STRING;
  needle.subtype = STRING_CONSTANT;
  needle.u.string = "anything";

  bool blocked = false;
  error_context_t econ{};
  save_context(&econ);
  try {
    (void)object_present(&needle, target);
    pop_context(&econ);
  } catch (...) {
    restore_context(&econ);
    blocked = true;
  }

  CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
  ASSERT_TRUE(blocked);

  vm_owner_clear_id(source);
  vm_owner_clear_id(target);
}

TEST_F(DriverTest, TestParserEnforcedModeBlocksCrossOwnerInterrogateApply) {
  object_t* parser = load_object_for_test("single/tests/efuns/parser_owner_probe");
  object_t* item = clone_object_for_test("single/tests/efuns/parser_owner_probe");
  ASSERT_NE(parser, nullptr);
  ASSERT_NE(item, nullptr);

  current_object = parser;
  vm_owner_set_id(parser, "owner/test/parser/enforced/source");
  vm_owner_set_id(item, "owner/test/parser/enforced/target");
  auto saved_mode = CONFIG_INT(__RC_MULTICORE_MODE__);
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_ENFORCED;

  auto* targets = allocate_array(1);
  targets->item[0].type = T_OBJECT;
  targets->item[0].u.ob = item;
  add_ref(item, "parser owner probe target");
  push_refed_array(targets);

  auto* ret = safe_apply("parse_targets", parser, 1, ORIGIN_DRIVER);
  ASSERT_NE(ret, nullptr);
  ASSERT_EQ(ret->type, T_NUMBER);
  ASSERT_EQ(ret->u.number, 0);

  CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
  vm_owner_clear_id(parser);
  vm_owner_clear_id(item);
  destruct_object(item);
}

TEST_F(DriverTest, TestMoveObjectRecordsCrossOwnerAccessTrace) {
  object_t* item = load_object_for_test("single/void");
  object_t* dest = find_object("single/simul_efun.c");
  ASSERT_NE(item, nullptr);
  ASSERT_NE(dest, nullptr);


  auto* before = vm_owner_access_trace(0);
  auto before_total = mapping_number(before, "total_traced");
  free_mapping(before);

  vm_owner_set_id(item, "owner/test/move/shared");
  vm_owner_set_id(dest, "owner/test/move/shared");
  move_object(item, dest);

  auto* same_owner = vm_owner_access_trace(0);
  ASSERT_EQ(mapping_number(same_owner, "total_traced"), before_total);
  free_mapping(same_owner);

  vm_owner_set_id(dest, "owner/test/move/dest");
  move_object(item, dest);

  auto* trace = vm_owner_access_trace(1);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  ASSERT_EQ(events->u.arr->size, 1);
  ASSERT_EQ(mapping_number(events->u.arr->item[0].u.map, "cross_owner"), 1);
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "operation"), "move_object");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"),
               "owner/test/move/shared");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"),
               "owner/test/move/dest");
  free_mapping(trace);

  vm_owner_clear_id(item);
  vm_owner_clear_id(dest);
  destruct_object(item);
}

TEST_F(DriverTest, TestMoveObjectEnforcedModeBlocksCrossOwnerMove) {
  object_t* item = load_object_for_test("single/void");
  object_t* dest = find_object("single/simul_efun.c");
  ASSERT_NE(item, nullptr);
  ASSERT_NE(dest, nullptr);


  vm_owner_set_id(item, "owner/test/move/enforced/source");
  vm_owner_set_id(dest, "owner/test/move/enforced/dest");
  auto saved_mode = CONFIG_INT(__RC_MULTICORE_MODE__);
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_ENFORCED;
  auto* before = vm_owner_access_trace(0);
  auto before_blocks = mapping_number(before, "enforced_blocks");
  free_mapping(before);

  bool blocked = false;
  error_context_t econ{};
  save_context(&econ);
  try {
    move_object(item, dest);
    pop_context(&econ);
  } catch (...) {
    restore_context(&econ);
    blocked = true;
  }

  auto* after = vm_owner_access_trace(0);
  auto after_blocks = mapping_number(after, "enforced_blocks");
  free_mapping(after);
  CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
  ASSERT_TRUE(blocked);
  ASSERT_GT(after_blocks, before_blocks);

  vm_owner_clear_id(item);
  vm_owner_clear_id(dest);
  destruct_object(item);
}

TEST_F(DriverTest, TestDestructRecordsCrossOwnerAccessTrace) {
  object_t* source = find_object("single/master.c");
  object_t* target = load_object_for_test("single/on_destruct_good");
  ASSERT_NE(source, nullptr);
  ASSERT_NE(target, nullptr);


  auto* before = vm_owner_access_trace(0);
  auto before_total = mapping_number(before, "total_traced");
  free_mapping(before);

  current_object = source;
  vm_owner_set_id(source, "owner/test/destruct/shared");
  vm_owner_set_id(target, "owner/test/destruct/shared");
  vm_owner_record_cross_owner_access(source, target, "destruct-primer");

  auto* same_owner = vm_owner_access_trace(0);
  ASSERT_EQ(mapping_number(same_owner, "total_traced"), before_total);
  free_mapping(same_owner);

  vm_owner_set_id(target, "owner/test/destruct/target");
  destruct_object(target);

  auto* trace = vm_owner_access_trace(1);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  ASSERT_EQ(events->u.arr->size, 1);
  ASSERT_EQ(mapping_number(events->u.arr->item[0].u.map, "cross_owner"), 1);
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "operation"), "destruct");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"),
               "owner/test/destruct/shared");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"),
               "owner/test/destruct/target");
  free_mapping(trace);

  vm_owner_clear_id(source);
}

TEST_F(DriverTest, TestDestructEnforcedModeBlocksCrossOwnerDestruct) {
  object_t* source = find_object("single/master.c");
  object_t* target = load_object_for_test("single/on_destruct_good");
  ASSERT_NE(source, nullptr);
  ASSERT_NE(target, nullptr);


  current_object = source;
  vm_owner_set_id(source, "owner/test/destruct/enforced/source");
  vm_owner_set_id(target, "owner/test/destruct/enforced/target");
  auto saved_mode = CONFIG_INT(__RC_MULTICORE_MODE__);
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_ENFORCED;
  auto* before = vm_owner_access_trace(0);
  auto before_blocks = mapping_number(before, "enforced_blocks");
  free_mapping(before);

  bool blocked = false;
  error_context_t econ{};
  save_context(&econ);
  try {
    destruct_object(target);
    pop_context(&econ);
  } catch (...) {
    restore_context(&econ);
    blocked = true;
  }

  auto* after = vm_owner_access_trace(0);
  auto after_blocks = mapping_number(after, "enforced_blocks");
  free_mapping(after);
  CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
  ASSERT_TRUE(blocked);
  ASSERT_GT(after_blocks, before_blocks);
  ASSERT_EQ(target->flags & O_DESTRUCTED, 0);

  vm_owner_clear_id(source);
  vm_owner_clear_id(target);
  destruct_object(target);
}

TEST_F(DriverTest, TestCallOtherRecordsCrossOwnerAccessTrace) {
  object_t* caller = load_object_for_test("single/void");
  object_t* target = load_object_for_test("single/on_destruct_good");
  ASSERT_NE(caller, nullptr);
  ASSERT_NE(target, nullptr);


  auto* before = vm_owner_access_trace(0);
  auto before_total = mapping_number(before, "total_traced");
  free_mapping(before);

  vm_owner_set_id(caller, "owner/test/call/shared");
  vm_owner_set_id(target, "owner/test/call/shared");
  push_object(target);
  ASSERT_NE(safe_apply("call_target", caller, 1, ORIGIN_DRIVER), nullptr);

  auto* same_owner = vm_owner_access_trace(0);
  ASSERT_EQ(mapping_number(same_owner, "total_traced"), before_total);
  free_mapping(same_owner);

  vm_owner_set_id(target, "owner/test/call/target");
  push_object(target);
  ASSERT_NE(safe_apply("call_target", caller, 1, ORIGIN_DRIVER), nullptr);

  auto* trace = vm_owner_access_trace(1);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  ASSERT_EQ(events->u.arr->size, 1);
  ASSERT_EQ(mapping_number(events->u.arr->item[0].u.map, "cross_owner"), 1);
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "operation"), "call_other");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"),
               "owner/test/call/shared");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"),
               "owner/test/call/target");
  free_mapping(trace);

  vm_owner_clear_id(caller);
  vm_owner_clear_id(target);
  destruct_object(caller);
  destruct_object(target);
}

TEST_F(DriverTest, TestCallOtherEnforcedModeBlocksCrossOwnerCall) {
  object_t* caller = load_object_for_test("single/void");
  object_t* target = load_object_for_test("single/on_destruct_good");
  ASSERT_NE(caller, nullptr);
  ASSERT_NE(target, nullptr);

  vm_owner_set_id(caller, "owner/test/call/enforced/source");
  vm_owner_set_id(target, "owner/test/call/enforced/target");


  auto saved_mode = CONFIG_INT(__RC_MULTICORE_MODE__);
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_ENFORCED;
  auto* before = vm_owner_access_trace(0);
  auto before_blocks = mapping_number(before, "enforced_blocks");
  free_mapping(before);

  bool blocked = false;
  error_context_t econ{};
  save_context(&econ);
  try {
    push_object(target);
    blocked = safe_apply("call_target", caller, 1, ORIGIN_DRIVER) == nullptr;
    pop_context(&econ);
  } catch (...) {
    restore_context(&econ);
    blocked = true;
  }

  auto* after = vm_owner_access_trace(0);
  auto after_blocks = mapping_number(after, "enforced_blocks");
  free_mapping(after);
  ASSERT_TRUE(blocked);
  ASSERT_GT(after_blocks, before_blocks);

  push_object(target);
  auto* submitted = safe_apply("call_owner_async_echo", caller, 1, ORIGIN_DRIVER);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  auto* submitted_map = submitted->u.map;
  auto future_id = mapping_number(submitted_map, "future_id");
  ASSERT_EQ(mapping_number(submitted_map, "success"), 1);
  ASSERT_GT(future_id, 0);
  ASSERT_EQ(mapping_number(submitted_map, "async_only"), 1);
  ASSERT_EQ(mapping_number(submitted_map, "frozen_payload"), 1);
  ASSERT_EQ(mapping_number(submitted_map, "payload_frozen"), 1);
  ASSERT_EQ(mapping_number(submitted_map, "requires_owner_mailbox"), 1);
  ASSERT_EQ(mapping_number(submitted_map, "requires_owner_main_queue"), 0);
  ASSERT_EQ(mapping_number(submitted_map, "main_required"), 0);
  ASSERT_EQ(mapping_number(submitted_map, "queued_on_main"), 0);
  ASSERT_EQ(mapping_number(submitted_map, "message_only_cross_owner"), 1);
  ASSERT_EQ(mapping_number(submitted_map, "direct_cross_owner_write"), 0);
  ASSERT_EQ(mapping_number(submitted_map, "target_handle_current"), 1);
  ASSERT_STREQ(mapping_string(submitted_map, "source_owner_id"), "owner/test/call/enforced/source");
  ASSERT_STREQ(mapping_string(submitted_map, "target_owner_id"), "owner/test/call/enforced/target");
  ASSERT_STREQ(mapping_string(submitted_map, "message_type"), "owner_async_echo");
  ASSERT_STREQ(mapping_string(submitted_map, "payload_key"), "cross-owner/echo/v1");

  auto* pending = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(pending, "success"), 1);
  ASSERT_STREQ(mapping_string(pending, "state"), "pending");
  ASSERT_EQ(mapping_number(pending, "requires_owner_message_completion"), 1);
  ASSERT_EQ(mapping_number(pending, "payload_frozen"), 1);
  ASSERT_EQ(mapping_number(pending, "frozen_result"), 0);
  free_mapping(pending);

  vm_owner_thread_start(1);
  for (int i = 0; i < 100; i++) {
    auto* maybe_completed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
    auto completed_state = std::string(mapping_string(maybe_completed, "state"));
    free_mapping(maybe_completed);
    if (completed_state == "completed") {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  auto* completed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(completed, "success"), 1);
  ASSERT_STREQ(mapping_string(completed, "state"), "completed");
  ASSERT_STREQ(mapping_string(completed, "result_key"), "owner_async_echo");
  ASSERT_EQ(mapping_number(completed, "requires_owner_message_completion"), 0);
  ASSERT_EQ(mapping_number(completed, "payload_frozen"), 1);
  ASSERT_EQ(mapping_number(completed, "frozen_result"), 1);
  ASSERT_EQ(mapping_number(completed, "direct_cross_owner_write"), 0);
  auto* result = find_string_in_mapping(completed, "result");
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_MAPPING);
  ASSERT_EQ(mapping_number(result->u.map, "reply"), 42);
  ASSERT_STREQ(mapping_string(result->u.map, "payload_key"), "cross-owner/echo/v1");
  ASSERT_STREQ(mapping_string(result->u.map, "target_owner_id"), "owner/test/call/enforced/target");
  free_mapping(completed);
  // Consume the completed future so the global store returns to baseline
  // (tests that assert a clean store later must not see this record).
  auto* taken = vm_owner_future_take(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(taken, "consumed"), 1);
  free_mapping(taken);
  vm_owner_thread_stop();

  auto* trace = vm_owner_message_trace(1);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  ASSERT_EQ(events->u.arr->size, 1);
  auto* message_event = events->u.arr->item[0].u.map;
  ASSERT_EQ(mapping_number(message_event, "message_id"), future_id);
  ASSERT_STREQ(mapping_string(message_event, "source_owner_id"), "owner/test/call/enforced/source");
  ASSERT_STREQ(mapping_string(message_event, "target_owner_id"), "owner/test/call/enforced/target");
  ASSERT_STREQ(mapping_string(message_event, "message_type"), "owner_async_echo");
  ASSERT_STREQ(mapping_string(message_event, "state"), "completed");
  ASSERT_STREQ(mapping_string(message_event, "route"), "owner_mailbox");
  ASSERT_STREQ(mapping_string(message_event, "result_key"), "owner_async_echo");
  ASSERT_STREQ(mapping_string(message_event, "error"), "");
  ASSERT_STREQ(mapping_string(message_event, "target_handle_status"), "current");
  ASSERT_EQ(mapping_number(message_event, "pending"), 0);
  ASSERT_EQ(mapping_number(message_event, "completed"), 1);
  ASSERT_EQ(mapping_number(message_event, "failed"), 0);
  ASSERT_EQ(mapping_number(message_event, "terminal"), 1);
  ASSERT_EQ(mapping_number(message_event, "direct_cross_owner_write"), 0);
  ASSERT_EQ(mapping_number(message_event, "payload_frozen"), 1);
  ASSERT_EQ(mapping_number(message_event, "frozen_result"), 1);
  ASSERT_EQ(mapping_number(message_event, "has_target_handle"), 1);
  ASSERT_EQ(mapping_number(message_event, "target_handle_current"), 1);
  ASSERT_EQ(mapping_number(message_event, "requires_owner_mailbox"), 1);
  ASSERT_EQ(mapping_number(message_event, "requires_owner_main_queue"), 0);
  ASSERT_EQ(mapping_number(message_event, "main_required"), 0);
  ASSERT_EQ(mapping_number(message_event, "queued_on_main"), 0);
  ASSERT_EQ(mapping_number(message_event, "message_only_cross_owner"), 1);
  free_mapping(trace);

  CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
  vm_owner_clear_id(caller);
  vm_owner_clear_id(target);
  destruct_object(caller);
  destruct_object(target);
}

TEST_F(DriverTest, TestVmOwnerObjectMessageRejectsNonFrozenResult) {
  const char* source_owner = "owner/test/async/non-frozen/source";
  const char* target_owner = "owner/test/async/non-frozen/target";

  object_t* target = load_object_for_test("single/on_destruct_good");
  ASSERT_NE(target, nullptr);
  vm_owner_set_id(target, target_owner);
  auto handle = vm_object_handle(target);


  auto* submitted = vm_owner_submit_object_message(
      source_owner, handle, "owner_async_non_frozen_result", "cross-owner/non-frozen/v1");
  auto future_id = mapping_number(submitted, "future_id");
  auto target_task_id = mapping_number(submitted, "target_task_id");
  ASSERT_EQ(mapping_number(submitted, "success"), 1);
  ASSERT_GT(future_id, 0);
  ASSERT_GT(target_task_id, 0);
  ASSERT_EQ(mapping_number(submitted, "has_target_handle"), 1);
  ASSERT_EQ(mapping_number(submitted, "target_handle_current"), 1);
  ASSERT_EQ(mapping_number(submitted, "payload_frozen"), 1);
  ASSERT_EQ(mapping_number(submitted, "requires_owner_mailbox"), 1);
  ASSERT_EQ(mapping_number(submitted, "requires_owner_main_queue"), 0);
  ASSERT_EQ(mapping_number(submitted, "main_required"), 0);
  ASSERT_EQ(mapping_number(submitted, "queued_on_main"), 0);
  ASSERT_STREQ(mapping_string(submitted, "source_owner_id"), source_owner);
  ASSERT_STREQ(mapping_string(submitted, "target_owner_id"), target_owner);
  free_mapping(submitted);

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

  auto* failed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(failed, "success"), 1);
  ASSERT_EQ(mapping_number(failed, "target_task_id"), target_task_id);
  ASSERT_STREQ(mapping_string(failed, "state"), "failed");
  ASSERT_STREQ(mapping_string(failed, "error"), "owner async result must be frozen data");
  ASSERT_EQ(mapping_number(failed, "requires_owner_message_completion"), 0);
  ASSERT_EQ(mapping_number(failed, "payload_frozen"), 1);
  ASSERT_EQ(mapping_number(failed, "frozen_result"), 0);
  ASSERT_EQ(mapping_number(failed, "direct_cross_owner_write"), 0);
  free_mapping(failed);
  // Consume the failed future so the global store returns to baseline.
  auto* taken = vm_owner_future_take(static_cast<uint64_t>(future_id));
  ASSERT_EQ(mapping_number(taken, "consumed"), 1);
  free_mapping(taken);
  vm_owner_thread_stop();

  auto* trace = vm_owner_message_trace(1);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  ASSERT_EQ(events->u.arr->size, 1);
  auto* message_event = events->u.arr->item[0].u.map;
  ASSERT_EQ(mapping_number(message_event, "message_id"), future_id);
  ASSERT_EQ(mapping_number(message_event, "target_task_id"), target_task_id);
  ASSERT_STREQ(mapping_string(message_event, "message_type"), "owner_async_non_frozen_result");
  ASSERT_STREQ(mapping_string(message_event, "state"), "failed");
  ASSERT_STREQ(mapping_string(message_event, "route"), "owner_mailbox");
  ASSERT_STREQ(mapping_string(message_event, "result_key"), "");
  ASSERT_STREQ(mapping_string(message_event, "error"), "owner async result must be frozen data");
  ASSERT_STREQ(mapping_string(message_event, "target_handle_status"), "current");
  ASSERT_EQ(mapping_number(message_event, "pending"), 0);
  ASSERT_EQ(mapping_number(message_event, "completed"), 0);
  ASSERT_EQ(mapping_number(message_event, "failed"), 1);
  ASSERT_EQ(mapping_number(message_event, "terminal"), 1);
  ASSERT_EQ(mapping_number(message_event, "direct_cross_owner_write"), 0);
  ASSERT_EQ(mapping_number(message_event, "payload_frozen"), 1);
  ASSERT_EQ(mapping_number(message_event, "frozen_result"), 0);
  ASSERT_EQ(mapping_number(message_event, "has_target_handle"), 1);
  ASSERT_EQ(mapping_number(message_event, "target_handle_current"), 1);
  ASSERT_EQ(mapping_number(message_event, "requires_owner_mailbox"), 1);
  ASSERT_EQ(mapping_number(message_event, "requires_owner_main_queue"), 0);
  ASSERT_EQ(mapping_number(message_event, "main_required"), 0);
  ASSERT_EQ(mapping_number(message_event, "queued_on_main"), 0);
  free_mapping(trace);

  auto* owner_status = vm_object_store_owner_status(target_owner);
  ASSERT_EQ(mapping_number(owner_status, "pending_messages"), 0);
  free_mapping(owner_status);

  vm_owner_clear_id(target);
  destruct_object(target);
}

TEST_F(DriverTest, TestVmOwnerScheduleRoundsOwnersAndKeepsOwnerFifo) {
  const char* owner_a = "owner/test/schedule/a";
  const char* owner_b = "owner/test/schedule/b";
  auto a_first = vm_owner_enqueue_task(owner_a, "command", "a-first");
  auto a_second = vm_owner_enqueue_task(owner_a, "command", "a-second");
  auto b_first = vm_owner_enqueue_task(owner_b, "executor_probe", "b-first");


  auto* result = vm_owner_schedule(3);
  ASSERT_EQ(mapping_number(result, "dispatched"), 3);
  ASSERT_EQ(mapping_number(result, "remaining"), 0);
  auto* tasks = find_string_in_mapping(result, "tasks");
  ASSERT_NE(tasks, nullptr);
  ASSERT_EQ(tasks->type, T_ARRAY);
  ASSERT_EQ(tasks->u.arr->size, 3);
  ASSERT_EQ(mapping_number(tasks->u.arr->item[0].u.map, "task_id"), static_cast<long>(a_first));
  ASSERT_STREQ(mapping_string(tasks->u.arr->item[0].u.map, "task_key"), "a-first");
  ASSERT_EQ(mapping_number(tasks->u.arr->item[1].u.map, "task_id"), static_cast<long>(b_first));
  ASSERT_STREQ(mapping_string(tasks->u.arr->item[1].u.map, "task_key"), "b-first");
  ASSERT_EQ(mapping_number(tasks->u.arr->item[2].u.map, "task_id"), static_cast<long>(a_second));
  ASSERT_STREQ(mapping_string(tasks->u.arr->item[2].u.map, "task_key"), "a-second");
  free_mapping(result);
}

TEST_F(DriverTest, TestVmOwnerThreadExperimentIsOptInAndDispatchesMailboxTasks) {
  const char* owner = "owner/test/thread";

  vm_owner_thread_stop();
  auto* initial = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(initial, "enabled"), 0);
  ASSERT_EQ(mapping_number(initial, "thread_count"), 0);
  free_mapping(initial);

  auto task_id = vm_owner_enqueue_task(owner, "command", "threaded-look");
  ASSERT_GT(task_id, 0u);
  auto* queued = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(queued, "owner_queue_depth"), 1);
  free_mapping(queued);

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

  auto* drained = vm_owner_mailbox_status(owner);
  ASSERT_EQ(mapping_number(drained, "owner_queue_depth"), 0);
  free_mapping(drained);

  auto* trace = vm_owner_task_trace(2);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  ASSERT_GE(events->u.arr->size, 2);
  ASSERT_STREQ(mapping_string(events->u.arr->item[events->u.arr->size - 1].u.map, "state"),
               "thread_dispatched");
  ASSERT_EQ(mapping_number(events->u.arr->item[events->u.arr->size - 1].u.map, "task_id"),
            static_cast<long>(task_id));
  ASSERT_STREQ(mapping_string(events->u.arr->item[events->u.arr->size - 1].u.map, "owner_id"),
               owner);
  free_mapping(trace);

  auto* running = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(running, "enabled"), 1);
  ASSERT_EQ(mapping_number(running, "thread_count"), 1);
  ASSERT_GE(mapping_number(running, "thread_dispatched"), 1);
  ASSERT_GE(mapping_number(running, "thread_context_bound"), 1);
  ASSERT_GE(mapping_number(running, "thread_object_store_isolated"), 1);
  ASSERT_GE(mapping_number(running, "thread_owner_bound"), 1);
  ASSERT_GE(mapping_number(running, "thread_owner_cleared"), 1);
  free_mapping(running);

  vm_owner_thread_stop();
  auto* stopped = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(stopped, "enabled"), 0);
  ASSERT_EQ(mapping_number(stopped, "thread_count"), 0);
  free_mapping(stopped);
}

TEST_F(DriverTest, TestVmOwnerExecutorCommandConsumeEntryDispatchesWithoutLpc) {
  const char* owner = "owner/test/executor/command-consume";

  vm_owner_thread_stop();

  auto* before = vm_owner_thread_status();
  auto before_consume = mapping_number(before, "executor_command_consume_entry_executed");
  auto before_safe = mapping_number(before, "executor_safe_task_dispatched");
  free_mapping(before);

  auto task_id = vm_owner_enqueue_task(owner, "command_consume", "gateway-command-consume-entry");
  ASSERT_GT(task_id, 0u);

  vm_owner_thread_start(1);
  for (int i = 0; i < 100; i++) {
    auto* status = vm_owner_thread_status();
    auto consume_done = mapping_number(status, "executor_command_consume_entry_executed");
    free_mapping(status);
    if (consume_done >= before_consume + 1) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  auto* running = vm_owner_thread_status();
  ASSERT_GE(mapping_number(running, "executor_command_consume_entry_executed"), before_consume + 1);
  ASSERT_GE(mapping_number(running, "executor_safe_task_dispatched"), before_safe + 1);
  ASSERT_EQ(mapping_number(running, "ordinary_lpc_default_closed"), 1);
  free_mapping(running);

  auto* trace = vm_owner_task_trace(8);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  bool found_entry_ready = false;
  for (int i = 0; i < events->u.arr->size; i++) {
    auto* event = events->u.arr->item[i].u.map;
    if (mapping_number(event, "task_id") == static_cast<long>(task_id) &&
        std::string(mapping_string(event, "state")) == "thread_command_consume_entry_ready") {
      found_entry_ready = true;
      ASSERT_STREQ(mapping_string(event, "task_type"), "command_consume");
      ASSERT_STREQ(mapping_string(event, "task_key"), "gateway-command-consume-entry");
      ASSERT_STREQ(mapping_string(event, "owner_id"), owner);
    }
  }
  ASSERT_TRUE(found_entry_ready);
  free_mapping(trace);

  vm_owner_thread_stop();
}

