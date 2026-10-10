#include "test_lpc_support.h"

#ifdef PACKAGE_ASYNC
namespace {
void verify_async_callback_drain(const char* method, LPC_INT expected_callbacks) {
  auto* object = load_object_for_test("single/tests/efuns/async_nested_callbacks");
  ASSERT_NE(object, nullptr);
  struct DrainGuard {
    object_t* object;
    ~DrainGuard() {
      complete_all_asyncio();
      destruct_object_for_test(object);
    }
  } guard{object};
  const auto refs_before = object->ref;
  auto* stack_before = sp;
  ASSERT_NE(safe_apply(method, object, 0, ORIGIN_DRIVER), nullptr);
  vm_apply_return_clear();

  // This is the shutdown drain, not a single callback pass: callbacks enqueue
  // more real file work, including after an earlier callback throws.
  complete_all_asyncio();
  EXPECT_EQ(sp, stack_before);
  EXPECT_EQ(object->ref, refs_before);
#ifdef DEBUGMALLOC_EXTENSIONS
  EXPECT_EQ(object->owner_runtime_refs, 0u);
#endif
  EXPECT_EQ(vm_owner_main_queue_total_depth(), 0u);
  auto* result = safe_apply("query_result", object, 0, ORIGIN_DRIVER);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_ARRAY);
  ASSERT_EQ(result->u.arr->size, 3);
  EXPECT_EQ(result->u.arr->item[0].u.number, expected_callbacks);
  EXPECT_EQ(result->u.arr->item[1].u.number, 1);
#if defined(DEBUGMALLOC) && defined(DEBUGMALLOC_EXTENSIONS)
  EXPECT_EQ(result->u.arr->item[2].u.number, expected_callbacks);
#endif
  vm_apply_return_clear();
  result = safe_apply("verify_cleanup", object, 0, ORIGIN_DRIVER);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_NUMBER);
  EXPECT_EQ(result->u.number, 1);
  vm_apply_return_clear();
}
}  // namespace

TEST_F(DriverTest, TestVmOwnerMainMessageMarksAcquiredReference) {
  auto* object = load_object_for_test("single/tests/efuns/async_nested_callbacks");
  ASSERT_NE(object, nullptr);
  struct CleanupGuard {
    object_t* object;
    ~CleanupGuard() {
      vm_owner_drain_main_tasks(64);
      destruct_object_for_test(object);
    }
  } guard{object};
  const auto refs_before = object->ref;
  const auto submit = [&] {
    return vm_owner_enqueue_main_task_with_payload(
        object, "owner_message", "verify_cleanup", nullptr, nullptr,
        [] { ADD_FAILURE() << "Message must execute the target lfun"; });
  };
  ASSERT_GT(submit(), 0u);
  EXPECT_EQ(object->ref, refs_before + 1);
#ifdef DEBUGMALLOC_EXTENSIONS
  EXPECT_EQ(object->owner_runtime_refs, 1u);
#endif
  vm_owner_drain_main_tasks(64);
  EXPECT_EQ(object->ref, refs_before);
#ifdef DEBUGMALLOC_EXTENSIONS
  EXPECT_EQ(object->owner_runtime_refs, 0u);
#endif
  auto* result = safe_apply("query_cleanup_calls", object, 0, ORIGIN_DRIVER);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_NUMBER);
  EXPECT_EQ(result->u.number, 1);
  vm_apply_return_clear();

  ASSERT_GT(submit(), 0u);
  vm_owner_set_id(object, "owner/test/stale-main-message");
  vm_owner_drain_main_tasks(64);
  EXPECT_EQ(object->ref, refs_before);
#ifdef DEBUGMALLOC_EXTENSIONS
  EXPECT_EQ(object->owner_runtime_refs, 0u);
#endif
  EXPECT_EQ(vm_owner_main_queue_total_depth(), 0);
  result = safe_apply("query_cleanup_calls", object, 0, ORIGIN_DRIVER);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->u.number, 1);
  vm_apply_return_clear();
}

TEST_F(DriverTest, TestAsyncNestedCallbacksDrainAllRequests) {
  verify_async_callback_drain("start_nested", 4);
}

TEST_F(DriverTest, TestAsyncThrowingCallbackStillDrainsNestedRequest) {
  verify_async_callback_drain("start_throwing", 2);
}

TEST_F(DriverTest, TestAsyncCallbackRejectsRecompileWhileExecuting) {
  struct RecompileConfigGuard {
    int saved;
    ~RecompileConfigGuard() { CONFIG_INT(__RECOMPILE_OBJECT_ENABLED__) = saved; }
  } config_guard{CONFIG_INT(__RECOMPILE_OBJECT_ENABLED__)};
  CONFIG_INT(__RECOMPILE_OBJECT_ENABLED__) = 1;
  verify_async_callback_drain("start_recompile", 1);
}

TEST_F(DriverTest, TestAsyncCallbackDropsDestructedOwnerAfterRealIo) {
  PathCleanupGuard marker{"log/async-callback-marker"};
  verify_async_callback_drain("start_marker", 1);
  ASSERT_TRUE(std::filesystem::exists(marker.path));
  ASSERT_EQ(std::remove(marker.path), 0);

  auto* victim = clone_object_for_test("single/tests/efuns/async_nested_callbacks");
  ASSERT_NE(victim, nullptr);
  EXPECT_NE(safe_apply("start_marker", victim, 0, ORIGIN_DRIVER), nullptr);
  vm_apply_return_clear();
  destruct_object_for_test(victim);
  complete_all_asyncio();
  EXPECT_FALSE(std::filesystem::exists(marker.path));
  EXPECT_EQ(vm_owner_main_queue_total_depth(), 0u);
}
#endif

TEST_F(DriverTest, TestAsyncPromiseFormsResolveAndRejectThroughOwnerAdmission) {
  clear_tick_events();
  auto* object = load_object_for_test("single/tests/efuns/async_promise");
  ASSERT_NE(object, nullptr);

  auto* result = safe_apply("run_promise_forms", object, 0, ORIGIN_DRIVER);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_PROMISE);
  auto* promise = result->u.prom;
  promise->ref++;
  vm_apply_return_clear();

  for (int pass = 0; pass < 256 && promise->state == PROMISE_PENDING; pass++) {
    if (tick_event_queue_size_for_test() != 0 || backend_wakeup_pending_for_test()) {
      // The worker completion wakes the loop through the backend self-pipe;
      // the tick pump drains that wakeup (and runs the async handler).
      run_tick_events_for_test();
    }
    if (walltime_event_queue_size_for_test() != 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      ASSERT_EQ(event_base_loop(g_event_base, EVLOOP_NONBLOCK), 0);
    }
    if (tick_event_queue_size_for_test() == 0 &&
        walltime_event_queue_size_for_test() == 0 &&
        !backend_wakeup_pending_for_test() && promise->state == PROMISE_PENDING) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  }

  ASSERT_EQ(promise->state, PROMISE_FULFILLED);
  ASSERT_EQ(promise->result.type, T_NUMBER);
  ASSERT_EQ(promise->result.u.number, 0);
  free_promise(promise);
  destruct_object_for_test(object);
}

TEST_F(DriverTest, TestAsyncAwaitDestructRejectsSuspendedFrame) {
  clear_tick_events();
  object_t* object = load_object_for_test("single/async_phase2_probe");
  ASSERT_NE(object, nullptr);
  program_t* owner_program = object->prog;
  const unsigned int owner_program_ref_before = owner_program->ref;

  auto* result = safe_apply("suspend_once", object, 0, ORIGIN_DRIVER);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_PROMISE);
  auto* promise = result->u.prom;
  promise->ref++;
  vm_apply_return_clear();
  ASSERT_EQ(promise->state, PROMISE_PENDING);
  EXPECT_EQ(owner_program->ref, owner_program_ref_before + 2u)
      << "destructed suspended frames must hold both program pins";

  destruct_object_for_test(object);
  ASSERT_EQ(promise->state, PROMISE_REJECTED);
  ASSERT_EQ(promise->result.type, T_STRING);
  ASSERT_STREQ(promise->result.u.string,
               "*async function owner was destructed while suspended");
  promise->handled = true;

  for (int pass = 0; pass < 16; pass++) {
    if (tick_event_queue_size_for_test() != 0) {
      ASSERT_GT(run_tick_events_for_test(), 0u);
    }
    if (walltime_event_queue_size_for_test() != 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      ASSERT_EQ(event_base_loop(g_event_base, EVLOOP_NONBLOCK), 0);
    }
    if (tick_event_queue_size_for_test() == 0 &&
        walltime_event_queue_size_for_test() == 0) {
      break;
    }
  }
  ASSERT_EQ(tick_event_queue_size_for_test(), 0u);
  ASSERT_EQ(walltime_event_queue_size_for_test(), 0u);
  free_promise(promise);
}

TEST_F(DriverTest, TestFutureFrozenMappingKeyAndValueBytesAreCounted) {
  OwnerFutureStore store;
  store.admit_pending(owner_future_store_test_record(1, 101));

  mapping_t *m = allocate_mapping(4);
  const std::string key = std::string(64, 'k');
  const std::string value = std::string(128, 'v');
  add_mapping_string(m, key.c_str(), value.c_str());
  svalue_t sv;
  sv.type = T_MAPPING;
  sv.subtype = 0;
  sv.u.map = m;
  auto frozen = vm_clone_frozen_value(&sv);
  free_svalue(&sv, "frozen mapping test");
  ASSERT_NE(frozen, nullptr);

  auto completion = store.complete(1, "completed", "result", "", frozen);
  ASSERT_TRUE(completion.has_value());
  ASSERT_FALSE(completion->quota_rejected);
  ASSERT_GE(store.terminal_payload_bytes(),
            static_cast<int64_t>(key.size() + value.size()));
  ASSERT_TRUE(store.take(1).consumed);
  ASSERT_EQ(store.terminal_payload_bytes(), 0);
}

TEST_F(DriverTest, TestFutureFrozenMappingCountsEveryNodeAgainstSinglePayloadCap) {
  OwnerFutureStore store;
  ASSERT_TRUE(store.admit_pending(owner_future_store_test_record(1, 101)));

  svalue_t sv;
  sv.type = T_MAPPING;
  sv.subtype = 0;
  sv.u.map = allocate_mapping(96);
  const std::string value(100000, 'v');
  for (int i = 0; i < 90; i++) {
    const auto key = "key-" + std::to_string(i);
    add_mapping_string(sv.u.map, key.c_str(), value.c_str());
  }
  auto frozen = vm_clone_frozen_value(&sv);
  free_svalue(&sv, "multi-node frozen mapping quota test");
  ASSERT_NE(frozen, nullptr);

  auto completion = store.complete(1, "completed", "result", "", frozen);
  ASSERT_TRUE(completion.has_value());
  ASSERT_TRUE(completion->quota_rejected);
  ASSERT_EQ(completion->record.state, "failed");
  ASSERT_EQ(completion->record.error, "future_payload_single_byte_cap");
  ASSERT_EQ(store.terminal_payload_bytes(), 0);
  ASSERT_TRUE(store.take(1).consumed);
}

TEST_F(DriverTest, TestFutureFrozenWideArrayDoesNotConsumeDepthAcrossSiblings) {
  OwnerFutureStore store;
  ASSERT_TRUE(store.admit_pending(owner_future_store_test_record(1, 101)));

  svalue_t sv;
  sv.type = T_ARRAY;
  sv.subtype = 0;
  sv.u.arr = allocate_array(100);
  const std::string value(100000, 'a');
  for (int i = 0; i < sv.u.arr->size; i++) {
    sv.u.arr->item[i].type = T_STRING;
    sv.u.arr->item[i].subtype = STRING_SHARED;
    sv.u.arr->item[i].u.string = make_shared_string(value.c_str());
  }
  auto frozen = vm_clone_frozen_value(&sv);
  free_svalue(&sv, "wide frozen array quota test");
  ASSERT_NE(frozen, nullptr);

  auto completion = store.complete(1, "completed", "result", "", frozen);
  ASSERT_TRUE(completion.has_value());
  ASSERT_TRUE(completion->quota_rejected);
  ASSERT_EQ(completion->record.state, "failed");
  ASSERT_EQ(completion->record.error, "future_payload_single_byte_cap");
  ASSERT_EQ(store.terminal_payload_bytes(), 0);
  ASSERT_TRUE(store.take(1).consumed);
}

TEST_F(DriverTest, TestFrozenValueCopyEnforcesDepthCyclesAndFailureCleanup) {
  auto build_nested = [](int depth) {
    svalue_t root;
    root = const0u;
    root.type = T_ARRAY;
    root.subtype = 0;
    array_t *current = allocate_array(1);
    for (int i = 1; i < depth; i++) {
      array_t *next = allocate_array(1);
      next->item[0].type = T_ARRAY;
      next->item[0].subtype = 0;
      next->item[0].u.arr = current;
      current = next;
    }
    root.u.arr = current;
    return root;
  };

  auto depth_eight = build_nested(8);
  svalue_t copied = const0u;
  ASSERT_TRUE(vm_copy_frozen_svalue(&copied, &depth_eight));
  free_svalue(&copied, "frozen depth eight copy");
  free_svalue(&depth_eight, "frozen depth eight source");

  auto depth_nine = build_nested(9);
  copied = const0u;
  ASSERT_FALSE(vm_copy_frozen_svalue(&copied, &depth_nine));
  ASSERT_EQ(copied.type, T_NUMBER);
  std::string error;
  ASSERT_FALSE(vm_frozen_value_safe(&depth_nine, 0, "frozen value", &error));
  ASSERT_NE(error.find("nesting is too deep"), std::string::npos);
  error.clear();
  ASSERT_TRUE(vm_frozen_value_safe_with_max_depth(&depth_nine, 0, 9, "frozen value", &error));
  free_svalue(&depth_nine, "frozen depth nine source");

  svalue_t null_string = const0u;
  null_string.type = T_STRING;
  null_string.subtype = STRING_SHARED;
  null_string.u.string = nullptr;
  copied = const0u;
  ASSERT_TRUE(vm_copy_frozen_svalue(&copied, &null_string));
  free_svalue(&copied, "frozen null string copy");
  ASSERT_TRUE(vm_frozen_value_safe(&null_string, 0, "frozen value", &error));

  svalue_t null_array = const0u;
  null_array.type = T_ARRAY;
  null_array.u.arr = nullptr;
  copied = const0u;
  ASSERT_FALSE(vm_copy_frozen_svalue(&copied, &null_array));
  ASSERT_FALSE(vm_frozen_value_safe(&null_array, 0, "frozen value", &error));

  svalue_t null_mapping = const0u;
  null_mapping.type = T_MAPPING;
  null_mapping.u.map = nullptr;
  copied = const0u;
  ASSERT_FALSE(vm_copy_frozen_svalue(&copied, &null_mapping));
  ASSERT_FALSE(vm_frozen_value_safe(&null_mapping, 0, "frozen value", &error));

  auto *cycle = allocate_array(1);
  cycle->item[0].type = T_ARRAY;
  cycle->item[0].subtype = 0;
  cycle->item[0].u.arr = cycle;
  cycle->ref++;
  svalue_t cycle_source = const0u;
  cycle_source.type = T_ARRAY;
  cycle_source.u.arr = cycle;
  copied = const0u;
  ASSERT_FALSE(vm_copy_frozen_svalue(&copied, &cycle_source));
  error.clear();
  ASSERT_FALSE(vm_frozen_value_safe(&cycle_source, 0, "frozen value", &error));
  ASSERT_NE(error.find("cycle"), std::string::npos);
  ASSERT_EQ(cycle->ref, 2u);
  // Detach the invalid self-reference before releasing the owner reference;
  // the ordinary recursive free path is not a cycle collector.
  cycle->item[0] = const0u;
  cycle->ref--;
  free_svalue(&cycle_source, "frozen cycle source");

  auto *shared_child = allocate_array(1);
  shared_child->item[0].type = T_STRING;
  shared_child->item[0].subtype = STRING_SHARED;
  shared_child->item[0].u.string = make_shared_string("dag");
  auto *dag = allocate_array(2);
  dag->item[0].type = T_ARRAY;
  dag->item[0].subtype = 0;
  dag->item[0].u.arr = shared_child;
  shared_child->ref++;
  dag->item[1].type = T_ARRAY;
  dag->item[1].subtype = 0;
  dag->item[1].u.arr = shared_child;
  svalue_t dag_source = const0u;
  dag_source.type = T_ARRAY;
  dag_source.u.arr = dag;
  copied = const0u;
  ASSERT_TRUE(vm_copy_frozen_svalue(&copied, &dag_source));
  ASSERT_EQ(copied.u.arr->size, 2);
  free_svalue(&copied, "frozen dag copy");
  free_svalue(&dag_source, "frozen dag source");

  auto *partial = allocate_array(2);
  partial->item[0].type = T_STRING;
  partial->item[0].subtype = STRING_SHARED;
  partial->item[0].u.string = make_shared_string("partial");
  partial->item[1].type = T_BUFFER;
  partial->item[1].subtype = 0;
  partial->item[1].u.buf = allocate_buffer(1);
  partial->item[1].u.buf->item[0] = 7;
  svalue_t partial_source = const0u;
  partial_source.type = T_ARRAY;
  partial_source.u.arr = partial;
  copied = const0u;
  ASSERT_FALSE(vm_copy_frozen_svalue(&copied, &partial_source));
  ASSERT_EQ(copied.type, T_NUMBER);
  free_svalue(&partial_source, "frozen partial failure source");

  auto *partial_map = allocate_mapping(1);
  svalue_t string_key = const0u;
  string_key.type = T_STRING;
  string_key.subtype = STRING_SHARED;
  string_key.u.string = make_shared_string("partial-map");
  auto *partial_map_slot = find_for_insert(partial_map, &string_key, 1);
  free_svalue(&string_key, "frozen partial mapping key");
  partial_map_slot->type = T_BUFFER;
  partial_map_slot->subtype = 0;
  partial_map_slot->u.buf = allocate_buffer(1);
  partial_map_slot->u.buf->item[0] = 9;
  svalue_t partial_map_source = const0u;
  partial_map_source.type = T_MAPPING;
  partial_map_source.u.map = partial_map;
  copied = const0u;
  ASSERT_FALSE(vm_copy_frozen_svalue(&copied, &partial_map_source));
  ASSERT_EQ(copied.type, T_NUMBER);
  free_svalue(&partial_map_source, "frozen partial mapping failure source");

  auto *bad_map = allocate_mapping(1);
  svalue_t numeric_key = const0u;
  numeric_key.type = T_NUMBER;
  numeric_key.u.number = 42;
  auto *bad_slot = find_for_insert(bad_map, &numeric_key, 1);
  bad_slot->type = T_STRING;
  bad_slot->subtype = STRING_SHARED;
  bad_slot->u.string = make_shared_string("bad-key-value");
  svalue_t bad_map_source = const0u;
  bad_map_source.type = T_MAPPING;
  bad_map_source.u.map = bad_map;
  copied = const0u;
  ASSERT_FALSE(vm_copy_frozen_svalue(&copied, &bad_map_source));
  error.clear();
  ASSERT_FALSE(vm_frozen_value_safe(&bad_map_source, 0, "frozen value", &error));
  ASSERT_NE(error.find("mapping keys must be strings"), std::string::npos);
  free_svalue(&bad_map_source, "frozen invalid key source");
}

// R2-F05: the frozen weight visitor is depth- and node-bounded and uses
// saturating arithmetic; exhausting either traversal budget is a conservative
// quota rejection rather than a potentially under-counted success.
TEST_F(DriverTest, TestFutureFrozenWeightVisitorSaturatesOnDepthAndNodes) {
  OwnerFutureStore store;

  // 8-deep nested array (max allowed by the frozen-value cloner): the
  // completion must succeed and the weight must stay bounded.
  auto build_nested = [](int depth) {
    svalue_t root;
    root.type = T_ARRAY;
    root.subtype = 0;
    array_t *prev = allocate_array(1);
    for (int d = 1; d < depth; d++) {
      array_t *cur = allocate_array(1);
      cur->item[0].type = T_ARRAY;
      cur->item[0].subtype = 0;
      cur->item[0].u.arr = prev;  // transfers the reference
      prev = cur;
    }
    root.u.arr = prev;
    return root;
  };
  store.admit_pending(owner_future_store_test_record(1, 101));
  auto deep = build_nested(8);
  auto frozen = vm_clone_frozen_value(&deep);
  free_svalue(&deep, "deep nested test");
  ASSERT_NE(frozen, nullptr);
  auto completion = store.complete(1, "completed", "result", "", frozen);
  ASSERT_TRUE(completion.has_value());
  ASSERT_FALSE(completion->quota_rejected);
  ASSERT_GT(store.terminal_payload_bytes(), 0);
  ASSERT_TRUE(store.take(1).consumed);

  // Node-cap saturation: an array of 15000 mappings x 4 string pairs is
  // ~150k metered nodes while the driver's max array size is 15000, so the
  // visitor's 65536-node cap must stop the bounded walk and reject the
  // payload conservatively instead of accepting an incomplete byte count.
  store.admit_pending(owner_future_store_test_record(2, 202));
  svalue_t wide_sv;
  wide_sv.type = T_ARRAY;
  wide_sv.subtype = 0;
  constexpr int kMaps = 15000;
  array_t *wide = allocate_array(kMaps);
  for (int i = 0; i < kMaps; i++) {
    mapping_t *sub = allocate_mapping(4);
    add_mapping_string(sub, "ka", "va");
    add_mapping_string(sub, "kb", "vb");
    add_mapping_string(sub, "kc", "vc");
    add_mapping_string(sub, "kd", "vd");
    wide->item[i].type = T_MAPPING;
    wide->item[i].subtype = 0;
    wide->item[i].u.map = sub;  // transfers the reference
  }
  wide_sv.u.arr = wide;
  auto frozen_wide = vm_clone_frozen_value(&wide_sv);
  free_svalue(&wide_sv, "wide array test");
  ASSERT_NE(frozen_wide, nullptr);
  auto wide_completion = store.complete(2, "completed", "result", "", frozen_wide);
  ASSERT_TRUE(wide_completion.has_value());
  ASSERT_TRUE(wide_completion->quota_rejected);
  ASSERT_EQ(wide_completion->record.state, "failed");
  ASSERT_EQ(wide_completion->record.error, "future_payload_single_byte_cap");
  ASSERT_EQ(store.terminal_payload_bytes(), 0);
  ASSERT_TRUE(store.take(2).consumed);

  // Frozen string accounting: the allocator block size (MSTR_SIZE) is the
  // exact byte count for counted strings (strlen would undercount embedded
  // NULs). A 100k string (below the testsuite max of 200000) must be metered
  // fully and stays under the payload cap.
  store.admit_pending(owner_future_store_test_record(3, 303));
  svalue_t str_sv;
  str_sv.type = T_STRING;
  str_sv.subtype = STRING_SHARED;
  const std::string mid(100000, 'x');
  str_sv.u.string = make_shared_string(mid.c_str());
  auto frozen_str = vm_clone_frozen_value(&str_sv);
  free_svalue(&str_sv, "frozen string test");
  ASSERT_NE(frozen_str, nullptr);
  auto str_ok = store.complete(3, "completed", "result", "", frozen_str);
  ASSERT_TRUE(str_ok.has_value());
  ASSERT_FALSE(str_ok->quota_rejected);
  ASSERT_GE(store.terminal_payload_bytes(), static_cast<int64_t>(mid.size()));
  ASSERT_TRUE(store.take(3).consumed);
}

// R2-F05: payload-bearing terminal records participate in the all-terminal
// oldest-age metric but are never TTL-reaped (no silent payload drop).
TEST(OwnerFutureStoreTest, PayloadBearingTerminalInOldestAgeButNotReapable) {
  OwnerFutureStore store;
  uint64_t fake_now_ns = 1'000'000'000ULL;
  store.set_clock_for_test([&fake_now_ns]() { return fake_now_ns; });
  store.admit_pending(owner_future_store_test_record(1, 101));
  auto completion =
      store.complete(1, "completed", "result", "", std::make_shared<VMFrozenValue>());
  ASSERT_TRUE(completion.has_value());
  fake_now_ns += OwnerFutureStore::kTerminalTtlNs + 1;
  ASSERT_GE(store.oldest_terminal_age_ns(), OwnerFutureStore::kTerminalTtlNs);
  store.reap_expired_terminal();
  ASSERT_NE(store.poll(1), std::nullopt);
  ASSERT_EQ(store.reaped_terminal_count(), 0u);
  ASSERT_TRUE(store.take(1).consumed);
}

TEST_F(DriverTest, TestArraySetOperationsDistinguishExtremeIntegers) {
  const auto make_number_array = [](std::initializer_list<LPC_INT> values) {
    auto *array = allocate_empty_array(static_cast<int>(values.size()));
    int index = 0;
    for (const LPC_INT value : values) {
      array->item[index] = const0u;
      array->item[index++].u.number = value;
    }
    return array;
  };

  auto *difference = subtract_array(
      make_number_array({std::numeric_limits<LPC_INT>::min()}), make_number_array({0}));
  ASSERT_EQ(difference->size, 1);
  ASSERT_EQ(difference->item[0].type, T_NUMBER);
  ASSERT_EQ(difference->item[0].u.number, std::numeric_limits<LPC_INT>::min());
  free_array(difference);

  auto *intersection = intersect_array(
      make_number_array({0}), make_number_array({std::numeric_limits<LPC_INT>::min()}));
  ASSERT_EQ(intersection, &the_null_array);

  auto *array_union = union_array(
      make_number_array({0}), make_number_array({std::numeric_limits<LPC_INT>::min()}));
  ASSERT_EQ(array_union->size, 2);
  bool found_zero = false;
  bool found_min = false;
  for (int index = 0; index < array_union->size; ++index) {
    found_zero = found_zero || array_union->item[index].u.number == 0;
    found_min = found_min ||
                array_union->item[index].u.number == std::numeric_limits<LPC_INT>::min();
  }
  ASSERT_TRUE(found_zero);
  ASSERT_TRUE(found_min);
  free_array(array_union);
}

TEST_F(DriverTest, TestLpcModernProfilePragmasAndAuditRules) {
  int flag = 0;
  ASSERT_TRUE(lpc_modern_pragma_name("modern_lpc", &flag));
  ASSERT_EQ(flag, LPC_MODERN_PRAGMA_MODERN_LPC);
  ASSERT_STREQ(lpc_modern_pragma_name_for_flag(flag), "modern_lpc");

  ASSERT_TRUE(lpc_modern_pragma_name("strict_owner", &flag));
  ASSERT_EQ(flag, LPC_MODERN_PRAGMA_STRICT_OWNER);
  ASSERT_STREQ(lpc_modern_pragma_name_for_flag(flag), "strict_owner");

  ASSERT_FALSE(lpc_modern_pragma_name("strict_types", &flag));
  ASSERT_STREQ(kLpcModernProfileSchemaV1, "lpc_modern_profile_v1");
  ASSERT_STREQ(kLpcOwnerAuditSchemaV1, "lpcc_owner_audit_v1");

  const auto &rules = lpc_owner_audit_rules();
  ASSERT_EQ(rules.size(), 4);
  ASSERT_STREQ(rules[0].code, "cross_owner_mutable_write");
  ASSERT_STREQ(rules[1].code, "bare_object_payload");
  ASSERT_STREQ(rules[2].code, "unfrozen_callback_payload");
  ASSERT_STREQ(rules[3].code, "direct_save_object_hot_path");

  auto report = lpc_owner_audit_source(
      "#pragma modern_lpc\n"
      "#pragma strict_owner\n"
      "void f(object ob) {\n"
      "  call_other(ob, \"mutate\");\n"
      "  owner_async(\"owner/x\", ([ \"object\": this_object() ]));\n"
      "  call_out(\"tick\", 1, this_object());\n"
      "  save_object(\"/tmp/x\");\n"
      "}\n");
  ASSERT_TRUE(report.modern_lpc);
  ASSERT_TRUE(report.strict_owner);
  ASSERT_STREQ(report.source_encoding.c_str(), "utf-8");
  ASSERT_FALSE(report.transcoded);
  ASSERT_EQ(report.invalid_sequence_count, 0);
  ASSERT_EQ(report.findings.size(), 4);
  ASSERT_EQ(report.findings[0].code, "cross_owner_mutable_write");
  ASSERT_FALSE(report.findings[0].suggestion.empty());
  ASSERT_EQ(report.findings[1].code, "bare_object_payload");
  ASSERT_EQ(report.findings[2].code, "unfrozen_callback_payload");
  ASSERT_EQ(report.findings[3].code, "direct_save_object_hot_path");
}

TEST_F(DriverTest, TestLpcModernProfileDetectsSourceEncodingPragma) {
  auto report = lpc_owner_audit_source(
      "#pragma source_encoding(\"GBK\")\n"
      "#pragma modern_lpc\n"
      "void f() {}\n");
  ASSERT_TRUE(report.modern_lpc);
  ASSERT_STREQ(report.source_encoding.c_str(), "GBK");
  ASSERT_TRUE(report.transcoded);
  ASSERT_EQ(report.invalid_sequence_count, 0);
}

TEST_F(DriverTest, TestCompileFileAcceptsGbkSourceEncodingPragma) {
  std::string source = "#pragma source_encoding(\"GBK\")\nstring value() { return \"";
  source.append("\xD6\xD0\xCE\xC4", 4);
  source += "\"; }\n";

  std::istringstream stream_source(source);
  auto stream = std::make_unique<IStreamLexStream>(stream_source);
  auto *compiled = compile_file(std::move(stream), "gbk_source_encoding_test");

  ASSERT_NE(compiled, nullptr);
  bool found_utf8_literal = false;
  for (int i = 0; i < compiled->num_strings; i++) {
    if (compiled->strings[i] && std::string(compiled->strings[i]) == u8"中文") {
      found_utf8_literal = true;
      break;
    }
  }
  ASSERT_TRUE(found_utf8_literal);
  deallocate_program(compiled);
}

TEST_F(DriverTest, TestLpcVmProfileRecordsApplyCacheLookups) {
  ASSERT_STREQ(kLpcVmProfileSchemaV1, "lpc_vm_profile_v1");
  ASSERT_STREQ(kLpcVmBenchSchemaV1, "lpc_vm_bench_v1");

  lpc_vm_profile_set_recording(true);
  lpc_vm_profile_reset();
  std::istringstream source("void dummy() {}\n");
  auto stream = std::make_unique<IStreamLexStream>(source);
  program_t *prog = compile_file(std::move(stream), "lpc_vm_profile_apply_cache_test");
  ASSERT_NE(prog, nullptr);

  auto hit = apply_cache_lookup("dummy", prog);
  ASSERT_NE(hit.funp, nullptr);
  auto direct_hit = apply_cache_lookup("dummy", prog);
  ASSERT_EQ(direct_hit.funp, hit.funp);
  auto miss = apply_cache_lookup("__missing_lpc_vm_profile_probe__", prog);
  ASSERT_EQ(miss.funp, nullptr);

  auto snapshot = lpc_vm_profile_snapshot();
  ASSERT_GE(snapshot.apply_cache_lookup_count, 3);
  ASSERT_GE(snapshot.apply_cache_hit_count, 2);
  ASSERT_GE(snapshot.apply_cache_miss_count, 1);
  ASSERT_GE(snapshot.apply_cache_table_build_count, 1);
  ASSERT_GT(snapshot.apply_cache_table_item_count, 0);
  ASSERT_GE(snapshot.apply_dispatch_cache_lookup_count, 3);
  ASSERT_GE(snapshot.apply_dispatch_cache_hit_count, 1);

  deallocate_program(prog);
  lpc_vm_profile_set_recording(false);
}

TEST_F(DriverTest, TestLpcVmProfileRecordsHotPathCounters) {
  lpc_vm_profile_set_recording(true);
  lpc_vm_profile_reset();

  lpc_vm_profile_record_opcode_dispatch();
  lpc_vm_profile_record_opcode_dispatch();
  lpc_vm_profile_record_efun_dispatch(17);
  lpc_vm_profile_record_call_other_dispatch();
  lpc_vm_profile_record_function_pointer_dispatch(true);
  lpc_vm_profile_record_parser_action_lookup(true);
  lpc_vm_profile_record_parser_action_lookup(false);
  lpc_vm_profile_record_string_push();

  mapping_t* map = allocate_mapping(3);
  add_mapping_pair(map, "alpha", 42);
  constexpr LPC_INT wide_value = (static_cast<LPC_INT>(1) << 32) + 7;
  add_mapping_pair(map, "wide_value", wide_value);
  ASSERT_EQ(find_string_in_mapping(map, "alpha")->type, T_NUMBER);
  ASSERT_EQ(find_string_in_mapping(map, "alpha")->u.number, 42);
  ASSERT_EQ(find_string_in_mapping(map, "wide_value")->u.number, wide_value);
  ASSERT_EQ(find_string_in_mapping(map, "missing")->type, T_NUMBER);
  free_mapping(map);

  auto snapshot = lpc_vm_profile_snapshot();
  ASSERT_GE(snapshot.opcode_dispatch_count, 2);
  ASSERT_EQ(snapshot.efun_dispatch_count, 1);
  ASSERT_EQ(snapshot.efun_dispatch_ns, 17);
  ASSERT_EQ(snapshot.call_other_dispatch_count, 1);
  ASSERT_EQ(snapshot.function_pointer_dispatch_count, 1);
  ASSERT_EQ(snapshot.function_pointer_efun_dispatch_count, 1);
  ASSERT_EQ(snapshot.parser_action_lookup_count, 2);
  ASSERT_EQ(snapshot.parser_action_match_count, 1);
  ASSERT_GE(snapshot.mapping_lookup_count, 3);
  ASSERT_GE(snapshot.mapping_insert_lookup_count, 1);
  ASSERT_EQ(snapshot.string_push_count, 1);
  lpc_vm_profile_set_recording(false);
}

TEST_F(DriverTest, TestLpcVmProfileRequiresExplicitThreadRecording) {
  lpc_vm_profile_set_recording(false);
  lpc_vm_profile_reset();
  lpc_vm_profile_record_opcode_dispatch();
  lpc_vm_profile_record_call_other_dispatch();
  auto disabled = lpc_vm_profile_snapshot();
  ASSERT_EQ(disabled.opcode_dispatch_count, 0);
  ASSERT_EQ(disabled.call_other_dispatch_count, 0);

  lpc_vm_profile_set_recording(true);
  lpc_vm_profile_record_opcode_dispatch();
  lpc_vm_profile_record_call_other_dispatch();
  auto enabled = lpc_vm_profile_snapshot();
  ASSERT_EQ(enabled.opcode_dispatch_count, 1);
  ASSERT_EQ(enabled.call_other_dispatch_count, 1);

  lpc_vm_profile_set_recording(false);
  lpc_vm_profile_record_opcode_dispatch();
  lpc_vm_profile_record_call_other_dispatch();
  auto stopped = lpc_vm_profile_snapshot();
  ASSERT_EQ(stopped.opcode_dispatch_count, enabled.opcode_dispatch_count);
  ASSERT_EQ(stopped.call_other_dispatch_count, enabled.call_other_dispatch_count);
}

TEST_F(DriverTest, TestLpcVmProfileRecordingIsThreadLocal) {
  lpc_vm_profile_set_recording(true);
  ASSERT_TRUE(lpc_vm_profile_recording_enabled());

  bool worker_default = true;
  bool worker_enabled = false;
  std::thread worker([&] {
    worker_default = lpc_vm_profile_recording_enabled();
    lpc_vm_profile_set_recording(true);
    worker_enabled = lpc_vm_profile_recording_enabled();
    lpc_vm_profile_set_recording(false);
  });
  worker.join();

  ASSERT_FALSE(worker_default);
  ASSERT_TRUE(worker_enabled);
  ASSERT_TRUE(lpc_vm_profile_recording_enabled());
  lpc_vm_profile_set_recording(false);
}

namespace {
std::string read_source_file_for_test(const char* path);
}

TEST_F(DriverTest, TestOwnerFuturePendingCountUsesTransitionCounter) {
  const auto header =
      read_source_file_for_test("../src/vm/internal/owner_future_store.h");
  const auto source =
      read_source_file_for_test("../src/vm/internal/owner_future_store.cc");
  const auto count_start =
      source.find("int64_t OwnerFutureStore::pending_count()");
  const auto count_end =
      source.find("int64_t OwnerFutureStore::size()", count_start);

  ASSERT_NE(count_start, std::string::npos);
  ASSERT_NE(count_end, std::string::npos);
  const auto count_body = source.substr(count_start, count_end - count_start);
  EXPECT_NE(header.find("std::atomic<int64_t> pending_"), std::string::npos);
  EXPECT_NE(count_body.find("pending_.load(std::memory_order_relaxed)"),
            std::string::npos);
  EXPECT_EQ(count_body.find("for ("), std::string::npos);
}

TEST_F(DriverTest, TestOwnerFutureCountChainUsesFixedWidthTypes) {
  const auto header =
      read_source_file_for_test("../src/vm/internal/owner_future_store.h");
  const auto source =
      read_source_file_for_test("../src/vm/internal/owner_future_store.cc");
  const auto owner = read_source_file_for_test("../src/vm/internal/owner.cc");

  EXPECT_NE(header.find("int64_t pending_count() const;"), std::string::npos);
  EXPECT_EQ(header.find("long pending_count() const;"), std::string::npos);
  EXPECT_NE(header.find("int64_t size() const;"), std::string::npos);
  EXPECT_EQ(header.find("long size() const;"), std::string::npos);
  EXPECT_NE(header.find("std::atomic<int64_t> pending_{0};"),
            std::string::npos);
  EXPECT_EQ(header.find("std::atomic<long> pending_{0};"), std::string::npos);

  EXPECT_NE(source.find("int64_t OwnerFutureStore::pending_count() const"),
            std::string::npos);
  EXPECT_NE(source.find("int64_t OwnerFutureStore::size() const"),
            std::string::npos);
  EXPECT_NE(source.find("static_cast<int64_t>(futures_.size())"),
            std::string::npos);
  EXPECT_EQ(source.find("static_cast<long>(futures_.size())"),
            std::string::npos);

  EXPECT_NE(owner.find("int64_t owner_pending_future_count()"),
            std::string::npos);
  EXPECT_EQ(owner.find("long owner_pending_future_count()"),
            std::string::npos);
  EXPECT_NE(owner.find("int64_t pending_futures{0};"), std::string::npos);
  EXPECT_EQ(owner.find("long pending_futures{0};"), std::string::npos);
}

TEST_F(DriverTest, TestOwnerFutureStoreResolvesTargetsAfterTerminalLockScope) {
  const auto source =
      read_source_file_for_test("../src/vm/internal/owner_future_store.cc");
  auto assert_resolved_after_unlock = [&](const char* start_marker,
                                          const char* end_marker,
                                          const char* resolution) {
    const auto start = source.find(start_marker);
    const auto end = source.find(end_marker, start);
    ASSERT_NE(start, std::string::npos) << start_marker;
    ASSERT_NE(end, std::string::npos) << end_marker;
    const auto body = source.substr(start, end - start);
    const auto lock_pos =
        body.find("std::unique_lock<std::mutex> lock(mutex_);");
    const auto unlock_pos = body.find("lock.unlock();", lock_pos);
    const auto resolution_pos = body.find(resolution);
    ASSERT_NE(lock_pos, std::string::npos) << start_marker;
    ASSERT_NE(unlock_pos, std::string::npos) << start_marker;
    ASSERT_NE(resolution_pos, std::string::npos) << start_marker;
    EXPECT_LT(lock_pos, unlock_pos) << start_marker;
    EXPECT_LT(unlock_pos, resolution_pos) << start_marker;
  };

  assert_resolved_after_unlock(
      "std::optional<OwnerFutureCompletion> OwnerFutureStore::complete(",
      "std::optional<OwnerFutureCompletion> OwnerFutureStore::complete_for_task(",
      "completion.target_status = target_status(completion.record);");
  assert_resolved_after_unlock(
      "std::optional<OwnerFutureCompletion> OwnerFutureStore::complete_for_task(",
      "std::optional<OwnerFutureCompletion> OwnerFutureStore::complete_string_for_task(",
      "completion.target_status = target_status(completion.record);");
  assert_resolved_after_unlock(
      "std::optional<OwnerFutureCompletion> OwnerFutureStore::complete_string_for_task(",
      "OwnerFutureTerminalResult OwnerFutureStore::fail_terminal(",
      "completion.target_status = target_status(completion.record);");
  assert_resolved_after_unlock(
      "OwnerFutureTerminalResult OwnerFutureStore::fail_terminal(",
      "int64_t OwnerFutureStore::pending_count()",
      "result.target_status = target_status(result.record);");

  const auto helper_start = source.find(
      "OwnerFutureCompletion OwnerFutureStore::complete_record(");
  ASSERT_NE(helper_start, std::string::npos);
  const auto helper_body = source.substr(helper_start);
  EXPECT_EQ(helper_body.find("target_status(record)"), std::string::npos);
}

TEST_F(DriverTest, TestGatewayStatusReportsSessionFifoContract) {
  const auto gateway_status_source =
      read_source_file_for_test("../src/packages/gateway/gateway.cc");
  ASSERT_NE(gateway_status_source.find(
                "gateway_receive_enqueue_to_dispatch_total_us"),
            std::string::npos);
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    svalue_t* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, &const0u) << key;
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER) << key;
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    svalue_t* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr) << key;
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING) << key;
    return (value && value->type == T_STRING) ? value->u.string : "";
  };

  mapping_t* status = gateway_status_internal();
  ASSERT_NE(status, nullptr);
  // Missing-field probe without gtest failure-capture macros: the lookup
  // itself must return the not-found sentinel, independent of
  // platform/debug runtime differences in EXPECT_NONFATAL_FAILURE.
  ASSERT_EQ(find_string_in_mapping(status, "__missing_gateway_status_field_probe__"), &const0u);
  ASSERT_EQ(mapping_number(status, "gateway_external_bind_allowed"), 0);
  ASSERT_GE(mapping_number(status, "gateway_external_bind_rejected"), 0);
  ASSERT_EQ(mapping_number(status, "gateway_event_priority_levels"), 2);
  ASSERT_EQ(mapping_number(status, "gateway_normal_event_priority"), 0);
  ASSERT_EQ(mapping_number(status, "gateway_io_event_priority"), 0);
  ASSERT_EQ(mapping_number(status,
                           "gateway_background_dispatch_max_interval_us"),
            2000);
  ASSERT_EQ(mapping_number(status,
                           "gateway_background_dispatch_max_callbacks"),
            8);
  ASSERT_EQ(mapping_number(status,
                           "gateway_background_dispatch_min_priority"),
            1);
  ASSERT_EQ(mapping_number(status, "gateway_backend_tick_slice_budget"), 64);
  ASSERT_EQ(mapping_number(status, "gateway_backend_tick_slice_wall_budget_us"),
            4000);
  ASSERT_EQ(mapping_number(status,
                           "gateway_backend_tick_continuation_delay_us"),
            1000);
  ASSERT_EQ(mapping_number(status, "gateway_backend_owner_main_slice_budget"),
            64);
  ASSERT_EQ(mapping_number(status,
                           "gateway_backend_owner_main_slice_wall_budget_us"),
            8000);
  ASSERT_EQ(mapping_number(status,
                           "gateway_backend_owner_main_continuation_delay_us"),
            1000);
  ASSERT_GE(mapping_number(status, "gateway_backend_tick_slice_runs"), 0);
  ASSERT_GE(mapping_number(status, "gateway_backend_tick_slice_callbacks_total"),
            0);
  ASSERT_GE(mapping_number(status, "gateway_backend_tick_slice_callbacks_max"),
            0);
  ASSERT_GE(mapping_number(status, "gateway_backend_tick_slice_budget_yields"),
            0);
  ASSERT_GE(mapping_number(status, "gateway_backend_tick_slice_wall_yields"),
            0);
  ASSERT_GE(mapping_number(
                status, "gateway_backend_tick_continuations_scheduled"),
            0);
  ASSERT_GE(mapping_number(
                status, "gateway_backend_tick_continuations_executed"),
            0);
  ASSERT_GE(mapping_number(status, "gateway_backend_owner_main_slice_runs"), 0);
  ASSERT_GE(mapping_number(
                status, "gateway_backend_owner_main_slice_tasks_total"),
            0);
  ASSERT_GE(mapping_number(
                status, "gateway_backend_owner_main_slice_tasks_max"),
            0);
  ASSERT_GE(mapping_number(
                status,
                "gateway_backend_owner_main_slice_task_budget_yields"),
            0);
  ASSERT_GE(mapping_number(
                status, "gateway_backend_owner_main_slice_wall_yields"),
            0);
  ASSERT_GE(mapping_number(
                status,
                "gateway_backend_owner_main_tasks_exceeding_wall_budget"),
            0);
  ASSERT_GE(mapping_number(
                status,
                "gateway_backend_owner_main_continuations_scheduled"),
            0);
  ASSERT_GE(mapping_number(
                status,
                "gateway_backend_owner_main_continuations_executed"),
            0);
  ASSERT_GE(mapping_number(
                status, "gateway_backend_owner_main_continuation_pending"),
            0);
  ASSERT_EQ(mapping_number(status, "session_fifo_contract_ready"), 1);
  ASSERT_GE(mapping_number(status, "session_fifo_depth"), 0);
  ASSERT_GE(mapping_number(status, "session_fifo_pending_reservations"), 0);
  ASSERT_GE(mapping_number(status, "session_fifo_wire_bytes"), 0);
  ASSERT_GE(mapping_number(status, "session_fifo_wire_limit_bytes"),
            64 * 1024);
  ASSERT_GE(mapping_number(status,
                           "session_fifo_wire_aggregate_limit_bytes"),
            64 * 1024);
  ASSERT_GE(mapping_number(status, "session_fifo_wire_detached_bytes"), 0);
  ASSERT_GE(mapping_number(status, "session_fifo_wire_bytes_rejected"), 0);
  ASSERT_GE(mapping_number(status, "session_fifo_enqueued"), 0);
  ASSERT_GE(mapping_number(status, "session_fifo_flushed"), 0);
  ASSERT_GE(mapping_number(status, "session_fifo_rejected"), 0);
  ASSERT_GE(mapping_number(status, "gateway_data_frames_received"), 0);
  ASSERT_GE(mapping_number(status, "gateway_data_frames_applied"), 0);
  ASSERT_GE(mapping_number(status, "gateway_data_frames_rejected"), 0);
  ASSERT_GE(mapping_number(status, "gateway_json_frames_rejected"), 0);
  ASSERT_GE(mapping_number(status,
                           "gateway_read_dispatch_frame_length_rejected"),
            0);
  ASSERT_GE(mapping_number(status,
                           "gateway_read_dispatch_buffer_limit_rejected"),
            0);
  ASSERT_GE(mapping_number(status,
                           "gateway_read_dispatch_native_bytes_deferred"),
            0);
  ASSERT_GE(mapping_number(status, "gateway_read_buffer_limit_bytes"), 1028);
  ASSERT_GE(mapping_number(status,
                           "gateway_raw_write_backpressure_rejected"),
            0);
  ASSERT_GE(mapping_number(status, "gateway_write_buffer_limit_bytes"),
            64 * 1024);
  ASSERT_EQ(mapping_number(status, "gateway_packet_size_hard_limit_bytes"),
            16 * 1024 * 1024);
  ASSERT_GE(mapping_number(status, "gateway_stale_master_frames_rejected"), 0);
  ASSERT_GE(mapping_number(status, "gateway_sessions_detached_total"), 0);
  ASSERT_GE(mapping_number(status, "gateway_session_rebind_attempts"), 0);
  ASSERT_GE(mapping_number(status, "gateway_session_rebind_completed"), 0);
  ASSERT_GE(mapping_number(status, "gateway_session_rebind_rejected"), 0);
  ASSERT_GE(mapping_number(status, "gateway_session_reconnect_expired"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_tasks_enqueued"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_tasks_dispatched"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_tasks_rejected"), 0);
  ASSERT_GE(mapping_number(status, "gateway_command_callbacks"), 0);
  ASSERT_GE(mapping_number(status, "gateway_command_tasks_enqueued"), 0);
  ASSERT_GE(mapping_number(status, "gateway_command_tasks_rejected"), 0);
  ASSERT_GE(mapping_number(status, "gateway_command_tasks_rejected_pending"), 0);
  ASSERT_GE(mapping_number(status, "gateway_command_tasks_finished"), 0);
  ASSERT_GE(mapping_number(status, "gateway_command_tasks_stale"), 0);
  ASSERT_GE(mapping_number(status, "gateway_command_tasks_cleared"), 0);
  ASSERT_GE(mapping_number(status, "gateway_command_input_pending_sessions"), 0);
  ASSERT_GE(mapping_number(status, "gateway_command_task_pending_sessions"), 0);
  ASSERT_GE(mapping_number(status, "gateway_command_pending_sessions"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_dispatch_pending_masters"), 0);
  ASSERT_GE(mapping_number(status, "gateway_buffered_input_pending_masters"), 0);
  ASSERT_GE(mapping_number(status, "gateway_command_pressure"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_queue_pending"), 0);
  ASSERT_EQ(mapping_number(status, "gateway_main_queue_read_high_watermark"), 32);
  ASSERT_EQ(mapping_number(status, "gateway_main_queue_read_low_watermark"), 16);
  ASSERT_GE(mapping_number(status, "gateway_main_queue_read_admission_limited"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_queue_read_pressure_events"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_queue_read_paused"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_queue_read_resumed"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_queue_read_paused_masters"), 0);
  ASSERT_GE(mapping_number(status, "gateway_reply_tasks_enqueued"), 0);
  ASSERT_GE(mapping_number(status, "gateway_reply_tasks_inline_fallbacks"), 0);
  ASSERT_GE(mapping_number(status, "gateway_reply_reschedule_cmd_in_buf"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_fifo_enqueued"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_fifo_flushed"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_fifo_rejected"), 0);
  ASSERT_GE(mapping_number(status,
                           "gateway_output_fifo_wire_bytes_rejected"),
            0);
  ASSERT_GE(mapping_number(
                status,
                "gateway_output_fifo_aggregate_wire_bytes_rejected"),
            0);
  ASSERT_GE(mapping_number(
                status,
                "gateway_output_fifo_rebucket_wire_bytes_dropped"),
            0);
  ASSERT_GE(mapping_number(status, "gateway_output_fifo_reserved"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_fifo_filled"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_fifo_released"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_fifo_reservation_misses"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_fifo_writer_failures"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_fifo_oversize_dropped"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_fifo_destroyed_ready"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_fifo_destroyed_pending"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_fifo_head_blocked_fills"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_fifo_head_blocked_predecessors_total"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_fifo_head_blocked_predecessors_max"), 0);
  ASSERT_GE(mapping_number(status, "gateway_future_watch_pending"), 0);
  ASSERT_GE(mapping_number(status, "gateway_generic_future_watch_pending"), 0);
  ASSERT_GE(mapping_number(status, "gateway_generic_future_watches_registered"), 0);
  ASSERT_GE(mapping_number(status, "gateway_generic_future_watches_rejected"), 0);
  ASSERT_GE(mapping_number(status, "gateway_generic_future_watches_completed"), 0);
  ASSERT_GE(mapping_number(status, "gateway_generic_future_watches_failed"), 0);
  ASSERT_GE(mapping_number(status, "gateway_generic_future_watches_timed_out"), 0);
  ASSERT_GE(mapping_number(status, "gateway_generic_future_watches_cancelled"), 0);
  ASSERT_GE(mapping_number(status, "gateway_generic_future_watch_callbacks"), 0);
  ASSERT_GE(mapping_number(status, "gateway_generic_future_watch_callback_failures"), 0);
  ASSERT_GE(mapping_number(status, "gateway_generic_future_watch_poll_runs"), 0);
  ASSERT_GE(mapping_number(status, "gateway_generic_future_watch_poll_items"), 0);
  ASSERT_GE(mapping_number(status, "gateway_generic_future_watch_poll_budget_hits"), 0);
  ASSERT_GE(mapping_number(status, "gateway_future_watches_registered"), 0);
  ASSERT_GE(mapping_number(status, "gateway_future_watches_rejected"), 0);
  ASSERT_GE(mapping_number(status, "gateway_future_watches_completed"), 0);
  ASSERT_GE(mapping_number(status, "gateway_future_watches_failed"), 0);
  ASSERT_GE(mapping_number(status, "gateway_future_watches_timed_out"), 0);
  ASSERT_GE(mapping_number(status, "gateway_future_watches_cancelled"), 0);
  ASSERT_GE(mapping_number(status, "gateway_future_watch_callbacks"), 0);
  ASSERT_GE(mapping_number(status, "gateway_future_watch_callback_failures"), 0);
  ASSERT_GE(mapping_number(status, "gateway_future_watch_poll_runs"), 0);
  ASSERT_GE(mapping_number(status, "gateway_future_watch_poll_items"), 0);
  ASSERT_GE(mapping_number(status, "gateway_future_watch_poll_budget_hits"), 0);
  ASSERT_EQ(mapping_number(status, "gateway_future_watch_completion_event_ready"), 1);
  ASSERT_GE(mapping_number(status, "gateway_future_watch_completion_notifications"), 0);
  ASSERT_GE(mapping_number(status, "gateway_future_watch_completion_wakeups"), 0);
  ASSERT_GE(mapping_number(status, "gateway_future_watch_timer_wakeups"), 0);
  for (const auto* prefix : {
           "gateway_output_reserve",
           "gateway_future_watch_register",
           "gateway_future_watch_terminal_lag",
           "gateway_future_watch_take",
           "gateway_future_watch_callback",
           "gateway_future_watch_end_to_end",
       }) {
    for (const auto* suffix : {"_samples", "_total_us", "_max_us"}) {
      auto field = std::string(prefix) + suffix;
      ASSERT_NE(find_string_in_mapping(status, field.c_str()), nullptr) << field;
      ASSERT_GE(mapping_number(status, field.c_str()), 0);
    }
  }
  for (const auto *prefix : {
           "gateway_room_output_projection_worker_thread_cpu",
           "gateway_room_output_projection_inline_thread_cpu",
       }) {
    for (const auto *suffix : {"_samples", "_total_us", "_avg_us", "_max_us"}) {
      const auto field = std::string(prefix) + suffix;
      ASSERT_NE(find_string_in_mapping(status, field.c_str()), nullptr) << field;
      ASSERT_GE(mapping_number(status, field.c_str()), 0);
    }
  }
  ASSERT_GE(mapping_number(
                status,
                "gateway_room_output_projection_worker_thread_cpu_unavailable"),
            0);
  ASSERT_GE(mapping_number(
                status,
                "gateway_room_output_projection_inline_thread_cpu_unavailable"),
            0);
  ASSERT_GE(mapping_number(status, "gateway_room_output_projection_submitted"), 0);
  ASSERT_GE(mapping_number(status, "gateway_room_output_projection_completed"), 0);
  ASSERT_GE(mapping_number(status, "gateway_room_output_projection_released"), 0);
  ASSERT_GE(mapping_number(status, "gateway_room_output_projection_pending"), 0);
  ASSERT_GE(mapping_number(status, "gateway_room_output_projection_forced_cleanup"), 0);
  ASSERT_GE(mapping_number(status, "gateway_raw_writes_sent"), 0);
  ASSERT_GE(mapping_number(status, "gateway_raw_writes_failed"), 0);
  ASSERT_GE(mapping_number(status, "gateway_master_tcp_nodelay_enabled"), 0);
  ASSERT_GE(mapping_number(status, "gateway_master_tcp_nodelay_failed"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_runs"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_tasks_total"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_tasks_max"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_budget_hits"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_scheduled"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_coalesced"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_executed"), 0);
  ASSERT_EQ(mapping_number(status, "gateway_main_drain_deferred_task_budget"), 64);
  ASSERT_EQ(mapping_number(status, "gateway_main_drain_deferred_base_wall_budget_us"), 8000);
  ASSERT_EQ(mapping_number(status, "gateway_main_drain_deferred_backlog_wall_budget_us"), 8000);
  ASSERT_EQ(mapping_number(status, "gateway_main_drain_deferred_backlog_threshold"), 64);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_backlog_boosted"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_tasks_total"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_tasks_max"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_wall_samples"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_wall_total_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_wall_avg_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_wall_max_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_wall_budget_yields"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_task_budget_yields"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_remaining_samples"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_remaining_total"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_remaining_avg"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_remaining_max"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_main_task_wall_max_us"), 0);
  ASSERT_GE(mapping_number(status,
                           "gateway_main_drain_deferred_main_tasks_exceeding_wall_budget"),
            0);
  ASSERT_EQ(mapping_number(status, "gateway_read_batch_drain_task_budget"), 32);
  ASSERT_EQ(mapping_number(status, "gateway_read_batch_drain_wall_budget_us"), 12000);
  ASSERT_GE(mapping_number(status, "gateway_read_batch_drain_runs"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_batch_drain_tasks_total"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_batch_drain_tasks_max"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_batch_drain_backlog_rescheduled"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_batch_drain_main_task_wall_max_us"), 0);
  ASSERT_GE(mapping_number(status,
                           "gateway_read_batch_drain_main_tasks_exceeding_wall_budget"),
            0);
  ASSERT_EQ(mapping_number(status, "gateway_read_dispatch_budget"), 1);
  ASSERT_GE(mapping_number(status, "gateway_read_dispatch_runs"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_dispatch_frames_total"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_dispatch_frames_max"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_dispatch_budget_hits"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_dispatch_deferred_scheduled"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_dispatch_deferred_coalesced"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_dispatch_deferred_executed"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_dispatch_input_paused"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_dispatch_input_resumed"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_inline_drain_calls"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_deferred_drain_requests"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_main_queue_depth_samples"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_main_queue_depth_total"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_main_queue_depth_avg"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_main_queue_depth_max"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_wait_samples"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_wait_total_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_wait_avg_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_wait_max_us"), 0);
  ASSERT_EQ(mapping_number(status,
                           "gateway_main_drain_deferred_wait_timer_queue_only"),
            1);
  ASSERT_GE(mapping_number(status, "gateway_receive_decode_samples"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_decode_avg_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_decode_max_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_payload_copy_samples"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_payload_copy_avg_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_payload_copy_max_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_enqueue_to_dispatch_samples"), 0);
  ASSERT_NE(find_string_in_mapping(status,
                                   "gateway_receive_enqueue_to_dispatch_total_us"),
            nullptr);
  ASSERT_GE(mapping_number(status, "gateway_receive_enqueue_to_dispatch_total_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_enqueue_to_dispatch_avg_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_enqueue_to_dispatch_max_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_apply_samples"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_apply_total_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_apply_avg_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_apply_max_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_apply_thread_cpu_samples"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_apply_thread_cpu_total_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_apply_thread_cpu_avg_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_apply_thread_cpu_max_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_apply_thread_cpu_unavailable"), 0);
  ASSERT_GE(mapping_number(status, "gateway_command_enqueue_to_dispatch_samples"), 0);
  ASSERT_GE(mapping_number(status, "gateway_command_enqueue_to_dispatch_avg_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_command_enqueue_to_dispatch_max_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_command_execute_samples"), 0);
  ASSERT_GE(mapping_number(status, "gateway_command_execute_avg_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_command_execute_max_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_reply_enqueue_to_dispatch_samples"), 0);
  ASSERT_GE(mapping_number(status, "gateway_reply_enqueue_to_dispatch_avg_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_reply_enqueue_to_dispatch_max_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_reply_execute_samples"), 0);
  ASSERT_GE(mapping_number(status, "gateway_reply_execute_avg_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_reply_execute_max_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_enqueue_to_dispatch_samples"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_enqueue_to_dispatch_avg_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_enqueue_to_dispatch_max_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_execute_samples"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_execute_avg_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_execute_max_us"), 0);
  ASSERT_STREQ(mapping_string(status, "gateway_io_boundary"), "main_thread_io_adapter");
  free_mapping(status);
}

TEST_F(DriverTest, TestGatewayStatusDerivesRoomProjectionThreadCpuAverages) {
  struct AtomicCounterRestore {
    std::atomic<uint64_t> &counter;
    uint64_t original;
    ~AtomicCounterRestore() {
      counter.store(original, std::memory_order_relaxed);
    }
  };

  auto &worker_total =
      g_gateway_runtime_counters.room_output_projection_worker_thread_cpu_ns_total;
  auto &worker_samples =
      g_gateway_runtime_counters.room_output_projection_worker_thread_cpu_samples;
  auto &inline_total =
      g_gateway_runtime_counters.room_output_projection_inline_thread_cpu_ns_total;
  auto &inline_samples =
      g_gateway_runtime_counters.room_output_projection_inline_thread_cpu_samples;
  AtomicCounterRestore worker_total_restore{
      worker_total, worker_total.load(std::memory_order_relaxed)};
  AtomicCounterRestore worker_samples_restore{
      worker_samples, worker_samples.load(std::memory_order_relaxed)};
  AtomicCounterRestore inline_total_restore{
      inline_total, inline_total.load(std::memory_order_relaxed)};
  AtomicCounterRestore inline_samples_restore{
      inline_samples, inline_samples.load(std::memory_order_relaxed)};

  worker_total.store(12000, std::memory_order_relaxed);
  worker_samples.store(3, std::memory_order_relaxed);
  inline_total.store(25000, std::memory_order_relaxed);
  inline_samples.store(5, std::memory_order_relaxed);

  auto *status = gateway_status_internal();
  ASSERT_NE(status, nullptr);
  auto *worker_avg = find_string_in_mapping(
      status, "gateway_room_output_projection_worker_thread_cpu_avg_us");
  auto *inline_avg = find_string_in_mapping(
      status, "gateway_room_output_projection_inline_thread_cpu_avg_us");
  ASSERT_NE(worker_avg, &const0u);
  ASSERT_NE(inline_avg, &const0u);
  ASSERT_EQ(worker_avg->type, T_NUMBER);
  ASSERT_EQ(inline_avg->type, T_NUMBER);
  EXPECT_EQ(worker_avg->u.number, 4);
  EXPECT_EQ(inline_avg->u.number, 5);
  free_mapping(status);
}

TEST_F(DriverTest, TestGatewayOutputReservationBlocksAndPreservesFifo) {
  static std::vector<std::string> writes;
  writes.clear();
  auto writer = [](int fd, const char* data, size_t len) -> int {
    EXPECT_EQ(fd, 77);
    writes.emplace_back(data, len);
    return 1;
  };

  GatewaySession session;
  session.session_id = "fifo-session";
  session.master_fd = 77;
  auto first_reservation_id = gateway_reserve_session_output(&session);
  auto second_reservation_id = gateway_reserve_session_output(&session);
  ASSERT_GT(first_reservation_id, 0u);
  ASSERT_GT(second_reservation_id, 0u);
  auto blocked_fills_before =
      g_gateway_runtime_counters.output_fifo_head_blocked_fills.load(std::memory_order_relaxed);
  auto blocked_predecessors_before =
      g_gateway_runtime_counters.output_fifo_head_blocked_predecessors_total.load(std::memory_order_relaxed);
  ASSERT_EQ(gateway_fill_session_protocol_output_with_writer(
                &session, second_reservation_id, "second", 6, writer),
            1);
  ASSERT_EQ(session.output_fifo.size(), 2u);
  ASSERT_FALSE(session.output_fifo.front().ready);
  ASSERT_TRUE(session.output_fifo.back().ready);
  ASSERT_TRUE(writes.empty());
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_head_blocked_fills.load(std::memory_order_relaxed),
            blocked_fills_before + 1);
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_head_blocked_predecessors_total.load(
                std::memory_order_relaxed),
            blocked_predecessors_before + 1);
  ASSERT_GE(g_gateway_runtime_counters.output_fifo_head_blocked_predecessors_max.load(
                std::memory_order_relaxed),
            1u);

  ASSERT_EQ(gateway_fill_session_protocol_output_with_writer(
                &session, first_reservation_id, "first", 5, writer),
            1);
  ASSERT_EQ(writes.size(), 2u);
  const auto first_wire = nlohmann::json::parse(writes[0]);
  const auto second_wire = nlohmann::json::parse(writes[1]);
  ASSERT_EQ(first_wire["type"], "output");
  ASSERT_EQ(first_wire["cid"], "fifo-session");
  ASSERT_EQ(first_wire["data"], "first");
  ASSERT_EQ(second_wire["type"], "output");
  ASSERT_EQ(second_wire["cid"], "fifo-session");
  ASSERT_EQ(second_wire["data"], "second");
  ASSERT_TRUE(session.output_fifo.empty());
}

TEST_F(DriverTest, TestGatewayPendingReservationDetectsReadySuccessor) {
  auto writer = [](int, const char*, size_t) -> int { return 1; };

  GatewaySession session;
  session.session_id = "ready-successor-session";
  session.master_fd = 78;
  auto first_reservation_id = gateway_reserve_session_output(&session);
  auto second_reservation_id = gateway_reserve_session_output(&session);
  ASSERT_GT(first_reservation_id, 0u);
  ASSERT_GT(second_reservation_id, 0u);

  ASSERT_FALSE(gateway_session_pending_reservation_has_ready_successor(
      nullptr, first_reservation_id));
  ASSERT_FALSE(gateway_session_pending_reservation_has_ready_successor(
      &session, 0));
  ASSERT_FALSE(gateway_session_pending_reservation_has_ready_successor(
      &session, second_reservation_id + 1));
  ASSERT_FALSE(gateway_session_pending_reservation_has_ready_successor(
      &session, first_reservation_id));
  ASSERT_FALSE(gateway_session_pending_reservation_has_ready_successor(
      &session, second_reservation_id));

  ASSERT_EQ(gateway_fill_session_protocol_output_with_writer(
                &session, second_reservation_id, "second", 6, writer),
            1);
  ASSERT_TRUE(gateway_session_pending_reservation_has_ready_successor(
      &session, first_reservation_id));
  ASSERT_FALSE(gateway_session_pending_reservation_has_ready_successor(
      &session, second_reservation_id));
}

TEST_F(DriverTest, TestGatewayOutputReservationTimeoutReleaseUnblocksFollowingOutput) {
  static std::vector<std::string> writes;
  writes.clear();
  auto writer = [](int, const char* data, size_t len) -> int {
    writes.emplace_back(data, len);
    return 1;
  };

  GatewaySession session;
  session.session_id = "release-session";
  session.master_fd = 88;
  auto reservation_id = gateway_reserve_session_output(&session);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_EQ(gateway_enqueue_session_protocol_output(
                &session, "after-release", sizeof("after-release") - 1),
            1);
  ASSERT_EQ(gateway_release_session_output_with_writer(&session, reservation_id, writer), 1);
  ASSERT_EQ(gateway_release_session_output(&session, reservation_id), 0);
  ASSERT_EQ(writes.size(), 1u);
  const auto wire = nlohmann::json::parse(writes[0]);
  ASSERT_EQ(wire["type"], "output");
  ASSERT_EQ(wire["cid"], "release-session");
  ASSERT_EQ(wire["data"], "after-release");
  ASSERT_TRUE(session.output_fifo.empty());
  ASSERT_EQ(session.output_fifo_wire_bytes, 0u);
}

TEST_F(DriverTest, TestGatewayOrdinaryOutputQueuesExactWireEnvelope) {
  GatewaySession session;
  session.session_id = "ordinary-session";
  session.master_fd = -1;
  const std::string frame{"ordinary\0frame", 14};

  ASSERT_EQ(gateway_enqueue_session_protocol_output(
                &session, frame.data(), frame.size()),
            1);
  ASSERT_EQ(session.output_fifo.size(), 1u);
  ASSERT_TRUE(session.output_fifo.front().ready);
  const auto wire = nlohmann::json::parse(
      session.output_fifo.front().wire_bytes);
  ASSERT_EQ(session.output_fifo_wire_bytes,
            session.output_fifo.front().wire_bytes.size());
  ASSERT_EQ(wire["type"], "output");
  ASSERT_EQ(wire["cid"], "ordinary-session");
  ASSERT_EQ(wire["data"].get<std::string>(), frame);
}

TEST_F(DriverTest, TestGatewayMasterOutputBufferRejectsSlowPeerBeforeUnboundedGrowth) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() { g_gateway_max_packet_size = original; }
  } packet_size_guard;

  g_gateway_max_packet_size = 1024;
  constexpr int master_fd = 1708;
  bufferevent *pair[2] = {nullptr, nullptr};
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  ASSERT_NE(gateway_register_master_for_test(master_fd, pair[0]), nullptr);

  const auto output_limit = gateway_write_buffer_limit_for_test();
  const auto rejected_before =
      g_gateway_runtime_counters.raw_write_backpressure_rejected.load(
          std::memory_order_relaxed);
  const std::string payload(128, 'x');
  size_t accepted = 0;
  while (gateway_send_raw_to_fd(master_fd, payload.data(), payload.size())) {
    accepted++;
    ASSERT_LE(evbuffer_get_length(bufferevent_get_output(pair[0])),
              output_limit);
  }

  ASSERT_GT(accepted, 0u);
  ASSERT_LT(evbuffer_get_length(bufferevent_get_output(pair[0])),
            output_limit + payload.size() + sizeof(uint32_t));
  ASSERT_EQ(
      g_gateway_runtime_counters.raw_write_backpressure_rejected.load(
          std::memory_order_relaxed),
      rejected_before + 1);
  ASSERT_EQ(gateway_send_raw_to_fd(master_fd, nullptr, payload.size()), 0);
  ASSERT_EQ(gateway_send_raw_to_fd(master_fd, payload.data(),
                                   g_gateway_max_packet_size + 1),
            0);
  ASSERT_EQ(
      g_gateway_runtime_counters.raw_write_backpressure_rejected.load(
          std::memory_order_relaxed),
      rejected_before + 1);

  gateway_remove_master_for_test(master_fd);
  pair[0] = nullptr;
  bufferevent_free(pair[1]);
}

TEST_F(DriverTest, TestGatewayWireFactoryRejectsOversizeBeforeFifoMutation) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() { g_gateway_max_packet_size = original; }
  } guard;
  static std::vector<std::string> writes;
  writes.clear();
  auto writer = [](int, const char *data, size_t len) -> int {
    writes.emplace_back(data, len);
    return 1;
  };
  g_gateway_max_packet_size = 96;
  const std::string oversized_payload(128, 'x');

  GatewaySession ordinary;
  ordinary.session_id = "oversize-ordinary";
  ordinary.master_fd = -1;
  ASSERT_EQ(gateway_enqueue_session_protocol_output(
                &ordinary, oversized_payload.data(), oversized_payload.size()),
            0);
  ASSERT_TRUE(ordinary.output_fifo.empty());

  GatewaySession reserved;
  reserved.session_id = "oversize-reserved";
  reserved.master_fd = 77;
  const auto reservation_id = gateway_reserve_session_output(&reserved);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_EQ(gateway_fill_session_protocol_output_with_writer(
                &reserved, reservation_id, oversized_payload.data(),
                oversized_payload.size(), writer),
            0);
  ASSERT_TRUE(writes.empty());
  ASSERT_EQ(reserved.output_fifo.size(), 1u);
  ASSERT_EQ(reserved.output_fifo.front().reservation_id, reservation_id);
  ASSERT_FALSE(reserved.output_fifo.front().ready);
  ASSERT_TRUE(reserved.output_fifo.front().wire_bytes.empty());
}

TEST_F(DriverTest,
       TestGatewaySessionFifoWireByteLimitRejectsWithoutQueueMutation) {
  const auto rejected_before =
      g_gateway_runtime_counters.output_fifo_wire_bytes_rejected.load(
          std::memory_order_relaxed);

  GatewaySession ordinary;
  ordinary.session_id = "wire-byte-limit-ordinary";
  ordinary.master_fd = -1;
  const auto ordinary_wire = gateway_encode_output_envelope_for_test(
      ordinary.session_id, "first", 5);
  ordinary.output_fifo_max_wire_bytes = ordinary_wire.size();
  ASSERT_EQ(gateway_enqueue_session_protocol_output(&ordinary, "first", 5), 1);
  ASSERT_EQ(ordinary.output_fifo_wire_bytes, ordinary_wire.size());
  ASSERT_EQ(gateway_enqueue_session_protocol_output(&ordinary, "second", 6),
            0);
  ASSERT_EQ(ordinary.output_fifo.size(), 1u);
  ASSERT_EQ(ordinary.output_fifo_wire_bytes, ordinary_wire.size());
  ASSERT_EQ(ordinary.output_fifo_wire_bytes_rejected, 1u);

  GatewaySession reserved;
  reserved.session_id = "wire-byte-limit-reserved";
  reserved.master_fd = -1;
  const auto retained_wire = gateway_encode_output_envelope_for_test(
      reserved.session_id, "retained", 8);
  reserved.output_fifo_max_wire_bytes = retained_wire.size();
  ASSERT_EQ(gateway_enqueue_session_protocol_output(
                &reserved, "retained", 8),
            1);
  const auto reservation_id = gateway_reserve_session_output(&reserved);
  ASSERT_GT(reservation_id, 0u);
  reserved.master_fd = 77;
  ASSERT_EQ(gateway_fill_session_protocol_output_with_writer(
                &reserved, reservation_id, "blocked", 7,
                [](int, const char *, size_t) -> int { return 1; }),
            0);
  ASSERT_EQ(reserved.output_fifo.size(), 2u);
  ASSERT_TRUE(reserved.output_fifo.front().ready);
  ASSERT_FALSE(reserved.output_fifo.back().ready);
  ASSERT_TRUE(reserved.output_fifo.back().wire_bytes.empty());
  ASSERT_EQ(reserved.output_fifo_wire_bytes, retained_wire.size());
  ASSERT_EQ(reserved.output_fifo_wire_bytes_rejected, 1u);
  ASSERT_EQ(
      g_gateway_runtime_counters.output_fifo_wire_bytes_rejected.load(
          std::memory_order_relaxed),
      rejected_before + 2);
}

TEST_F(DriverTest,
       TestGatewayFlushDropsReadyWireMadeOversizeByLimitReduction) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() { g_gateway_max_packet_size = original; }
  } guard;
  static std::vector<size_t> attempted_lengths;
  static std::vector<std::string> writes;
  attempted_lengths.clear();
  writes.clear();
  auto writer = [](int fd, const char *data, size_t len) -> int {
    EXPECT_EQ(fd, 77);
    attempted_lengths.push_back(len);
    if (len > g_gateway_max_packet_size) {
      return 0;
    }
    writes.emplace_back(data, len);
    return 1;
  };

  GatewaySession session;
  session.session_id = "limit-reduction-ordinary";
  session.master_fd = -1;
  g_gateway_max_packet_size = 4096;
  const std::string oversized_payload(2048, 'x');
  ASSERT_EQ(gateway_enqueue_session_protocol_output(
                &session, oversized_payload.data(), oversized_payload.size()),
            1);
  ASSERT_EQ(gateway_enqueue_session_protocol_output(
                &session, "successor", sizeof("successor") - 1),
            1);
  ASSERT_EQ(session.output_fifo.size(), 2u);
  ASSERT_TRUE(session.output_fifo.front().ready);
  ASSERT_EQ(session.output_fifo.front().reservation_id, 0u);
  ASSERT_GT(session.output_fifo.front().wire_bytes.size(), 1024u);
  ASSERT_LT(session.output_fifo.back().wire_bytes.size(), 1024u);
  ASSERT_EQ(session.output_fifo_wire_bytes,
            session.output_fifo.front().wire_bytes.size() +
                session.output_fifo.back().wire_bytes.size());
  const auto oversize_dropped_before =
      g_gateway_runtime_counters.output_fifo_oversize_dropped.load(
          std::memory_order_relaxed);
  const auto released_before =
      g_gateway_runtime_counters.output_fifo_released.load(
          std::memory_order_relaxed);
  const auto writer_failures_before =
      g_gateway_runtime_counters.output_fifo_writer_failures.load(
          std::memory_order_relaxed);

  g_gateway_max_packet_size = 1024;
  session.master_fd = 77;
  ASSERT_EQ(gateway_flush_session_output_fifo_with_writer(&session, writer), 1);
  ASSERT_TRUE(session.output_fifo.empty());
  ASSERT_EQ(session.output_fifo_wire_bytes, 0u);
  ASSERT_EQ(attempted_lengths.size(), 1u);
  ASSERT_EQ(writes.size(), 1u);
  const auto successor = nlohmann::json::parse(writes.front());
  ASSERT_EQ(successor["type"], "output");
  ASSERT_EQ(successor["cid"], session.session_id);
  ASSERT_EQ(successor["data"], "successor");
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_oversize_dropped.load(
                std::memory_order_relaxed),
            oversize_dropped_before + 1);
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_released.load(
                std::memory_order_relaxed),
            released_before);
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_writer_failures.load(
                std::memory_order_relaxed),
            writer_failures_before);
}

TEST_F(DriverTest,
       TestGatewayFlushClassifiesReadyReservationOversizeExactlyOnce) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() { g_gateway_max_packet_size = original; }
  } guard;
  static std::vector<size_t> attempted_lengths;
  static std::vector<std::string> writes;
  attempted_lengths.clear();
  writes.clear();
  auto writer = [](int fd, const char *data, size_t len) -> int {
    EXPECT_EQ(fd, 78);
    attempted_lengths.push_back(len);
    if (len > g_gateway_max_packet_size) {
      return 0;
    }
    writes.emplace_back(data, len);
    return 1;
  };

  GatewaySession session;
  session.session_id = "limit-reduction-reserved";
  session.master_fd = -1;
  g_gateway_max_packet_size = 4096;
  const auto reservation_id = gateway_reserve_session_output(&session);
  ASSERT_GT(reservation_id, 0u);
  const std::string oversized_payload(2048, 'y');
  ASSERT_EQ(gateway_fill_session_protocol_output_with_writer(
                &session, reservation_id, oversized_payload.data(),
                oversized_payload.size(), writer),
            1);
  ASSERT_EQ(gateway_enqueue_session_protocol_output(
                &session, "successor", sizeof("successor") - 1),
            1);
  ASSERT_EQ(session.output_fifo.size(), 2u);
  ASSERT_TRUE(session.output_fifo.front().ready);
  ASSERT_EQ(session.output_fifo.front().reservation_id, reservation_id);
  ASSERT_GT(session.output_fifo.front().wire_bytes.size(), 1024u);
  ASSERT_LT(session.output_fifo.back().wire_bytes.size(), 1024u);
  ASSERT_EQ(session.output_fifo_wire_bytes,
            session.output_fifo.front().wire_bytes.size() +
                session.output_fifo.back().wire_bytes.size());
  const auto oversize_dropped_before =
      g_gateway_runtime_counters.output_fifo_oversize_dropped.load(
          std::memory_order_relaxed);
  const auto released_before =
      g_gateway_runtime_counters.output_fifo_released.load(
          std::memory_order_relaxed);
  const auto writer_failures_before =
      g_gateway_runtime_counters.output_fifo_writer_failures.load(
          std::memory_order_relaxed);

  g_gateway_max_packet_size = 1024;
  session.master_fd = 78;
  ASSERT_EQ(gateway_flush_session_output_fifo_with_writer(&session, writer), 1);
  ASSERT_TRUE(session.output_fifo.empty());
  ASSERT_EQ(session.output_fifo_wire_bytes, 0u);
  ASSERT_EQ(attempted_lengths.size(), 1u);
  ASSERT_EQ(writes.size(), 1u);
  const auto successor = nlohmann::json::parse(writes.front());
  ASSERT_EQ(successor["type"], "output");
  ASSERT_EQ(successor["cid"], session.session_id);
  ASSERT_EQ(successor["data"], "successor");
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_oversize_dropped.load(
                std::memory_order_relaxed),
            oversize_dropped_before + 1);
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_released.load(
                std::memory_order_relaxed),
            released_before);
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_writer_failures.load(
                std::memory_order_relaxed),
            writer_failures_before);
}

TEST_F(DriverTest, TestGatewayWireReadyQueueRejectsMalformedSouthboundJson) {
  GatewaySession session;
  session.session_id = "wire-ready-session";
  session.master_fd = -1;

  ASSERT_EQ(gateway_enqueue_session_wire_json_for_test(
                &session,
                R"({"type":"output","cid":"wire-ready-session","data":"\u001bXKMSGE{}"})"),
            1);
  ASSERT_EQ(session.output_fifo.size(), 1u);
  const auto valid_wire = session.output_fifo.front().wire_bytes;

  for (const auto &invalid : {
           R"({"cid":"wire-ready-session","data":"frame"})",
           R"({"type":"sys","cid":"wire-ready-session","data":"frame"})",
           R"({"type":"output","cid":"other-session","data":"frame"})",
           R"({"type":"output","cid":"wire-ready-session"})",
           R"({"type":"output","cid":"wire-ready-session","data":{"frame":1}})",
           R"({"type":"output","cid":"wire-ready-session","data":"frame","extra":1})",
           R"(["output","wire-ready-session","frame"])",
       }) {
    EXPECT_EQ(gateway_enqueue_session_wire_json_for_test(&session, invalid), 0)
        << invalid;
    EXPECT_EQ(session.output_fifo.size(), 1u) << invalid;
    EXPECT_EQ(session.output_fifo.front().wire_bytes, valid_wire) << invalid;
  }
}

TEST_F(DriverTest, TestGatewayProjectedWireBatchRejectsUnboundRecipientsAtomically) {
  static std::vector<std::string> writes;
  writes.clear();
  auto writer = [](int, const char *data, size_t len) -> int {
    writes.emplace_back(data, len);
    return 1;
  };

  const auto assert_rejected = [&](const std::vector<std::string> &wire_json) {
    GatewaySession first;
    GatewaySession second;
    first.session_id = "projected-first";
    second.session_id = "projected-second";
    first.master_fd = 71;
    second.master_fd = 72;
    const auto first_id = gateway_reserve_session_output(&first);
    const auto second_id = gateway_reserve_session_output(&second);
    ASSERT_GT(first_id, 0u);
    ASSERT_GT(second_id, 0u);
    writes.clear();

    EXPECT_FALSE(gateway_fill_projected_wires_for_test(
        {&first, &second}, {first_id, second_id}, wire_json, writer));
    EXPECT_TRUE(writes.empty());
    ASSERT_EQ(first.output_fifo.size(), 1u);
    ASSERT_EQ(second.output_fifo.size(), 1u);
    EXPECT_FALSE(first.output_fifo.front().ready);
    EXPECT_FALSE(second.output_fifo.front().ready);
  };

  const auto first_wire = gateway_encode_output_envelope_for_test(
      "projected-first", "first-frame", 11);
  const auto second_wire = gateway_encode_output_envelope_for_test(
      "projected-second", "second-frame", 12);
  assert_rejected({"not-json", second_wire});
  assert_rejected({second_wire, second_wire});
  assert_rejected({second_wire, first_wire});

  GatewaySession first;
  GatewaySession second;
  first.session_id = "projected-first";
  second.session_id = "projected-second";
  first.master_fd = 81;
  second.master_fd = 82;
  const auto first_id = gateway_reserve_session_output(&first);
  const auto second_id = gateway_reserve_session_output(&second);
  writes.clear();
  ASSERT_TRUE(gateway_fill_projected_wires_for_test(
      {&first, &second}, {first_id, second_id}, {first_wire, second_wire}, writer));
  ASSERT_EQ(writes.size(), 2u);
  EXPECT_EQ(nlohmann::json::parse(writes[0])["cid"], "projected-first");
  EXPECT_EQ(nlohmann::json::parse(writes[1])["cid"], "projected-second");
}

TEST_F(DriverTest, TestGatewayProjectedWireBatchRejectsOversizeAtomically) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() { g_gateway_max_packet_size = original; }
  } guard;
  static std::vector<std::string> writes;
  writes.clear();
  auto writer = [](int, const char *data, size_t len) -> int {
    writes.emplace_back(data, len);
    return 1;
  };
  g_gateway_max_packet_size = 128;

  GatewaySession first;
  GatewaySession second;
  first.session_id = "projected-size-first";
  second.session_id = "projected-size-second";
  first.master_fd = 81;
  second.master_fd = 82;
  const auto first_id = gateway_reserve_session_output(&first);
  const auto second_id = gateway_reserve_session_output(&second);
  ASSERT_GT(first_id, 0u);
  ASSERT_GT(second_id, 0u);
  const auto first_wire = gateway_encode_output_envelope_for_test(
      first.session_id, "ok", 2);
  const std::string oversized_payload(256, 'y');
  const auto second_wire = gateway_encode_output_envelope_for_test(
      second.session_id, oversized_payload.data(), oversized_payload.size());
  ASSERT_LT(first_wire.size(), g_gateway_max_packet_size);
  ASSERT_GT(second_wire.size(), g_gateway_max_packet_size);

  ASSERT_FALSE(gateway_fill_projected_wires_for_test(
      {&first, &second}, {first_id, second_id}, {first_wire, second_wire},
      writer));
  ASSERT_TRUE(writes.empty());
  ASSERT_EQ(first.output_fifo.size(), 1u);
  ASSERT_EQ(second.output_fifo.size(), 1u);
  ASSERT_FALSE(first.output_fifo.front().ready);
  ASSERT_FALSE(second.output_fifo.front().ready);
  ASSERT_TRUE(first.output_fifo.front().wire_bytes.empty());
  ASSERT_TRUE(second.output_fifo.front().wire_bytes.empty());
}

TEST_F(DriverTest,
       TestGatewayProjectedWireBatchRejectsWireByteLimitAtomically) {
  static std::vector<std::string> writes;
  writes.clear();
  auto writer = [](int, const char *data, size_t len) -> int {
    writes.emplace_back(data, len);
    return 1;
  };

  GatewaySession first;
  GatewaySession second;
  first.session_id = "projected-byte-first";
  second.session_id = "projected-byte-second";
  first.master_fd = -1;
  second.master_fd = 82;
  const auto retained_wire = gateway_encode_output_envelope_for_test(
      first.session_id, "retained", 8);
  first.output_fifo_max_wire_bytes = retained_wire.size();
  ASSERT_EQ(gateway_enqueue_session_protocol_output(
                &first, "retained", 8),
            1);
  const auto first_id = gateway_reserve_session_output(&first);
  const auto second_id = gateway_reserve_session_output(&second);
  ASSERT_GT(first_id, 0u);
  ASSERT_GT(second_id, 0u);
  first.master_fd = 81;
  const auto first_wire = gateway_encode_output_envelope_for_test(
      first.session_id, "first", 5);
  const auto second_wire = gateway_encode_output_envelope_for_test(
      second.session_id, "second", 6);

  ASSERT_FALSE(gateway_fill_projected_wires_for_test(
      {&first, &second}, {first_id, second_id}, {first_wire, second_wire},
      writer));
  ASSERT_TRUE(writes.empty());
  ASSERT_EQ(first.output_fifo.size(), 2u);
  ASSERT_TRUE(first.output_fifo.front().ready);
  ASSERT_FALSE(first.output_fifo.back().ready);
  ASSERT_EQ(first.output_fifo_wire_bytes, retained_wire.size());
  ASSERT_EQ(first.output_fifo_wire_bytes_rejected, 1u);
  ASSERT_EQ(second.output_fifo.size(), 1u);
  ASSERT_FALSE(second.output_fifo.front().ready);
  ASSERT_EQ(second.output_fifo_wire_bytes, 0u);
  ASSERT_EQ(second.output_fifo_wire_bytes_rejected, 0u);
}

TEST_F(DriverTest, TestGatewaySessionSendKeepsHistoricalMixedPayloadSemantics) {
  const auto assert_payload = [](const std::string &payload_json,
                                 const nlohmann::json &expected) {
    GatewaySession session;
    session.session_id = "mixed-session";
    session.master_fd = -1;
    ASSERT_EQ(gateway_enqueue_session_payload_json_for_test(
                  &session, payload_json),
              1);
    ASSERT_EQ(session.output_fifo.size(), 1u);
    EXPECT_EQ(nlohmann::json::parse(session.output_fifo.front().wire_bytes),
              expected);
  };

  assert_payload("7", {{"type", "output"}, {"cid", "mixed-session"}, {"data", 7}});
  assert_payload(R"([1,"two"])",
                 {{"type", "output"},
                  {"cid", "mixed-session"},
                  {"data", nlohmann::json::array({1, "two"})}});
  assert_payload(R"({"type":"custom","data":{"answer":42}})",
                 {{"type", "custom"},
                  {"cid", "mixed-session"},
                  {"data", {{"answer", 42}}}});
}

TEST_F(DriverTest, TestGatewayWriterFailureRetainsOneReadyFrameForExactRetry) {
  static std::vector<std::string> writes;
  writes.clear();
  auto failing_writer = [](int, const char *, size_t) -> int { return 0; };
  auto succeeding_writer = [](int, const char *data, size_t len) -> int {
    writes.emplace_back(data, len);
    return 1;
  };

  GatewaySession session;
  session.session_id = "writer-retry-session";
  session.master_fd = 71;
  const auto reservation_id = gateway_reserve_session_output(&session);
  ASSERT_GT(reservation_id, 0u);
  const auto failures_before =
      g_gateway_runtime_counters.output_fifo_writer_failures.load(
          std::memory_order_relaxed);

  ASSERT_EQ(gateway_fill_session_protocol_output_with_writer(
                &session, reservation_id, "retry-frame", 11, failing_writer),
            1);
  ASSERT_EQ(session.output_fifo.size(), 1u);
  ASSERT_TRUE(session.output_fifo.front().ready);
  ASSERT_EQ(session.output_fifo.front().reservation_id, reservation_id);
  ASSERT_EQ(session.output_fifo_wire_bytes,
            session.output_fifo.front().wire_bytes.size());
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_writer_failures.load(
                std::memory_order_relaxed),
            failures_before + 1);

  ASSERT_EQ(gateway_flush_session_output_fifo_with_writer(
                &session, succeeding_writer),
            1);
  ASSERT_EQ(gateway_flush_session_output_fifo_with_writer(
                &session, succeeding_writer),
            0);
  ASSERT_TRUE(session.output_fifo.empty());
  ASSERT_EQ(session.output_fifo_wire_bytes, 0u);
  ASSERT_EQ(writes.size(), 1u);
  const auto wire = nlohmann::json::parse(writes.front());
  ASSERT_EQ(wire["type"], "output");
  ASSERT_EQ(wire["cid"], "writer-retry-session");
  ASSERT_EQ(wire["data"], "retry-frame");
}

TEST_F(DriverTest, TestGatewayOutputReservationIdsDoNotRepeatAcrossSessions) {
  GatewaySession first;
  GatewaySession second;
  first.master_fd = 91;
  second.master_fd = 92;

  auto first_id = gateway_reserve_session_output(&first);
  auto second_id = gateway_reserve_session_output(&second);

  ASSERT_GT(first_id, 0u);
  ASSERT_GT(second_id, 0u);
  ASSERT_NE(first_id, second_id);
}

TEST_F(DriverTest, TestGatewayBatchReservationReusesOnlyPendingTailSlots) {
  GatewaySession first;
  GatewaySession second;
  first.session_id = "batch-first";
  second.session_id = "batch-second";
  first.master_fd = 93;
  second.master_fd = 94;

  const auto first_id = gateway_reserve_session_output(&first);
  ASSERT_GT(first_id, 0u);

  GatewaySessionBatchReservationResult result;
  ASSERT_TRUE(gateway_reserve_session_outputs(
      {&first, &second}, {first_id, 0}, &result));
  ASSERT_EQ(result.reservation_ids.size(), 2u);
  ASSERT_EQ(result.reused, (std::vector<bool>{true, false}));
  ASSERT_EQ(result.reservation_ids[0], first_id);
  ASSERT_GT(result.reservation_ids[1], 0u);
  ASSERT_EQ(first.output_fifo.size(), 1u);
  ASSERT_EQ(second.output_fifo.size(), 1u);

  ASSERT_EQ(gateway_enqueue_session_protocol_output(
                &first, "after-pending", sizeof("after-pending") - 1),
            1);
  GatewaySessionBatchReservationResult after_intervening;
  ASSERT_TRUE(gateway_reserve_session_outputs(
      {&first}, {first_id}, &after_intervening));
  ASSERT_FALSE(after_intervening.reused[0]);
  ASSERT_NE(after_intervening.reservation_ids[0], first_id);
  ASSERT_EQ(first.output_fifo.size(), 3u);
}

TEST_F(DriverTest, TestGatewayBatchReservationPrevalidatesAllSessions) {
  GatewaySession first;
  GatewaySession full;
  first.master_fd = 95;
  full.master_fd = 96;
  full.output_fifo_max_depth = 0;
  const auto first_id = gateway_reserve_session_output(&first);
  ASSERT_GT(first_id, 0u);

  GatewaySessionBatchReservationResult result;
  ASSERT_FALSE(gateway_reserve_session_outputs(
      {&first, &full}, {first_id, 0}, &result));
  ASSERT_TRUE(result.reservation_ids.empty());
  ASSERT_TRUE(result.reused.empty());
  ASSERT_EQ(first.output_fifo.size(), 1u);
  ASSERT_EQ(first.output_fifo.front().reservation_id, first_id);
  ASSERT_FALSE(first.output_fifo.front().ready);
  ASSERT_TRUE(full.output_fifo.empty());
}

TEST_F(DriverTest, TestGatewayBatchReservationRejectsDuplicateSessions) {
  GatewaySession session;
  session.master_fd = 97;

  GatewaySessionBatchReservationResult result;
  ASSERT_FALSE(gateway_reserve_session_outputs(
      {&session, &session}, {0, 0}, &result));
  ASSERT_TRUE(result.reservation_ids.empty());
  ASSERT_TRUE(result.reused.empty());
  ASSERT_TRUE(session.output_fifo.empty());
}

TEST_F(DriverTest, TestGatewayPendingMessageEventBatchAppendsMultipleProducerWaves) {
  GatewaySession first;
  GatewaySession second;
  first.session_id = "session-one";
  second.session_id = "session-two";
  first.master_fd = 98;
  second.master_fd = 99;
  const auto first_id = gateway_reserve_session_output(&first);
  const auto second_id = gateway_reserve_session_output(&second);
  ASSERT_GT(first_id, 0u);
  ASSERT_GT(second_id, 0u);

  const std::vector<GatewaySession *> sessions{&first, &second};
  const std::vector<uint64_t> reservation_ids{first_id, second_id};
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      sessions, reservation_ids, {101, 201}, {102, 202}, {3, 4}, {1001, 2001},
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"first\",\"payload\":{}}",
      "player", 123456, 128));
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      sessions, reservation_ids, {103, 203}, {104, 204}, {3, 4}, {1002, 2002},
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"second\",\"payload\":{}}",
      "player", 123457, 128));
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 2u);
  ASSERT_EQ(gateway_pending_message_event_count(&second, second_id), 2u);
  ASSERT_TRUE(first.output_fifo.front().pending_message_batch);
  ASSERT_TRUE(second.output_fifo.front().pending_message_batch);
  ASSERT_EQ(first.output_fifo.front().pending_message_batch->events[0].wave,
            second.output_fifo.front().pending_message_batch->events[0].wave);
  ASSERT_TRUE(first.output_fifo.front()
                  .pending_message_batch->events[0]
                  .wave->message_template.encoded.empty());
  ASSERT_EQ(first.output_fifo.front().pending_message_batch->events[0].message_seq, 1001);
  ASSERT_EQ(second.output_fifo.front().pending_message_batch->events[0].message_seq, 2001);
  const auto cache_hits_after_append =
      g_gateway_runtime_counters.message_event_template_cache_hits.load();
  const auto cache_misses_after_append =
      g_gateway_runtime_counters.message_event_template_cache_misses.load();

  static std::vector<std::string> writes;
  writes.clear();
  const auto writer = [](int, const char *data, size_t len) {
    writes.emplace_back(data, len);
    return 1;
  };
  ASSERT_EQ(gateway_fill_pending_message_event_batch_with_writer(
                &first, first_id, "player-one", 7, writer),
            1);
  ASSERT_EQ(writes.size(), 1u);
  const auto first_wire = nlohmann::json::parse(writes[0]);
  ASSERT_EQ(first_wire["type"], "output");
  ASSERT_EQ(first_wire["cid"], "session-one");
  const auto first_frame = first_wire["data"].get<std::string>();
  ASSERT_EQ(first_frame.substr(0, 7), "\x1bXKBACH");
  ASSERT_NE(first_frame.find("first"), std::string::npos);
  ASSERT_NE(first_frame.find("second"), std::string::npos);
  ASSERT_LT(first_frame.find("first"), first_frame.find("second"));
  ASSERT_TRUE(first.output_fifo.empty());
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 0u);
  ASSERT_EQ(gateway_fill_pending_message_event_batch_with_writer(
                &second, second_id, "player-two", 8, writer),
            1);
  ASSERT_EQ(writes.size(), 2u);
  const auto second_wire = nlohmann::json::parse(writes[1]);
  ASSERT_EQ(second_wire["type"], "output");
  ASSERT_EQ(second_wire["cid"], "session-two");
  ASSERT_EQ(second_wire["data"].get<std::string>().substr(0, 7), "\x1bXKBACH");
  ASSERT_EQ(
      g_gateway_runtime_counters.message_event_template_cache_hits.load(),
      cache_hits_after_append);
  ASSERT_EQ(
      g_gateway_runtime_counters.message_event_template_cache_misses.load(),
      cache_misses_after_append);
}

TEST_F(DriverTest, TestGatewayPendingMessageEventBatchFillsRecipientSliceNatively) {
  GatewaySession first;
  GatewaySession second;
  first.session_id = "session-one";
  second.session_id = "session-two";
  first.master_fd = 120;
  second.master_fd = 121;
  const auto first_id = gateway_reserve_session_output(&first);
  const auto second_id = gateway_reserve_session_output(&second);
  ASSERT_GT(first_id, 0u);
  ASSERT_GT(second_id, 0u);

  const std::string stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"slice\",\"payload\":{}}";
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      {&first, &second}, {first_id, second_id}, {101, 201}, {102, 202},
      {3, 4}, {1001, 2001}, stable, "player", 123456, 128));

  static std::vector<std::string> writes;
  writes.clear();
  const auto writer = [](int, const char *data, size_t len) {
    writes.emplace_back(data, len);
    return 1;
  };
  GatewayPendingMessageEventBatchFillResult result;

  ASSERT_FALSE(gateway_fill_pending_message_event_batches_with_writer(
      {&first, &second}, {first_id, second_id}, {"player-one", ""}, {7, 8},
      writer, &result));
  ASSERT_TRUE(writes.empty());
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 1u);
  ASSERT_EQ(gateway_pending_message_event_count(&second, second_id), 1u);

  const auto inline_cpu_samples_before =
      g_gateway_runtime_counters
          .room_output_projection_inline_thread_cpu_samples.load(
              std::memory_order_relaxed);
  const auto inline_cpu_unavailable_before =
      g_gateway_runtime_counters
          .room_output_projection_inline_thread_cpu_unavailable.load(
              std::memory_order_relaxed);

  ASSERT_TRUE(gateway_fill_pending_message_event_batches_with_writer(
      {&first, &second}, {first_id, second_id}, {"player-one", "player-two"},
      {7, 8}, writer, &result));
  ASSERT_EQ(result.filled, (std::vector<bool>{true, true}));
  ASSERT_EQ(result.event_counts, (std::vector<LPC_INT>{1, 1}));
  ASSERT_EQ(result.text_length_totals, (std::vector<LPC_INT>{0, 0}));
  ASSERT_EQ(result.slot_server_seqs, (std::vector<LPC_INT>{101, 201}));
  ASSERT_EQ(writes.size(), 2u);
  const auto first_wire = nlohmann::json::parse(writes[0]);
  const auto second_wire = nlohmann::json::parse(writes[1]);
  ASSERT_EQ(first_wire["type"], "output");
  ASSERT_EQ(first_wire["cid"], "session-one");
  ASSERT_EQ(second_wire["type"], "output");
  ASSERT_EQ(second_wire["cid"], "session-two");
  ASSERT_NE(first_wire["data"].get<std::string>().find("player-one"),
            std::string::npos);
  ASSERT_NE(second_wire["data"].get<std::string>().find("player-two"),
            std::string::npos);
  const auto inline_cpu_samples_after =
      g_gateway_runtime_counters
          .room_output_projection_inline_thread_cpu_samples.load(
              std::memory_order_relaxed);
  const auto inline_cpu_unavailable_after =
      g_gateway_runtime_counters
          .room_output_projection_inline_thread_cpu_unavailable.load(
              std::memory_order_relaxed);
  ASSERT_EQ((inline_cpu_samples_after - inline_cpu_samples_before) +
                (inline_cpu_unavailable_after -
                 inline_cpu_unavailable_before),
            2u);
  ASSERT_TRUE(first.output_fifo.empty());
  ASSERT_TRUE(second.output_fifo.empty());
}

TEST_F(DriverTest, TestGatewayPendingMessageEventSnapshotProjectsByteEquivalentFrame) {
  GatewaySession session;
  session.session_id = "owner-\"projection\\session";
  session.master_fd = -1;
  const auto reservation_id = gateway_reserve_session_output(&session);
  ASSERT_GT(reservation_id, 0u);

  const std::string first_stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"first-owner-event "
      "\\\"quoted\\\" \\\\ line\\n\",\"payload\":{}}";
  const std::string second_stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"second-owner-event\\tend\","
      "\"payload\":{}}";
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      {&session}, {reservation_id}, {901}, {902}, {11}, {7001},
      first_stable, "player", 123456, 128));
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      {&session}, {reservation_id}, {901}, {904}, {12}, {7002},
      second_stable, "player", 123457, 128));

  GatewayPendingMessageEventProjectionSnapshot snapshot;
  GatewayPendingMessageEventProjectionColumns columns;
  ASSERT_TRUE(gateway_snapshot_pending_message_event_batch(
      &session, reservation_id, "player-\"owner\\scope", 13, &snapshot,
      &columns));
  ASSERT_EQ(snapshot.wave_table.size(), 2u);
  ASSERT_EQ(snapshot.event_wave_indices, (std::vector<size_t>{0, 1}));
  ASSERT_EQ(snapshot.wave_table[0]->message_template.stable_members.find(
                "first-owner-event") != std::string::npos,
            true);
  ASSERT_EQ(snapshot.wave_table[1]->message_template.stable_members.find(
                "second-owner-event") != std::string::npos,
            true);
  ASSERT_EQ(columns.session_id, "owner-\"projection\\session");
  ASSERT_EQ(columns.scope_id, "player-\"owner\\scope");
  ASSERT_EQ(columns.message_seqs, (std::vector<LPC_INT>{7001, 7002}));
  ASSERT_EQ(columns.server_seqs, (std::vector<LPC_INT>{902, 904}));
  ASSERT_EQ(columns.message_epochs, (std::vector<LPC_INT>{11, 12}));
  ASSERT_EQ(columns.slot_server_seq, 901);
  ASSERT_EQ(columns.slot_epoch, 13);
  ASSERT_EQ(columns.slot_sent_at, 123456);

  std::string owner_projected;
  ASSERT_TRUE(gateway_encode_pending_message_event_projection(
      snapshot, columns, &owner_projected));
  const auto inline_inner =
      gateway_encode_preencoded_message_event_batch_for_test(
          {first_stable, second_stable}, {"player", "player"},
          "player-\"owner\\scope", {7001, 7002}, {902, 904}, {11, 12},
          {123456, 123457}, 901, 13, 123456);
  ASSERT_FALSE(inline_inner.empty());
  const auto inline_projected = gateway_encode_output_envelope_for_test(
      session.session_id, inline_inner.data(), inline_inner.size());
  ASSERT_EQ(owner_projected, inline_projected);
  const auto wire = nlohmann::json::parse(owner_projected);
  ASSERT_EQ(wire["type"], "output");
  ASSERT_EQ(wire["cid"], session.session_id);
  ASSERT_EQ(wire["data"], inline_inner);
  ASSERT_EQ(gateway_pending_message_event_count(&session, reservation_id), 2u);
  ASSERT_FALSE(session.output_fifo.front().ready);
}

TEST_F(DriverTest,
       TestGatewayLocalProjectionSkipsFullValidationWhileUntrustedWireKeepsIt) {
  static std::vector<std::string> writes;
  writes.clear();
  const auto writer = [](int, const char *data, size_t len) {
    writes.emplace_back(data, len);
    return 1;
  };

  GatewaySession local;
  local.session_id = "local-projection-session";
  local.master_fd = 120;
  const auto local_id = gateway_reserve_session_output(&local);
  ASSERT_GT(local_id, 0u);
  const std::string stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"local-projection\",\"payload\":{}}";
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      {&local}, {local_id}, {101}, {102}, {3}, {1001}, stable, "player",
      123456, 128));

  gateway_reset_projected_wire_full_validation_count_for_test();
  GatewayPendingMessageEventBatchFillResult result;
  ASSERT_TRUE(gateway_fill_pending_message_event_batches_with_writer(
      {&local}, {local_id}, {"player-one"}, {7}, writer, &result));
  ASSERT_EQ(writes.size(), 1u);
  EXPECT_EQ(gateway_projected_wire_full_validation_count_for_test(), 0u);

  GatewaySession untrusted;
  untrusted.session_id = "untrusted-projection-session";
  untrusted.master_fd = 121;
  const auto valid_id = gateway_reserve_session_output(&untrusted);
  ASSERT_GT(valid_id, 0u);
  const auto valid_wire = gateway_encode_output_envelope_for_test(
      untrusted.session_id, "validated", 9);
  ASSERT_TRUE(gateway_fill_projected_wires_for_test(
      {&untrusted}, {valid_id}, {valid_wire}, writer));
  EXPECT_EQ(gateway_projected_wire_full_validation_count_for_test(), 1u);

  const auto invalid_id = gateway_reserve_session_output(&untrusted);
  ASSERT_GT(invalid_id, 0u);
  const auto wrong_session_wire = gateway_encode_output_envelope_for_test(
      "wrong-session", "rejected", 8);
  ASSERT_FALSE(gateway_fill_projected_wires_for_test(
      {&untrusted}, {invalid_id}, {wrong_session_wire}, writer));
  EXPECT_EQ(gateway_projected_wire_full_validation_count_for_test(), 2u);
  ASSERT_EQ(untrusted.output_fifo.size(), 1u);
  EXPECT_FALSE(untrusted.output_fifo.front().ready);
}

TEST_F(DriverTest, TestGatewayPendingMessageEventBatchPreencodesEveryWireBeforeFirstWrite) {
  GatewaySession first;
  GatewaySession invalid;
  first.session_id = "session-one";
  invalid.session_id = std::string("invalid-\xc3\x28", 10);
  first.master_fd = 124;
  invalid.master_fd = 125;
  const auto first_id = gateway_reserve_session_output(&first);
  const auto invalid_id = gateway_reserve_session_output(&invalid);
  ASSERT_GT(first_id, 0u);
  ASSERT_GT(invalid_id, 0u);

  const std::string stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"atomic-wire\",\"payload\":{}}";
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      {&first, &invalid}, {first_id, invalid_id}, {101, 201}, {102, 202},
      {3, 4}, {1001, 2001}, stable, "player", 123456, 128));

  static std::vector<std::string> writes;
  writes.clear();
  const auto writer = [](int, const char *data, size_t len) {
    writes.emplace_back(data, len);
    return 1;
  };
  GatewayPendingMessageEventBatchFillResult result;

  ASSERT_FALSE(gateway_fill_pending_message_event_batches_with_writer(
      {&first, &invalid}, {first_id, invalid_id}, {"player-one", "player-two"},
      {7, 8}, writer, &result));
  ASSERT_TRUE(writes.empty());
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 1u);
  ASSERT_EQ(gateway_pending_message_event_count(&invalid, invalid_id), 1u);
  ASSERT_FALSE(first.output_fifo.front().ready);
  ASSERT_FALSE(invalid.output_fifo.front().ready);
}

TEST_F(DriverTest, TestGatewayPendingMessageEventBatchFillsTwoSlotsForSameSessionInFifoOrder) {
  GatewaySession session;
  session.session_id = "same-session";
  session.master_fd = 126;
  const auto first_id = gateway_reserve_session_output(&session);
  const auto second_id = gateway_reserve_session_output(&session);
  ASSERT_GT(first_id, 0u);
  ASSERT_GT(second_id, 0u);

  const std::string first_stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"first-slot\",\"payload\":{}}";
  const std::string second_stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"second-slot\",\"payload\":{}}";
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      {&session}, {first_id}, {501}, {502}, {7}, {5001}, first_stable,
      "player", 123456, 128));
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      {&session}, {second_id}, {503}, {504}, {7}, {5002}, second_stable,
      "player", 123457, 128));

  static std::vector<std::string> writes;
  writes.clear();
  const auto writer = [](int, const char *data, size_t len) {
    writes.emplace_back(data, len);
    return 1;
  };
  GatewayPendingMessageEventBatchFillResult result;
  ASSERT_TRUE(gateway_fill_pending_message_event_batches_with_writer(
      {&session, &session}, {second_id, first_id},
      {"player-one", "player-one"}, {9, 9}, writer, &result));
  ASSERT_EQ(result.filled, (std::vector<bool>{true, true}));
  ASSERT_EQ(writes.size(), 2u);
  const auto first_wire = nlohmann::json::parse(writes[0]);
  const auto second_wire = nlohmann::json::parse(writes[1]);
  const auto expected_first_frame =
      gateway_encode_preencoded_message_event_batch_for_test(
          {first_stable}, {"player"}, "player-one", {5001}, {502}, {7},
          {123456}, 501, 9, 123456);
  const auto expected_second_frame =
      gateway_encode_preencoded_message_event_batch_for_test(
          {second_stable}, {"player"}, "player-one", {5002}, {504}, {7},
          {123457}, 503, 9, 123457);
  ASSERT_EQ(first_wire["type"], "output");
  ASSERT_EQ(first_wire["cid"], "same-session");
  ASSERT_EQ(first_wire["data"].get<std::string>(), expected_first_frame);
  ASSERT_EQ(second_wire["type"], "output");
  ASSERT_EQ(second_wire["cid"], "same-session");
  ASSERT_EQ(second_wire["data"].get<std::string>(), expected_second_frame);
  ASSERT_TRUE(session.output_fifo.empty());
}

TEST_F(DriverTest, TestGatewayPendingMessageEventBatchSameSessionFailureWritesNothing) {
  GatewaySession session;
  session.session_id = "same-session-atomic";
  session.master_fd = 127;
  const auto first_id = gateway_reserve_session_output(&session);
  const auto second_id = gateway_reserve_session_output(&session);
  ASSERT_GT(first_id, 0u);
  ASSERT_GT(second_id, 0u);

  const std::string stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"atomic-slot\",\"payload\":{}}";
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      {&session}, {first_id}, {601}, {602}, {8}, {6001}, stable, "player",
      123456, 128));
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      {&session}, {second_id}, {603}, {604}, {8}, {6002}, stable, "player",
      123457, 128));

  static std::vector<std::string> writes;
  writes.clear();
  const auto writer = [](int, const char *data, size_t len) {
    writes.emplace_back(data, len);
    return 1;
  };
  const std::string invalid_scope("invalid-\xc3\x28", 10);
  GatewayPendingMessageEventBatchFillResult result;
  ASSERT_FALSE(gateway_fill_pending_message_event_batches_with_writer(
      {&session, &session}, {first_id, first_id},
      {"player-one", "player-one"}, {10, 10}, writer, &result));
  ASSERT_TRUE(writes.empty());
  ASSERT_EQ(gateway_pending_message_event_count(&session, first_id), 1u);
  ASSERT_EQ(gateway_pending_message_event_count(&session, second_id), 1u);

  ASSERT_FALSE(gateway_fill_pending_message_event_batches_with_writer(
      {&session, &session}, {first_id, second_id},
      {"player-one", invalid_scope}, {10, 10}, writer, &result));
  ASSERT_TRUE(writes.empty());
  ASSERT_EQ(gateway_pending_message_event_count(&session, first_id), 1u);
  ASSERT_EQ(gateway_pending_message_event_count(&session, second_id), 1u);
  ASSERT_FALSE(session.output_fifo[0].ready);
  ASSERT_FALSE(session.output_fifo[1].ready);
}

TEST_F(DriverTest,
       TestGatewayPendingMessageEventBatchStagesWholeWaveBeforeFirstWrite) {
  GatewaySession first;
  GatewaySession second;
  first.session_id = "session-one";
  second.session_id = "session-two";
  first.master_fd = 122;
  second.master_fd = 123;
  const auto first_id = gateway_reserve_session_output(&first);
  const auto second_id = gateway_reserve_session_output(&second);
  ASSERT_GT(first_id, 0u);
  ASSERT_GT(second_id, 0u);

  const std::string stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"partial\",\"payload\":{}}";
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      {&first, &second}, {first_id, second_id}, {301, 401}, {302, 402},
      {5, 6}, {3001, 4001}, stable, "player", 123456, 128));

  static GatewaySession *session_to_release = nullptr;
  static uint64_t reservation_to_release = 0;
  static bool release_on_first_write = false;
  static int release_result = -1;
  static int writes = 0;
  session_to_release = &second;
  reservation_to_release = second_id;
  release_on_first_write = true;
  release_result = -1;
  writes = 0;
  const auto writer = [](int, const char *, size_t) {
    ++writes;
    if (release_on_first_write) {
      release_on_first_write = false;
      release_result = gateway_release_session_output(
          session_to_release, reservation_to_release);
    }
    return 1;
  };
  GatewayPendingMessageEventBatchFillResult result;

  ASSERT_TRUE(gateway_fill_pending_message_event_batches_with_writer(
      {&first, &second}, {first_id, second_id}, {"player-one", "player-two"},
      {9, 10}, writer, &result));
  ASSERT_EQ(result.filled, (std::vector<bool>{true, true}));
  ASSERT_EQ(result.event_counts, (std::vector<LPC_INT>{1, 1}));
  ASSERT_EQ(result.slot_server_seqs, (std::vector<LPC_INT>{301, 401}));
  ASSERT_EQ(release_result, 0);
  ASSERT_EQ(writes, 2);
  ASSERT_TRUE(first.output_fifo.empty());
  ASSERT_TRUE(second.output_fifo.empty());
}

TEST_F(DriverTest, TestGatewayPendingMessageEventBatchRejectsWaveAtomically) {
  GatewaySession first;
  GatewaySession second;
  first.master_fd = 100;
  second.master_fd = 101;
  const auto first_id = gateway_reserve_session_output(&first);
  const auto second_id = gateway_reserve_session_output(&second);
  ASSERT_GT(first_id, 0u);
  ASSERT_GT(second_id, 0u);

  ASSERT_FALSE(gateway_append_preencoded_message_event_wave(
      {&first, &second}, {first_id, second_id + 1}, {101, 201}, {102, 202},
      {3, 4}, {1001, 2001},
      "{\"schema_version\":1,\"text\":\"valid\"}", "player", 123456,
      128));
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 0u);
  ASSERT_EQ(gateway_pending_message_event_count(&second, second_id), 0u);

  ASSERT_FALSE(gateway_append_preencoded_message_event_wave(
      {&first, &second}, {first_id, second_id}, {101, 201}, {102, 202}, {3, 4},
      {1001, 2001}, "{\"schema_version\":1,\"text\":", "player", 123456,
      128));
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 0u);
  ASSERT_EQ(gateway_pending_message_event_count(&second, second_id), 0u);
}

TEST_F(DriverTest, TestGatewayPendingMessageEventBatchHonorsLimitAndRelease) {
  GatewaySession session;
  session.master_fd = 102;
  const auto reservation_id = gateway_reserve_session_output(&session);
  ASSERT_GT(reservation_id, 0u);
  const std::string stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"limited\",\"payload\":{}}";
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      {&session}, {reservation_id}, {101}, {102}, {3}, {1001}, stable, "player",
      123456, 1));
  ASSERT_FALSE(gateway_append_preencoded_message_event_wave(
      {&session}, {reservation_id}, {103}, {104}, {3}, {1001}, stable, "player",
      123457, 1));
  ASSERT_EQ(gateway_pending_message_event_count(&session, reservation_id), 1u);
  ASSERT_EQ(gateway_release_session_output_with_writer(
                &session, reservation_id,
                [](int, const char *, size_t) -> int { return 1; }),
            1);
  ASSERT_EQ(gateway_pending_message_event_count(&session, reservation_id), 0u);
  ASSERT_TRUE(session.output_fifo.empty());
}

TEST_F(DriverTest, TestGatewayReserveAndAppendWaveRollsBackOnlyNewWork) {
  GatewaySession reused_session;
  GatewaySession new_session;
  reused_session.session_id = "session-reused";
  new_session.session_id = "session-new";
  reused_session.master_fd = 103;
  new_session.master_fd = 104;
  const auto existing_id = gateway_reserve_session_output(&reused_session);
  ASSERT_GT(existing_id, 0u);
  const std::string stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"wave\",\"payload\":{}}";
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      {&reused_session}, {existing_id}, {101}, {102}, {3}, {1001}, stable,
      "player", 123456, 128));

  GatewaySessionBatchReservationResult result;
  ASSERT_TRUE(gateway_reserve_and_append_preencoded_message_event_wave(
      {&reused_session, &new_session}, {existing_id, 0}, 103,
      {3, 4}, {1002, 2001}, stable, "player", 123457, 128,
      &result));
  ASSERT_EQ(result.reused, (std::vector<bool>{true, false}));
  ASSERT_GT(result.wave_id, 0u);
  ASSERT_EQ(gateway_pending_message_event_count(&reused_session, existing_id), 2u);
  ASSERT_EQ(new_session.output_fifo.size(), 1u);
  ASSERT_EQ(reused_session.output_fifo.front()
                .pending_message_batch->events.back().server_seq,
            104);
  ASSERT_EQ(new_session.output_fifo.front().pending_message_batch->slot_server_seq,
            105);
  ASSERT_EQ(new_session.output_fifo.front()
                .pending_message_batch->events.front().server_seq,
            106);

  ASSERT_TRUE(gateway_rollback_preencoded_message_event_wave_with_writer(
      {&reused_session, &new_session}, result.reservation_ids, result.reused,
      result.wave_id, [](int, const char *, size_t) -> int { return 1; }));
  ASSERT_EQ(gateway_pending_message_event_count(&reused_session, existing_id), 1u);
  ASSERT_TRUE(new_session.output_fifo.empty());

  static std::vector<std::string> writes;
  writes.clear();
  ASSERT_EQ(gateway_fill_pending_message_event_batch_with_writer(
                &reused_session, existing_id, "player-one", 7,
                [](int, const char *data, size_t len) {
                  writes.emplace_back(data, len);
                  return 1;
                }),
            1);
  ASSERT_EQ(writes.size(), 1u);
  const auto wire = nlohmann::json::parse(writes[0]);
  ASSERT_EQ(wire["type"], "output");
  ASSERT_EQ(wire["cid"], "session-reused");
  const auto frame = wire["data"].get<std::string>();
  ASSERT_NE(frame.find("\"text\":\"wave\""), std::string::npos);
  ASSERT_NE(frame.find("\"seq\":1001"), std::string::npos);
  ASSERT_EQ(frame.find("\"seq\":1002"), std::string::npos);
  ASSERT_TRUE(reused_session.output_fifo.empty());
}

TEST_F(DriverTest, TestGatewayWaveRollbackRejectsWrongIdentityAtomically) {
  GatewaySession first;
  GatewaySession second;
  first.master_fd = 105;
  second.master_fd = 106;
  const auto first_id = gateway_reserve_session_output(&first);
  ASSERT_GT(first_id, 0u);
  const std::string stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"identity\",\"payload\":{}}";
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      {&first}, {first_id}, {101}, {102}, {3}, {1001}, stable, "player",
      123456, 128));

  GatewaySessionBatchReservationResult result;
  ASSERT_TRUE(gateway_reserve_and_append_preencoded_message_event_wave(
      {&first, &second}, {first_id, 0}, 103, {3, 4},
      {1002, 2001}, stable, "player", 123457, 128, &result));
  ASSERT_EQ(result.reused, (std::vector<bool>{true, false}));

  ASSERT_FALSE(gateway_rollback_preencoded_message_event_wave_with_writer(
      {&first, &second}, result.reservation_ids, {true, true}, result.wave_id,
      [](int, const char *, size_t) -> int { return 1; }));
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 2u);
  ASSERT_EQ(gateway_pending_message_event_count(
                &second, result.reservation_ids[1]),
            1u);

  ASSERT_FALSE(gateway_rollback_preencoded_message_event_wave_with_writer(
      {&first, &second}, result.reservation_ids, result.reused,
      result.wave_id + 1,
      [](int, const char *, size_t) -> int { return 1; }));
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 2u);
  ASSERT_EQ(gateway_pending_message_event_count(
                &second, result.reservation_ids[1]),
            1u);

  ASSERT_FALSE(gateway_rollback_preencoded_message_event_wave_with_writer(
      {&first, &first}, {first_id, first_id}, {true, true}, result.wave_id,
      [](int, const char *, size_t) -> int { return 1; }));
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 2u);
  ASSERT_EQ(gateway_pending_message_event_count(
                &second, result.reservation_ids[1]),
            1u);

  ASSERT_TRUE(gateway_rollback_preencoded_message_event_wave_with_writer(
      {&first, &second}, result.reservation_ids, result.reused, result.wave_id,
      [](int, const char *, size_t) -> int { return 1; }));
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 1u);
  ASSERT_TRUE(second.output_fifo.empty());
}

TEST_F(DriverTest, TestGatewayReserveAndAppendWaveFailuresLeaveNoNewSlots) {
  GatewaySession first;
  GatewaySession full;
  first.master_fd = 107;
  full.master_fd = 108;
  full.output_fifo_max_depth = 0;
  const auto first_id = gateway_reserve_session_output(&first);
  ASSERT_GT(first_id, 0u);
  const std::string stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"atomic\",\"payload\":{}}";
  GatewaySessionBatchReservationResult result;

  ASSERT_FALSE(gateway_reserve_and_append_preencoded_message_event_wave(
      {&first, &full}, {first_id, 0}, 101, {3, 4},
      {1001, 2001}, stable, "player", 123456, 128, &result));
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 0u);
  ASSERT_EQ(first.output_fifo.size(), 1u);
  ASSERT_TRUE(full.output_fifo.empty());
  ASSERT_TRUE(result.reservation_ids.empty());
  ASSERT_TRUE(result.reused.empty());

  result.reservation_ids = {999};
  result.reused = {true};
  result.wave_id = 999;
  ASSERT_FALSE(gateway_reserve_and_append_preencoded_message_event_wave(
      {&first}, {first_id}, std::numeric_limits<LPC_INT>::max(), {3}, {1001},
      stable, "player", 123456, 128, &result));
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 0u);
  ASSERT_EQ(first.output_fifo.size(), 1u);
  ASSERT_TRUE(result.reservation_ids.empty());
  ASSERT_TRUE(result.reused.empty());
  ASSERT_EQ(result.wave_id, 0u);

  result.reservation_ids = {999};
  result.reused = {true};
  result.wave_id = 999;
  ASSERT_FALSE(gateway_reserve_and_append_preencoded_message_event_wave(
      {&first}, {first_id}, 101, {3}, {1001},
      "{\"schema_version\":1,\"text\":", "player", 123456, 128,
      &result));
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 0u);
  ASSERT_EQ(first.output_fifo.size(), 1u);
  ASSERT_TRUE(result.reservation_ids.empty());
  ASSERT_TRUE(result.reused.empty());
}

TEST_F(DriverTest, TestGatewayCompactRoomWaveReturnsOnlySparseBookkeeping) {
  GatewaySession first;
  GatewaySession second;
  first.session_id = "compact-first";
  second.session_id = "compact-second";
  first.master_fd = 109;
  second.master_fd = 110;
  const std::string stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"compact\",\"payload\":{}}";
  GatewaySessionCompactMessageEventWaveResult first_wave;

  ASSERT_TRUE(gateway_reserve_and_append_compact_preencoded_message_event_wave(
      {&first, &second}, 101, {3, 4}, {1001, 2001}, stable, "player",
      123456, 2, 7, &first_wave));
  ASSERT_EQ(first_wave.created_indices, (std::vector<size_t>{0, 1}));
  ASSERT_EQ(first_wave.created_reservation_ids.size(), 2u);
  ASSERT_EQ(first_wave.created_slot_server_seqs,
            (std::vector<LPC_INT>{101, 103}));
  ASSERT_TRUE(first_wave.full_indices.empty());
  ASSERT_EQ(first_wave.reused_count, 0u);
  ASSERT_GT(first_wave.wave_id, 0u);

  // A newly-created slot cannot be reused until LPC session/FIFO bookkeeping
  // has committed the sparse registration result.
  GatewaySessionCompactMessageEventWaveResult uncommitted_wave;
  ASSERT_FALSE(gateway_reserve_and_append_compact_preencoded_message_event_wave(
      {&first, &second}, 105, {3, 4}, {1002, 2002}, stable, "player",
      123457, 2, 7, &uncommitted_wave));
  ASSERT_TRUE(gateway_commit_compact_preencoded_message_event_wave(
      {&first, &second}, first_wave.wave_id));

  GatewaySessionCompactMessageEventWaveResult reused_wave;
  ASSERT_TRUE(gateway_reserve_and_append_compact_preencoded_message_event_wave(
      {&first, &second}, 105, {3, 4}, {1002, 2002}, stable, "player",
      123457, 2, 11, &reused_wave));
  ASSERT_TRUE(reused_wave.created_indices.empty());
  ASSERT_TRUE(reused_wave.created_reservation_ids.empty());
  ASSERT_TRUE(reused_wave.created_slot_server_seqs.empty());
  ASSERT_EQ(reused_wave.full_indices, (std::vector<size_t>{0, 1}));
  ASSERT_EQ(reused_wave.reused_count, 2u);

  GatewayPendingMessageEventBatchStatus status;
  ASSERT_TRUE(gateway_pending_message_event_batch_status(
      &first, first_wave.created_reservation_ids[0], &status));
  ASSERT_EQ(status.event_count, 2u);
  ASSERT_EQ(status.text_length_total, 18u);
  ASSERT_TRUE(status.registered);
  ASSERT_GT(status.created_at_ns, 0u);
  ASSERT_GE(status.last_append_at_ns, status.created_at_ns);

  ASSERT_TRUE(gateway_rollback_compact_preencoded_message_event_wave_with_writer(
      {&first, &second}, reused_wave.wave_id,
      [](int, const char *, size_t) -> int { return 1; }));
  ASSERT_TRUE(gateway_pending_message_event_batch_status(
      &first, first_wave.created_reservation_ids[0], &status));
  ASSERT_EQ(status.event_count, 1u);
  ASSERT_EQ(status.text_length_total, 7u);
  ASSERT_TRUE(status.registered);
  ASSERT_EQ(first.output_fifo.size(), 1u);
  ASSERT_EQ(second.output_fifo.size(), 1u);
}

TEST_F(DriverTest,
       TestGatewayPendingMessageEventBatchFillReturnsAuthoritativeTotals) {
  GatewaySession session;
  session.session_id = "native-fill-totals";
  session.master_fd = 115;
  const std::string stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"totals\",\"payload\":{}}";
  GatewaySessionCompactMessageEventWaveResult first_wave;
  ASSERT_TRUE(gateway_reserve_and_append_compact_preencoded_message_event_wave(
      {&session}, 101, {3}, {1001}, stable, "player", 123456, 128, 7,
      &first_wave));
  ASSERT_TRUE(gateway_commit_compact_preencoded_message_event_wave(
      {&session}, first_wave.wave_id));

  GatewaySessionCompactMessageEventWaveResult second_wave;
  ASSERT_TRUE(gateway_reserve_and_append_compact_preencoded_message_event_wave(
      {&session}, 103, {3}, {1002}, stable, "player", 123457, 128, 11,
      &second_wave));
  ASSERT_TRUE(second_wave.created_indices.empty());
  ASSERT_EQ(second_wave.reused_count, 1u);
  ASSERT_TRUE(gateway_commit_compact_preencoded_message_event_wave(
      {&session}, second_wave.wave_id));

  GatewayPendingMessageEventBatchFillResult result;
  ASSERT_TRUE(gateway_fill_pending_message_event_batches_with_writer(
      {&session}, {first_wave.created_reservation_ids[0]}, {"player-one"},
      {9}, [](int, const char *, size_t) -> int { return 1; }, &result));
  ASSERT_EQ(result.filled, (std::vector<bool>{true}));
  ASSERT_EQ(result.event_counts, (std::vector<LPC_INT>{2}));
  ASSERT_EQ(result.text_length_totals, (std::vector<LPC_INT>{18}));
  ASSERT_EQ(result.slot_server_seqs, (std::vector<LPC_INT>{101}));
  ASSERT_TRUE(session.output_fifo.empty());
}

TEST_F(DriverTest, TestGatewayCompactRoomWaveRollsBackUncommittedCreatedSlots) {
  GatewaySession first;
  GatewaySession second;
  first.session_id = "compact-created-first";
  second.session_id = "compact-created-second";
  first.master_fd = 111;
  second.master_fd = 112;
  const std::string stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"created\",\"payload\":{}}";
  GatewaySessionCompactMessageEventWaveResult wave;

  ASSERT_TRUE(gateway_reserve_and_append_compact_preencoded_message_event_wave(
      {&first, &second}, 201, {7, 8}, {3001, 4001}, stable, "player",
      223456, 128, 7, &wave));
  ASSERT_EQ(wave.created_indices, (std::vector<size_t>{0, 1}));
  ASSERT_EQ(wave.reused_count, 0u);

  GatewayPendingMessageEventBatchStatus status;
  ASSERT_TRUE(gateway_pending_message_event_batch_status(
      &first, wave.created_reservation_ids[0], &status));
  ASSERT_FALSE(status.registered);
  ASSERT_EQ(status.event_count, 1u);
  ASSERT_TRUE(gateway_rollback_compact_preencoded_message_event_wave_with_writer(
      {&first, &second}, wave.wave_id,
      [](int, const char *, size_t) -> int { return 1; }));
  ASSERT_TRUE(first.output_fifo.empty());
  ASSERT_TRUE(second.output_fifo.empty());
  ASSERT_FALSE(gateway_pending_message_event_batch_status(
      &first, wave.created_reservation_ids[0], &status));
  ASSERT_EQ(status.event_count, 0u);
  ASSERT_FALSE(status.registered);
}

TEST_F(DriverTest, TestGatewayCompactRoomWaveMixedRollbackIsAtomic) {
  GatewaySession reused;
  GatewaySession created;
  reused.session_id = "compact-mixed-reused";
  created.session_id = "compact-mixed-created";
  reused.master_fd = 113;
  created.master_fd = 114;
  const std::string stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"mixed\",\"payload\":{}}";
  GatewaySessionCompactMessageEventWaveResult base;
  ASSERT_TRUE(gateway_reserve_and_append_compact_preencoded_message_event_wave(
      {&reused}, 301, {9}, {5001}, stable, "player", 323456, 2, 5,
      &base));
  ASSERT_TRUE(gateway_commit_compact_preencoded_message_event_wave(
      {&reused}, base.wave_id));

  GatewaySessionCompactMessageEventWaveResult mixed;
  ASSERT_TRUE(gateway_reserve_and_append_compact_preencoded_message_event_wave(
      {&reused, &created}, 303, {9, 10}, {5002, 6001}, stable, "player",
      323457, 2, 6, &mixed));
  ASSERT_EQ(mixed.created_indices, (std::vector<size_t>{1}));
  ASSERT_EQ(mixed.created_slot_server_seqs, (std::vector<LPC_INT>{305}));
  ASSERT_EQ(mixed.full_indices, (std::vector<size_t>{0}));
  ASSERT_EQ(mixed.reused_count, 1u);

  GatewayPendingMessageEventBatchStatus status;
  ASSERT_FALSE(gateway_commit_compact_preencoded_message_event_wave(
      {&reused, &created}, mixed.wave_id + 1));
  ASSERT_FALSE(gateway_rollback_compact_preencoded_message_event_wave_with_writer(
      {&reused, &created}, mixed.wave_id + 1,
      [](int, const char *, size_t) -> int { return 1; }));
  ASSERT_TRUE(gateway_pending_message_event_batch_status(
      &reused, base.created_reservation_ids[0], &status));
  ASSERT_EQ(status.event_count, 2u);
  ASSERT_EQ(status.text_length_total, 11u);
  ASSERT_EQ(created.output_fifo.size(), 1u);

  ASSERT_TRUE(gateway_commit_compact_preencoded_message_event_wave(
      {&reused, &created}, mixed.wave_id));
  ASSERT_TRUE(gateway_rollback_compact_preencoded_message_event_wave_with_writer(
      {&reused, &created}, mixed.wave_id,
      [](int, const char *, size_t) -> int { return 1; }));
  ASSERT_TRUE(gateway_pending_message_event_batch_status(
      &reused, base.created_reservation_ids[0], &status));
  ASSERT_EQ(status.event_count, 1u);
  ASSERT_EQ(status.text_length_total, 5u);
  ASSERT_TRUE(status.registered);
  ASSERT_TRUE(created.output_fifo.empty());
  ASSERT_FALSE(gateway_pending_message_event_batch_status(
      &created, mixed.created_reservation_ids[0], &status));
}

TEST_F(DriverTest, TestGatewayPreencodedChatBatchBuilderKeepsStableAndDynamicBoundaries) {
  const std::vector<std::string> stable_children{
      "{\"content\":\"first\",\"direction\":\"incoming\"}",
      "{\"content\":\"第二条\",\"direction\":\"incoming\"}",
  };
  const auto frame = gateway_encode_preencoded_chat_batch_for_test(
      stable_children, 7, 101, 123456,
      "{\"meta\":{\"server_seq\":100,\"stream\":\"message\"}}");

  ASSERT_EQ(
      frame,
      "\x1bXKBACH{\"messages\":[{\"type\":\"CHAT\",\"payload\":{\"content\":\"first\","
      "\"direction\":\"incoming\",\"meta\":{\"stream\":\"message\",\"server_seq\":101,"
      "\"sent_at\":123456,\"priority\":\"normal\",\"epoch\":7,"
      "\"reliability\":\"important\"}}},{\"type\":\"CHAT\",\"payload\":{"
      "\"content\":\"第二条\",\"direction\":\"incoming\",\"meta\":{\"stream\":\"message\","
      "\"server_seq\":102,\"sent_at\":123456,\"priority\":\"normal\",\"epoch\":7,"
      "\"reliability\":\"important\"}}}],\"meta\":{\"server_seq\":100,"
      "\"stream\":\"message\"}}\x1b\n");
  ASSERT_TRUE(gateway_encode_preencoded_chat_batch_for_test(
                  {"[]"}, 7, 101, 123456, "{\"meta\":{}}")
                  .empty());
  ASSERT_TRUE(gateway_encode_preencoded_chat_batch_for_test(
                  stable_children, -1, 101, 123456, "{\"meta\":{}}")
                  .empty());
  ASSERT_TRUE(gateway_encode_preencoded_chat_batch_for_test(
                  stable_children, 7, std::numeric_limits<LPC_INT>::max(),
                  123456, "{\"meta\":{}}")
                  .empty());
  ASSERT_TRUE(gateway_encode_preencoded_chat_batch_for_test(
                  stable_children, 7, 101, 123456, "[]")
                  .empty());
  ASSERT_TRUE(gateway_encode_preencoded_chat_batch_for_test(
                  {"{\"content\":\"bad\",}"}, 7, 101, 123456,
                  "{\"meta\":{}}")
                  .empty());
  ASSERT_TRUE(gateway_encode_preencoded_chat_batch_for_test(
                  {"{\"content\":\"bad\",\"meta\":{}}"}, 7, 101,
                  123456, "{\"meta\":{}}")
                  .empty());
  ASSERT_TRUE(gateway_encode_preencoded_chat_batch_for_test(
                  stable_children, 7, 101, 123456,
                  "{\"messages\":[]}")
                  .empty());
  ASSERT_TRUE(gateway_encode_preencoded_chat_batch_for_test(
                  stable_children, 7, 101, 123456,
                  "{\"meta\":\"bad\"}")
                  .empty());
  ASSERT_TRUE(gateway_encode_preencoded_chat_batch_for_test(
                  stable_children, 7, 101, 123456,
                  "{\"meta\":{},\"extra\":1}")
                  .empty());
}

TEST_F(DriverTest, TestGatewayPreencodedMessageEventBatchBuilderKeepsEnvelopeAndMetaSemantics) {
  const std::vector<std::string> stable_children{
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"normal\",\"reliability\":\"important\","
      "\"display_mode\":\"paced\",\"ttl_ms\":30000,\"collapse_key\":\"\","
      "\"text\":\"first\",\"payload\":{}}",
      "{\"id\":\"fixed-id\",\"scope\":{\"type\":\"room\",\"id\":\"room-1\"},"
      "\"causation_id\":\"fixed-cause\",\"correlation_id\":\"fixed-correlation\","
      "\"schema_version\":1,\"channel\":\"combat\",\"intent\":\"append\","
      "\"priority\":\"critical\",\"reliability\":\"critical\","
      "\"display_mode\":\"paced\",\"ttl_ms\":45000,"
      "\"collapse_key\":\"combat/wave\",\"text\":\"second\","
      "\"payload\":{\"kind\":\"combat\"}}",
  };
  const auto frame = gateway_encode_preencoded_message_event_batch_for_test(
      stable_children, {"player", "player"}, "observer-7", {11, 12},
      {201, 202}, {7, 8}, {1000, 1001}, 200, 0, 999);

  ASSERT_GE(frame.size(), 8u);
  ASSERT_EQ(frame.substr(0, 7), "\x1bXKBACH");
  ASSERT_EQ(frame.substr(frame.size() - 2), "\x1b\n");
  const auto payload = nlohmann::json::parse(
      frame.substr(7, frame.size() - 9));
  ASSERT_EQ(payload["meta"],
            (nlohmann::json{{"server_seq", 200}, {"stream", "system"},
                            {"epoch", 0}, {"reliability", "important"},
                            {"priority", "normal"}, {"sent_at", 999}}));
  ASSERT_EQ(payload["messages"].size(), 2u);

  const auto &first = payload["messages"][0];
  ASSERT_EQ(first["type"], "MSGE");
  ASSERT_EQ(first["payload"]["id"], "msg_1000_11");
  ASSERT_EQ(first["payload"]["seq"], 11);
  ASSERT_EQ(first["payload"]["scope"],
            (nlohmann::json{{"type", "player"}, {"id", "observer-7"}}));
  ASSERT_EQ(first["payload"]["causation_id"], "cmd_11");
  ASSERT_EQ(first["payload"]["correlation_id"], "txn_11");
  ASSERT_EQ(first["payload"]["timestamp"], 1000);
  ASSERT_EQ(first["payload"]["text"], "first");
  ASSERT_EQ(first["payload"]["meta"],
            (nlohmann::json{{"server_seq", 201}, {"stream", "message"},
                            {"epoch", 7}, {"reliability", "important"},
                            {"priority", "normal"}, {"sent_at", 1000},
                            {"ttl_ms", 30000}}));

  const auto &second = payload["messages"][1]["payload"];
  ASSERT_EQ(second["id"], "fixed-id");
  ASSERT_EQ(second["scope"],
            (nlohmann::json{{"type", "room"}, {"id", "room-1"}}));
  ASSERT_EQ(second["causation_id"], "fixed-cause");
  ASSERT_EQ(second["correlation_id"], "fixed-correlation");
  ASSERT_EQ(second["meta"]["server_seq"], 202);
  ASSERT_EQ(second["meta"]["epoch"], 8);
  ASSERT_EQ(second["meta"]["reliability"], "critical");
  ASSERT_EQ(second["meta"]["priority"], "critical");
  ASSERT_EQ(second["meta"]["collapse_key"], "combat/wave");
  ASSERT_EQ(second["meta"]["ttl_ms"], 45000);
}

TEST_F(DriverTest, TestGatewayPreencodedMessageEventSingletonUsesReservedOuterSequence) {
  const std::vector<std::string> stable_children{
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"normal\",\"reliability\":\"important\","
      "\"display_mode\":\"paced\",\"ttl_ms\":30000,\"collapse_key\":\"\","
      "\"text\":\"only\",\"payload\":{}}",
  };
  const auto frame = gateway_encode_preencoded_message_event_batch_for_test(
      stable_children, {"player"}, "observer-8", {13}, {301}, {9},
      {2000}, 300, 0, 1999);

  ASSERT_EQ(frame.substr(0, 7), "\x1bXKMSGE");
  const auto payload = nlohmann::json::parse(
      frame.substr(7, frame.size() - 9));
  ASSERT_EQ(payload["seq"], 13);
  ASSERT_EQ(payload["meta"]["server_seq"], 300);
  ASSERT_EQ(payload["meta"]["epoch"], 9);
  ASSERT_EQ(payload["meta"]["sent_at"], 2000);
}

TEST_F(DriverTest, TestGatewayPreencodedMessageEventBuilderRejectsMalformedOrDynamicStableInput) {
  const std::string valid =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"normal\",\"reliability\":\"important\","
      "\"display_mode\":\"paced\",\"ttl_ms\":30000,\"collapse_key\":\"\","
      "\"text\":\"only\",\"payload\":{}}";
  const auto encode = [&](const std::vector<std::string> &stable,
                          const std::vector<std::string> &scope_types,
                          const std::vector<LPC_INT> &message_seqs) {
    return gateway_encode_preencoded_message_event_batch_for_test(
        stable, scope_types, "observer-9", message_seqs, {401}, {10},
        {3000}, 400, 0, 2999);
  };

  ASSERT_TRUE(encode({"[]"}, {"player"}, {14}).empty());
  ASSERT_TRUE(encode({valid}, {}, {14}).empty());
  ASSERT_TRUE(encode({valid}, {"player"}, {}).empty());
  ASSERT_TRUE(encode({valid.substr(0, valid.size() - 1) +
                      ",\"seq\":999}"},
                     {"player"}, {14})
                  .empty());
  ASSERT_TRUE(encode({valid.substr(0, valid.size() - 1) +
                      ",\"meta\":{\"server_seq\":999}}"},
                     {"player"}, {14})
                  .empty());
  ASSERT_TRUE(gateway_encode_preencoded_message_event_batch_for_test(
                  {valid}, {"player"}, "observer-9", {14}, {401}, {10},
                  {3000}, 0, 0, 2999)
                  .empty());
}

TEST_F(DriverTest, TestGatewayPreencodedMessageEventTemplateCacheKeepsDynamicMetadataIsolated) {
  const std::string stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"normal\",\"reliability\":\"important\","
      "\"display_mode\":\"paced\",\"ttl_ms\":30000,\"collapse_key\":\"\","
      "\"text\":\"shared room fact\",\"payload\":{}}";

  gateway_clear_message_event_template_cache_for_test();
  const auto hits_before =
      g_gateway_runtime_counters.message_event_template_cache_hits.load();
  const auto misses_before =
      g_gateway_runtime_counters.message_event_template_cache_misses.load();

  const auto first_frame = gateway_encode_preencoded_message_event_batch_for_test(
      {stable}, {"player"}, "observer-a", {11}, {201}, {7}, {1000},
      200, 0, 999);
  const auto second_frame = gateway_encode_preencoded_message_event_batch_for_test(
      {stable}, {"player"}, "observer-b", {12}, {301}, {8}, {2000},
      300, 0, 1999);

  ASSERT_FALSE(first_frame.empty());
  ASSERT_FALSE(second_frame.empty());
  const auto first = nlohmann::json::parse(
      first_frame.substr(7, first_frame.size() - 9));
  const auto second = nlohmann::json::parse(
      second_frame.substr(7, second_frame.size() - 9));
  ASSERT_EQ(first["scope"]["id"], "observer-a");
  ASSERT_EQ(first["seq"], 11);
  ASSERT_EQ(first["meta"]["server_seq"], 200);
  ASSERT_EQ(second["scope"]["id"], "observer-b");
  ASSERT_EQ(second["seq"], 12);
  ASSERT_EQ(second["meta"]["server_seq"], 300);
  ASSERT_EQ(
      g_gateway_runtime_counters.message_event_template_cache_misses.load(),
      misses_before + 1);
  ASSERT_EQ(
      g_gateway_runtime_counters.message_event_template_cache_hits.load(),
      hits_before + 1);
}

TEST_F(DriverTest, TestGatewayPreencodedMessageEventTemplateEscapesDynamicScope) {
  const std::string stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"normal\",\"reliability\":\"important\","
      "\"display_mode\":\"paced\",\"ttl_ms\":30000,\"collapse_key\":\"\","
      "\"text\":\"shared room fact\",\"payload\":{}}";
  const std::string scope_type = "player\nobserver";
  const std::string scope_id = "观察者\"甲\\乙";

  gateway_clear_message_event_template_cache_for_test();
  const auto frame = gateway_encode_preencoded_message_event_batch_for_test(
      {stable}, {scope_type}, scope_id, {15}, {501}, {11}, {4000},
      500, 0, 3999);

  ASSERT_FALSE(frame.empty());
  const auto payload = nlohmann::json::parse(
      frame.substr(7, frame.size() - 9));
  ASSERT_EQ(payload["scope"]["type"], scope_type);
  ASSERT_EQ(payload["scope"]["id"], scope_id);
  ASSERT_EQ(payload["text"], "shared room fact");
}

TEST_F(DriverTest, TestGatewayPreencodedMessageEventTemplateCacheRejectsInvalidAndBypassesOversizedInput) {
  const std::string stable_prefix =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"normal\",\"reliability\":\"important\","
      "\"display_mode\":\"paced\",\"ttl_ms\":30000,\"collapse_key\":\"\","
      "\"text\":\"";
  const std::string stable_suffix = "\",\"payload\":{}}";
  const std::string invalid = stable_prefix + "invalid\"} ";
  const std::string oversized =
      stable_prefix + std::string(70 * 1024, 'x') + stable_suffix;

  gateway_clear_message_event_template_cache_for_test();
  const auto hits_before =
      g_gateway_runtime_counters.message_event_template_cache_hits.load();
  const auto misses_before =
      g_gateway_runtime_counters.message_event_template_cache_misses.load();
  const auto bypasses_before =
      g_gateway_runtime_counters.message_event_template_cache_bypasses.load();
  const auto encode = [](const std::string &stable) {
    return gateway_encode_preencoded_message_event_batch_for_test(
        {stable}, {"player"}, "observer-cache", {14}, {401}, {10},
        {3000}, 400, 0, 2999);
  };

  ASSERT_TRUE(encode(invalid).empty());
  ASSERT_TRUE(encode(invalid).empty());
  ASSERT_FALSE(encode(oversized).empty());
  ASSERT_FALSE(encode(oversized).empty());
  ASSERT_EQ(
      g_gateway_runtime_counters.message_event_template_cache_hits.load(),
      hits_before);
  ASSERT_EQ(
      g_gateway_runtime_counters.message_event_template_cache_misses.load(),
      misses_before + 2);
  ASSERT_EQ(
      g_gateway_runtime_counters.message_event_template_cache_bypasses.load(),
      bypasses_before + 2);
}

TEST_F(DriverTest, TestGatewayPreencodedMessageEventTemplateCacheEvictsAtBound) {
  const std::string stable_prefix =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"normal\",\"reliability\":\"important\","
      "\"display_mode\":\"paced\",\"ttl_ms\":30000,\"collapse_key\":\"\","
      "\"text\":\"event ";
  const std::string stable_suffix = "\",\"payload\":{}}";
  std::vector<std::string> stable_events;
  stable_events.reserve(257);
  for (size_t index = 0; index < 257; ++index) {
    stable_events.push_back(stable_prefix + std::to_string(index) + stable_suffix);
  }

  gateway_clear_message_event_template_cache_for_test();
  const auto evictions_before =
      g_gateway_runtime_counters.message_event_template_cache_evictions.load();
  for (const auto &stable : stable_events) {
    ASSERT_FALSE(gateway_encode_preencoded_message_event_batch_for_test(
                     {stable}, {"player"}, "observer-bound", {16}, {601},
                     {12}, {5000}, 600, 0, 4999)
                     .empty());
  }
  ASSERT_EQ(
      g_gateway_runtime_counters.message_event_template_cache_evictions.load(),
      evictions_before + 256);

  const auto hits_before =
      g_gateway_runtime_counters.message_event_template_cache_hits.load();
  ASSERT_FALSE(gateway_encode_preencoded_message_event_batch_for_test(
                   {stable_events.back()}, {"player"}, "observer-bound", {17},
                   {602}, {12}, {5001}, 601, 0, 5000)
                   .empty());
  ASSERT_EQ(
      g_gateway_runtime_counters.message_event_template_cache_hits.load(),
      hits_before + 1);
}

TEST_F(DriverTest, TestGatewayReceiveDoesNotDrainMainTasksInsideReadCallback) {
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway.cc");
  const auto apply_pos = source.find("bool gateway_apply_receive");
  ASSERT_NE(apply_pos, std::string::npos);
  const auto next_function_pos = source.find("\nsvalue_t json_to_gateway_svalue", apply_pos);
  ASSERT_NE(next_function_pos, std::string::npos);

  const auto apply_source = source.substr(apply_pos, next_function_pos - apply_pos);
  const auto event_base_pos = apply_source.find("if (g_event_base) {");
  const auto fallback_pos = apply_source.find("\n    } else {", event_base_pos);
  ASSERT_EQ(apply_source.find("vm_owner_drain_main_tasks"), std::string::npos);
  ASSERT_NE(apply_source.find("vm_owner_enqueue_main_task"), std::string::npos);
  ASSERT_NE(event_base_pos, std::string::npos);
  ASSERT_NE(fallback_pos, std::string::npos);
  const auto event_base_source =
      apply_source.substr(event_base_pos, fallback_pos - event_base_pos);
  ASSERT_EQ(event_base_source.find("gateway_drain_owner_main_tasks_later"),
            std::string::npos);
  ASSERT_EQ(event_base_source.find("gateway_drain_owner_main_tasks_now"),
            std::string::npos);
  ASSERT_EQ(apply_source.find("vm_owner_executor_available"), std::string::npos);
}

TEST_F(DriverTest, TestGatewayReadBatchDrainsAdmittedTasksBeforeDeferredRemainder) {
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway.cc");
  const auto header = read_source_file_for_test("../src/packages/gateway/gateway.h");

  const auto helper_pos = source.find("void gateway_service_admitted_receive_tasks()");
  const auto helper_end = source.find("\nbool gateway_apply_receive", helper_pos);
  ASSERT_NE(helper_pos, std::string::npos);
  ASSERT_NE(helper_end, std::string::npos);
  const auto helper_source = source.substr(helper_pos, helper_end - helper_pos);
  ASSERT_NE(helper_source.find("gateway_drain_owner_main_tasks_with_budget("),
            std::string::npos);
  ASSERT_NE(helper_source.find("kGatewayReadBatchMainDrainBudget"),
            std::string::npos);
  ASSERT_NE(helper_source.find("kGatewayReadBatchMainDrainWallBudget"),
            std::string::npos);
  ASSERT_NE(helper_source.find("drain_result.remaining_main_tasks > 0"),
            std::string::npos);
  ASSERT_NE(helper_source.find("gateway_drain_owner_main_tasks_later()"),
            std::string::npos);
  ASSERT_NE(source.find(
                "kGatewayDeferredMainDrainContinuationDelay = std::chrono::milliseconds(1)"),
            std::string::npos);
  ASSERT_NE(source.find("gateway_deferred_main_drain_wall_budget(backlog_before)"),
            std::string::npos);
  ASSERT_NE(source.find("kGatewayDeferredMainDrainBaseWallBudget = std::chrono::milliseconds(8)"),
            std::string::npos);
  ASSERT_NE(source.find("kGatewayDeferredMainDrainBacklogWallBudget = std::chrono::milliseconds(8)"),
            std::string::npos);
  ASSERT_NE(source.find("kGatewayDeferredMainDrainBacklogThreshold = 64"),
            std::string::npos);
  ASSERT_NE(source.find("kGatewayDeferredMainDrainWaitTimerQueueOnly = 1"),
            std::string::npos);

  const auto deferred_pos = source.find("void gateway_drain_owner_main_tasks_later()");
  const auto deferred_end = source.find("\nvoid gateway_service_admitted_receive_tasks", deferred_pos);
  ASSERT_NE(deferred_pos, std::string::npos);
  ASSERT_NE(deferred_end, std::string::npos);
  const auto deferred_source = source.substr(deferred_pos, deferred_end - deferred_pos);
  ASSERT_NE(deferred_source.find("add_walltime_event("), std::string::npos);
  ASSERT_NE(deferred_source.find("kGatewayDeferredMainDrainContinuationDelay"),
            std::string::npos);
  ASSERT_NE(deferred_source.find("BackendEventPriority::kGateway"),
            std::string::npos);
  const auto callback_started = deferred_source.find("auto callback_started_at = gateway_now_ns();");
  const auto drain_call = deferred_source.find("gateway_drain_owner_main_tasks_with_budget(");
  const auto wait_sample = deferred_source.find("callback_started_at - scheduled_at");
  ASSERT_NE(callback_started, std::string::npos);
  ASSERT_NE(drain_call, std::string::npos);
  ASSERT_NE(wait_sample, std::string::npos);
  ASSERT_LT(callback_started, drain_call);
  ASSERT_LT(drain_call, wait_sample);
  ASSERT_EQ(deferred_source.find("gateway_now_ns() - scheduled_at"),
            std::string::npos);

  const auto schedule_pos = source.find("void gateway_schedule_buffered_read(int fd) {");
  const auto readcb_pos = source.find("void gateway_readcb", schedule_pos);
  ASSERT_NE(schedule_pos, std::string::npos);
  ASSERT_NE(readcb_pos, std::string::npos);
  const auto schedule_source = source.substr(schedule_pos, readcb_pos - schedule_pos);
  const auto continuation_dispatch = schedule_source.find(
      "gateway_dispatch_buffered_frames(scheduled_master,");
  const auto continuation_service = schedule_source.find(
      "gateway_service_admitted_receive_tasks();", continuation_dispatch);
  ASSERT_NE(continuation_dispatch, std::string::npos);
  ASSERT_NE(continuation_service, std::string::npos);

  const auto listener_pos = source.find("\nvoid gateway_listener_cb", readcb_pos);
  ASSERT_NE(listener_pos, std::string::npos);
  const auto readcb_source = source.substr(readcb_pos, listener_pos - readcb_pos);
  const auto read_dispatch = readcb_source.find(
      "gateway_dispatch_buffered_frames(master, kGatewayReadFrameBudget)");
  const auto read_service = readcb_source.find(
      "gateway_service_admitted_receive_tasks();", read_dispatch);
  ASSERT_NE(read_dispatch, std::string::npos);
  ASSERT_NE(read_service, std::string::npos);

  for (const auto *field : {
           "read_batch_drain_runs",
           "read_batch_drain_tasks_total",
           "read_batch_drain_tasks_max",
           "read_batch_drain_backlog_rescheduled",
           "read_batch_drain_wall_ns_total",
           "read_batch_drain_wall_ns_max",
           "read_batch_drain_wall_samples",
           "read_batch_drain_wall_budget_yields",
           "read_batch_drain_task_budget_yields",
           "read_batch_drain_remaining_total",
           "read_batch_drain_remaining_max",
           "read_batch_drain_remaining_samples",
           "read_batch_drain_main_task_wall_ns_max",
           "read_batch_drain_main_tasks_exceeding_wall_budget",
           "main_drain_deferred_backlog_boosted",
           "main_drain_deferred_tasks_total",
           "main_drain_deferred_tasks_max",
           "main_drain_deferred_wall_ns_total",
           "main_drain_deferred_wall_ns_max",
           "main_drain_deferred_wall_samples",
           "main_drain_deferred_wall_budget_yields",
           "main_drain_deferred_task_budget_yields",
           "main_drain_deferred_remaining_total",
           "main_drain_deferred_remaining_max",
           "main_drain_deferred_remaining_samples",
           "main_drain_deferred_main_task_wall_ns_max",
           "main_drain_deferred_main_tasks_exceeding_wall_budget",
       }) {
    ASSERT_NE(header.find(field), std::string::npos) << field;
  }
}

TEST_F(DriverTest, TestGatewayCommandPendingCountUsesAtomicSessionLifecycle) {
  const auto source =
      read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto count_pos =
      source.find("LPC_INT gateway_session_command_pending_count()");
  const auto count_end = source.find("\nuint64_t gateway_session_fifo_enqueued_total", count_pos);
  const auto unbind_pos = source.find("void gateway_unbind_session_object");
  const auto unbind_end = source.find("\nvoid gateway_cleanup_master_sessions", unbind_pos);
  const auto cleanup_pos = source.find("void cleanup_gateway_sessions()");
  const auto cleanup_end = source.find("\nvoid f_gateway_session_send", cleanup_pos);

  ASSERT_NE(source.find("std::atomic<LPC_INT> g_gateway_command_input_pending_sessions{0}"),
            std::string::npos);
  ASSERT_NE(source.find("std::atomic<LPC_INT> g_gateway_command_task_pending_sessions{0}"),
            std::string::npos);
  ASSERT_NE(source.find("void gateway_release_command_input_pending(GatewaySession *sess)"),
            std::string::npos);
  ASSERT_NE(source.find("void gateway_release_command_task_pending(GatewaySession *sess)"),
            std::string::npos);
  ASSERT_NE(source.find("g_gateway_command_input_pending_sessions.fetch_add("),
            std::string::npos);
  ASSERT_NE(source.find("g_gateway_command_task_pending_sessions.fetch_add("),
            std::string::npos);
  ASSERT_NE(count_pos, std::string::npos);
  ASSERT_NE(count_end, std::string::npos);
  const auto count_source = source.substr(count_pos, count_end - count_pos);
  ASSERT_NE(count_source.find("gateway_session_command_input_pending_count()"),
            std::string::npos);
  ASSERT_NE(count_source.find("gateway_session_command_task_pending_count()"),
            std::string::npos);
  ASSERT_EQ(count_source.find("g_gateway_sessions"), std::string::npos);

  ASSERT_NE(unbind_pos, std::string::npos);
  ASSERT_NE(unbind_end, std::string::npos);
  const auto unbind_source = source.substr(unbind_pos, unbind_end - unbind_pos);
  ASSERT_NE(unbind_source.find("gateway_release_command_input_pending(sess);"),
            std::string::npos);
  ASSERT_NE(unbind_source.find("gateway_release_command_task_pending(sess);"),
            std::string::npos);
  ASSERT_LT(unbind_source.find("gateway_release_command_task_pending(sess);"),
            unbind_source.find("g_gateway_sessions.erase(session_id);"));

  ASSERT_NE(cleanup_pos, std::string::npos);
  ASSERT_NE(cleanup_end, std::string::npos);
  const auto cleanup_source = source.substr(cleanup_pos, cleanup_end - cleanup_pos);
  ASSERT_NE(cleanup_source.find(
                "g_gateway_command_input_pending_sessions.store(0, std::memory_order_release);"),
            std::string::npos);
  ASSERT_NE(cleanup_source.find(
                "g_gateway_command_task_pending_sessions.store(0, std::memory_order_release);"),
            std::string::npos);
}

TEST_F(DriverTest, TestGatewayLivePressureCountsUseLpcIntegerWidth) {
  const auto header =
      read_source_file_for_test("../src/packages/gateway/gateway.h");
  const auto source =
      read_source_file_for_test("../src/packages/gateway/gateway.cc");
  const auto session_source =
      read_source_file_for_test("../src/packages/gateway/gateway_session.cc");

  for (const auto *signature : {
           "LPC_INT gateway_session_command_input_pending_count();",
           "LPC_INT gateway_session_command_task_pending_count();",
           "LPC_INT gateway_session_command_pending_count();",
           "LPC_INT gateway_read_dispatch_pending_count();",
           "LPC_INT gateway_buffered_input_pending_count();",
           "LPC_INT gateway_command_pressure_count();",
       }) {
    EXPECT_NE(header.find(signature), std::string::npos) << signature;
  }
  EXPECT_EQ(header.find("long gateway_session_command_input_pending_count();"),
            std::string::npos);
  EXPECT_EQ(header.find("long gateway_read_dispatch_pending_count();"),
            std::string::npos);
  EXPECT_EQ(header.find("long gateway_command_pressure_count();"),
            std::string::npos);

  EXPECT_NE(session_source.find(
                "std::atomic<LPC_INT> g_gateway_command_input_pending_sessions{0}"),
            std::string::npos);
  EXPECT_NE(session_source.find(
                "std::atomic<LPC_INT> g_gateway_command_task_pending_sessions{0}"),
            std::string::npos);
  EXPECT_NE(session_source.find(
                "void gateway_decrement_pending_counter(std::atomic<LPC_INT> &counter)"),
            std::string::npos);
  for (const auto *signature : {
           "LPC_INT gateway_session_command_input_pending_count()",
           "LPC_INT gateway_session_command_task_pending_count()",
           "LPC_INT gateway_session_command_pending_count()",
       }) {
    EXPECT_NE(session_source.find(signature), std::string::npos) << signature;
  }
  EXPECT_EQ(session_source.find("std::atomic<long> g_gateway_command_input_pending_sessions"),
            std::string::npos);
  EXPECT_EQ(session_source.find("long gateway_session_command_pending_count()"),
            std::string::npos);

  EXPECT_NE(source.find("std::atomic<LPC_INT> g_gateway_read_dispatch_pending_masters{0}"),
            std::string::npos);
  EXPECT_NE(source.find("LPC_INT gateway_main_queue_read_paused_count()"),
            std::string::npos);
  EXPECT_NE(source.find("LPC_INT gateway_read_dispatch_pending_count()"),
            std::string::npos);
  EXPECT_NE(source.find("LPC_INT gateway_buffered_input_pending_count()"),
            std::string::npos);
  EXPECT_NE(source.find("LPC_INT gateway_command_pressure_count()"),
            std::string::npos);
  EXPECT_NE(source.find("LPC_INT pending = 0;"), std::string::npos);
  EXPECT_EQ(source.find("std::atomic<long> g_gateway_read_dispatch_pending_masters"),
            std::string::npos);
  EXPECT_EQ(source.find("long gateway_buffered_input_pending_count()"),
            std::string::npos);
  EXPECT_EQ(source.find("long gateway_command_pressure_count()"),
            std::string::npos);
}

TEST_F(DriverTest, TestGatewayReadDispatchPressureIsTransitionCountedAndUnderflowSafe) {
  const auto before = gateway_read_dispatch_pending_count();
  {
    GatewayMaster master;
    gateway_set_read_dispatch_pending_for_test(&master, true);
    ASSERT_EQ(gateway_read_dispatch_pending_count(), before + 1);
    gateway_set_read_dispatch_pending_for_test(&master, true);
    ASSERT_EQ(gateway_read_dispatch_pending_count(), before + 1);
    ASSERT_GE(gateway_command_pressure_count(), before + 1);
    gateway_set_read_dispatch_pending_for_test(&master, false);
    ASSERT_EQ(gateway_read_dispatch_pending_count(), before);
    gateway_set_read_dispatch_pending_for_test(&master, false);
    ASSERT_EQ(gateway_read_dispatch_pending_count(), before);
    gateway_set_read_dispatch_pending_for_test(&master, true);
    ASSERT_EQ(gateway_read_dispatch_pending_count(), before + 1);
  }
  ASSERT_EQ(gateway_read_dispatch_pending_count(), before);
}

TEST_F(DriverTest, TestGatewayStatusSnapshotDefersPastCurrentReadDispatch) {
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway.cc");
  const auto schedule_pos = source.find("void gateway_schedule_status_response");
  const auto schedule_end = source.find("\nvoid gateway_handle_sys", schedule_pos);
  const auto status_branch = source.find("if (action == \"status\")");
  const auto status_branch_end = source.find("\n  if (action == \"pong\")", status_branch);

  ASSERT_NE(schedule_pos, std::string::npos);
  ASSERT_NE(schedule_end, std::string::npos);
  const auto schedule_source = source.substr(schedule_pos, schedule_end - schedule_pos);
  ASSERT_NE(schedule_source.find("add_walltime_event("), std::string::npos);
  ASSERT_NE(schedule_source.find("BackendEventPriority::kGateway"), std::string::npos);
  ASSERT_NE(schedule_source.find("gateway_has_master(fd)"), std::string::npos);
  ASSERT_NE(schedule_source.find("gateway_status_to_json(&status)"), std::string::npos);

  ASSERT_NE(status_branch, std::string::npos);
  ASSERT_NE(status_branch_end, std::string::npos);
  const auto status_source = source.substr(status_branch, status_branch_end - status_branch);
  ASSERT_NE(status_source.find("gateway_schedule_status_response(fd, msg)"),
            std::string::npos);
  ASSERT_EQ(status_source.find("gateway_status_to_json(&status)"), std::string::npos);
}

TEST_F(DriverTest, TestGatewayStatusWireIncludesMudlibRuntimeExtension) {
  constexpr int master_fd = 993;
  bufferevent *pair[2] = {nullptr, nullptr};

  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  ASSERT_EQ(bufferevent_enable(pair[0], EV_WRITE), 0);
  ASSERT_EQ(bufferevent_enable(pair[1], EV_READ), 0);
  ASSERT_NE(gateway_register_master_for_test(master_fd, pair[0]), nullptr);

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      master_fd,
      R"({"type":"sys","action":"status","ts":1700000000123456})"));

  auto *input = bufferevent_get_input(pair[1]);
  ASSERT_NE(input, nullptr);
  for (int attempt = 0; attempt < 16 && evbuffer_get_length(input) == 0;
       ++attempt) {
    event_base_loop(g_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK);
  }
  const auto framed_size = evbuffer_get_length(input);
  ASSERT_GT(framed_size, sizeof(uint32_t));
  std::string framed(framed_size, '\0');
  ASSERT_EQ(evbuffer_copyout(input, framed.data(), framed.size()),
            static_cast<int>(framed.size()));
  uint32_t network_length = 0;
  memcpy(&network_length, framed.data(), sizeof(network_length));
  const auto payload_length = ntohl(network_length);
  ASSERT_EQ(payload_length, framed.size() - sizeof(network_length));
  const auto response = nlohmann::json::parse(
      framed.substr(sizeof(network_length), payload_length));
  ASSERT_EQ(response.at("type"), "sys");
  ASSERT_EQ(response.at("action"), "status");
  ASSERT_EQ(response.at("ts"), 1700000000123456);
  const auto &data = response.at("data");
  ASSERT_EQ(data.at("client_sync_pending_entries"), 37);
  ASSERT_EQ(data.at("client_sync_chat_pending"), 31);
  ASSERT_EQ(data.at("client_sync_chat_fanout_pending_targets"), 23);
  ASSERT_EQ(data.at("client_sync_score_refresh_pending"), 11);

  gateway_remove_master_for_test(master_fd);
  pair[0] = nullptr;
  bufferevent_free(pair[1]);
  pair[1] = nullptr;
}

TEST_F(DriverTest, TestGatewayBufferedInputPressureRequiresACompleteFrame) {
  GatewayMaster master;
  const std::string payload = R"({"type":"hello"})";
  const auto size = static_cast<uint32_t>(payload.size());
  const auto frame = [&] {
    std::string encoded;
    encoded.push_back(static_cast<char>((size >> 24) & 0xff));
    encoded.push_back(static_cast<char>((size >> 16) & 0xff));
    encoded.push_back(static_cast<char>((size >> 8) & 0xff));
    encoded.push_back(static_cast<char>(size & 0xff));
    encoded += payload;
    return encoded;
  }();

  master.read_buffer.append(frame.data(), sizeof(uint32_t));
  master.read_buffer += payload.substr(0, payload.size() - 1);
  ASSERT_FALSE(gateway_master_has_buffered_input_for_test(&master));

  master.read_buffer += payload.substr(payload.size() - 1);
  ASSERT_TRUE(gateway_master_has_buffered_input_for_test(&master));

  GatewayMaster native_master;
  bufferevent *pair[2] = {nullptr, nullptr};
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  ASSERT_EQ(bufferevent_enable(pair[0], EV_WRITE), 0);
  ASSERT_EQ(bufferevent_enable(pair[1], EV_READ), 0);
  native_master.bev = pair[0];
  ASSERT_EQ(bufferevent_enable(pair[0], EV_READ), 0);
  ASSERT_EQ(bufferevent_enable(pair[1], EV_WRITE), 0);
  auto *native_input = bufferevent_get_input(pair[0]);
  ASSERT_NE(native_input, nullptr);
  const auto pump_until = [&](size_t expected) {
    for (int attempt = 0;
         attempt < 8 && evbuffer_get_length(native_input) < expected;
         ++attempt) {
      event_base_loop(g_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK);
    }
    ASSERT_EQ(evbuffer_get_length(native_input), expected);
  };

  ASSERT_EQ(bufferevent_write(pair[1], frame.data(), frame.size() - 1), 0);
  pump_until(frame.size() - 1);
  ASSERT_FALSE(gateway_master_has_buffered_input_for_test(&native_master));
  ASSERT_EQ(bufferevent_write(pair[1], frame.data() + frame.size() - 1, 1), 0);
  pump_until(frame.size());
  ASSERT_TRUE(gateway_master_has_buffered_input_for_test(&native_master));

  ASSERT_EQ(evbuffer_drain(native_input, frame.size()), 0);
  native_master.read_buffer.assign(frame.data(), 2);
  ASSERT_EQ(bufferevent_write(pair[1], frame.data() + 2, frame.size() - 2), 0);
  pump_until(frame.size() - 2);
  ASSERT_TRUE(gateway_master_has_buffered_input_for_test(&native_master));

  bufferevent_free(pair[1]);
}

TEST_F(DriverTest, TestGatewayMainQueueReadAdmissionUsesComposedHighLowWaterBackpressure) {
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway.cc");
  const auto header = read_source_file_for_test("../src/packages/gateway/gateway.h");
  const auto spec = read_source_file_for_test("../src/packages/gateway/gateway.spec");

  ASSERT_NE(source.find("constexpr long kGatewayMainQueueReadHighWatermark = 32"),
            std::string::npos);
  ASSERT_NE(source.find("constexpr long kGatewayMainQueueReadLowWatermark = 16"),
            std::string::npos);
  const auto admission_assert_pos = source.find(
      "static_assert(kGatewayReadFrameBudget <=");
  ASSERT_NE(admission_assert_pos, std::string::npos);
  ASSERT_NE(source.find("kGatewayMainQueueReadHighWatermark -",
                        admission_assert_pos),
            std::string::npos);
  ASSERT_NE(source.find("kGatewayMainQueueReadLowWatermark);",
                        admission_assert_pos),
            std::string::npos);
  ASSERT_NE(header.find("GATEWAY_READ_PAUSE_BUFFERED_BACKLOG"), std::string::npos);
  ASSERT_NE(header.find("GATEWAY_READ_PAUSE_MAIN_QUEUE"), std::string::npos);
  ASSERT_NE(header.find("uint8_t read_dispatch_pause_reasons{0}"), std::string::npos);

  const auto dispatch_pos = source.find("int gateway_dispatch_buffered_frames");
  const auto dispatch_end = source.find("\nvoid gateway_record_read_dispatch", dispatch_pos);
  ASSERT_NE(dispatch_pos, std::string::npos);
  ASSERT_NE(dispatch_end, std::string::npos);
  const auto dispatch_source = source.substr(dispatch_pos, dispatch_end - dispatch_pos);
  ASSERT_NE(dispatch_source.find("gateway_main_queue_admission_budget(budget)"),
            std::string::npos);
  ASSERT_NE(dispatch_source.find("gateway_pause_all_reads_for_main_queue_pressure()"),
            std::string::npos);

  const auto deferred_pos = source.find("void gateway_drain_owner_main_tasks_later");
  const auto deferred_end = source.find("\nvoid gateway_service_admitted_receive_tasks",
                                        deferred_pos);
  ASSERT_NE(deferred_pos, std::string::npos);
  ASSERT_NE(deferred_end, std::string::npos);
  const auto deferred_source = source.substr(deferred_pos, deferred_end - deferred_pos);
  ASSERT_NE(deferred_source.find(
                "gateway_resume_main_queue_reads_if_below_low_watermark();"),
            std::string::npos);

  const auto service_pos = source.find("void gateway_service_admitted_receive_tasks");
  const auto service_end = source.find("\nbool gateway_apply_receive", service_pos);
  ASSERT_NE(service_pos, std::string::npos);
  ASSERT_NE(service_end, std::string::npos);
  const auto service_source = source.substr(service_pos, service_end - service_pos);
  ASSERT_NE(service_source.find(
                "gateway_resume_main_queue_reads_if_below_low_watermark();"),
            std::string::npos);

  ASSERT_NE(source.find("int64_t gateway_main_queue_pending_count()"), std::string::npos);
  ASSERT_EQ(source.find("long gateway_main_queue_pending_count()"), std::string::npos);
  ASSERT_NE(header.find("int64_t gateway_main_queue_pending_count();"), std::string::npos);
  ASSERT_EQ(header.find("long gateway_main_queue_pending_count();"), std::string::npos);
  ASSERT_NE(source.find(
                "std::chrono::nanoseconds gateway_deferred_main_drain_wall_budget(int64_t backlog)"),
            std::string::npos);
  ASSERT_EQ(source.find("gateway_deferred_main_drain_wall_budget(long backlog)"),
            std::string::npos);
  ASSERT_NE(source.find("std::max<int64_t>(0, vm_owner_main_queue_total_depth())"),
            std::string::npos);
  ASSERT_NE(source.find("std::max<int64_t>(0, drain_result.remaining_main_tasks)"),
            std::string::npos);
  ASSERT_EQ(source.find("std::max<long>(0, vm_owner_main_queue_total_depth())"),
            std::string::npos);
  ASSERT_EQ(source.find("std::max<long>(0, drain_result.remaining_main_tasks)"),
            std::string::npos);
  ASSERT_NE(spec.find("int gateway_main_queue_pending();"), std::string::npos);
  ASSERT_NE(source.find("LPC_INT gateway_buffered_input_pending_count()"),
            std::string::npos);
  ASSERT_NE(spec.find("int gateway_buffered_input_pending();"),
            std::string::npos);
  ASSERT_NE(source.find("bufferevent_get_input(master->bev)"),
            std::string::npos);
  ASSERT_NE(source.find("evbuffer_copyout("), std::string::npos);
  ASSERT_NE(source.find("static_cast<ev_ssize_t>"), std::string::npos);
  const auto efun_pos = source.find("void f_gateway_main_queue_pending()");
  const auto efun_end = source.find("\nvoid f_gateway_status()", efun_pos);
  ASSERT_NE(efun_pos, std::string::npos);
  ASSERT_NE(efun_end, std::string::npos);
  ASSERT_NE(source.substr(efun_pos, efun_end - efun_pos)
                .find("gateway_main_queue_pending_count()"),
            std::string::npos);

  const auto buffered_efun_pos = source.find("void f_gateway_buffered_input_pending()");
  ASSERT_NE(buffered_efun_pos, std::string::npos);
  ASSERT_NE(source.substr(buffered_efun_pos)
                .find("gateway_buffered_input_pending_count()"),
            std::string::npos);

  const auto pressure_pos =
      source.find("LPC_INT gateway_command_pressure_count()");
  const auto pressure_end = source.find("\nint64_t gateway_main_queue_pending_count", pressure_pos);
  ASSERT_NE(pressure_pos, std::string::npos);
  ASSERT_NE(pressure_end, std::string::npos);
  ASSERT_EQ(source.substr(pressure_pos, pressure_end - pressure_pos)
                .find("gateway_main_queue_pending_count()"),
            std::string::npos);
}

TEST_F(DriverTest, TestGatewayReadCallbackYieldsAfterEveryCompleteFrame) {
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway.cc");
  const auto header = read_source_file_for_test("../src/packages/gateway/gateway.h");

  ASSERT_NE(source.find("constexpr int kGatewayReadFrameBudget = 1"), std::string::npos);
  ASSERT_NE(source.find("constexpr auto kGatewayReadContinuationDelay = std::chrono::milliseconds(1)"),
            std::string::npos);
  ASSERT_NE(header.find("bool read_dispatch_scheduled{false}"), std::string::npos);
  ASSERT_NE(header.find("bool read_dispatch_input_paused{false}"), std::string::npos);
  ASSERT_NE(source.find("gateway_dispatch_buffered_frames(master, kGatewayReadFrameBudget)"),
            std::string::npos);
  ASSERT_NE(source.find("gateway_schedule_buffered_read(master->fd)"), std::string::npos);
  ASSERT_NE(source.find("g_gateway_masters.find(fd)"), std::string::npos);

  const auto schedule_pos = source.find("void gateway_schedule_buffered_read(int fd) {");
  const auto readcb_pos = source.find("void gateway_readcb", schedule_pos);
  ASSERT_NE(schedule_pos, std::string::npos);
  ASSERT_NE(readcb_pos, std::string::npos);
  const auto schedule_source = source.substr(schedule_pos, readcb_pos - schedule_pos);
  ASSERT_NE(schedule_source.find("gateway_pause_buffered_read(master);"), std::string::npos);
  ASSERT_NE(schedule_source.find("gateway_resume_buffered_read(scheduled_master);"),
            std::string::npos);
  ASSERT_NE(schedule_source.find("add_walltime_event("), std::string::npos);
  ASSERT_NE(schedule_source.find("kGatewayReadContinuationDelay"),
            std::string::npos);
  ASSERT_EQ(schedule_source.find("std::chrono::milliseconds(0)"),
            std::string::npos);
  ASSERT_NE(schedule_source.find("BackendEventPriority::kGateway"),
            std::string::npos);
  ASSERT_NE(source.find("void gateway_pause_buffered_read"), std::string::npos);
  ASSERT_NE(source.find("bufferevent_disable(master->bev, EV_READ)"), std::string::npos);
  ASSERT_NE(source.find("void gateway_resume_buffered_read"), std::string::npos);
  ASSERT_NE(source.find("bufferevent_enable(master->bev, EV_READ)"), std::string::npos);
}

TEST_F(DriverTest, TestGatewayBufferedReadParserUsesCursorInsteadOfPerFrameFrontErase) {
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway.cc");
  const auto header = read_source_file_for_test("../src/packages/gateway/gateway.h");
  const auto parser_pos = source.find("int gateway_dispatch_buffered_frames");
  const auto parser_end = source.find("\nvoid gateway_record_read_dispatch", parser_pos);

  ASSERT_NE(parser_pos, std::string::npos);
  ASSERT_NE(parser_end, std::string::npos);
  ASSERT_NE(header.find("size_t read_buffer_offset{0}"), std::string::npos);
  const auto parser_source = source.substr(parser_pos, parser_end - parser_pos);
  ASSERT_NE(parser_source.find("read_buffer_offset"), std::string::npos);
  ASSERT_NE(parser_source.find("gateway_compact_read_buffer"), std::string::npos);
  ASSERT_EQ(parser_source.find("read_buffer.erase(0, sizeof(uint32_t) + frame_len)"),
            std::string::npos);
}

TEST_F(DriverTest, TestGatewayBufferedReadParserHonorsFrameBudgetAndOrder) {
  GatewayMaster master;
  master.fd = -1;
  const auto append_frame = [&master](const std::string &payload) {
    const auto size = static_cast<uint32_t>(payload.size());
    master.read_buffer.push_back(static_cast<char>((size >> 24) & 0xff));
    master.read_buffer.push_back(static_cast<char>((size >> 16) & 0xff));
    master.read_buffer.push_back(static_cast<char>((size >> 8) & 0xff));
    master.read_buffer.push_back(static_cast<char>(size & 0xff));
    master.read_buffer += payload;
  };
  for (int index = 0; index < 5; index++) {
    append_frame(R"({"type":"hello"})");
  }
  const auto buffered_size = master.read_buffer.size();

  ASSERT_EQ(gateway_dispatch_buffered_frames_for_test(&master, 2), 2);
  ASSERT_EQ(master.messages_received, 2u);
  ASSERT_GT(master.read_buffer_offset, 0u);
  ASSERT_EQ(master.read_buffer.size(), buffered_size);
  ASSERT_FALSE(master.read_buffer.empty());
  ASSERT_EQ(gateway_dispatch_buffered_frames_for_test(&master, 2), 2);
  ASSERT_EQ(master.messages_received, 4u);
  ASSERT_GT(master.read_buffer_offset, 0u);
  ASSERT_EQ(master.read_buffer.size(), buffered_size);
  ASSERT_FALSE(master.read_buffer.empty());
  ASSERT_EQ(gateway_dispatch_buffered_frames_for_test(&master, 2), 1);
  ASSERT_EQ(master.messages_received, 5u);
  ASSERT_EQ(master.read_buffer_offset, 0u);
  ASSERT_TRUE(master.read_buffer.empty());
}

TEST_F(DriverTest, TestGatewayBufferedReadParserPreservesPartialFramesAndChargesBadJson) {
  const auto frame_for = [](const std::string &payload) {
    const auto size = static_cast<uint32_t>(payload.size());
    std::string frame;
    frame.push_back(static_cast<char>((size >> 24) & 0xff));
    frame.push_back(static_cast<char>((size >> 16) & 0xff));
    frame.push_back(static_cast<char>((size >> 8) & 0xff));
    frame.push_back(static_cast<char>(size & 0xff));
    frame += payload;
    return frame;
  };

  GatewayMaster master;
  master.fd = -1;
  const auto complete_tail = frame_for(R"({"type":"hello"})");
  master.read_buffer = frame_for(R"({"type":"hello"})") + frame_for("{") +
                       complete_tail.substr(0, 7);

  ASSERT_EQ(gateway_dispatch_buffered_frames_for_test(&master, 2), 2);
  ASSERT_EQ(master.messages_received, 1u);
  ASSERT_EQ(master.read_buffer, complete_tail.substr(0, 7));

  master.read_buffer += complete_tail.substr(7);
  ASSERT_EQ(gateway_dispatch_buffered_frames_for_test(&master, 2), 1);
  ASSERT_EQ(master.messages_received, 2u);
  ASSERT_TRUE(master.read_buffer.empty());

  GatewayMaster invalid_length;
  invalid_length.fd = -1;
  invalid_length.read_buffer.assign(sizeof(uint32_t), '\0');
  ASSERT_EQ(gateway_dispatch_buffered_frames_for_test(&invalid_length, 2), -1);
}

TEST_F(DriverTest, TestGatewayBufferedReadParserRejectsConfiguredOversizeFrame) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() { g_gateway_max_packet_size = original; }
  } packet_size_guard;

  g_gateway_max_packet_size = 1024;
  GatewayMaster master;
  master.fd = -1;
  const auto rejected_before =
      g_gateway_runtime_counters.read_dispatch_frame_length_rejected.load(
          std::memory_order_relaxed);
  const uint32_t network_length = htonl(1025);
  master.read_buffer.assign(
      reinterpret_cast<const char *>(&network_length), sizeof(network_length));

  ASSERT_EQ(gateway_dispatch_buffered_frames_for_test(&master, 2), -1);
  ASSERT_EQ(
      g_gateway_runtime_counters.read_dispatch_frame_length_rejected.load(
          std::memory_order_relaxed),
      rejected_before + 1);
}

TEST_F(DriverTest, TestGatewayReadBufferRejectsAggregateOverflowBeforeCopy) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() { g_gateway_max_packet_size = original; }
  } packet_size_guard;

  g_gateway_max_packet_size = 1024;
  const auto limit = gateway_read_buffer_limit_for_test();
  ASSERT_EQ(limit, 1024u + sizeof(uint32_t));
  GatewayMaster master;
  const std::string bounded(limit, 'x');
  const auto rejected_before =
      g_gateway_runtime_counters.read_dispatch_buffer_limit_rejected.load(
          std::memory_order_relaxed);

  ASSERT_TRUE(gateway_append_read_bytes_for_test(
      &master, bounded.data(), bounded.size()));
  ASSERT_EQ(master.read_buffer.size(), limit);
  ASSERT_FALSE(gateway_append_read_bytes_for_test(&master, "x", 1));
  ASSERT_EQ(master.read_buffer.size(), limit);
  ASSERT_EQ(
      g_gateway_runtime_counters.read_dispatch_buffer_limit_rejected.load(
          std::memory_order_relaxed),
      rejected_before + 1);
}

TEST_F(DriverTest, TestGatewayMasterReadWatermarkMatchesBoundedAggregateBuffer) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() { g_gateway_max_packet_size = original; }
  } packet_size_guard;

  g_gateway_max_packet_size = 2048;
  constexpr int master_fd = 1707;
  bufferevent *pair[2] = {nullptr, nullptr};
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  ASSERT_NE(gateway_register_master_for_test(master_fd, pair[0]), nullptr);
  size_t lowmark = 0;
  size_t highmark = 0;

  ASSERT_EQ(bufferevent_getwatermark(pair[0], EV_READ, &lowmark, &highmark),
            0);
  ASSERT_EQ(lowmark, 0u);
  ASSERT_EQ(highmark, gateway_read_buffer_limit_for_test());

  gateway_remove_master_for_test(master_fd);
  pair[0] = nullptr;
  bufferevent_free(pair[1]);
}

TEST_F(DriverTest, TestGatewayPacketSizeRuntimeOverrideHonorsWireHardLimit) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() {
      if (!gateway_set_max_packet_size_for_test(original)) {
        g_gateway_max_packet_size = original;
      }
    }
  } packet_size_guard;

  const auto hard_limit = gateway_packet_size_hard_limit_for_test();
  ASSERT_EQ(hard_limit, 16u * 1024u * 1024u);
  const auto original = g_gateway_max_packet_size;

  ASSERT_FALSE(gateway_set_max_packet_size_for_test(hard_limit + 1));
  ASSERT_EQ(g_gateway_max_packet_size, original);
  ASSERT_TRUE(gateway_set_max_packet_size_for_test(4096));
  ASSERT_EQ(g_gateway_max_packet_size, 4096u);
  ASSERT_EQ(gateway_read_buffer_limit_for_test(), 4096u + sizeof(uint32_t));
}

TEST_F(DriverTest, TestGatewayMasterListenerEnablesTcpNoDelayAndGatewayPriority) {
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway.cc");
  const auto listener_pos = source.find("void gateway_listener_cb");
  ASSERT_NE(listener_pos, std::string::npos);
  const auto listener_end = source.find("\nvoid gateway_listener_error_cb", listener_pos);
  ASSERT_NE(listener_end, std::string::npos);

  const auto listener_source = source.substr(listener_pos, listener_end - listener_pos);
  ASSERT_NE(source.find("setsockopt(fd, IPPROTO_TCP, TCP_NODELAY"), std::string::npos);
  ASSERT_NE(listener_source.find("gateway_enable_master_tcp_nodelay(fd)"), std::string::npos);
  ASSERT_NE(listener_source.find("bufferevent_priority_set("), std::string::npos);
  ASSERT_NE(listener_source.find("BackendEventPriority::kGateway"),
            std::string::npos);
}

TEST_F(DriverTest, TestGatewayListenerDescriptorsCloseOnExec) {
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway.cc");
  const auto listen_pos = source.find("int gateway_listen_internal");
  ASSERT_NE(listen_pos, std::string::npos);
  const auto listen_end = source.find("\nnamespace {", listen_pos);
  ASSERT_NE(listen_end, std::string::npos);

  const auto listen_source = source.substr(listen_pos, listen_end - listen_pos);
  ASSERT_NE(listen_source.find("LEV_OPT_CLOSE_ON_EXEC"), std::string::npos);
}

TEST_F(DriverTest, TestGatewayRejectsExternalBindWithoutAuthenticatedTransport) {
  ASSERT_FALSE(gateway_external_bind_allowed_for_test());
  const auto rejected_before =
      g_gateway_runtime_counters.external_bind_rejected.load(
          std::memory_order_relaxed);
  ASSERT_EQ(gateway_listen_internal(65535, 1), 0);
  ASSERT_EQ(g_gateway_runtime_counters.external_bind_rejected.load(
                std::memory_order_relaxed),
            rejected_before + 1);
}

TEST_F(DriverTest, TestMudPortRejectsInvalidWirePayloadLengths) {
  char unaligned_header[5] = {0};
  auto* header = unaligned_header + 1;
  size_t payload_length = 0;

  header[0] = 0x00;
  header[1] = 0x0f;
  header[2] = static_cast<char>(0xff);
  header[3] = static_cast<char>(0xfb);
  ASSERT_TRUE(decode_mud_port_payload_length_for_test(header, 4, &payload_length));
  ASSERT_EQ(payload_length, static_cast<size_t>(MAX_TEXT - 5));

  header[0] = static_cast<char>(0x80);
  header[1] = 0x00;
  header[2] = 0x00;
  header[3] = 0x00;
  ASSERT_FALSE(decode_mud_port_payload_length_for_test(header, 4, &payload_length));

  header[0] = 0x00;
  header[1] = 0x10;
  header[2] = 0x00;
  header[3] = 0x00;
  ASSERT_FALSE(decode_mud_port_payload_length_for_test(header, 4, &payload_length));

  std::memset(header, 0, 4);
  ASSERT_FALSE(decode_mud_port_payload_length_for_test(header, 4, &payload_length));
  ASSERT_FALSE(decode_mud_port_payload_length_for_test(header, 3, &payload_length));
}

TEST_F(DriverTest, TestAsciiReadSpaceMatchesRemainingInputRoom) {
  // TRANSPORT-ASCII-1: the ASCII path accumulated into ip->text while sizing
  // its read from a MAX_TEXT sized scratch buffer, so a client that withheld a
  // newline could grow text_end and have the next read overflow interactive_t.
  // The read size must always match the room left at text_end.
  interactive_t ip{};
  ip.text_start = 0;
  ip.text_end = 100;
  int space = comm_reserve_input_space_for_test(&ip, sizeof(ip.text) / 16);
  ASSERT_EQ(space, static_cast<int>(sizeof(ip.text)) - ip.text_end);
  ASSERT_EQ(space, MAX_TEXT - 100);

  // With a full buffer and a consumed prefix, the prefix is moved down and the
  // answer still matches the room at the new text_end.
  ip.text_start = 1000000;
  ip.text_end = MAX_TEXT;
  space = comm_reserve_input_space_for_test(&ip, sizeof(ip.text) / 16);
  ASSERT_EQ(ip.text_start, 0);
  ASSERT_EQ(ip.text_end, MAX_TEXT - 1000000);
  ASSERT_EQ(space, static_cast<int>(sizeof(ip.text)) - ip.text_end);
  ASSERT_EQ(ip.iflags & SKIP_COMMAND, 0u);

  // A full buffer with nothing consumable is dropped instead of read into.
  ip.text_start = 0;
  ip.text_end = MAX_TEXT;
  space = comm_reserve_input_space_for_test(&ip, sizeof(ip.text) / 16);
  ASSERT_EQ(space, MAX_TEXT);
  ASSERT_EQ(ip.text_end, 0);
  ASSERT_NE(ip.iflags & SKIP_COMMAND, 0u);
}

TEST_F(DriverTest, TestInputAppendNeverWritesPastTheBuffer) {
  interactive_t ip{};
  std::vector<unsigned char> data(64, 'x');

  ip.text_end = 4;
  ASSERT_EQ(comm_append_input_for_test(&ip, data.data(), 64), 64);
  ASSERT_EQ(ip.text_end, 68);

  // The copy itself is bounded, so a mis-sized read cannot clip the fields
  // that follow ip->text inside interactive_t.
  ip.text_end = MAX_TEXT - 4;
  ASSERT_EQ(comm_append_input_for_test(&ip, data.data(), 64), 4);
  ASSERT_EQ(ip.text_end, MAX_TEXT);
  ASSERT_EQ(ip.text[MAX_TEXT - 1], 'x');
  ASSERT_EQ(comm_append_input_for_test(&ip, data.data(), 64), 0);
  ASSERT_EQ(ip.text_end, MAX_TEXT);
}

// move_object() lazily calls try_reset() on the destination just before
// linking the moved item into it. reset() is arbitrary LPC and can
// self-destruct the destination as a perfectly ordinary side effect (no
// error() involved, so safe_apply() inside try_reset() doesn't catch it).
// move_object() must notice that and not link the item into an object that is
// no longer live.
TEST_F(DriverTest, TestCleanUpDeadlineSweepAppliesAndRevertsToOneShot) {
  // The backend sweep is scheduled 5 minutes out; drop the queued ticks so a
  // manually driven sweep is the only one that can run, then restore the
  // queue on the way out.
  clear_tick_events();
  struct TickQueueGuard {
    ~TickQueueGuard() { clear_tick_events(); }
  } tick_queue_guard;

  ASSERT_GT(CONFIG_INT(__TIME_TO_CLEAN_UP__), 0) << "sweep requires a clean_up interval";

  current_object = master_ob;
  auto *ob = load_object_for_test("clone/clean_up_deadline");
  ASSERT_NE(ob, nullptr);
  ASSERT_TRUE(ob->flags & O_WILL_CLEAN_UP)
      << "an object that defines clean_up() is queried by the sweep";
  // Control: an object whose deadline has not passed yet must survive the
  // same sweep (a future deadline is not "due" and the idle-time rule does
  // not apply to a freshly loaded object).
  auto *future = load_object_for_test("clone/clean_up_deadline");
  ASSERT_NE(future, nullptr);
  future->next_cleanup = current_gametick() + 1000;

  // An explicit deadline overrides the idle-time rule; make it due.
  ob->next_cleanup = current_gametick() + 1;
  advance_gametick_for_test(2);
  look_for_objects_to_swap_for_test();

  // clean_up() ran (the fixture destructs itself, which is the observable),
  // and the deadline reverted to the idle rule (one-shot).
  EXPECT_TRUE(ob->flags & O_DESTRUCTED) << "the due deadline must invoke clean_up()";
  EXPECT_EQ(ob->next_cleanup, 0) << "one-shot: a fired deadline reverts to the idle rule";
  EXPECT_FALSE(future->flags & O_DESTRUCTED) << "a not-yet-due deadline must not fire";
  EXPECT_EQ(future->next_cleanup, current_gametick() + 1000 - 2);

  // A second sweep must not re-fire anything for the already-dead object.
  look_for_objects_to_swap_for_test();
  EXPECT_TRUE(ob->flags & O_DESTRUCTED);
}

TEST_F(DriverTest, TestMoveObjectDestructDuringReset) {
  auto saved_lazy_resets = CONFIG_INT(__RC_LAZY_RESETS__);
  auto saved_no_resets = CONFIG_INT(__RC_NO_RESETS__);
  CONFIG_INT(__RC_LAZY_RESETS__) = 1;
  CONFIG_INT(__RC_NO_RESETS__) = 0;

  current_object = master_ob;
  object_t *dest = nullptr;
  object_t *item = nullptr;

  error_context_t econ{};
  save_context(&econ);
  try {
    dest = load_object_for_test("clone/move_object_reset_dest");
    item = load_object_for_test("clone/move_object_item");
    ASSERT_NE(dest, nullptr);
    ASSERT_NE(item, nullptr);
    // try_reset()'s "is a reset due" check is `next_reset < gametick`; the
    // harness never pumps the backend loop, so push the tick clock past the
    // deadline and clear the already-reset flag to make it fire inside
    // move_object().
    dest->next_reset = 1;
    advance_gametick_for_test(2);
    dest->flags &= ~O_RESET_STATE;
    move_object(item, dest);
  } catch (...) {
    restore_context(&econ);
  }
  pop_context(&econ);

  CONFIG_INT(__RC_LAZY_RESETS__) = saved_lazy_resets;
  CONFIG_INT(__RC_NO_RESETS__) = saved_no_resets;

  // dest self-destructed out of reset(): the item must be left unlinked
  // instead of becoming inventory of a dead object.
  ASSERT_NE(dest, nullptr);
  EXPECT_TRUE(dest->flags & O_DESTRUCTED);
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->super, nullptr);
  EXPECT_EQ(dest->contains, nullptr);
}

TEST_F(DriverTest, TestReadBytesPreservesLpc64BitOffsets) {
  constexpr LPC_INT kLargeOffset = static_cast<LPC_INT>(1) << 32;
  const char* relative_path = "log/read-bytes-large-offset.bin";
  const char* mudlib_path = "/log/read-bytes-large-offset.bin";
  PathCleanupGuard guard{relative_path};
  ScopedCurrentObjectAsMaster master_scope;

  std::ofstream file(relative_path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(file.is_open());
  file.seekp(static_cast<std::streamoff>(kLargeOffset));
  file.put('Z');
  file.close();
  ASSERT_TRUE(file.good());

  int length = 0;
  char* contents = read_bytes(mudlib_path, kLargeOffset, 1, &length);
  ASSERT_NE(contents, nullptr);
  EXPECT_EQ(length, 1);
  EXPECT_EQ(contents[0], 'Z');
  FREE_MSTR(contents);
}

TEST_F(DriverTest, TestWriteBytesPreservesLpc64BitOffsets) {
  constexpr LPC_INT kLargeOffset = static_cast<LPC_INT>(1) << 32;
  const char* relative_path = "log/write-bytes-large-offset.bin";
  const char* mudlib_path = "/log/write-bytes-large-offset.bin";
  PathCleanupGuard guard{relative_path};
  ScopedCurrentObjectAsMaster master_scope;

  std::ofstream file(relative_path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(file.is_open());
  file.seekp(static_cast<std::streamoff>(kLargeOffset));
  file.put('\0');
  file.close();
  ASSERT_TRUE(file.good());

  const char marker = 'W';
  ASSERT_EQ(write_bytes(mudlib_path, kLargeOffset, &marker, 1), 1);

  int length = 0;
  char* contents = read_bytes(mudlib_path, kLargeOffset, 1, &length);
  ASSERT_NE(contents, nullptr);
  EXPECT_EQ(length, 1);
  EXPECT_EQ(contents[0], marker);
  FREE_MSTR(contents);
}

TEST_F(DriverTest, TestFileSizePreservesLpc64BitSize) {
  constexpr LPC_INT kLargeSize = static_cast<LPC_INT>(1) << 32;
  const char* relative_path = "log/file-size-large.bin";
  const char* mudlib_path = "/log/file-size-large.bin";
  PathCleanupGuard guard{relative_path};
  ScopedCurrentObjectAsMaster master_scope;

  std::ofstream file(relative_path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(file.is_open());
  file.seekp(static_cast<std::streamoff>(kLargeSize - 1));
  file.put('\0');
  file.close();
  ASSERT_TRUE(file.good());

  ASSERT_EQ(file_size(mudlib_path), kLargeSize);
}

#ifdef PACKAGE_SOCKETS
TEST_F(DriverTest, TestSocketGetOptionRetainsStringAndStackSentinel) {
  ScopedCurrentObjectAsMaster master_scope;
  SocketPairGuard sockets;
  sockets.first = socket_create(STREAM, nullptr, nullptr);
  sockets.second = socket_create(STREAM, nullptr, nullptr);
  ASSERT_GE(sockets.first, 0);
  ASSERT_GE(sockets.second, 0);
  copy_and_push_string("retained.example");
  assign_svalue(&lpc_socks_get(sockets.second)->options[SO_TLS_SNI_HOSTNAME], sp);
  pop_stack();

  auto* base = sp;
  error_context_t context{};
  save_context(&context);
  try {
    push_number(sockets.first);
    push_number(sockets.second);
    push_number(SO_TLS_SNI_HOSTNAME);
    f_socket_get_option();
    EXPECT_EQ(sp, base + 2);
    EXPECT_EQ(base[1].type, T_NUMBER);
    EXPECT_EQ(base[1].u.number, sockets.first);
    EXPECT_EQ(sp->type, T_STRING);
    EXPECT_EQ(socket_close(sockets.second, 0), EESUCCESS);
    sockets.second = -1;
    if (sp->type == T_STRING) {
      EXPECT_STREQ(sp->u.string, "retained.example");
    }
  } catch (...) {
    ADD_FAILURE() << "valid getter call raised an error";
  }
  restore_context(&context);
  pop_context(&context);
  EXPECT_EQ(sp, base);
}

TEST_F(DriverTest, TestSocketGetOptionNumbersIgnoreStackSentinel) {
  ScopedCurrentObjectAsMaster master_scope;
  SocketPairGuard sockets;
  sockets.first = socket_create(STREAM, nullptr, nullptr);
  ASSERT_GE(sockets.first, 0);
  constexpr LPC_INT kSentinel = 0x12345678;
  auto* base = sp;
  for (const int value : {0, 1, 0, 1}) {
    lpc_socks_get(sockets.first)->options[SO_TLS_VERIFY_PEER].u.number = value;
    error_context_t context{};
    save_context(&context);
    try {
      push_number(kSentinel);
      push_number(sockets.first);
      push_number(SO_TLS_VERIFY_PEER);
      f_socket_get_option();
      EXPECT_EQ(sp, base + 2);
      EXPECT_EQ(base[1].type, T_NUMBER);
      EXPECT_EQ(base[1].u.number, kSentinel);
      EXPECT_EQ(sp->type, T_NUMBER);
      if (sp->type == T_NUMBER) {
        EXPECT_EQ(sp->u.number, value);
        EXPECT_EQ(sp->subtype, 0);
      }
    } catch (...) {
      ADD_FAILURE() << "valid getter call raised an error";
    }
    restore_context(&context);
    pop_context(&context);
    EXPECT_EQ(sp, base);
  }
}
#endif

#ifndef _WIN32
TEST_F(DriverTest, TestSocketAcceptMarksAcceptedDescriptorCloseOnExec) {
  struct FixtureGuard {
    int listener = -1;
    int accepted = -1;
    evutil_socket_t client = -1;
    int accidental_fd = -1;
    int accidental_fd_flags = -1;
    ~FixtureGuard() {
      ScopedCurrentObjectAsMaster inner;
      if (accepted >= 0 && lpc_socks_get(accepted)->state != STATE_CLOSED) {
        socket_close(accepted, 0);
      }
      if (listener >= 0 && lpc_socks_get(listener)->state != STATE_CLOSED) {
        socket_close(listener, 0);
      }
      if (client >= 0) {
        evutil_closesocket(client);
      }
      if (accidental_fd_flags >= 0) {
        fcntl(accidental_fd, F_SETFD, accidental_fd_flags);
      }
    }
  } guard;
  ScopedCurrentObjectAsMaster master_scope;

  guard.listener = socket_create(STREAM, nullptr, nullptr);
  ASSERT_GE(guard.listener, 0);
  ASSERT_EQ(socket_bind(guard.listener, 0, "127.0.0.1 0"), EESUCCESS);
  ASSERT_EQ(socket_listen(guard.listener, nullptr), EESUCCESS);

  auto* listener = lpc_socks_get(guard.listener);
  sockaddr_storage address{};
  socklen_t address_length = sizeof(address);
  ASSERT_EQ(getsockname(listener->fd, reinterpret_cast<sockaddr*>(&address),
                        &address_length),
            0);

  guard.client = socket(address.ss_family, SOCK_STREAM, 0);
  ASSERT_GE(guard.client, 0);
  ASSERT_EQ(connect(guard.client, reinterpret_cast<sockaddr*>(&address), address_length), 0);

  pollfd listener_ready{listener->fd, POLLIN, 0};
  ASSERT_EQ(poll(&listener_ready, 1, 1000), 1);
  ASSERT_NE(listener_ready.revents & POLLIN, 0);

  guard.accidental_fd = guard.listener;
  guard.accidental_fd_flags = fcntl(guard.accidental_fd, F_GETFD);
  guard.accepted = socket_accept(guard.listener, nullptr, nullptr);
  ASSERT_GE(guard.accepted, 0);

  const auto accepted_fd = lpc_socks_get(guard.accepted)->fd;
  const auto descriptor_flags = fcntl(accepted_fd, F_GETFD);
  ASSERT_NE(descriptor_flags, -1);
  ASSERT_NE(descriptor_flags & FD_CLOEXEC, 0);
}
#endif

TEST_F(DriverTest, TestExternalSpawnFailureDoesNotPublishSocket) {
#ifdef _WIN32
  char invalid_command[] = "Z:\\this\\path\\does\\not\\exist\\fluffos-external-test.exe";
#else
  char invalid_command[] = "/this/path/does/not/exist/fluffos-external-test";
#endif
  ExternalCommandGuard guard{external_cmd[0]};
  ScopedCurrentObjectAsMaster master_scope;
  external_cmd[0] = invalid_command;

  std::vector<bool> closed_before(lpc_socks_num(), false);
  size_t active_before = 0;
  for (int i = 0; i < lpc_socks_num(); ++i) {
    closed_before[i] = lpc_socks_get(i)->state == STATE_CLOSED;
    if (!closed_before[i]) {
      ++active_before;
    }
  }

  svalue_t args = const0u;
  args.type = T_STRING;
  args.subtype = STRING_SHARED;
  args.u.string = const_cast<char*>("");
  const auto result = external_start(0, &args, nullptr, nullptr, nullptr);

  size_t active_after = 0;
  for (int i = 0; i < lpc_socks_num(); ++i) {
    auto* socket = lpc_socks_get(i);
    if (socket->state != STATE_CLOSED) {
      ++active_after;
      if (i >= static_cast<int>(closed_before.size()) || closed_before[i]) {
        ASSERT_EQ(socket_close(i, 0), EESUCCESS);
      }
    }
  }

  ASSERT_LT(result, 0);
  ASSERT_EQ(active_after, active_before);
}

TEST_F(DriverTest, TestExternalEmptyCommandDoesNotPublishSocket) {
  ExternalCommandGuard guard{external_cmd[0]};
  ScopedCurrentObjectAsMaster master_scope;
  external_cmd[0] = nullptr;

  std::vector<bool> closed_before(lpc_socks_num(), false);
  size_t active_before = 0;
  for (int i = 0; i < lpc_socks_num(); ++i) {
    closed_before[i] = lpc_socks_get(i)->state == STATE_CLOSED;
    if (!closed_before[i]) {
      ++active_before;
    }
  }

  svalue_t args = const0u;
  args.type = T_STRING;
  args.subtype = STRING_SHARED;
  args.u.string = const_cast<char*>("");
  const auto result = external_start(0, &args, nullptr, nullptr, nullptr);

  size_t active_after = 0;
  for (int i = 0; i < lpc_socks_num(); ++i) {
    auto* socket = lpc_socks_get(i);
    if (socket->state != STATE_CLOSED) {
      ++active_after;
      if (i >= static_cast<int>(closed_before.size()) || closed_before[i]) {
        ASSERT_EQ(socket_close(i, 0), EESUCCESS);
      }
    }
  }

  ASSERT_LT(result, 0);
  ASSERT_EQ(active_after, active_before);
}

TEST_F(DriverTest, TestExternalPromiseHandleAndStart) {
  clear_tick_events();
  struct FixtureGuard {
    object_t* object = nullptr;
    promise_t* handle_promise = nullptr;
    promise_t* start_promise = nullptr;
#ifndef _WIN32
    promise_t* cat_promise = nullptr;
#endif
    ~FixtureGuard() {
      clear_tick_events();
      if (object != nullptr) {
        destruct_object_for_test(object);
      }
      if (handle_promise != nullptr) {
        free_promise(handle_promise);
      }
      if (start_promise != nullptr) {
        free_promise(start_promise);
      }
#ifndef _WIN32
      if (cat_promise != nullptr) {
        free_promise(cat_promise);
      }
#endif
      vm_apply_return_clear();
    }
  } guard;

  guard.object = load_object_for_test("single/tests/efuns/external_promise");
  ASSERT_NE(guard.object, nullptr);

  auto invoke = [&](const char* method) -> promise_t* {
    auto* result = safe_apply(method, guard.object, 0, ORIGIN_DRIVER);
    if (result == nullptr || result->type != T_PROMISE) {
      vm_apply_return_clear();
      return nullptr;
    }
    auto* promise = result->u.prom;
    promise->ref++;
    vm_apply_return_clear();
    return promise;
  };

  auto drain = [&](promise_t* promise) {
    for (int pass = 0; pass < 256 && promise->state == PROMISE_PENDING; pass++) {
      if (tick_event_queue_size_for_test() != 0) {
        ASSERT_GT(run_tick_events_for_test(), 0u);
      }
      if (walltime_event_queue_size_for_test() != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        ASSERT_EQ(event_base_loop(g_event_base, EVLOOP_NONBLOCK), 0);
      }
      if (tick_event_queue_size_for_test() == 0 &&
          walltime_event_queue_size_for_test() == 0 &&
          promise->state == PROMISE_PENDING) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
    }
    ASSERT_NE(promise->state, PROMISE_PENDING);
  };

#ifdef _WIN32
  const char* handle_method = "run_handle_windows";
  const char* start_method = "run_start_windows";
#else
  const char* handle_method = "run_handle_posix";
  const char* start_method = "run_start_posix";
#endif

  guard.handle_promise = invoke(handle_method);
  ASSERT_NE(guard.handle_promise, nullptr);
  drain(guard.handle_promise);
  ASSERT_EQ(guard.handle_promise->state, PROMISE_FULFILLED);
  ASSERT_EQ(guard.handle_promise->result.type, T_ARRAY);
  auto* handle_result = guard.handle_promise->result.u.arr;
  ASSERT_EQ(handle_result->size, 3);
  ASSERT_EQ(handle_result->item[0].type, T_ARRAY);
  auto* process_result = handle_result->item[0].u.arr;
  ASSERT_EQ(process_result->size, 3);
  ASSERT_EQ(process_result->item[0].type, T_STRING);
  EXPECT_NE(std::string(process_result->item[0].u.string).find("fluffos-external-promise"),
            std::string::npos);
  ASSERT_EQ(process_result->item[1].type, T_STRING);
  EXPECT_STREQ(process_result->item[1].u.string, "");
  ASSERT_EQ(process_result->item[2].type, T_NUMBER);
  EXPECT_EQ(process_result->item[2].u.number, 0);
  ASSERT_EQ(handle_result->item[1].type, T_STRING);
  EXPECT_EQ(std::string(handle_result->item[1].u.string),
            std::string(process_result->item[0].u.string));
  ASSERT_EQ(handle_result->item[2].type, T_NUMBER);
  EXPECT_EQ(handle_result->item[2].u.number, 0);

  guard.start_promise = invoke(start_method);
  ASSERT_NE(guard.start_promise, nullptr);
  drain(guard.start_promise);
  ASSERT_EQ(guard.start_promise->state, PROMISE_FULFILLED);
  ASSERT_EQ(guard.start_promise->result.type, T_ARRAY);
  auto* start_result = guard.start_promise->result.u.arr;
  ASSERT_EQ(start_result->size, 3);
  ASSERT_EQ(start_result->item[0].type, T_STRING);
  EXPECT_NE(std::string(start_result->item[0].u.string).find("fluffos-external-promise"),
            std::string::npos);
  ASSERT_EQ(start_result->item[1].type, T_STRING);
  EXPECT_STREQ(start_result->item[1].u.string, "");
  ASSERT_EQ(start_result->item[2].type, T_NUMBER);
  EXPECT_EQ(start_result->item[2].u.number, 0);

#ifndef _WIN32
  guard.cat_promise = invoke("run_cat_posix");
  ASSERT_NE(guard.cat_promise, nullptr);
  drain(guard.cat_promise);
  ASSERT_EQ(guard.cat_promise->state, PROMISE_FULFILLED);
  ASSERT_EQ(guard.cat_promise->result.type, T_ARRAY);
  auto* cat_result = guard.cat_promise->result.u.arr;
  ASSERT_EQ(cat_result->size, 3);
  ASSERT_EQ(cat_result->item[0].type, T_STRING);
  EXPECT_STREQ(cat_result->item[0].u.string, "fluffos-external-stdin");
  ASSERT_EQ(cat_result->item[1].type, T_STRING);
  EXPECT_STREQ(cat_result->item[1].u.string, "");
  ASSERT_EQ(cat_result->item[2].type, T_NUMBER);
  EXPECT_EQ(cat_result->item[2].u.number, 0);
#endif
}

#ifndef _WIN32
TEST_F(DriverTest, TestExternalPromiseCancellationKillsChildAndDestructCleansHandle) {
  clear_tick_events();
  struct FixtureGuard {
    object_t* object = nullptr;
    promise_t* promise = nullptr;
    ~FixtureGuard() {
      clear_tick_events();
      if (object != nullptr) {
        destruct_object_for_test(object);
      }
      if (promise != nullptr) {
        free_promise(promise);
      }
      vm_apply_return_clear();
    }
  } guard;

  guard.object = load_object_for_test("single/tests/efuns/external_promise");
  ASSERT_NE(guard.object, nullptr);
  auto* result = safe_apply("run_long_posix", guard.object, 0, ORIGIN_DRIVER);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_PROMISE);
  guard.promise = result->u.prom;
  guard.promise->ref++;
  vm_apply_return_clear();
  ASSERT_EQ(guard.promise->state, PROMISE_PENDING);

  svalue_t reason = const0u;
  reason.type = T_STRING;
  reason.subtype = STRING_CONSTANT;
  reason.u.string = const_cast<char*>("cancel external process");
  ASSERT_EQ(promise_settle(guard.promise, &reason, 1), 1);
  guard.promise->handled = true;
  ASSERT_EQ(guard.promise->state, PROMISE_REJECTED);

  destruct_object_for_test(guard.object);
  guard.object = nullptr;
  for (int pass = 0; pass < 128; pass++) {
    if (walltime_event_queue_size_for_test() != 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      ASSERT_EQ(event_base_loop(g_event_base, EVLOOP_NONBLOCK), 0);
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    if (walltime_event_queue_size_for_test() == 0) {
      break;
    }
  }
  ASSERT_EQ(walltime_event_queue_size_for_test(), 0u);
}
#endif

TEST_F(DriverTest, TestReadJsonRejectsUnsignedIntegerOutsideLpcRange) {
  const char* relative_path = "log/read-json-integer-boundary.json";
  const char* mudlib_path = "/log/read-json-integer-boundary.json";
  PathCleanupGuard guard{relative_path};
  ScopedCurrentObjectAsMaster master_scope;

  auto write_fixture = [relative_path](const char* contents) {
    std::ofstream file(relative_path, std::ios::trunc);
    file << contents;
    return file.good();
  };

  ASSERT_TRUE(write_fixture("9223372036854775807"));
  auto max_value = read_json(mudlib_path);
  ASSERT_EQ(max_value.type, T_NUMBER);
  ASSERT_EQ(max_value.u.number, std::numeric_limits<LPC_INT>::max());
  free_svalue(&max_value, "read_json max integer test");

  ASSERT_TRUE(write_fixture("9223372036854775808"));
  bool rejected = false;
  error_context_t econ{};
  save_context(&econ);
  try {
    auto overflow = read_json(mudlib_path);
    free_svalue(&overflow, "read_json overflow test");
    pop_context(&econ);
  } catch (...) {
    restore_context(&econ);
    rejected = true;
  }
  ASSERT_TRUE(rejected);

  ASSERT_TRUE(write_fixture(R"({"outer":[9223372036854775808]})"));
  const auto arrays_before = num_arrays;
  rejected = false;
  save_context(&econ);
  try {
    auto overflow = read_json(mudlib_path);
    free_svalue(&overflow, "read_json nested overflow test");
    pop_context(&econ);
  } catch (...) {
    restore_context(&econ);
    rejected = true;
  }
  ASSERT_TRUE(rejected);
  ASSERT_EQ(num_arrays, arrays_before);
}

TEST_F(DriverTest, TestGatewayLoginRunsThroughOwnerMainQueue) {
  const char* session_id = "gw-test-login-main-queue";
  ASSERT_EQ(gateway_find_session(session_id), nullptr);

  copy_and_push_string("/clone/gateway_login_example");
  safe_apply("set_test_login_ob", master_ob, 1, ORIGIN_DRIVER);
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1, R"({"type":"login","cid":"gw-test-login-main-queue","data":{"ip":"127.0.0.1","port":6040}})"));

  auto* sess = gateway_find_session(session_id);
  ASSERT_NE(sess, nullptr);
  ASSERT_NE(sess->user_ob, nullptr);
  ASSERT_EQ(vm_owner_drain_main_tasks(1), 0);
  safe_apply("reset_test_login_ob", master_ob, 0, ORIGIN_DRIVER);
  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
}

TEST_F(DriverTest, TestGatewayLoginMainQueueDropsStaleMaster) {
  const char* session_id = "gw-test-login-stale-master";
  ASSERT_EQ(gateway_find_session(session_id), nullptr);
  ASSERT_FALSE(gateway_has_master(4040));

  copy_and_push_string("/clone/gateway_login_example");
  safe_apply("set_test_login_ob", master_ob, 1, ORIGIN_DRIVER);
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      4040, R"({"type":"login","cid":"gw-test-login-stale-master","data":{"ip":"127.0.0.1","port":6040}})"));

  ASSERT_EQ(gateway_find_session(session_id), nullptr);
  ASSERT_EQ(vm_owner_drain_main_tasks(1), 0);
  safe_apply("reset_test_login_ob", master_ob, 0, ORIGIN_DRIVER);
}

TEST_F(DriverTest, TestOwnerServiceRegistryUsesKeyedShardsForHotPaths) {
  ASSERT_EQ(owner_service_hot_path_service_owner_count(), 0);
  ASSERT_GT(owner_service_hot_path_service_shard_count(), 0);
  for (const auto& descriptor : owner_service_shard_descriptors()) {
    if (!descriptor.hot_path) {
      continue;
    }
    ASSERT_STRNE(descriptor.owner_policy, "service_owner") << descriptor.domain;
    ASSERT_NE(descriptor.shard_policy, nullptr) << descriptor.domain;
    ASSERT_NE(std::string(descriptor.shard_policy), "") << descriptor.domain;
  }
}

TEST(FilenameToObnameTest, RejectsTruncatedOutputWithoutCrossingDestination) {
  char destination[9];
  std::memset(destination, 'S', sizeof(destination));

  ASSERT_EQ(filename_to_obname("/12345678", destination, 8), 0);
  ASSERT_EQ(destination[8], 'S');
}

TEST(FilenameToObnameTest, RejectsInvalidArguments) {
  char destination[8] = {};

  EXPECT_EQ(filename_to_obname("x", destination, 0), 0);
  EXPECT_EQ(filename_to_obname("x", destination, -1), 0);
  EXPECT_EQ(filename_to_obname(nullptr, destination, sizeof(destination)), 0);
  EXPECT_EQ(filename_to_obname("x", nullptr, sizeof(destination)), 0);
}

TEST(FilenameToObnameTest, PreservesNormalizedObjectNameSemantics) {
  char destination[32] = {};

  ASSERT_EQ(filename_to_obname("//foo///bar.c.c", destination, sizeof(destination)), 1);
  ASSERT_STREQ(destination, "foo/bar");
}

TEST(FilenameToObnameTest, ReclaimsCapacityAfterRemovingCSourceSuffix) {
  char destination[8] = {};

  ASSERT_EQ(filename_to_obname("/1234567.c", destination, sizeof(destination)), 1);
  ASSERT_STREQ(destination, "1234567");
}

TEST_F(DriverTest, TestSimulEfunUsesFullSourceNameBuffer) {
  const auto source = read_source_file_for_test("../src/vm/internal/simul_efun.cc");
  const auto simulate_source = read_source_file_for_test("../src/vm/internal/simulate.cc");

  ASSERT_NE(source.find("filename_to_obname(file, buf, sizeof(buf))"), std::string::npos);
  // Preserve the config spelling when loading simul_efun: explicit
  // extensions must remain exact, while extension-less names are resolved
  // by load_object() in its .lpc-then-.c order.
  ASSERT_NE(source.find("new_ob = load_object(file, 1)"), std::string::npos);
  ASSERT_EQ(source.find("file_length < 2"), std::string::npos);
  ASSERT_EQ(simulate_source.find("strcpy(inhbuf, inherit_file)"), std::string::npos);
}

TEST_F(DriverTest, TestSaveObjectStringRejectsInvalidDestinationBeforeWriting) {
  auto *object = load_object_for_test("single/void");
  ASSERT_NE(object, nullptr);

  char destination[32];
  std::memset(destination, 'S', sizeof(destination));
  ASSERT_EQ(save_object_str(object, 0, destination, 0), 0);
  ASSERT_EQ(destination[0], 'S');
  ASSERT_EQ(save_object_str(object, 0, destination, -1), 0);
  ASSERT_EQ(save_object_str(object, 0, nullptr, sizeof(destination)), 0);

  destruct_object_for_test(object);
}

TEST_F(DriverTest, TestSaveObjectStringRecursionAccountsForConsumedPrefix) {
  const auto source = read_source_file_for_test("../src/vm/internal/base/object.cc");

  ASSERT_NE(source.find("bufsize - (textsize - 1)"), std::string::npos);
}

TEST_F(DriverTest, TestSaveObjectStringPreservesHeaderAndRejectsShortHeader) {
  auto *object = load_object_for_test("std/json");
  ASSERT_NE(object, nullptr);

  std::vector<char> destination(256, 'S');
  ASSERT_EQ(save_object_str(object, 0, destination.data(), static_cast<int>(destination.size() - 1)), 1);
  const std::string serialized(destination.data());
  ASSERT_EQ(serialized.compare(0, std::strlen("#/std/json.c\n"), "#/std/json.c\n"), 0);

  const auto header_size = serialized.find('\n') + 1;
  ASSERT_GT(header_size, 0U);
  std::vector<char> exact_header_destination(header_size + 1, 'S');
  ASSERT_EQ(save_object_str(object, 0, exact_header_destination.data(),
                            static_cast<int>(header_size)),
            1);
  ASSERT_EQ(std::string(exact_header_destination.data(), header_size),
            serialized.substr(0, header_size));
  ASSERT_EQ(exact_header_destination[header_size], '\0');
  std::vector<char> short_destination(header_size + 1, 'S');
  ASSERT_EQ(save_object_str(object, 0, short_destination.data(), static_cast<int>(header_size - 1)), 0);
  ASSERT_EQ(short_destination[0], 'S');

  destruct_object_for_test(object);
}

TEST_F(DriverTest, TestSaveObjectPathAvoidsFixedBuffers) {
  const auto source = read_source_file_for_test("../src/vm/internal/base/object.cc");
  const auto save_object_pos = source.find("int save_object(");
  const auto save_object_str_pos = source.find("int save_object_str(");

  ASSERT_NE(save_object_pos, std::string::npos);
  ASSERT_NE(save_object_str_pos, std::string::npos);
  const auto save_object_source = source.substr(save_object_pos, save_object_str_pos - save_object_pos);

  ASSERT_NE(save_object_source.find("len >= 2 && file[len - 2]"), std::string::npos);
  ASSERT_NE(save_object_source.find("const std::string tmp_name"), std::string::npos);
  ASSERT_EQ(save_object_source.find("char save_name[256]"), std::string::npos);
  ASSERT_EQ(save_object_source.find("char tmp_name[256]"), std::string::npos);
  ASSERT_EQ(save_object_source.find("char buf[1024]"), std::string::npos);
}

TEST_F(DriverTest, TestSocketAddressFormattingAvoidsFixedOutputBuffer) {
  const auto source = read_source_file_for_test("../src/packages/sockets/sockets.cc");
  const auto function_start = source.find("void f_socket_address()");
  const auto function_end = source.find("void f_socket_status()", function_start);

  ASSERT_NE(function_start, std::string::npos);
  ASSERT_NE(function_end, std::string::npos);
  const auto function_source = source.substr(function_start, function_end - function_start);
  ASSERT_EQ(function_source.find("char buf["), std::string::npos);
  ASSERT_EQ(function_source.find("sprintf("), std::string::npos);
  ASSERT_NE(function_source.find("std::string(host) + \" \" + service"), std::string::npos);
}

TEST_F(DriverTest, TestFileMovePathConstructionUsesOwnedStrings) {
  const auto source = read_source_file_for_test("../src/packages/core/file.cc");
  const auto rename_start = source.find("int do_rename(");
  const auto copy_start = source.find("int copy_file(");

  ASSERT_NE(rename_start, std::string::npos);
  ASSERT_NE(copy_start, std::string::npos);
  const auto move_source = source.substr(rename_start, copy_start - rename_start);
  ASSERT_EQ(move_source.find("char newfrom["), std::string::npos);
  ASSERT_EQ(move_source.find("char newto["), std::string::npos);
  ASSERT_EQ(move_source.find("sprintf("), std::string::npos);
  ASSERT_NE(move_source.find("std::string from_path"), std::string::npos);
  ASSERT_NE(move_source.find("std::string to_path"), std::string::npos);

  ASSERT_EQ(copy_file(nullptr, nullptr), -1);
}

TEST_F(DriverTest, TestQueryIpNumberRejectsInvalidSocketAddress) {
  object_t object{};
  interactive_t interactive{};
  object.interactive = &interactive;
  interactive.addr.ss_family = AF_UNSPEC;
  interactive.addrlen = sizeof(interactive.addr);

  ASSERT_EQ(query_ip_number(&object), nullptr);
}

TEST_F(DriverTest, TestPerfCounterUsesMonotonicSteadyClock) {
  const auto time_source = read_source_file_for_test("../src/packages/core/time.cc");
  const auto perf_counter_pos = time_source.find("void f_perf_counter_ns()");

  ASSERT_NE(perf_counter_pos, std::string::npos);
  ASSERT_NE(time_source.find("std::chrono::steady_clock::now()", perf_counter_pos),
            std::string::npos);
  ASSERT_EQ(time_source.find("std::chrono::high_resolution_clock::now()", perf_counter_pos),
            std::string::npos);
}

TEST_F(DriverTest, TestPerformanceWallClockUsesRawMonotonicSource) {
  const auto port_header = read_source_file_for_test("../src/base/internal/port.h");
  const auto port_source = read_source_file_for_test("../src/base/internal/port.cc");
  const auto time_source = read_source_file_for_test("../src/packages/core/time.cc");
  const auto spec_source = read_source_file_for_test("../src/packages/core/core.spec");
  const auto helper_pos = port_source.find("get_current_performance_wall_time_ns");
  const auto efun_pos = time_source.find("void f_performance_wall_time_ns()");

  ASSERT_NE(port_header.find("int64_t get_current_performance_wall_time_ns();"),
            std::string::npos);
  ASSERT_NE(helper_pos, std::string::npos);
  ASSERT_NE(port_source.find("CLOCK_MONOTONIC_RAW", helper_pos), std::string::npos);
  ASSERT_NE(port_source.find("clock_gettime(CLOCK_MONOTONIC_RAW", helper_pos),
            std::string::npos);
  ASSERT_NE(port_source.find("QueryPerformanceCounter", helper_pos), std::string::npos);
  ASSERT_NE(spec_source.find("int performance_wall_time_ns();"), std::string::npos);
  ASSERT_NE(efun_pos, std::string::npos);
  ASSERT_NE(time_source.find("get_current_performance_wall_time_ns()", efun_pos),
            std::string::npos);
}

TEST_F(DriverTest, TestPerformanceWallClockContainsCurrentThreadCpuInterval) {
  const auto wall_before = get_current_performance_wall_time_ns();
  const auto cpu_before = get_current_thread_cpu_time_ns();
  if (wall_before < 0 || cpu_before < 0) {
    GTEST_SKIP() << "current platform lacks compatible performance clocks";
  }

#ifdef _WIN32
  // GetThreadTimes may report the next 15.625 ms tick ahead of the wall
  // clock, and the Windows scheduler can add a small second tick while the
  // test samples both clocks.
  constexpr int64_t kTargetCpuNs = 100000000;
  constexpr int64_t kCpuClockQuantizationAllowanceNs = 32000000;
#else
  constexpr int64_t kTargetCpuNs = 10000000;
  constexpr int64_t kCpuClockQuantizationAllowanceNs = 0;
#endif
  constexpr int64_t kSafetyWallNs = 1000000000;
  volatile uint64_t accumulator = 0;
  int64_t cpu_after = cpu_before;
  while (cpu_after - cpu_before < kTargetCpuNs &&
         get_current_performance_wall_time_ns() - wall_before < kSafetyWallNs) {
    for (uint64_t value = 1; value <= 10000; ++value) {
      accumulator += value;
    }
    cpu_after = get_current_thread_cpu_time_ns();
  }
  const auto wall_after = get_current_performance_wall_time_ns();

  ASSERT_GT(accumulator, 0u);
  ASSERT_GE(cpu_after - cpu_before, kTargetCpuNs);
  ASSERT_GE(wall_after, wall_before);
  ASSERT_GE(wall_after - wall_before + kCpuClockQuantizationAllowanceNs,
            cpu_after - cpu_before);
}

TEST_F(DriverTest, TestThreadCpuClockUsesDedicatedPerThreadSources) {
  const auto port_source = read_source_file_for_test("../src/base/internal/port.cc");
  const auto time_source = read_source_file_for_test("../src/packages/core/time.cc");
  const auto spec_source = read_source_file_for_test("../src/packages/core/core.spec");
  const auto thread_cpu_pos = port_source.find("get_current_thread_cpu_time_ns");

  ASSERT_NE(thread_cpu_pos, std::string::npos);
  ASSERT_NE(port_source.find("CLOCK_THREAD_CPUTIME_ID"), std::string::npos);
  ASSERT_NE(port_source.find("GetThreadTimes"), std::string::npos);
  ASSERT_EQ(port_source.substr(thread_cpu_pos).find("RUSAGE_SELF"), std::string::npos);
  ASSERT_NE(spec_source.find("int thread_cpu_time_ns();"), std::string::npos);
  ASSERT_NE(time_source.find("void f_thread_cpu_time_ns()"), std::string::npos);
  ASSERT_NE(time_source.find("get_current_thread_cpu_time_ns()"), std::string::npos);
}

TEST_F(DriverTest, TestThreadCpuClockIsMonotonicWhenAvailable) {
  const auto before = get_current_thread_cpu_time_ns();
  if (before < 0) {
    GTEST_SKIP() << "current platform has no current-thread CPU clock";
  }

  volatile uint64_t accumulator = 0;
  for (uint64_t value = 1; value <= 100000; ++value) {
    accumulator += value;
  }
  const auto after = get_current_thread_cpu_time_ns();

  ASSERT_GT(accumulator, 0u);
  ASSERT_GE(after, before);
}

TEST_F(DriverTest, TestOwnerAsyncLpcAndCompletionExposeWorkerThreadCpuCounters) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto metrics_source =
      read_source_file_for_test("../src/vm/internal/owner_runtime_metrics.h");

  ASSERT_NE(owner_source.find("get_current_thread_cpu_time_ns()"), std::string::npos);
  ASSERT_NE(metrics_source.find("owner_async_lpc_execute_thread_cpu_ns_total"),
            std::string::npos);
  ASSERT_NE(metrics_source.find("owner_async_lpc_execute_thread_cpu_unavailable"),
            std::string::npos);
  ASSERT_NE(metrics_source.find("owner_async_result_completion_thread_cpu_ns_total"),
            std::string::npos);
  ASSERT_NE(metrics_source.find("owner_async_result_completion_thread_cpu_unavailable"),
            std::string::npos);
  ASSERT_NE(owner_source.find("owner_async_lpc_execute_thread_cpu_total_us"),
            std::string::npos);
  ASSERT_NE(owner_source.find("owner_async_result_completion_thread_cpu_total_us"),
            std::string::npos);
}

TEST_F(DriverTest, TestOwnerRuntimeMetricMappingsUseLpcIntegerWidth) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto status_start =
      owner_source.find("void add_owner_runtime_v2_status_fields(");
  const auto status_end = owner_source.find(
      "std::shared_ptr<VMFrozenValue> frozen_compute_result_mapping", status_start);

  ASSERT_NE(status_start, std::string::npos);
  ASSERT_NE(status_end, std::string::npos);
  ASSERT_GT(status_end, status_start);
  const auto status_body =
      owner_source.substr(status_start, status_end - status_start);
  ASSERT_EQ(status_body.find("static_cast<long>"), std::string::npos);
}

TEST_F(DriverTest, TestOwnerStatusMappingsUseLpcIntegerWidth) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  auto assert_status_uses_lpc_width = [&](const char* start_marker,
                                          const char* end_marker) {
    const auto start = owner_source.find(start_marker);
    const auto end = owner_source.find(end_marker, start);

    ASSERT_NE(start, std::string::npos) << start_marker;
    ASSERT_NE(end, std::string::npos) << end_marker;
    ASSERT_GT(end, start) << start_marker;
    const auto body = owner_source.substr(start, end - start);
    EXPECT_EQ(body.find("static_cast<long>"), std::string::npos)
        << start_marker;
    EXPECT_NE(body.find("static_cast<LPC_INT>"), std::string::npos)
        << start_marker;
  };

  assert_status_uses_lpc_width(
      "mapping_t *vm_owner_thread_status()",
      "mapping_t *vm_owner_runtime_status()");
  assert_status_uses_lpc_width(
      "mapping_t *vm_owner_runtime_status()",
      "#ifdef DEBUGMALLOC_EXTENSIONS");
}

TEST_F(DriverTest, TestGatewayStatusMappingsUseLpcIntegerWidth) {
  const auto gateway_source =
      read_source_file_for_test("../src/packages/gateway/gateway.cc");
  const auto helper_name = gateway_source.find("gateway_avg_us(");
  const auto helper_start = gateway_source.rfind('\n', helper_name);
  const auto helper_end =
      gateway_source.find("void gateway_record_main_drain(", helper_name);
  const auto status_start =
      gateway_source.find("mapping_t *gateway_status_internal()");
  const auto status_end =
      gateway_source.find("void f_is_gateway_user()", status_start);

  ASSERT_NE(helper_name, std::string::npos);
  ASSERT_NE(helper_start, std::string::npos);
  ASSERT_NE(helper_end, std::string::npos);
  ASSERT_GT(helper_end, helper_start);
  ASSERT_NE(status_start, std::string::npos);
  ASSERT_NE(status_end, std::string::npos);
  ASSERT_GT(status_end, status_start);
  const auto helper_body =
      gateway_source.substr(helper_start + 1, helper_end - helper_start - 1);
  const auto status_body =
      gateway_source.substr(status_start, status_end - status_start);
  EXPECT_EQ(helper_body.find("static_cast<long>"), std::string::npos);
  EXPECT_EQ(helper_body.find("long gateway_"), std::string::npos);
  EXPECT_NE(helper_body.find("LPC_INT gateway_avg_us"), std::string::npos);
  EXPECT_EQ(status_body.find("static_cast<long>"), std::string::npos);
  EXPECT_NE(status_body.find("static_cast<LPC_INT>"), std::string::npos);
}

TEST_F(DriverTest, TestVmWorkerMappingCountersUseLpcIntegerWidth) {
  const auto worker_source =
      read_source_file_for_test("../src/packages/core/vm_worker.cc");
  auto assert_mapping_uses_lpc_width = [&](const char* start_marker,
                                           const char* end_marker) {
    const auto start = worker_source.find(start_marker);
    const auto end = worker_source.find(end_marker, start);

    ASSERT_NE(start, std::string::npos) << start_marker;
    ASSERT_NE(end, std::string::npos) << end_marker;
    ASSERT_GT(end, start) << start_marker;
    const auto body = worker_source.substr(start, end - start);
    EXPECT_EQ(body.find("static_cast<long>"), std::string::npos)
        << start_marker;
    EXPECT_NE(body.find("static_cast<LPC_INT>"), std::string::npos)
        << start_marker;
  };

  assert_mapping_uses_lpc_width(
      "mapping_t *worker_bench_response(mapping_t *options)",
      "mapping_t *worker_actor_bench_response(mapping_t *options)");
  assert_mapping_uses_lpc_width(
      "mapping_t *worker_status_response()",
      "mapping_t *worker_submit_response(");
  assert_mapping_uses_lpc_width(
      "void f_vm_worker_bench()",
      "#ifdef F_VM_WORKER_TASK");
}

TEST_F(DriverTest, TestGatewaySessionCountMappingsUseLpcIntegerWidth) {
  const auto gateway_header =
      read_source_file_for_test("../src/packages/gateway/gateway.h");
  const auto session_source =
      read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const char* count_functions[] = {
      "gateway_session_fifo_depth_total",
      "gateway_session_fifo_pending_reservations_total",
      "gateway_session_future_watch_count",
      "gateway_room_output_projection_pending_count",
      "gateway_room_output_projection_wave_count",
      "gateway_room_output_projection_reservation_count",
      "gateway_room_output_projection_retry_count",
      "gateway_future_watch_count",
  };

  for (const auto* function_name : count_functions) {
    const auto long_signature =
        std::string("long ") + function_name + "(";
    const auto lpc_signature =
        std::string("LPC_INT ") + function_name + "(";
    EXPECT_EQ(gateway_header.find(long_signature), std::string::npos)
        << function_name;
    EXPECT_NE(gateway_header.find(lpc_signature), std::string::npos)
        << function_name;
    EXPECT_EQ(session_source.find(long_signature), std::string::npos)
        << function_name;
    EXPECT_NE(session_source.find(lpc_signature), std::string::npos)
        << function_name;
  }

  const auto projection_start = session_source.find(
      "gateway_room_output_projection_pending_count()");
  const auto projection_end = session_source.find(
      "GatewaySession *gateway_find_session(", projection_start);
  const auto watch_start =
      session_source.find("gateway_session_future_watch_count()");
  const auto watch_end = session_source.find(
      "uint64_t gateway_session_fifo_enqueued_total()", watch_start);
  const auto quiesce_start = session_source.find(
      "mapping_t *gateway_owner_output_quiesce(const char *reason)");
  const auto quiesce_end = session_source.find(
      "int gateway_fill_session_protocol_output_with_writer(",
      quiesce_start);

  ASSERT_NE(projection_start, std::string::npos);
  ASSERT_NE(projection_end, std::string::npos);
  ASSERT_GT(projection_end, projection_start);
  ASSERT_NE(watch_start, std::string::npos);
  ASSERT_NE(watch_end, std::string::npos);
  ASSERT_GT(watch_end, watch_start);
  ASSERT_NE(quiesce_start, std::string::npos);
  ASSERT_NE(quiesce_end, std::string::npos);
  ASSERT_GT(quiesce_end, quiesce_start);

  const auto projection_body = session_source.substr(
      projection_start, projection_end - projection_start);
  const auto watch_body =
      session_source.substr(watch_start, watch_end - watch_start);
  const auto quiesce_body =
      session_source.substr(quiesce_start, quiesce_end - quiesce_start);
  EXPECT_EQ(projection_body.find("static_cast<long>"), std::string::npos);
  EXPECT_EQ(projection_body.find("numeric_limits<long>"),
            std::string::npos);
  EXPECT_EQ(watch_body.find("static_cast<long>"), std::string::npos);
  EXPECT_EQ(quiesce_body.find("static_cast<long>"), std::string::npos);
  EXPECT_EQ(quiesce_body.find("numeric_limits<long>"),
            std::string::npos);
  EXPECT_EQ(quiesce_body.find("std::max<long>"), std::string::npos);
}

TEST_F(DriverTest, TestGatewaySessionInfoMappingsUseLpcIntegerWidth) {
  const auto session_source =
      read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto payload_start = session_source.find(
      "svalue_t gateway_command_task_payload(");
  const auto payload_end = session_source.find(
      "void cleanup_temp_gateway_interactive(", payload_start);
  const auto info_start =
      session_source.find("void f_gateway_session_info()");
  const auto info_end =
      session_source.find("void f_gateway_inject_input()", info_start);

  ASSERT_NE(payload_start, std::string::npos);
  ASSERT_NE(payload_end, std::string::npos);
  ASSERT_GT(payload_end, payload_start);
  ASSERT_NE(info_start, std::string::npos);
  ASSERT_NE(info_end, std::string::npos);
  ASSERT_GT(info_end, info_start);
  const auto payload_body =
      session_source.substr(payload_start, payload_end - payload_start);
  const auto info_body =
      session_source.substr(info_start, info_end - info_start);
  EXPECT_EQ(payload_body.find("static_cast<long>"), std::string::npos);
  EXPECT_NE(payload_body.find("static_cast<LPC_INT>"),
            std::string::npos);
  EXPECT_EQ(info_body.find("static_cast<long>"), std::string::npos);
  EXPECT_NE(info_body.find("static_cast<LPC_INT>"), std::string::npos);
  EXPECT_EQ(info_body.find("long pending_reservations"),
            std::string::npos);
  EXPECT_NE(info_body.find("LPC_INT pending_reservations"),
            std::string::npos);
}

TEST_F(DriverTest, TestObjectStoreShardMappingsUseLpcIntegerWidth) {
  const auto object_store_source =
      read_source_file_for_test("../src/vm/internal/object_store.cc");
  auto assert_mapping_uses_lpc_width = [&](const char* start_marker,
                                           const char* end_marker) {
    const auto start = object_store_source.find(start_marker);
    const auto end = object_store_source.find(end_marker, start);

    ASSERT_NE(start, std::string::npos) << start_marker;
    ASSERT_NE(end, std::string::npos) << end_marker;
    ASSERT_GT(end, start) << start_marker;
    const auto body = object_store_source.substr(start, end - start);
    EXPECT_EQ(body.find("static_cast<long>"), std::string::npos)
        << start_marker;
    EXPECT_NE(body.find("static_cast<LPC_INT>"), std::string::npos)
        << start_marker;
  };

  assert_mapping_uses_lpc_width(
      "mapping_t *vm_object_shard_contract_mapping(",
      "void append_migration_trace_locked(");
  assert_mapping_uses_lpc_width(
      "execution_runnable_tasks(",
      "mapping_t *status_record_mapping(");
  assert_mapping_uses_lpc_width(
      "mapping_t *status_record_mapping(",
      "mapping_t *execution_shard_mapping(");
  assert_mapping_uses_lpc_width(
      "mapping_t *execution_shard_mapping(",
      "mapping_t *shard_mapping(");
  assert_mapping_uses_lpc_width(
      "mapping_t *shard_mapping(",
      "}  // namespace");
  EXPECT_EQ(object_store_source.find("long execution_runnable_tasks("),
            std::string::npos);
  EXPECT_NE(object_store_source.find("LPC_INT execution_runnable_tasks("),
            std::string::npos);
}

TEST_F(DriverTest, TestObjectStoreGlobalStatusMappingUsesLpcIntegerWidth) {
  const auto object_store_source =
      read_source_file_for_test("../src/vm/internal/object_store.cc");
  const auto status_start =
      object_store_source.find("mapping_t *vm_object_store_status()");
  const auto status_end = object_store_source.find(
      "mapping_t *vm_object_store_owner_status(", status_start);

  ASSERT_NE(status_start, std::string::npos);
  ASSERT_NE(status_end, std::string::npos);
  ASSERT_GT(status_end, status_start);
  const auto status_body =
      object_store_source.substr(status_start, status_end - status_start);
  EXPECT_EQ(status_body.find("static_cast<long>"), std::string::npos);
  EXPECT_NE(status_body.find("static_cast<LPC_INT>"),
            std::string::npos);
}

TEST_F(DriverTest, TestOwnerSchedulerCountsUseFixedIntegerWidth) {
  const auto scheduler_header = read_source_file_for_test(
      "../src/vm/internal/owner_scheduler_state.h");
  const auto scheduler_source = read_source_file_for_test(
      "../src/vm/internal/owner_scheduler_state.cc");
  const auto trace_header = read_source_file_for_test(
      "../src/vm/internal/owner_trace_store.h");
  const auto owner_header = read_source_file_for_test("../src/vm/owner.h");
  const auto owner_source = read_source_file_for_test(
      "../src/vm/internal/owner.cc");

  EXPECT_EQ(scheduler_header.find("long "), std::string::npos);
  EXPECT_NE(scheduler_header.find("int64_t mailbox_depth("),
            std::string::npos);
  EXPECT_EQ(scheduler_source.find("static_cast<long>"),
            std::string::npos);
  EXPECT_EQ(scheduler_source.find("long OwnerSchedulerState::"),
            std::string::npos);
  EXPECT_NE(scheduler_source.find("int64_t OwnerSchedulerState::"),
            std::string::npos);

  const auto trace_start = trace_header.find("struct OwnerExecutorTrace {");
  const auto trace_end = trace_header.find(
      "template <typename Trace>", trace_start);
  ASSERT_NE(trace_start, std::string::npos);
  ASSERT_NE(trace_end, std::string::npos);
  ASSERT_GT(trace_end, trace_start);
  const auto trace_body =
      trace_header.substr(trace_start, trace_end - trace_start);
  EXPECT_EQ(trace_body.find("long "), std::string::npos);
  EXPECT_NE(trace_body.find("int64_t backlog"), std::string::npos);

  const auto drain_result_start =
      owner_header.find("struct VMOwnerMainDrainResult {");
  const auto drain_result_end = owner_header.find(
      "using VMOwnerFutureTerminalNotifier", drain_result_start);
  ASSERT_NE(drain_result_start, std::string::npos);
  ASSERT_NE(drain_result_end, std::string::npos);
  ASSERT_GT(drain_result_end, drain_result_start);
  const auto drain_result_body = owner_header.substr(
      drain_result_start, drain_result_end - drain_result_start);
  EXPECT_EQ(drain_result_body.find("long remaining_"),
            std::string::npos);
  EXPECT_NE(drain_result_body.find("int64_t remaining_main_tasks"),
            std::string::npos);
  EXPECT_EQ(owner_header.find("long vm_owner_main_queue_total_depth()"),
            std::string::npos);
  EXPECT_NE(owner_header.find("int64_t vm_owner_main_queue_total_depth()"),
            std::string::npos);

  const char* scheduler_count_functions[] = {
      "owner_mailbox_depth",
      "owner_mailbox_total_depth",
      "owner_main_queue_total_depth",
      "owner_main_queue_depth",
      "owner_mailbox_active_owners",
      "owner_executor_runnable_queue_depth",
      "owner_executor_safe_queue_depth",
      "owner_main_required_queue_depth",
      "owner_runnable_owner_count",
      "owner_main_runnable_owner_count",
  };
  for (const auto* function_name : scheduler_count_functions) {
    EXPECT_EQ(owner_source.find(std::string("long ") + function_name + "("),
              std::string::npos)
        << function_name;
    EXPECT_NE(owner_source.find(std::string("int64_t ") + function_name + "("),
              std::string::npos)
        << function_name;
  }
  EXPECT_EQ(owner_source.find("long vm_owner_main_queue_total_depth()"),
            std::string::npos);
  EXPECT_NE(owner_source.find("int64_t vm_owner_main_queue_total_depth()"),
            std::string::npos);

  const auto drain_start = owner_source.find(
      "VMOwnerMainDrainResult vm_owner_drain_main_tasks_with_budget(");
  const auto drain_end = owner_source.find(
      "int vm_owner_drain_main_tasks(int limit)", drain_start);
  ASSERT_NE(drain_start, std::string::npos);
  ASSERT_NE(drain_end, std::string::npos);
  ASSERT_GT(drain_end, drain_start);
  const auto drain_body =
      owner_source.substr(drain_start, drain_end - drain_start);
  EXPECT_EQ(drain_body.find("static_cast<long>"), std::string::npos);
  EXPECT_NE(drain_body.find("static_cast<int64_t>"),
            std::string::npos);
}

TEST_F(DriverTest, TestOwnerDiagnosticMappingsUseLpcIntegerWidth) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  auto assert_mapping_uses_lpc_width = [&](const char* start_marker,
                                           const char* end_marker) {
    const auto start = owner_source.find(start_marker);
    const auto end = owner_source.find(end_marker, start);

    ASSERT_NE(start, std::string::npos) << start_marker;
    ASSERT_NE(end, std::string::npos) << end_marker;
    ASSERT_GT(end, start) << start_marker;
    const auto body = owner_source.substr(start, end - start);
    EXPECT_EQ(body.find("static_cast<long>"), std::string::npos)
        << start_marker;
    EXPECT_NE(body.find("static_cast<LPC_INT>"), std::string::npos)
        << start_marker;
  };

  assert_mapping_uses_lpc_width(
      "mapping_t *vm_owner_status(object_t *object)",
      "mapping_t *vm_owner_guard(");
  assert_mapping_uses_lpc_width(
      "mapping_t *vm_owner_drain_mailbox(",
      "mapping_t *vm_owner_purge_mailbox(");
  assert_mapping_uses_lpc_width(
      "mapping_t *vm_owner_purge_mailbox(",
      "mapping_t *vm_owner_schedule(");
  assert_mapping_uses_lpc_width(
      "mapping_t *vm_owner_schedule(",
      "mapping_t *vm_owner_mailbox_status(");
  assert_mapping_uses_lpc_width(
      "mapping_t *vm_owner_mailbox_status(",
      "mapping_t *vm_owner_task_trace(");
  assert_mapping_uses_lpc_width(
      "mapping_t *vm_owner_task_trace(",
      "mapping_t *vm_owner_executor_trace(");
  assert_mapping_uses_lpc_width(
      "mapping_t *vm_owner_executor_trace(",
      "mapping_t *vm_owner_access_trace(");
  assert_mapping_uses_lpc_width(
      "mapping_t *vm_owner_access_trace(",
      "mapping_t *submit_owner_message(");
  assert_mapping_uses_lpc_width(
      "mapping_t *vm_owner_message_trace(",
      "mapping_t *vm_owner_future_poll(");
  assert_mapping_uses_lpc_width(
      "mapping_t *vm_owner_commit_trace(",
      "void vm_owner_thread_start(");

  const auto context_start =
      owner_source.find("mapping_t *vm_context_contract_mapping()");
  const auto context_end = owner_source.find(
      "mapping_t *owner_executor_boundary_contract_mapping()", context_start);
  ASSERT_NE(context_start, std::string::npos);
  ASSERT_NE(context_end, std::string::npos);
  ASSERT_GT(context_end, context_start);
  const auto context_body =
      owner_source.substr(context_start, context_end - context_start);
  EXPECT_EQ(context_body.find(
                "static_cast<long>(vm_context_object_store_sync_rejections())"),
            std::string::npos);
  EXPECT_NE(context_body.find(
                "static_cast<LPC_INT>(vm_context_object_store_sync_rejections())"),
            std::string::npos);
}

TEST_F(DriverTest, TestOwnerTraceAppendMetadataUsesStoreLockBoundary) {
  const auto trace_source =
      read_source_file_for_test("../src/vm/internal/owner_trace_store.cc");
  auto assert_ordered_markers = [&](const char* start_marker,
                                    const char* end_marker,
                                    std::initializer_list<const char*> markers) {
    const auto start = trace_source.find(start_marker);
    const auto end = trace_source.find(end_marker, start);

    ASSERT_NE(start, std::string::npos) << start_marker;
    ASSERT_NE(end, std::string::npos) << end_marker;
    ASSERT_GT(end, start) << start_marker;
    const auto body = trace_source.substr(start, end - start);
    size_t cursor = 0;
    for (const auto* marker : markers) {
      const auto pos = body.find(marker, cursor);
      ASSERT_NE(pos, std::string::npos) << start_marker << ": " << marker;
      cursor = pos + std::strlen(marker);
    }
  };

  assert_ordered_markers(
      "uint64_t OwnerTraceStore::append_task(",
      "uint64_t OwnerTraceStore::append_executor(",
      {"std::lock_guard<std::mutex> lock(mutex_);",
       "next_task_trace_id_.fetch_add(",
       "if (trace.sequence == 0)",
       "task_traces_.push_back(",
       "total_task_traced_.fetch_add("});
  assert_ordered_markers(
      "uint64_t OwnerTraceStore::append_executor(",
      "uint64_t OwnerTraceStore::append_access(",
      {"std::lock_guard<std::mutex> lock(mutex_);",
       "next_executor_trace_id_.fetch_add(",
       "trace.sequence = trace.trace_id;",
       "executor_traces_.push_back(",
       "total_executor_traced_.fetch_add("});
  assert_ordered_markers(
      "uint64_t OwnerTraceStore::append_access(",
      "void OwnerTraceStore::append_message(",
      {"std::lock_guard<std::mutex> lock(mutex_);",
       "next_access_trace_id_.fetch_add(",
       "total_access_traced_.load(",
       "access_traces_.push_back(",
       "total_access_traced_.fetch_add("});
  assert_ordered_markers(
      "void OwnerTraceStore::append_message(",
      "OwnerCommitTrace OwnerTraceStore::append_commit(",
      {"std::lock_guard<std::mutex> lock(mutex_);",
       "total_message_traced_.load(",
       "message_traces_.push_back(",
       "total_message_traced_.fetch_add("});
  assert_ordered_markers(
      "OwnerCommitTrace OwnerTraceStore::append_commit(",
      "uint64_t OwnerTraceStore::append_commit_observed(",
      {"std::lock_guard<std::mutex> lock(mutex_);",
       "next_commit_trace_id_.fetch_add(",
       "total_commit_traced_.load(",
       "commit_traces_.push_back(",
       "total_commit_traced_.fetch_add("});
  assert_ordered_markers(
      "uint64_t OwnerTraceStore::append_commit_observed(",
      "uint64_t OwnerTraceStore::next_message_id(",
      {"std::lock_guard<std::mutex> lock(mutex_);",
       "next_commit_trace_id_.fetch_add(",
       "total_commit_traced_.load(",
       "commit_traces_.push_back(",
       "total_commit_traced_.fetch_add("});

  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto record_start = owner_source.find(
      "uint64_t vm_owner_record_task_trace(");
  const auto record_end = owner_source.find(
      "void record_owner_main_queue_fallback(", record_start);
  ASSERT_NE(record_start, std::string::npos);
  ASSERT_NE(record_end, std::string::npos);
  ASSERT_GT(record_end, record_start);
  const auto record_body =
      owner_source.substr(record_start, record_end - record_start);
  EXPECT_EQ(record_body.find("total_task_traced()"), std::string::npos);
  EXPECT_NE(record_body.find("append_owner_task_trace(0, 0,"),
            std::string::npos);
}

TEST_F(DriverTest, TestOwnerMessageSubmissionLimitsCoordinatorLockToRegistrationAndEnqueue) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto start = owner_source.find("mapping_t *submit_owner_message(");
  const auto end = owner_source.find("mapping_t *vm_owner_submit_message(", start);

  ASSERT_NE(start, std::string::npos);
  ASSERT_NE(end, std::string::npos);
  ASSERT_GT(end, start);
  const auto body = owner_source.substr(start, end - start);
  const auto lock_pos =
      body.find("std::lock_guard<std::mutex> lock(owner_runtime_mutex);");
  ASSERT_NE(lock_pos, std::string::npos);
  const auto lock_scope_start = body.rfind('{', lock_pos);
  ASSERT_NE(lock_scope_start, std::string::npos);

  size_t depth = 0;
  size_t lock_scope_end = std::string::npos;
  for (size_t i = lock_scope_start; i < body.size(); i++) {
    if (body[i] == '{') {
      depth++;
    } else if (body[i] == '}') {
      ASSERT_GT(depth, 0u);
      depth--;
      if (depth == 0) {
        lock_scope_end = i + 1;
        break;
      }
    }
  }
  ASSERT_NE(lock_scope_end, std::string::npos);

  const auto before_lock = body.substr(0, lock_scope_start);
  const auto locked_body =
      body.substr(lock_scope_start, lock_scope_end - lock_scope_start);
  const auto after_lock = body.substr(lock_scope_end);

  EXPECT_NE(before_lock.find("vm_object_store_record_message("),
            std::string::npos);
  EXPECT_NE(before_lock.find("vm_object_handle_resolve_status("),
            std::string::npos);
  EXPECT_NE(locked_body.find("owner_future_store.admit_pending("),
            std::string::npos);
  EXPECT_NE(locked_body.find("owner_trace_store.append_message("),
            std::string::npos);
  EXPECT_NE(locked_body.find("enqueue_owner_task_locked("),
            std::string::npos);
  for (const char* external_store_work : {
           "vm_object_store_record_message(",
           "vm_object_store_remove_message(",
           "complete_owner_future_for_task_locked(",
           "vm_object_handle_resolve_status(",
       }) {
    EXPECT_EQ(locked_body.find(external_store_work), std::string::npos)
        << external_store_work;
  }
  EXPECT_NE(after_lock.find("vm_object_store_remove_message("),
            std::string::npos);
  EXPECT_NE(after_lock.find("complete_owner_future_for_task_locked("),
            std::string::npos);
}

TEST_F(DriverTest, TestOwnerQueueTraceRecordsQueuedAfterSuccessfulAdmissionOnly) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto helper_start = owner_source.find("bool enqueue_owner_task_locked(");
  const auto helper_end = owner_source.find("void finish_active_main_owner_task(",
                                            helper_start);

  ASSERT_NE(helper_start, std::string::npos);
  ASSERT_NE(helper_end, std::string::npos);
  ASSERT_GT(helper_end, helper_start);
  const auto helper_body =
      owner_source.substr(helper_start, helper_end - helper_start);
  EXPECT_NE(helper_body.find("append_owner_task_trace(task, \"queued\");"),
            std::string::npos);

  auto expect_helper_is_single_queued_trace_writer =
      [&](const char *start_marker, const char *end_marker) {
        const auto start = owner_source.find(start_marker);
        const auto end = owner_source.find(end_marker, start);

        ASSERT_NE(start, std::string::npos) << start_marker;
        ASSERT_NE(end, std::string::npos) << end_marker;
        ASSERT_GT(end, start) << start_marker;
        const auto body = owner_source.substr(start, end - start);
        EXPECT_NE(body.find("enqueue_owner_task_locked("), std::string::npos)
            << start_marker;
        EXPECT_EQ(body.find("append_owner_task_trace(task, \"queued\");"),
                  std::string::npos)
            << start_marker;
      };

  expect_helper_is_single_queued_trace_writer(
      "uint64_t vm_owner_enqueue_task_epoch(",
      "uint64_t vm_owner_enqueue_command_frame_restore(");
  expect_helper_is_single_queued_trace_writer(
      "uint64_t vm_owner_enqueue_command_frame_restore(",
      "bool vm_owner_executor_available()");
  expect_helper_is_single_queued_trace_writer(
      "uint64_t vm_owner_enqueue_test_main_required_message(",
      "mapping_t *vm_owner_lpc_probe(");
  expect_helper_is_single_queued_trace_writer(
      "mapping_t *vm_owner_lpc_probe(",
      "mapping_t *vm_owner_lpc_canary(");
  expect_helper_is_single_queued_trace_writer(
      "mapping_t *vm_owner_lpc_canary(",
      "mapping_t *vm_owner_lpc_task(");
  expect_helper_is_single_queued_trace_writer(
      "mapping_t *vm_owner_lpc_task(",
      "mapping_t *vm_owner_ordinary_lpc_task(");
  expect_helper_is_single_queued_trace_writer(
      "mapping_t *vm_owner_ordinary_lpc_task(",
      "uint64_t vm_owner_record_task_trace(");
}

TEST_F(DriverTest, TestOwnerRejectedSubmissionsCompleteAfterCoordinatorUnlock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  auto assert_completion_after_lock = [&](const char* start_marker,
                                           const char* end_marker,
                                           const char* completion_guard) {
    const auto start = owner_source.find(start_marker);
    const auto end = owner_source.find(end_marker, start);

    ASSERT_NE(start, std::string::npos) << start_marker;
    ASSERT_NE(end, std::string::npos) << end_marker;
    ASSERT_GT(end, start) << start_marker;
    const auto body = owner_source.substr(start, end - start);
    const auto lock_pos =
        body.find("std::lock_guard<std::mutex> lock(owner_runtime_mutex);");
    ASSERT_NE(lock_pos, std::string::npos) << start_marker;
    const auto lock_scope_start = body.rfind('{', lock_pos);
    ASSERT_NE(lock_scope_start, std::string::npos) << start_marker;

    size_t depth = 0;
    size_t lock_scope_end = std::string::npos;
    for (size_t i = lock_scope_start; i < body.size(); i++) {
      if (body[i] == '{') {
        depth++;
      } else if (body[i] == '}') {
        ASSERT_GT(depth, 0u) << start_marker;
        depth--;
        if (depth == 0) {
          lock_scope_end = i + 1;
          break;
        }
      }
    }
    ASSERT_NE(lock_scope_end, std::string::npos) << start_marker;

    const auto locked_body =
        body.substr(lock_scope_start, lock_scope_end - lock_scope_start);
    const auto after_lock = body.substr(lock_scope_end);
    EXPECT_NE(locked_body.find("owner_future_store.admit_pending("),
              std::string::npos)
        << start_marker;
    EXPECT_NE(locked_body.find("enqueue_owner_task_locked("),
              std::string::npos)
        << start_marker;
    EXPECT_EQ(locked_body.find("complete_owner_future_for_task_locked("),
              std::string::npos)
        << start_marker;
    const auto guard_pos = after_lock.find(completion_guard);
    const auto completion_pos =
        after_lock.find("complete_owner_future_for_task_locked(");
    ASSERT_NE(guard_pos, std::string::npos) << start_marker;
    ASSERT_NE(completion_pos, std::string::npos) << start_marker;
    EXPECT_LT(guard_pos, completion_pos) << start_marker;
  };

  assert_completion_after_lock(
      "VMOwnerStringTaskSubmission vm_owner_submit_frozen_string_task(",
      "uint64_t vm_owner_enqueue_executor_callback_cleanup(",
      "if (future_registered && !queued)");
  assert_completion_after_lock(
      "mapping_t *vm_owner_lpc_task(",
      "mapping_t *vm_owner_ordinary_lpc_task(", "if (!queued)");
  assert_completion_after_lock(
      "mapping_t *vm_owner_ordinary_lpc_task(",
      "uint64_t vm_owner_record_task_trace(", "if (!queued)");
}

TEST_F(DriverTest, TestOwnerComputeResultBackpressureCompletesAfterCoordinatorUnlock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto start = owner_source.find(
      "uint64_t vm_owner_enqueue_compute_result_fields(");
  const auto end = owner_source.find("mapping_t *vm_owner_message_trace(", start);

  ASSERT_NE(start, std::string::npos);
  ASSERT_NE(end, std::string::npos);
  ASSERT_GT(end, start);
  const auto body = owner_source.substr(start, end - start);
  const auto lock_pos =
      body.find("std::lock_guard<std::mutex> lock(owner_runtime_mutex);");
  ASSERT_NE(lock_pos, std::string::npos);
  const auto lock_scope_start = body.rfind('{', lock_pos);
  ASSERT_NE(lock_scope_start, std::string::npos);

  size_t depth = 0;
  size_t lock_scope_end = std::string::npos;
  for (size_t i = lock_scope_start; i < body.size(); i++) {
    if (body[i] == '{') {
      depth++;
    } else if (body[i] == '}') {
      ASSERT_GT(depth, 0u);
      depth--;
      if (depth == 0) {
        lock_scope_end = i + 1;
        break;
      }
    }
  }
  ASSERT_NE(lock_scope_end, std::string::npos);

  const auto locked_body =
      body.substr(lock_scope_start, lock_scope_end - lock_scope_start);
  const auto after_lock = body.substr(lock_scope_end);
  EXPECT_NE(locked_body.find("enqueue_owner_task_locked("),
            std::string::npos);
  EXPECT_EQ(locked_body.find("complete_owner_future_for_task_locked("),
            std::string::npos);
  const auto guard_pos = after_lock.find("if (!queued)");
  const auto completion_pos =
      after_lock.find("complete_owner_future_for_task_locked(");
  ASSERT_NE(guard_pos, std::string::npos);
  ASSERT_NE(completion_pos, std::string::npos);
  EXPECT_LT(guard_pos, completion_pos);
}

TEST_F(DriverTest, TestOwnerTerminalCompletionPathsAvoidCoordinatorLock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  auto assert_function_has_no_coordinator_lock =
      [&](const char* start_marker, const char* end_marker,
          const char* completion_call) {
        const auto start = owner_source.find(start_marker);
        const auto end = owner_source.find(end_marker, start);

        ASSERT_NE(start, std::string::npos) << start_marker;
        ASSERT_NE(end, std::string::npos) << end_marker;
        ASSERT_GT(end, start) << start_marker;
        const auto body = owner_source.substr(start, end - start);
        EXPECT_NE(body.find(completion_call), std::string::npos)
            << start_marker;
        EXPECT_EQ(body.find("owner_runtime_mutex"), std::string::npos)
            << start_marker;
      };

  assert_function_has_no_coordinator_lock(
      "void dispatch_owner_message_in_current_context(",
      "void complete_owner_compute_result_task_locked(",
      "complete_owner_message_task_locked(task);");
  assert_function_has_no_coordinator_lock(
      "  void complete_owner_message_task(const OwnerMailboxTask &task) {",
      "  void complete_owner_compute_result_task(",
      "complete_owner_message_task_locked(task);");
  assert_function_has_no_coordinator_lock(
      "  void complete_owner_compute_result_task(const OwnerMailboxTask &task) {",
      "  void run_task(OwnerMailboxTask &task) {",
      "complete_owner_compute_result_task_locked(task);");

  const auto drain_start =
      owner_source.find("mapping_t *vm_owner_drain_mailbox(");
  const auto drain_end =
      owner_source.find("mapping_t *vm_owner_purge_mailbox(", drain_start);
  ASSERT_NE(drain_start, std::string::npos);
  ASSERT_NE(drain_end, std::string::npos);
  ASSERT_GT(drain_end, drain_start);
  const auto drain_body =
      owner_source.substr(drain_start, drain_end - drain_start);
  const auto compute_start = drain_body.find(
      "} else if (task.task_type == \"compute_result\") {");
  const auto compute_end = drain_body.find(
      "} else if (task.task_type == \"lpc_task\") {", compute_start);
  ASSERT_NE(compute_start, std::string::npos);
  ASSERT_NE(compute_end, std::string::npos);
  ASSERT_GT(compute_end, compute_start);
  const auto compute_branch =
      drain_body.substr(compute_start, compute_end - compute_start);
  EXPECT_NE(compute_branch.find("complete_owner_compute_result_task_locked(task);"),
            std::string::npos);
  EXPECT_EQ(compute_branch.find("owner_runtime_mutex"), std::string::npos);
}

TEST_F(DriverTest, TestOwnerStatusMappingsBuildAfterRuntimeSnapshotLockScope) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  ASSERT_NE(owner_source.find("OwnerStatusSnapshot owner_status_snapshot_locked()"),
            std::string::npos);

  auto assert_mapping_builds_after_snapshot = [&](const char* start_marker,
                                                   const char* end_marker) {
    const auto start = owner_source.find(start_marker);
    const auto end = owner_source.find(end_marker, start);

    ASSERT_NE(start, std::string::npos) << start_marker;
    ASSERT_NE(end, std::string::npos) << end_marker;
    ASSERT_GT(end, start) << start_marker;
    const auto body = owner_source.substr(start, end - start);
    const auto lock_pos =
        body.find("std::lock_guard<std::mutex> lock(owner_runtime_mutex);");
    const auto snapshot_pos = body.find("owner_status_snapshot_locked();");
    const auto lock_scope_end = body.find("\n  }\n", snapshot_pos);
    const auto mapping_pos = body.find("allocate_mapping", lock_scope_end);

    ASSERT_NE(lock_pos, std::string::npos) << start_marker;
    ASSERT_NE(snapshot_pos, std::string::npos) << start_marker;
    ASSERT_NE(lock_scope_end, std::string::npos) << start_marker;
    ASSERT_NE(mapping_pos, std::string::npos) << start_marker;
    EXPECT_LT(lock_pos, snapshot_pos) << start_marker;
    EXPECT_LT(snapshot_pos, lock_scope_end) << start_marker;
    EXPECT_LT(lock_scope_end, mapping_pos) << start_marker;
    const auto mapping_body = body.substr(mapping_pos);
    for (const char* protected_access : {
             "owner_scheduler_state.",
             "owner_threads.empty()",
             "owner_threads.size()",
             "owner_thread_stopping",
             "owner_executor_callback_main_cleanups",
             "owner_deferred_target_releases",
             "owner_executor_last_budget_yield_owner",
             "owner_executor_last_budget_yield_backlog",
             "owner_executor_last_budget_yield_safe_backlog",
             "owner_mailbox_total_depth()",
             "owner_executor_runnable_queue_depth()",
             "owner_executor_safe_queue_depth()",
             "owner_main_required_queue_depth()",
             "owner_runnable_owner_count()",
             "owner_main_queue_total_depth()",
             "owner_main_runnable_owner_count()",
         }) {
      EXPECT_EQ(mapping_body.find(protected_access), std::string::npos)
          << start_marker << ": " << protected_access;
    }
  };

  assert_mapping_builds_after_snapshot(
      "mapping_t *vm_owner_thread_status()",
      "mapping_t *vm_owner_runtime_status()");
  assert_mapping_builds_after_snapshot(
      "mapping_t *vm_owner_runtime_status()",
      "#ifdef DEBUGMALLOC_EXTENSIONS");
}

TEST_F(DriverTest, TestOwnerDebugReferenceMarkUsesCoordinatorLockBoundary) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto start = owner_source.find("void vm_owner_mark_runtime_refs()");
  const auto end = owner_source.find("#endif", start);

  ASSERT_NE(start, std::string::npos);
  ASSERT_NE(end, std::string::npos);
  ASSERT_GT(end, start);
  const auto body = owner_source.substr(start, end - start);
  const auto lock_pos =
      body.find("std::lock_guard<std::mutex> lock(owner_runtime_mutex);");
  const auto scheduler_pos =
      body.find("owner_scheduler_state.mark_debug_refs(seen);");
  const auto lock_scope_end = body.find("\n  }\n", scheduler_pos);
  const auto trace_pos = body.find("owner_trace_store.mark_debug_refs(seen);");
  const auto future_pos = body.find("owner_future_store.mark_debug_refs(seen);");

  ASSERT_NE(lock_pos, std::string::npos);
  ASSERT_NE(scheduler_pos, std::string::npos);
  ASSERT_NE(lock_scope_end, std::string::npos);
  ASSERT_NE(trace_pos, std::string::npos);
  ASSERT_NE(future_pos, std::string::npos);
  EXPECT_LT(lock_pos, scheduler_pos);
  EXPECT_LT(scheduler_pos, lock_scope_end);
  EXPECT_LT(lock_scope_end, trace_pos);
  EXPECT_LT(trace_pos, future_pos);
}

TEST_F(DriverTest, TestOwnerMainQueueDepthGetterUsesRuntimeLock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto start = owner_source.find("int64_t vm_owner_main_queue_total_depth()");
  const auto end = owner_source.find("uint64_t vm_owner_enqueue_executor_task", start);

  ASSERT_NE(start, std::string::npos);
  ASSERT_NE(end, std::string::npos);
  ASSERT_GT(end, start);
  const auto body = owner_source.substr(start, end - start);
  const auto lock_pos =
      body.find("std::lock_guard<std::mutex> lock(owner_runtime_mutex);");
  const auto read_pos = body.find("return owner_main_queue_total_depth();");

  ASSERT_NE(lock_pos, std::string::npos);
  ASSERT_NE(read_pos, std::string::npos);
  EXPECT_LT(lock_pos, read_pos);
}

TEST_F(DriverTest, TestOwnerDrainMailboxSnapshotsRemainingDepthUnderRuntimeLock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto start = owner_source.find("mapping_t *vm_owner_drain_mailbox(");
  const auto end = owner_source.find("mapping_t *vm_owner_purge_mailbox(", start);

  ASSERT_NE(start, std::string::npos);
  ASSERT_NE(end, std::string::npos);
  ASSERT_GT(end, start);
  const auto body = owner_source.substr(start, end - start);
  const auto snapshot_pos =
      body.find("remaining = owner_mailbox_depth(normalized_owner_id);");
  const auto lock_pos = body.rfind(
      "std::lock_guard<std::mutex> lock(owner_runtime_mutex);", snapshot_pos);
  const auto lock_scope_end = body.find("\n  }\n", snapshot_pos);
  const auto mapping_pos = body.find("auto *map = allocate_mapping", snapshot_pos);

  ASSERT_NE(snapshot_pos, std::string::npos);
  ASSERT_NE(lock_pos, std::string::npos);
  ASSERT_NE(lock_scope_end, std::string::npos);
  ASSERT_NE(mapping_pos, std::string::npos);
  EXPECT_LT(lock_pos, snapshot_pos);
  EXPECT_LT(snapshot_pos, lock_scope_end);
  EXPECT_LT(lock_scope_end, mapping_pos);
  const auto mapping_body = body.substr(mapping_pos);
  EXPECT_NE(mapping_body.find("add_mapping_pair(map, \"remaining\", remaining);"),
            std::string::npos);
  EXPECT_EQ(mapping_body.find("owner_mailbox_depth(normalized_owner_id)"),
            std::string::npos);
}

TEST_F(DriverTest, TestOwnerDrainProcessesDetachedTasksAfterRuntimeUnlock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto start = owner_source.find("mapping_t *vm_owner_drain_mailbox(");
  const auto end = owner_source.find("mapping_t *vm_owner_purge_mailbox(", start);

  ASSERT_NE(start, std::string::npos);
  ASSERT_NE(end, std::string::npos);
  ASSERT_GT(end, start);
  const auto body = owner_source.substr(start, end - start);
  const auto tasks_pos =
      body.find("std::vector<OwnerMailboxTask> drained_tasks;");
  const auto lock_pos = body.find(
      "std::lock_guard<std::mutex> lock(owner_runtime_mutex);", tasks_pos);
  const auto detach_pos = body.find(
      "drained_tasks = owner_scheduler_state.drain_owner_mailbox(", lock_pos);
  const auto lock_scope_end = body.find("\n  }\n", detach_pos);
  const auto trace_pos =
      body.find("append_owner_task_trace(task, \"drained\");", detach_pos);
  const auto record_pos =
      body.find("record_owner_mailbox_task_drained(task);", detach_pos);
  const auto total_pos = body.find(
      "total_drained.fetch_add(drained_tasks.size(), std::memory_order_relaxed);",
      detach_pos);
  const auto array_pos = body.find("auto *tasks = allocate_array", detach_pos);

  ASSERT_NE(tasks_pos, std::string::npos);
  ASSERT_NE(lock_pos, std::string::npos);
  ASSERT_NE(detach_pos, std::string::npos);
  ASSERT_NE(lock_scope_end, std::string::npos);
  ASSERT_NE(trace_pos, std::string::npos);
  ASSERT_NE(record_pos, std::string::npos);
  ASSERT_NE(total_pos, std::string::npos);
  ASSERT_NE(array_pos, std::string::npos);
  EXPECT_LT(lock_pos, detach_pos);
  EXPECT_LT(detach_pos, lock_scope_end);
  EXPECT_LT(lock_scope_end, trace_pos);
  EXPECT_LT(lock_scope_end, record_pos);
  EXPECT_LT(lock_scope_end, total_pos);
  EXPECT_LT(total_pos, array_pos);
}

TEST_F(DriverTest,
       TestOwnerQueuedFutureCancellationProcessesRemovedTaskAfterRuntimeUnlock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto start =
      owner_source.find("mapping_t *vm_owner_future_cancel_queued_task(");
  const auto end =
      owner_source.find("mapping_t *vm_owner_future_timeout(", start);

  ASSERT_NE(start, std::string::npos);
  ASSERT_NE(end, std::string::npos);
  ASSERT_GT(end, start);
  const auto body = owner_source.substr(start, end - start);
  const auto terminal_pos =
      body.find("mark_owner_future_failed_terminal(");
  const auto lock_pos = body.find(
      "std::lock_guard<std::mutex> lock(owner_runtime_mutex);");
  const auto detach_pos =
      body.find("owner_scheduler_state.remove_owner_task(", lock_pos);
  const auto lock_scope_end = body.find("\n    }\n", detach_pos);
  const auto kind_pos =
      body.find("queued_task_kind = removed.task_kind;", detach_pos);
  const auto trace_pos = body.find("append_owner_task_trace(", detach_pos);
  const auto record_pos =
      body.find("record_owner_mailbox_task_drained(removed);", detach_pos);
  const auto total_pos = body.find(
      "total_drained.fetch_add(1, std::memory_order_relaxed);", detach_pos);
  const auto cleanup_pos = body.find(
      "schedule_owner_executor_callback_cleanup_on_main(removed);", detach_pos);
  const auto release_pos =
      body.find("release_owner_task_target(&removed);", detach_pos);

  ASSERT_NE(terminal_pos, std::string::npos);
  ASSERT_NE(lock_pos, std::string::npos);
  ASSERT_NE(detach_pos, std::string::npos);
  ASSERT_NE(lock_scope_end, std::string::npos);
  ASSERT_NE(kind_pos, std::string::npos);
  ASSERT_NE(trace_pos, std::string::npos);
  ASSERT_NE(record_pos, std::string::npos);
  ASSERT_NE(total_pos, std::string::npos);
  ASSERT_NE(cleanup_pos, std::string::npos);
  ASSERT_NE(release_pos, std::string::npos);
  EXPECT_LT(terminal_pos, lock_pos);
  EXPECT_LT(lock_pos, detach_pos);
  EXPECT_LT(detach_pos, lock_scope_end);
  EXPECT_LT(lock_scope_end, kind_pos);
  EXPECT_LT(kind_pos, trace_pos);
  EXPECT_LT(lock_scope_end, trace_pos);
  EXPECT_LT(lock_scope_end, record_pos);
  EXPECT_LT(lock_scope_end, total_pos);
  EXPECT_LT(lock_scope_end, cleanup_pos);
  EXPECT_LT(trace_pos, cleanup_pos);
  EXPECT_LT(cleanup_pos, release_pos);
  EXPECT_LT(record_pos, release_pos);
  EXPECT_LT(total_pos, release_pos);
  EXPECT_EQ(body.find("enqueue_owner_executor_callback_cleanup_locked(removed);"),
            std::string::npos);
}

TEST_F(DriverTest, TestOwnerPurgeProcessesDetachedTasksAfterRuntimeUnlock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto start = owner_source.find("mapping_t *vm_owner_purge_mailbox(");
  const auto end = owner_source.find("mapping_t *vm_owner_schedule(", start);

  ASSERT_NE(start, std::string::npos);
  ASSERT_NE(end, std::string::npos);
  ASSERT_GT(end, start);
  const auto body = owner_source.substr(start, end - start);
  const auto tasks_pos =
      body.find("std::vector<OwnerMailboxTask> purged_tasks;");
  const auto lock_pos = body.find(
      "std::lock_guard<std::mutex> lock(owner_runtime_mutex);", tasks_pos);
  const auto detach_pos = body.find(
      "purged_tasks = owner_scheduler_state.remove_owner_mailbox(", lock_pos);
  const auto lock_scope_end = body.find("\n  }\n", detach_pos);
  const auto terminal_pos =
      body.find("complete_owner_future_for_task_locked(", lock_scope_end);
  const auto release_pos =
      body.find("release_owner_task_target(&task);", lock_scope_end);
  const auto mapping_pos = body.find("auto *map = allocate_mapping", lock_scope_end);

  ASSERT_NE(tasks_pos, std::string::npos);
  ASSERT_NE(lock_pos, std::string::npos);
  ASSERT_NE(detach_pos, std::string::npos);
  ASSERT_NE(lock_scope_end, std::string::npos);
  ASSERT_NE(terminal_pos, std::string::npos);
  ASSERT_NE(release_pos, std::string::npos);
  ASSERT_NE(mapping_pos, std::string::npos);
  EXPECT_LT(lock_pos, detach_pos);
  EXPECT_LT(detach_pos, lock_scope_end);
  EXPECT_LT(lock_scope_end, terminal_pos);
  EXPECT_LT(lock_scope_end, release_pos);
  EXPECT_LT(lock_scope_end, mapping_pos);
  EXPECT_NE(body.find("schedule_owner_executor_callback_cleanup_on_main(task);"),
            std::string::npos);
  EXPECT_EQ(body.find("enqueue_owner_executor_callback_cleanup_locked(task);"),
            std::string::npos);
}

TEST_F(DriverTest, TestOwnerScheduleProcessesDetachedTasksAfterRuntimeUnlock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto start = owner_source.find("mapping_t *vm_owner_schedule(");
  const auto end = owner_source.find("mapping_t *vm_owner_mailbox_status(", start);

  ASSERT_NE(start, std::string::npos);
  ASSERT_NE(end, std::string::npos);
  ASSERT_GT(end, start);
  const auto body = owner_source.substr(start, end - start);
  const auto tasks_pos =
      body.find("std::vector<OwnerMailboxTask> scheduled_tasks;");
  const auto lock_pos = body.find(
      "std::lock_guard<std::mutex> lock(owner_runtime_mutex);", tasks_pos);
  const auto detach_pos =
      body.find("pop_next_schedulable_task(&task, false)", lock_pos);
  const auto lock_scope_end = body.find("\n  }\n", detach_pos);
  const auto array_pos = body.find("auto *tasks = allocate_array", lock_scope_end);
  const auto process_pos =
      body.find("for (auto &task : scheduled_tasks)", lock_scope_end);
  const auto release_pos =
      body.find("release_owner_task_target(&task);", lock_scope_end);

  ASSERT_NE(tasks_pos, std::string::npos);
  ASSERT_NE(lock_pos, std::string::npos);
  ASSERT_NE(detach_pos, std::string::npos);
  ASSERT_NE(lock_scope_end, std::string::npos);
  ASSERT_NE(array_pos, std::string::npos);
  ASSERT_NE(process_pos, std::string::npos);
  ASSERT_NE(release_pos, std::string::npos);
  EXPECT_LT(lock_pos, detach_pos);
  EXPECT_LT(detach_pos, lock_scope_end);
  EXPECT_LT(lock_scope_end, array_pos);
  EXPECT_LT(lock_scope_end, process_pos);
  EXPECT_LT(lock_scope_end, release_pos);
  EXPECT_NE(body.find("schedule_owner_executor_callback_cleanup_on_main(task);"),
            std::string::npos);
  EXPECT_EQ(body.find("enqueue_owner_executor_callback_cleanup_locked(task);"),
            std::string::npos);
}

TEST_F(DriverTest,
       TestOwnerMainDrainAppendsDetachedTaskTracesAfterRuntimeUnlock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  auto expect_no_trace_under_runtime_lock =
      [&](const char *start_marker, const char *end_marker) {
        const auto start = owner_source.find(start_marker);
        const auto end = owner_source.find(end_marker, start);

        ASSERT_NE(start, std::string::npos) << start_marker;
        ASSERT_NE(end, std::string::npos) << end_marker;
        ASSERT_GT(end, start) << start_marker;
        const auto body = owner_source.substr(start, end - start);
        ASSERT_NE(body.find("append_owner_task_trace("), std::string::npos)
            << start_marker;

        constexpr const char *lock_marker =
            "std::lock_guard<std::mutex> lock(owner_runtime_mutex);";
        size_t search_pos = 0;
        int lock_count = 0;
        while (true) {
          const auto lock_pos = body.find(lock_marker, search_pos);
          if (lock_pos == std::string::npos) {
            break;
          }
          std::vector<size_t> open_scopes;
          for (size_t i = 0; i < lock_pos; ++i) {
            if (body[i] == '{') {
              open_scopes.push_back(i);
            } else if (body[i] == '}') {
              ASSERT_FALSE(open_scopes.empty()) << start_marker;
              open_scopes.pop_back();
            }
          }
          ASSERT_FALSE(open_scopes.empty()) << start_marker;
          const auto lock_scope_start = open_scopes.back();

          size_t depth = 0;
          size_t lock_scope_end = std::string::npos;
          for (size_t i = lock_scope_start; i < body.size(); ++i) {
            if (body[i] == '{') {
              ++depth;
            } else if (body[i] == '}') {
              ASSERT_GT(depth, 0u) << start_marker;
              --depth;
              if (depth == 0) {
                lock_scope_end = i + 1;
                break;
              }
            }
          }
          ASSERT_NE(lock_scope_end, std::string::npos) << start_marker;
          ASSERT_GT(lock_scope_end, lock_pos) << start_marker;
          const auto locked_body = body.substr(
              lock_pos, lock_scope_end - lock_pos);
          EXPECT_EQ(locked_body.find("append_owner_task_trace("),
                    std::string::npos)
              << start_marker;
          search_pos = lock_pos + 1;
          ++lock_count;
        }
        EXPECT_GT(lock_count, 0) << start_marker;
      };

  expect_no_trace_under_runtime_lock(
      "int drain_owner_executor_callback_cleanups(",
      "void record_owner_mailbox_task_drained(");
  expect_no_trace_under_runtime_lock(
      "VMOwnerMainDrainResult vm_owner_drain_main_tasks_with_budget(",
      "int vm_owner_drain_main_tasks(");
}

TEST_F(DriverTest, TestOwnerMailboxStatusBuildsMappingAfterRuntimeUnlock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto start = owner_source.find("mapping_t *vm_owner_mailbox_status(");
  const auto end = owner_source.find("mapping_t *vm_owner_task_trace(", start);

  ASSERT_NE(start, std::string::npos);
  ASSERT_NE(end, std::string::npos);
  ASSERT_GT(end, start);
  const auto body = owner_source.substr(start, end - start);
  const auto snapshot_pos = body.find("int64_t owner_queue_depth;");
  const auto lock_pos = body.find(
      "std::lock_guard<std::mutex> lock(owner_runtime_mutex);", snapshot_pos);
  const auto lock_scope_end = body.find("\n  }\n", lock_pos);
  const auto mapping_pos = body.find("auto *map = allocate_mapping", lock_scope_end);

  ASSERT_NE(snapshot_pos, std::string::npos);
  ASSERT_NE(lock_pos, std::string::npos);
  ASSERT_NE(lock_scope_end, std::string::npos);
  ASSERT_NE(mapping_pos, std::string::npos);
  EXPECT_LT(snapshot_pos, lock_pos);
  EXPECT_LT(lock_pos, lock_scope_end);
  EXPECT_LT(lock_scope_end, mapping_pos);
  const auto mapping_body = body.substr(mapping_pos);
  for (const char* protected_read : {
           "owner_mailbox_depth(",
           "owner_executor_runnable_queue_depth(",
           "owner_executor_safe_queue_depth(",
           "owner_main_required_queue_depth(",
           "owner_main_queue_depth(",
           "owner_mailbox_total_depth()",
           "owner_main_queue_total_depth()",
           "owner_mailbox_active_owners()",
           "owner_scheduler_state.active_main_owner_count()",
       }) {
    EXPECT_EQ(mapping_body.find(protected_read), std::string::npos)
        << protected_read;
  }
}

TEST_F(DriverTest, TestGatewayFutureCompletionExposesMainThreadCpuCounter) {
  const auto gateway_header = read_source_file_for_test("../src/packages/gateway/gateway.h");
  const auto gateway_source =
      read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto gateway_status = read_source_file_for_test("../src/packages/gateway/gateway.cc");

  ASSERT_NE(gateway_source.find("get_current_thread_cpu_time_ns()"),
            std::string::npos);
  ASSERT_NE(gateway_header.find("future_watch_main_completion_thread_cpu_ns_total"),
            std::string::npos);
  ASSERT_NE(gateway_header.find("future_watch_main_completion_thread_cpu_unavailable"),
            std::string::npos);
  ASSERT_NE(gateway_status.find("gateway_future_watch_main_completion_thread_cpu_total_us"),
            std::string::npos);
  ASSERT_NE(gateway_status.find("gateway_future_watch_main_completion_thread_cpu_unavailable"),
            std::string::npos);
}

TEST_F(DriverTest, TestGatewayRoomProjectionExposesDedicatedThreadCpuCounters) {
  const auto gateway_header = read_source_file_for_test("../src/packages/gateway/gateway.h");
  const auto gateway_source =
      read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto gateway_status = read_source_file_for_test("../src/packages/gateway/gateway.cc");

  ASSERT_NE(gateway_header.find(
                "room_output_projection_worker_thread_cpu_ns_total"),
            std::string::npos);
  ASSERT_NE(gateway_header.find(
                "room_output_projection_worker_thread_cpu_ns_max"),
            std::string::npos);
  ASSERT_NE(gateway_header.find(
                "room_output_projection_worker_thread_cpu_samples"),
            std::string::npos);
  ASSERT_NE(gateway_header.find(
                "room_output_projection_worker_thread_cpu_unavailable"),
            std::string::npos);
  ASSERT_NE(gateway_header.find(
                "room_output_projection_inline_thread_cpu_ns_total"),
            std::string::npos);
  ASSERT_NE(gateway_header.find(
                "room_output_projection_inline_thread_cpu_ns_max"),
            std::string::npos);
  ASSERT_NE(gateway_header.find(
                "room_output_projection_inline_thread_cpu_samples"),
            std::string::npos);
  ASSERT_NE(gateway_header.find(
                "room_output_projection_inline_thread_cpu_unavailable"),
            std::string::npos);
  ASSERT_NE(gateway_source.find(
                "worker_cpu_started_ns = get_current_thread_cpu_time_ns()"),
            std::string::npos);
  ASSERT_NE(gateway_source.find(
                "cpu_started_ns = get_current_thread_cpu_time_ns()"),
            std::string::npos);
  ASSERT_NE(gateway_status.find(
                "gateway_room_output_projection_worker_thread_cpu"),
            std::string::npos);
  ASSERT_NE(gateway_status.find(
                "gateway_room_output_projection_worker_thread_cpu_unavailable"),
            std::string::npos);
  ASSERT_NE(gateway_status.find(
                "gateway_room_output_projection_inline_thread_cpu"),
            std::string::npos);
  ASSERT_NE(gateway_status.find(
                "gateway_room_output_projection_inline_thread_cpu_unavailable"),
            std::string::npos);
}

TEST_F(DriverTest, TestGatewayReceiveApplyExposesMainThreadCpuCounter) {
  const auto gateway_header = read_source_file_for_test("../src/packages/gateway/gateway.h");
  const auto gateway_source = read_source_file_for_test("../src/packages/gateway/gateway.cc");
  const auto gateway_status = read_source_file_for_test("../src/packages/gateway/gateway.cc");

  ASSERT_NE(gateway_header.find("receive_apply_thread_cpu_ns_total"), std::string::npos);
  ASSERT_NE(gateway_header.find("receive_apply_thread_cpu_ns_max"), std::string::npos);
  ASSERT_NE(gateway_header.find("receive_apply_thread_cpu_samples"), std::string::npos);
  ASSERT_NE(gateway_header.find("receive_apply_thread_cpu_unavailable"), std::string::npos);
  ASSERT_NE(gateway_source.find("auto apply_cpu_started_at = get_current_thread_cpu_time_ns()"),
            std::string::npos);
  ASSERT_NE(gateway_source.find("gateway_record_thread_cpu("), std::string::npos);
  ASSERT_NE(gateway_status.find("gateway_receive_apply_total_us"), std::string::npos);
  ASSERT_NE(gateway_status.find("gateway_receive_apply_thread_cpu_total_us"), std::string::npos);
  ASSERT_NE(gateway_status.find("gateway_receive_apply_thread_cpu_unavailable"),
            std::string::npos);
}

TEST_F(DriverTest, TestGatewayReceiveApplyUsesPerformanceWallClock) {
  const auto gateway_source = read_source_file_for_test("../src/packages/gateway/gateway.cc");
  const auto apply_pos = gateway_source.find(
      "auto apply_started_at = get_current_performance_wall_time_ns()");

  ASSERT_NE(apply_pos, std::string::npos);
  ASSERT_NE(gateway_source.find("gateway_record_performance_wall(", apply_pos),
            std::string::npos);
  ASSERT_NE(gateway_source.find("std::chrono::steady_clock::now()"),
            std::string::npos);
}

TEST_F(DriverTest, TestSaveSvalueDepthIsThreadLocal) {
  save_svalue_depth = 41;
  std::atomic<int> worker_depth{0};
  std::thread worker([&] {
    save_svalue_depth = 7;
    worker_depth.store(save_svalue_depth, std::memory_order_relaxed);
  });
  worker.join();

  ASSERT_EQ(worker_depth.load(std::memory_order_relaxed), 7);
  ASSERT_EQ(save_svalue_depth, 41);
  save_svalue_depth = 0;
}

TEST_F(DriverTest, TestStringIndexRejectsEmptySource) {
  ASSERT_EQ(u8_egc_index_as_single_codepoint(nullptr, 0, 0), -2);
  ASSERT_EQ(u8_egc_index_as_single_codepoint("", 0, 0), -2);
}

TEST_F(DriverTest, TestStringIndexHandlesConcurrentEgcIterators) {
  const char* ascii = "abcdef";
  const char* utf8 = "\xE4\xB8\xADx";
  std::atomic<int> failures{0};
  std::vector<std::thread> workers;

  for (int t = 0; t < 8; t++) {
    workers.emplace_back([&] {
      for (int i = 0; i < 2000; i++) {
        if (u8_egc_index_as_single_codepoint(ascii, 6, 2) != 'c') failures++;
        if (u8_egc_index_as_single_codepoint(utf8, 4, 0) != 0x4E2D) failures++;
        if (u8_egc_index_as_single_codepoint(utf8, 4, 9) != -2) failures++;
      }
    });
  }

  for (auto& worker : workers) worker.join();
  ASSERT_EQ(failures.load(), 0);
}

TEST_F(DriverTest, TestEgcAsciiSubrangesAndBoundedFind) {
  std::string ascii(2048, 'x');
  for (size_t i = 10; i < ascii.size(); i += 11) ascii[i] = ' ';

  EGCIterator iterator(ascii.data(), static_cast<int32_t>(ascii.size()));
  ASSERT_TRUE(iterator.ok());
  ASSERT_TRUE(iterator.is_ascii());
  iterator.reset(ascii.data() + 11, static_cast<int32_t>(ascii.size() - 11));
  EXPECT_TRUE(iterator.ok());
  EXPECT_TRUE(iterator.is_ascii());
  EXPECT_EQ(ascii.data() + 11, iterator.data());

  const char bounded[] = "\xE4\xB8\xAD--";
  EGCIterator partial(bounded, 4);  // 你-; the second dash is outside the range.
  ASSERT_TRUE(partial.ok());
  ASSERT_FALSE(partial.is_ascii());
  EXPECT_EQ(-1, u8_egc_find_as_offset(partial, "--", 2, false));

  EXPECT_FALSE(EGCIterator::scan_is_ascii("hello", -1));
  EGCIterator invalid_length("hello", -3);
  EXPECT_FALSE(invalid_length.ok());
}

TEST_F(DriverTest, TestEgcSplitPreservesAsciiAndCrLfClusters) {
  const char* ascii = "hello";
  auto ascii_parts = u8_egc_split(ascii, 5);
  ASSERT_EQ(5u, ascii_parts.size());
  EXPECT_EQ("h", ascii_parts[0]);
  EXPECT_EQ("o", ascii_parts[4]);

  const char* crlf = "a\r\nb";
  auto crlf_parts = u8_egc_split(crlf, 4);
  ASSERT_EQ(3u, crlf_parts.size());
  EXPECT_EQ("a", crlf_parts[0]);
  EXPECT_EQ(std::string_view("\r\n", 2), crlf_parts[1]);
  EXPECT_EQ("b", crlf_parts[2]);
}

TEST_F(DriverTest, TestMappingNodeFreelistIsOwnerThreadLocal) {
  std::atomic<int> failures{0};
  std::vector<std::thread> workers;

  for (int t = 0; t < 8; t++) {
    workers.emplace_back([&] {
      mapping_t owner_local_map{};
      owner_local_map.count = 0;
      for (int i = 0; i < 4000; i++) {
        auto* node = new_map_node();
        if (node == nullptr) {
          failures++;
          continue;
        }
        node->values[0] = const0u;
        node->values[1] = const0u;
        free_node(&owner_local_map, node);
      }
    });
  }

  for (auto& worker : workers) worker.join();
  ASSERT_EQ(failures.load(), 0);
}

TEST_F(DriverTest, TestCompileDumpProgWorks) {
  ScopedCurrentObjectAsMaster master_scope;
  const char* file = "single/master.c";
  struct object_t* obj = nullptr;

  error_context_t econ{};
  save_context(&econ);
  try {
    obj = find_object(file);
  } catch (...) {
    restore_context(&econ);
    FAIL();
  }
  pop_context(&econ);

  ASSERT_NE(obj, nullptr);
  ASSERT_NE(obj->prog, nullptr);

  dump_prog(obj->prog, stdout, 1 | 2);

  free_object(&obj, "DriverTest::TestCompileDumpProgWorks");
}

TEST_F(DriverTest, TestCompileDumpProgHandlesLongFunctionNames) {
  std::string function_name(3000, 'f');
  std::string source = "void " + function_name + "() {}\n";
  std::istringstream stream(source);
  auto program = compile_file(std::make_unique<IStreamLexStream>(stream),
                              "long_dump_prog_test");

  ASSERT_NE(program, nullptr);
  FILE* output = tmpfile();
  ASSERT_NE(output, nullptr);
  dump_prog(program, output, 1);
  ASSERT_EQ(fclose(output), 0);
  deallocate_program(program);
}

TEST_F(DriverTest, TestCallOtherDiagnosticsAvoidFixedMessageBuffers) {
  const auto source = read_source_file_for_test("../src/vm/internal/apply.cc");

  ASSERT_EQ(source.find("char buf[1024]"), std::string::npos);
  ASSERT_EQ(source.find("sprintf(buf"), std::string::npos);
  ASSERT_NE(source.find("auto message = fmt::format("), std::string::npos);
  ASSERT_NE(source.find("error(\"%s\", message.c_str())"), std::string::npos);
}

TEST_F(DriverTest, TestAddVmessageUsesThreadLocalBoundedBuffer) {
  const auto source = read_source_file_for_test("../src/comm.cc");

  ASSERT_NE(source.find("static thread_local char buf[LARGEST_PRINTABLE_STRING + 1];"),
            std::string::npos);
  ASSERT_NE(source.find("static_cast<size_t>(result) < sizeof(buf)"), std::string::npos);
}

TEST_F(DriverTest, TestUserLogonSchedulingUsesCancellableEvent) {
  const std::string source_root = std::string(TESTSUITE_DIR) + "/../src/";
  const auto comm_source = read_source_file_for_test((source_root + "comm.cc").c_str());
  ASSERT_NE(comm_source.find("schedule_user_logon("), std::string::npos);
  ASSERT_NE(comm_source.find("cancel_user_logon"), std::string::npos);

  const auto websocket_source = read_source_file_for_test((source_root + "net/websocket.cc").c_str());
  ASSERT_NE(websocket_source.find("bool websocket_establish_session("), std::string::npos);
  ASSERT_NE(websocket_source.find("schedule_user_logon("), std::string::npos);
  ASSERT_NE(websocket_source.find("void websocket_session_teardown"), std::string::npos);
  for (const auto *path : {"net/ws_ascii.cc", "net/ws_telnet.cc"}) {
    const auto source = read_source_file_for_test((source_root + path).c_str());
    ASSERT_NE(source.find("websocket_establish_session("), std::string::npos) << path;
    ASSERT_NE(source.find("websocket_session_teardown"), std::string::npos) << path;
  }
  const auto gateway_source = read_source_file_for_test(
      (source_root + "packages/gateway/gateway_session.cc").c_str());
  ASSERT_NE(gateway_source.find("cancel_user_logon"), std::string::npos);

  auto *ip = user_add();
  ASSERT_NE(ip, nullptr);
  ASSERT_TRUE(schedule_user_logon(g_event_base, ip));
  ASSERT_NE(ip->ev_logon, nullptr);
  cancel_user_logon(ip);
  ASSERT_EQ(ip->ev_logon, nullptr);
  ASSERT_FALSE(schedule_user_logon(nullptr, ip));
  user_del(ip);
  FREE(ip);
}

TEST_F(DriverTest, TestWebsocketPrelogonTeardownKeepsSessionIdentity) {
  ASSERT_NE(master_ob, nullptr);
  ASSERT_EQ(master_ob->interactive, nullptr);
  const auto master_refs = master_ob->ref;
  const auto user_count = users_num(true);
  const auto event_count = event_base_get_num_events(g_event_base, EVENT_BASE_COUNT_ADDED);

  for (bool another_session : {false, true}) {
    SCOPED_TRACE(another_session);
    interactive_t* attached = nullptr;
    if (another_session) {
      attached = user_add();
      attached->ob = master_ob;
      attached->connection_type = PORT_TYPE_WEBSOCKET;
      attached->iflags = HANDSHAKE_COMPLETE;
      attached->addr.ss_family = AF_INET;
      attached->addrlen = sizeof(sockaddr_in);
      master_ob->interactive = attached;
      add_ref(master_ob, "websocket teardown test attached user");
    }

    auto* pending = user_add();
    pending->ob = master_ob;
    pending->connection_type = PORT_TYPE_WEBSOCKET;
    pending->iflags = HANDSHAKE_COMPLETE;
    pending->ev_command = evtimer_new(g_event_base, [](evutil_socket_t, short, void*) {}, nullptr);
    ASSERT_NE(pending->ev_command, nullptr);
    ASSERT_TRUE(schedule_user_logon(g_event_base, pending));
    auto* buffer = evbuffer_new();
    ASSERT_NE(buffer, nullptr);
    auto* session_user = pending;

    websocket_session_teardown(nullptr, &session_user, &buffer);
    EXPECT_EQ(session_user, nullptr);
    EXPECT_EQ(buffer, nullptr);
    EXPECT_EQ(users_num(true), user_count + (another_session ? 1 : 0));
    EXPECT_EQ(master_ob->interactive, attached);
    EXPECT_EQ(master_ob->ref, master_refs + (another_session ? 1 : 0));
    EXPECT_EQ(event_base_get_num_events(g_event_base, EVENT_BASE_COUNT_ADDED), event_count);
    websocket_session_teardown(nullptr, &session_user, &buffer);

    // Keep the fail-first run isolated when the old teardown leaves the pending user registered.
    if (std::find(users().begin(), users().end(), pending) != users().end()) {
      cancel_user_logon(pending);
      event_free(pending->ev_command);
      user_del(pending);
      FREE(pending);
    }
    if (master_ob->interactive) {
      remove_interactive(master_ob, 1);
    }
    EXPECT_EQ(users_num(true), user_count);
    EXPECT_EQ(master_ob->ref, master_refs);
  }
}

#ifndef _WIN32
TEST_F(DriverTest, TestTransportPrelogonCleanupReleasesDescriptors) {
  ASSERT_EQ(master_ob->interactive, nullptr);
  const auto master_refs = master_ob->ref;
  const auto user_count = users_num(true);
  std::unique_ptr<event_base, decltype(&event_base_free)> base(event_base_new(), event_base_free);
  ASSERT_NE(base, nullptr);

  for (bool tls : {false, true}) {
    SCOPED_TRACE(tls);
    int descriptors[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors), 0);
    ASSERT_EQ(evutil_make_socket_nonblocking(descriptors[0]), 0);
    auto* ip = user_add();
    ip->ob = master_ob;
    ip->connection_type = PORT_TYPE_TELNET;
    ip->fd = descriptors[0];
    if (tls) {
      auto* context = SSL_CTX_new(TLS_server_method());
      ASSERT_NE(context, nullptr);
      ip->ssl = SSL_new(context);
      SSL_CTX_free(context);
      ASSERT_NE(ip->ssl, nullptr);
      ip->ev_buffer = bufferevent_openssl_socket_new(
          base.get(), ip->fd, ip->ssl, BUFFEREVENT_SSL_ACCEPTING, BEV_OPT_CLOSE_ON_FREE);
    } else {
      ip->ev_buffer = bufferevent_socket_new(base.get(), ip->fd, BEV_OPT_CLOSE_ON_FREE);
    }
    ASSERT_NE(ip->ev_buffer, nullptr);
    ip->ev_command = evtimer_new(base.get(), [](evutil_socket_t, short, void*) {}, nullptr);
    ASSERT_NE(ip->ev_command, nullptr);
    ip->telnet = net_telnet_init(ip);
    ASSERT_NE(ip->telnet, nullptr);
    UErrorCode status = U_ZERO_ERROR;
    ip->trans = ucnv_open("UTF-8", &status);
    ASSERT_TRUE(U_SUCCESS(status));
    ASSERT_NE(ip->trans, nullptr);
    ASSERT_TRUE(schedule_user_logon(base.get(), ip));

    remove_user_connection(ip);
    // bufferevent_free() cancels callbacks, then schedules the transport finalizer.
    EXPECT_NE(event_base_loop(base.get(), EVLOOP_NONBLOCK), -1);
    EXPECT_EQ(users_num(true), user_count);
    EXPECT_EQ(master_ob->interactive, nullptr);
    EXPECT_EQ(master_ob->ref, master_refs);
    EXPECT_EQ(event_base_get_num_events(base.get(), EVENT_BASE_COUNT_ADDED), 0);
    errno = 0;
    EXPECT_EQ(fcntl(descriptors[0], F_GETFD), -1);
    EXPECT_EQ(errno, EBADF);
    evutil_closesocket(descriptors[1]);
  }
}
#endif

TEST_F(DriverTest, TestMudPortPrelogonInvalidLengthCleansUser) {
  ASSERT_EQ(master_ob->interactive, nullptr);
  const auto master_refs = master_ob->ref;
  const auto user_count = users_num(true);
  std::unique_ptr<event_base, decltype(&event_base_free)> base(event_base_new(), event_base_free);
  ASSERT_NE(base, nullptr);
  const uint32_t length = htonl(MAX_TEXT);

  for (bool buffered_header : {false, true}) {
    SCOPED_TRACE(buffered_header);
    auto* ip = user_add();
    ip->ob = master_ob;
    ip->fd = -1;
    ip->connection_type = PORT_TYPE_MUD;
    ip->ev_buffer = bufferevent_socket_new(base.get(), -1, 0);
    ASSERT_NE(ip->ev_buffer, nullptr);
    ip->ev_command = evtimer_new(base.get(), [](evutil_socket_t, short, void*) {}, nullptr);
    ASSERT_NE(ip->ev_command, nullptr);
    ASSERT_TRUE(schedule_user_logon(base.get(), ip));
    if (buffered_header) {
      memcpy(ip->text, &length, sizeof(length));
      ip->text_end = sizeof(length);
    } else {
      auto* input = bufferevent_get_input(ip->ev_buffer);
      ASSERT_EQ(evbuffer_unfreeze(input, 0), 0);
      ASSERT_EQ(evbuffer_add(input, &length, sizeof(length)), 0);
      ASSERT_EQ(evbuffer_freeze(input, 0), 0);
    }

    get_user_data(ip);
    const bool registered = std::find(users().begin(), users().end(), ip) != users().end();
    EXPECT_FALSE(registered);
    EXPECT_EQ(users_num(true), user_count);
    if (registered) {
      remove_user_connection(ip);
    }
    EXPECT_NE(event_base_loop(base.get(), EVLOOP_NONBLOCK), -1);
    EXPECT_EQ(event_base_get_num_events(base.get(), EVENT_BASE_COUNT_ADDED), 0);
    EXPECT_EQ(master_ob->interactive, nullptr);
    EXPECT_EQ(master_ob->ref, master_refs);
  }
}

TEST_F(DriverTest, TestLibeventOnceReleasesBaseLockAfterAddFailure) {
  ASSERT_NE(g_event_base, nullptr);

#ifndef _WIN32
  const char *method = event_base_get_method(g_event_base);
  if (!method || std::strcmp(method, "epoll") != 0) {
    GTEST_SKIP() << "The deterministic epoll add-failure injection is unavailable";
  }
  const int invalid_fd = open("/dev/null", O_RDONLY | O_NONBLOCK);
  ASSERT_GE(invalid_fd, 0);
  ASSERT_EQ(event_base_once(g_event_base, invalid_fd, EV_READ,
                            [](evutil_socket_t, short, void *) {}, nullptr, nullptr),
            -1);
  evutil_closesocket(invalid_fd);
#else
  GTEST_SKIP() << "The epoll-specific failure injection is not available on Windows";
#endif

  int callbacks = 0;
  ASSERT_EQ(event_base_once(g_event_base, -1, EV_TIMEOUT,
                            [](evutil_socket_t, short, void *arg) {
                              ++*static_cast<int *>(arg);
                            },
                            &callbacks, nullptr),
            0);
  ASSERT_EQ(event_base_loop(g_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK), 0);
  ASSERT_EQ(callbacks, 1);
}

TEST_F(DriverTest, TestCompressFilePathsUseOwnedDynamicStorage) {
  const auto source = read_source_file_for_test("../src/packages/compress/compress.cc");

  ASSERT_EQ(source.find("char outname[1024]"), std::string::npos);
  ASSERT_EQ(source.find("strcpy(outname"), std::string::npos);
  ASSERT_EQ(source.find("FREE_MSTR(output_file)"), std::string::npos);
  ASSERT_EQ(source.find("input_file + len - strlen(GZ_EXTENSION)"), std::string::npos);

  const auto first_input_owner = source.find("std::string input_path(input_file)");
  ASSERT_NE(first_input_owner, std::string::npos);
  ASSERT_NE(source.find("std::string input_path(input_file)", first_input_owner + 1),
            std::string::npos);

  const auto first_output_owner = source.find("std::string output_path(real_output_file)");
  ASSERT_NE(first_output_owner, std::string::npos);
  ASSERT_NE(source.find("std::string output_path(real_output_file)", first_output_owner + 1),
            std::string::npos);

  const auto first_suffix_guard = source.find("input_path.size() >= kGzExtensionLength");
  ASSERT_NE(first_suffix_guard, std::string::npos);
  ASSERT_NE(source.find("input_path.size() >= kGzExtensionLength", first_suffix_guard + 1),
            std::string::npos);
}

TEST_F(DriverTest, TestVmContextTracksTopLevelState) {
  ASSERT_EQ(vm_context().event_loop, g_event_base);
  ASSERT_EQ(vm_context().current_gametick, current_gametick());
  ASSERT_EQ(vm_context().execution.current_object, nullptr);
  ASSERT_EQ(vm_context().execution.command_giver, nullptr);
  ASSERT_EQ(vm_context().execution.current_interactive, nullptr);
  ASSERT_EQ(vm_context().execution.previous_ob, nullptr);
  ASSERT_EQ(vm_context().execution.current_prog, nullptr);
  ASSERT_EQ(vm_context().execution.caller_type, 0);
  ASSERT_EQ(vm_context().execution.call_origin, 0);
  ASSERT_EQ(vm_context().execution.function_index_offset, 0);
  ASSERT_EQ(vm_context().execution.variable_index_offset, 0);
  ASSERT_EQ(vm_context().execution.stack_in_use_as_temporary, 0);
  ASSERT_EQ(vm_context().error.current_error_context, nullptr);
  ASSERT_EQ(vm_context().error.too_deep_error, 0);
  ASSERT_EQ(vm_context().error.max_eval_error, 0);
  ASSERT_EQ(vm_context().error.error_depth, 0);
  ASSERT_EQ(vm_context().error.mudlib_error_depth, 0);
  ASSERT_EQ(vm_context().object_store.objects, obj_list);
  ASSERT_EQ(vm_context().object_store.destructed_objects, obj_list_destruct);
  ASSERT_EQ(vm_context().object_store.load_object_depth, 0);
  ASSERT_EQ(vm_context().object_store.restricted_destruct_object, nullptr);
}

TEST_F(DriverTest, TestTimeToNextGametickClampsLargeDelay) {
  const auto tick_msec = CONFIG_INT(__RC_GAMETICK_MSEC__);
  ASSERT_GT(tick_msec, 0);
  ASSERT_EQ(time_to_next_gametick(std::chrono::milliseconds::zero()), 1);
  ASSERT_EQ(time_to_next_gametick(std::chrono::milliseconds(-1)), 1);
  ASSERT_EQ(time_to_next_gametick(std::chrono::milliseconds(tick_msec)), 1);
  ASSERT_EQ(time_to_next_gametick(std::chrono::milliseconds(tick_msec + 1)), 2);
  ASSERT_EQ(time_to_next_gametick(std::chrono::milliseconds::max()),
            std::numeric_limits<int>::max());
}

TEST_F(DriverTest, TestGameTickQueueSupportsConcurrentProducersAndFullDrain) {
  clear_tick_events();
  struct TickQueueGuard {
    ~TickQueueGuard() { clear_tick_events(); }
  } tick_queue_guard;
  constexpr int kProducerCount = 8;
  constexpr int kEventsPerProducer = 2000;
  constexpr size_t kExpectedEvents = kProducerCount * kEventsPerProducer;
  std::atomic<bool> start{false};
  std::atomic<size_t> callbacks{0};
  std::vector<std::thread> producers;
  producers.reserve(kProducerCount);

  for (int producer = 0; producer < kProducerCount; producer++) {
    producers.emplace_back([&] {
      while (!start.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
      for (int event = 0; event < kEventsPerProducer; event++) {
        add_gametick_event(0, [&] { callbacks.fetch_add(1, std::memory_order_relaxed); });
      }
    });
  }

  start.store(true, std::memory_order_release);
  for (auto &producer : producers) {
    producer.join();
  }

  ASSERT_EQ(tick_event_queue_size_for_test(), kExpectedEvents);
  size_t total_drained = 0;
  size_t drain_slices = 0;
  while (tick_event_queue_size_for_test() > 0) {
    auto drained = run_tick_events_for_test();
    ASSERT_GT(drained, 0u);
    ASSERT_LE(drained, kBackendTickEventCallbackBudget);
    total_drained += drained;
    drain_slices++;
  }
  ASSERT_EQ(total_drained, kExpectedEvents);
  ASSERT_GT(drain_slices, 1u);
  ASSERT_EQ(callbacks.load(std::memory_order_relaxed), kExpectedEvents);
  ASSERT_EQ(tick_event_queue_size_for_test(), 0u);
}

TEST_F(DriverTest, TestGameTickQueueRunsReentrantCallbacksOutsideLockAndHonorsCancellation) {
  clear_tick_events();
  struct TickQueueGuard {
    ~TickQueueGuard() { clear_tick_events(); }
  } tick_queue_guard;
  std::atomic<int> callbacks{0};

  add_gametick_event(0, [&] {
    callbacks.fetch_add(1, std::memory_order_relaxed);
    add_gametick_event(0, [&] { callbacks.fetch_add(1, std::memory_order_relaxed); });
  });
  auto *cancelled = add_gametick_event(
      0, [&] { callbacks.fetch_add(100, std::memory_order_relaxed); });
  cancelled->cancel();

  ASSERT_EQ(run_tick_events_for_test(), 3u);
  ASSERT_EQ(callbacks.load(std::memory_order_relaxed), 2);
  ASSERT_EQ(tick_event_queue_size_for_test(), 0u);
}

TEST_F(DriverTest, TestGameTickAndOwnerMainDrainsUseBoundedPositiveDelaySlices) {
  const auto source = read_source_file_for_test("../src/backend.cc");

  ASSERT_EQ(kBackendTickEventCallbackBudget, 64u);
  ASSERT_EQ(kBackendTickEventWallBudget, std::chrono::milliseconds(4));
  ASSERT_EQ(kBackendTickContinuationDelay, std::chrono::milliseconds(1));
  ASSERT_EQ(kBackendOwnerMainDrainTaskBudget, 64);
  ASSERT_EQ(kBackendOwnerMainDrainWallBudget, std::chrono::milliseconds(8));
  ASSERT_EQ(kBackendOwnerMainDrainContinuationDelay,
            std::chrono::milliseconds(1));
  ASSERT_NE(source.find("vm_owner_drain_main_tasks_with_budget("),
            std::string::npos);
  ASSERT_EQ(source.find("vm_owner_drain_main_tasks(1024)"),
            std::string::npos);
  ASSERT_NE(source.find("kBackendTickContinuationDelay"), std::string::npos);
  ASSERT_NE(source.find("kBackendOwnerMainDrainContinuationDelay"),
            std::string::npos);
  const auto tick_slice_start = source.find("void drain_game_tick_slice(");
  const auto tick_slice_end = source.find("// Call one bounded event slice", tick_slice_start);
  const auto tick_slice_source = source.substr(
      tick_slice_start, tick_slice_end - tick_slice_start);
  const auto tick_drain = tick_slice_source.find("call_tick_events_slice()");
  const auto owner_drain = tick_slice_source.find(
      "drain_backend_owner_main_tasks_slice(false)");
  const auto continuation_check = tick_slice_source.find(
      "if (result.due_events_remaining)");
  ASSERT_NE(tick_drain, std::string::npos);
  ASSERT_NE(owner_drain, std::string::npos);
  ASSERT_NE(continuation_check, std::string::npos);
  ASSERT_LT(tick_drain, owner_drain);
  ASSERT_LT(owner_drain, continuation_check);
}

TEST_F(DriverTest, TestBackendWakeupPipeRoutesWorkerWakeupToMainThreadDrain) {
  // P7-2 contract: a thread that is not running the loop must not build or
  // activate libevent events; it writes one byte into the backend self-pipe and
  // the loop (or the test tick pump) runs the registered main-thread drain.
  std::atomic<int> drain_count{0};
  std::atomic<bool> drained_on_main{false};
  struct HandlerGuard {
    ~HandlerGuard() { backend_set_wakeup_handler(check_reqs); }
  } handler_guard;
  backend_set_wakeup_handler([&drain_count, &drained_on_main] {
    drained_on_main.store(vm_context_is_main_thread(), std::memory_order_relaxed);
    drain_count.fetch_add(1, std::memory_order_relaxed);
  });

  run_tick_events_for_test();  // drop any wakeup queued by an earlier test
  drain_count.store(0);

  bool wrote = false;
  std::thread worker([&wrote] { wrote = backend_wakeup_event_loop(); });
  worker.join();
  ASSERT_TRUE(wrote) << "init_backend() must have created the wakeup pipe";
  EXPECT_EQ(drain_count.load(), 0)
      << "the writing thread must not run the main-thread drain itself";

  run_tick_events_for_test();
  EXPECT_EQ(drain_count.load(), 1) << "the pump must drain the pending wakeup";
  EXPECT_TRUE(drained_on_main.load()) << "the drain must run on the main thread";

  // A second pump with nothing pending must not re-run the drain.
  run_tick_events_for_test();
  EXPECT_EQ(drain_count.load(), 1);

  auto async_source = read_source_file_for_test("../src/packages/async/async.cc");
  ASSERT_FALSE(async_source.empty());
  EXPECT_NE(async_source.find("backend_wakeup_event_loop();"), std::string::npos)
      << "async worker completion must wake the loop through the self-pipe";
  EXPECT_EQ(async_source.find("add_walltime_event(std::chrono::milliseconds(0),"),
            std::string::npos)
      << "the async worker must not create libevent events off the main thread";
}

TEST_F(DriverTest, TestWalltimeEventsShareInteractivePriorityAndCleanup) {
  clear_tick_events();
  struct TickQueueGuard {
    ~TickQueueGuard() { clear_tick_events(); }
  } tick_queue_guard;

  ASSERT_EQ(event_base_get_npriorities(g_event_base), kBackendEventPriorityLevels);
  auto *normal = add_walltime_event(std::chrono::hours(1), [] {});
  auto *gateway = add_walltime_event(
      std::chrono::hours(1), [] {}, BackendEventPriority::kGateway);
  auto *background = add_walltime_event(
      std::chrono::hours(1), [] {}, BackendEventPriority::kBackground);

  ASSERT_NE(normal, nullptr);
  ASSERT_NE(gateway, nullptr);
  ASSERT_NE(background, nullptr);
  ASSERT_EQ(walltime_event_queue_size_for_test(), 3u);
  ASSERT_EQ(walltime_event_priority_for_test(normal),
            static_cast<int>(BackendEventPriority::kNormal));
  ASSERT_EQ(walltime_event_priority_for_test(gateway),
            static_cast<int>(BackendEventPriority::kGateway));
  ASSERT_EQ(walltime_event_priority_for_test(background),
            static_cast<int>(BackendEventPriority::kBackground));
  ASSERT_EQ(walltime_event_priority_for_test(normal),
            walltime_event_priority_for_test(gateway));
  ASSERT_LT(walltime_event_priority_for_test(gateway),
            walltime_event_priority_for_test(background));

  normal->cancel();
  gateway->cancel();
  background->cancel();
  clear_tick_events();
  ASSERT_EQ(walltime_event_queue_size_for_test(), 0u);
}

TEST_F(DriverTest, TestNormalAndGatewaySelfReschedulingAreBidirectionallyFair) {
  struct FairnessProbe {
    event *pressure_event{nullptr};
    int pressure_callbacks{0};
    int observer_after{-1};
  };
  constexpr int kPressureCallbacks = 64;

  auto run_probe = [&](BackendEventPriority pressure_priority,
                       BackendEventPriority observer_priority) {
    FairnessProbe probe;
    probe.pressure_event = evtimer_new(
        g_event_base,
        [](evutil_socket_t, short, void *arg) {
          auto *state = static_cast<FairnessProbe *>(arg);
          state->pressure_callbacks++;
          if (state->pressure_callbacks < kPressureCallbacks) {
            event_active(state->pressure_event, EV_TIMEOUT, 1);
          }
        },
        &probe);
    auto *observer_event = evtimer_new(
        g_event_base,
        [](evutil_socket_t, short, void *arg) {
          auto *state = static_cast<FairnessProbe *>(arg);
          state->observer_after = state->pressure_callbacks;
        },
        &probe);
    ASSERT_NE(probe.pressure_event, nullptr);
    ASSERT_NE(observer_event, nullptr);
    ASSERT_EQ(event_priority_set(probe.pressure_event,
                                 static_cast<int>(pressure_priority)),
              0);
    ASSERT_EQ(event_priority_set(observer_event,
                                 static_cast<int>(observer_priority)),
              0);

    event_active(probe.pressure_event, EV_TIMEOUT, 1);
    event_active(observer_event, EV_TIMEOUT, 1);
    ASSERT_EQ(event_base_loop(g_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK), 0);
    EXPECT_EQ(probe.pressure_callbacks, kPressureCallbacks);
    EXPECT_EQ(probe.observer_after, 1);

    event_free(probe.pressure_event);
    event_free(observer_event);
  };

  run_probe(BackendEventPriority::kNormal, BackendEventPriority::kGateway);
  run_probe(BackendEventPriority::kGateway, BackendEventPriority::kNormal);
}

TEST_F(DriverTest, TestPrioritizedWalltimeCalloutsPreserveHandleLifecycle) {
  clear_call_outs();
  clear_tick_events();
  struct CalloutGuard {
    ~CalloutGuard() {
      clear_call_outs();
      clear_tick_events();
    }
  } callout_guard;

  auto *obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  auto call_number = [](const char *method, object_t *target) -> long {
    auto *ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
    EXPECT_EQ(ret ? ret->type : T_INVALID, T_NUMBER);
    return ret && ret->type == T_NUMBER ? ret->u.number : -1;
  };

  const auto normal_handle =
      static_cast<LPC_INT>(call_number("start_walltime_callout_probe", obj));
  const auto gateway_handle = static_cast<LPC_INT>(
      call_number("start_gateway_walltime_callout_probe", obj));
  const auto background_handle = static_cast<LPC_INT>(
      call_number("start_background_walltime_callout_probe", obj));
  ASSERT_GT(normal_handle, 0);
  ASSERT_GT(gateway_handle, 0);
  ASSERT_GT(background_handle, 0);
  ASSERT_EQ(vm_call_out_test_support_priority(normal_handle),
            static_cast<int>(BackendEventPriority::kNormal));
  ASSERT_EQ(vm_call_out_test_support_priority(gateway_handle),
            static_cast<int>(BackendEventPriority::kGateway));
  ASSERT_EQ(vm_call_out_test_support_priority(background_handle),
            static_cast<int>(BackendEventPriority::kBackground));
  ASSERT_GE(find_call_out_by_handle(obj, normal_handle), 0);
  ASSERT_GE(find_call_out_by_handle(obj, gateway_handle), 0);
  ASSERT_GE(find_call_out_by_handle(obj, background_handle), 0);

  ASSERT_GE(remove_call_out_by_handle(obj, normal_handle), 0);
  ASSERT_GE(remove_call_out_by_handle(obj, gateway_handle), 0);
  ASSERT_GE(remove_call_out_by_handle(obj, background_handle), 0);
  ASSERT_EQ(vm_call_out_test_support_priority(normal_handle), -1);
  ASSERT_EQ(vm_call_out_test_support_priority(gateway_handle), -1);
  ASSERT_EQ(vm_call_out_test_support_priority(background_handle), -1);
}

TEST_F(DriverTest, TestGatewayPriorityPreemptsQueuedBackgroundWarmups) {
  struct PriorityProbe {
    event *gateway_event{nullptr};
    int background_callbacks{0};
    int gateway_observed_after{-1};
  } probe;
  std::vector<event *> background_events;
  constexpr int kBackgroundEvents = 4;

  probe.gateway_event = evtimer_new(
      g_event_base,
      [](evutil_socket_t, short, void *arg) {
        auto *state = static_cast<PriorityProbe *>(arg);
        state->gateway_observed_after = state->background_callbacks;
      },
      &probe);
  ASSERT_NE(probe.gateway_event, nullptr);
  ASSERT_EQ(event_priority_set(
                probe.gateway_event,
                static_cast<int>(BackendEventPriority::kGateway)),
            0);

  for (int i = 0; i < kBackgroundEvents; ++i) {
    auto *background_event = evtimer_new(
        g_event_base,
        [](evutil_socket_t, short, void *arg) {
          auto *state = static_cast<PriorityProbe *>(arg);
          state->background_callbacks++;
          if (state->background_callbacks == 1) {
            event_active(state->gateway_event, EV_TIMEOUT, 1);
          }
        },
        &probe);
    ASSERT_NE(background_event, nullptr);
    ASSERT_EQ(event_priority_set(
                  background_event,
                  static_cast<int>(BackendEventPriority::kBackground)),
              0);
    background_events.push_back(background_event);
    event_active(background_event, EV_TIMEOUT, 1);
  }

  ASSERT_EQ(event_base_loop(g_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK), 0);
  ASSERT_EQ(probe.gateway_observed_after, 1);
  ASSERT_EQ(probe.background_callbacks, kBackgroundEvents);

  for (auto *background_event : background_events) {
    event_free(background_event);
  }
  event_free(probe.gateway_event);
}

TEST_F(DriverTest, TestBackgroundDispatchRepollsBeforeDrainingWholeActiveQueue) {
  struct DispatchProbe {
    event *normal_event{nullptr};
    int background_callbacks{0};
    int normal_observed_after{-1};
  } probe;
  std::vector<event *> background_events;
  constexpr int kBackgroundEvents = kBackendBackgroundDispatchMaxCallbacks * 2;

  probe.normal_event = evtimer_new(
      g_event_base,
      [](evutil_socket_t, short, void *arg) {
        auto *state = static_cast<DispatchProbe *>(arg);
        state->normal_observed_after = state->background_callbacks;
      },
      &probe);
  ASSERT_NE(probe.normal_event, nullptr);
  ASSERT_EQ(event_priority_set(
                probe.normal_event,
                static_cast<int>(BackendEventPriority::kNormal)),
            0);

  for (int i = 0; i < kBackgroundEvents; ++i) {
    auto *background_event = evtimer_new(
        g_event_base,
        [](evutil_socket_t, short, void *arg) {
          auto *state = static_cast<DispatchProbe *>(arg);
          state->background_callbacks++;
          if (state->background_callbacks == 1) {
            timeval due_now{};
            ASSERT_EQ(event_add(state->normal_event, &due_now), 0);
          }
        },
        &probe);
    ASSERT_NE(background_event, nullptr);
    ASSERT_EQ(event_priority_set(
                  background_event,
                  static_cast<int>(BackendEventPriority::kBackground)),
              0);
    background_events.push_back(background_event);
    event_active(background_event, EV_TIMEOUT, 1);
  }

  ASSERT_EQ(event_base_loop(g_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK), 0);
  ASSERT_EQ(probe.background_callbacks, kBackgroundEvents);
  ASSERT_GT(probe.normal_observed_after, 0);
  ASSERT_LE(probe.normal_observed_after,
            kBackendBackgroundDispatchMaxCallbacks);

  for (auto *background_event : background_events) {
    event_free(background_event);
  }
  event_free(probe.normal_event);
}

TEST_F(DriverTest,
       TestGatewayOwnerOutputQuiescesBeforeOwnerStopAndCleanup) {
  const auto source =
      read_source_file_for_test("../src/vm/internal/simulate.cc");
  const auto shutdown_pos = source.find("void shutdownMudOS(int exit_code)");
  const auto owner_quiesce_pos = source.find(
      "gateway_owner_output_quiesce(\"driver shutdown\")", shutdown_pos);
  const auto owner_stop_pos = source.find("vm_owner_thread_stop();", shutdown_pos);
  const auto gateway_cleanup_pos = source.find("cleanup_gateway();", shutdown_pos);
  const auto callout_cleanup_pos = source.find("clear_call_outs();", shutdown_pos);
  const auto tick_cleanup_pos = source.find("clear_tick_events();", shutdown_pos);

  ASSERT_NE(shutdown_pos, std::string::npos);
  ASSERT_NE(owner_quiesce_pos, std::string::npos);
  ASSERT_NE(owner_stop_pos, std::string::npos);
  ASSERT_NE(gateway_cleanup_pos, std::string::npos);
  ASSERT_NE(callout_cleanup_pos, std::string::npos);
  ASSERT_NE(tick_cleanup_pos, std::string::npos);
  ASSERT_LT(owner_quiesce_pos, owner_stop_pos);
  ASSERT_LT(owner_stop_pos, gateway_cleanup_pos);
  ASSERT_LT(gateway_cleanup_pos, callout_cleanup_pos);
  ASSERT_LT(callout_cleanup_pos, tick_cleanup_pos);
}

TEST_F(DriverTest, TestSigtermUsesControlledShutdownWithSuccessExitCode) {
  const auto main_source = read_source_file_for_test("../src/mainlib.cc");
  const auto backend_source = read_source_file_for_test("../src/backend.cc");
  const auto simulate_source =
      read_source_file_for_test("../src/vm/internal/simulate.cc");

  ASSERT_NE(main_source.find("signal(SIGTERM, startshutdownMudOS);"),
            std::string::npos);
  ASSERT_NE(main_source.find(
                "// Install the async-signal-safe shutdown request before mudlib startup."),
            std::string::npos);
  ASSERT_EQ(main_source.find("signal(SIGTERM, attempt_shutdown);"),
            std::string::npos);
  ASSERT_NE(simulate_source.find("sig == SIGTERM ? 0 : -1"),
            std::string::npos);
  ASSERT_NE(backend_source.find("shutdownMudOS(MudOS_shutdown_exit_code);"),
            std::string::npos);
  ASSERT_NE(backend_source.find("event_base_loopbreak(g_event_base);"),
            std::string::npos);
}

TEST_F(DriverTest, TestControlledShutdownPreparesMudlibBeforeRuntimeTeardown) {
  const auto applies_source =
      read_source_file_for_test("../src/vm/internal/applies");
  const auto shutdown_source =
      read_source_file_for_test("../src/vm/internal/simulate.cc");
  const auto shutdown_pos =
      shutdown_source.find("void shutdownMudOS(int exit_code)");
  const auto prepare_pos = shutdown_source.find(
      "safe_apply_master_ob(APPLY_PREPARE_SHUTDOWN, 1)", shutdown_pos);
  const auto async_stop_pos =
      shutdown_source.find("complete_all_asyncio();", shutdown_pos);
  const auto owner_stop_pos =
      shutdown_source.find("vm_owner_thread_stop();", shutdown_pos);

  ASSERT_NE(applies_source.find("PREPARE_SHUTDOWN"), std::string::npos);
  ASSERT_NE(shutdown_pos, std::string::npos);
  ASSERT_NE(prepare_pos, std::string::npos);
  ASSERT_NE(async_stop_pos, std::string::npos);
  ASSERT_NE(owner_stop_pos, std::string::npos);
  ASSERT_LT(prepare_pos, async_stop_pos);
  ASSERT_LT(prepare_pos, owner_stop_pos);
}

TEST_F(DriverTest,
       TestGatewayOwnerOutputQuiesceRejectsOffMainBeforeContainerAccess) {
  const auto source =
      read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto quiesce_pos =
      source.find("mapping_t *gateway_owner_output_quiesce(const char *reason)");
  const auto next_function_pos = source.find(
      "int gateway_fill_session_protocol_output_with_writer", quiesce_pos);
  const auto main_guard_pos = source.find(
      "if (!vm_context_is_main_thread())", quiesce_pos);
  const auto session_watch_read_pos = source.find(
      "g_gateway_session_future_watches", quiesce_pos);
  const auto room_wave_read_pos = source.find(
      "g_gateway_room_output_waves", quiesce_pos);
  const auto room_pending_read_pos = source.find(
      "gateway_room_output_projection_pending_count()", quiesce_pos);

  ASSERT_NE(quiesce_pos, std::string::npos);
  ASSERT_NE(next_function_pos, std::string::npos);
  ASSERT_NE(main_guard_pos, std::string::npos);
  ASSERT_NE(session_watch_read_pos, std::string::npos);
  ASSERT_NE(room_wave_read_pos, std::string::npos);
  ASSERT_NE(room_pending_read_pos, std::string::npos);
  ASSERT_LT(main_guard_pos, next_function_pos);
  ASSERT_LT(main_guard_pos, session_watch_read_pos);
  ASSERT_LT(main_guard_pos, room_wave_read_pos);
  ASSERT_LT(main_guard_pos, room_pending_read_pos);
}

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
  auto *main_context = &vm_context();
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

  ASSERT_GT(worker_remaining,
            kWorkerDeadlineMicros - kSchedulingToleranceMicros);
  ASSERT_LE(worker_remaining, kWorkerDeadlineMicros);
  ASSERT_GT(main_remaining,
            kMainDeadlineMicros - kSchedulingToleranceMicros);
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
    const auto wait_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
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
  auto *main_context = &vm_context();
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
    worker_store_rejected = !worker_context.object_store.main_thread_owned && worker_context.object_store.objects == nullptr;
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    svalue_t* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr) << key;
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER) << key;
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto* runtime = vm_owner_runtime_status();
  ASSERT_EQ(mapping_number(runtime, "destructed_object_backlog"), 0);
  ASSERT_GE(mapping_number(runtime, "destructed_object_cleanup_total"), 3);
  ASSERT_EQ(mapping_number(runtime, "destructed_object_incremental_cleanup_ready"), 1);
  free_mapping(runtime);
}

TEST_F(DriverTest, TestVmOwnerScopeBindsAndRestoresCurrentOwner) {
  auto *context = &vm_context();
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  const char* helper_paths[] = {"std/base64.c", "std/break_string.c", "std/element_of_weighted.c",
                                "std/reduce.c", "std/highest.c",       "std/lowest.c",
                                "std/number_string.c", "std/sum.c"};
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
    ASSERT_EQ(vm_object_store_owner_resolve(vm_owner_default_id(), handle.object_id), service) << path;
    ASSERT_EQ(vm_object_store_owner_resolve(vm_owner_id(player), handle.object_id), nullptr) << path;
    ASSERT_EQ(vm_object_store_owner_path_resolve(vm_owner_default_id(), handle.object_path.c_str()), service)
        << path;
    ASSERT_EQ(vm_object_store_owner_path_resolve(vm_owner_id(player), handle.object_path.c_str()), nullptr)
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  ASSERT_EQ(vm_object_store_owner_path_resolve(vm_owner_default_id(), "test/virtual"), virtual_object);
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
  auto handle_live_object_bridge_ready = mapping_number(handle_status, "global_live_object_bridge_retirement_ready");
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
  auto handle_record_bridge_ready = mapping_number(handle_status, "global_record_bridge_retirement_ready");
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
      ASSERT_EQ(mapping_number(record, "owner_epoch"), static_cast<long>(vm_owner_epoch(virtual_object)));
      ASSERT_EQ(mapping_number(record, "destructed"), 0);
      ASSERT_EQ(mapping_number(record, "live"), 1);
      ASSERT_EQ(mapping_number(record, "owner_local_object_ref_entry"), 1);
      ASSERT_STREQ(mapping_string(record, "owner_local_object_ref_source"), "vm_object_shard.local_objects");
      ASSERT_EQ(mapping_number(record, "owner_local_object_ref_index_entry"), 1);
      ASSERT_STREQ(mapping_string(record, "owner_local_object_ref_index_source"),
                   "vm_object_shard.local_object_index");
      ASSERT_EQ(mapping_number(record, "owner_local_path_index_entry"), 1);
      ASSERT_STREQ(mapping_string(record, "owner_local_path_index_source"), "vm_object_shard.object_path_index");
      break;
    }
  }
  ASSERT_TRUE(found_virtual_record);
  free_mapping(owner_status);
  auto* virtual_lookup = vm_object_store_owner_lookup_status(vm_owner_default_id(), handle.object_id);
  ASSERT_EQ(mapping_number(virtual_lookup, "success"), 1);
  ASSERT_EQ(mapping_number(virtual_lookup, "found"), 1);
  ASSERT_EQ(mapping_number(virtual_lookup, "owner_local_object_ref_found"), 1);
  ASSERT_STREQ(mapping_string(virtual_lookup, "owner_local_object_ref_source"), "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(virtual_lookup, "owner_local_object_ref_index_found"), 1);
  ASSERT_STREQ(mapping_string(virtual_lookup, "owner_local_object_ref_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(virtual_lookup, "owner_local_object_pointer_index_found"), 1);
  ASSERT_STREQ(mapping_string(virtual_lookup, "owner_local_object_pointer_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(virtual_lookup, "owner_local_resolve_found"), 1);
  ASSERT_STREQ(mapping_string(virtual_lookup, "owner_local_resolve_source"), "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(virtual_lookup, "owner_local_path_index_found"), 1);
  ASSERT_EQ(mapping_number(virtual_lookup, "owner_local_destructed_path_index_found"), 0);
  ASSERT_STREQ(mapping_string(virtual_lookup, "owner_local_path_index_source"), "vm_object_shard.object_path_index");
  ASSERT_STREQ(mapping_string(virtual_lookup, "object_path"), "test/virtual");
  free_mapping(virtual_lookup);
  auto* virtual_path_lookup = vm_object_store_owner_path_lookup_status(vm_owner_default_id(), "test/virtual");
  ASSERT_EQ(mapping_number(virtual_path_lookup, "success"), 1);
  ASSERT_EQ(mapping_number(virtual_path_lookup, "record_found"), 1);
  ASSERT_EQ(mapping_number(virtual_path_lookup, "found"), 1);
  ASSERT_EQ(mapping_number(virtual_path_lookup, "object_id"), static_cast<long>(handle.object_id));
  ASSERT_EQ(mapping_number(virtual_path_lookup, "owner_local_object_ref_found"), 1);
  ASSERT_STREQ(mapping_string(virtual_path_lookup, "owner_local_object_ref_source"), "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(virtual_path_lookup, "owner_local_object_ref_index_found"), 1);
  ASSERT_STREQ(mapping_string(virtual_path_lookup, "owner_local_object_ref_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(virtual_path_lookup, "owner_local_object_pointer_index_found"), 1);
  ASSERT_STREQ(mapping_string(virtual_path_lookup, "owner_local_object_pointer_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(virtual_path_lookup, "owner_local_resolve_found"), 1);
  ASSERT_STREQ(mapping_string(virtual_path_lookup, "owner_local_resolve_source"), "vm_object_shard.local_objects");
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  ASSERT_EQ(mapping_number(first_tasks->u.arr->item[0].u.map, "task_id"), static_cast<long>(first_id));
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
  ASSERT_EQ(mapping_number(second_tasks->u.arr->item[0].u.map, "task_id"), static_cast<long>(second_id));
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
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

  auto task_id = vm_owner_enqueue_main_task(obj, "unit_main", "dispatch", [&] {
    ran = true;
    seen_owner = vm_context().owner.current_owner_id;
    auto* running = vm_owner_thread_status();
    active_owners_during_callback = mapping_number(running, "main_active_owners");
    free_mapping(running);
  }, nullptr, VM_OWNER_MAIN_TASK_IO_ADAPTER);
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
  ASSERT_EQ(mapping_number(after_runtime, "normal_path_main_fallback_count"), before_normal_fallback);
  ASSERT_EQ(mapping_number(after_runtime, "main_io_adapter_count"), before_io_adapter + 1);
  ASSERT_EQ(mapping_number(after_runtime, "main_fallback_policy_ready"), 1);
  free_mapping(after_runtime);

  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  const auto first_ref_before = first->ref;
  bool second_ran = false;
  ASSERT_GT(vm_owner_enqueue_main_task(
                first, "unit_main", "throws",
                [] { throw "owner main callback test"; }),
            0u);
  ASSERT_GT(vm_owner_enqueue_main_task(
                second, "unit_main", "after-throw",
                [&] { second_ran = true; }),
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
  ASSERT_GT(vm_owner_enqueue_main_task(first, "unit_main", "wall-budget-first", [&] {
              first_ran++;
              auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(3);
              while (std::chrono::steady_clock::now() < deadline) {
              }
            }),
            0u);
  ASSERT_GT(vm_owner_enqueue_main_task(second, "unit_main", "wall-budget-second",
                                       [&] { second_ran++; }),
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
  auto task_id = vm_owner_enqueue_main_task(obj, "unit_main", "stale", [&] { ran = true; });
  ASSERT_GT(task_id, 0u);
  vm_owner_set_id(obj, "owner/test/main-stale-new");
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);
  ASSERT_FALSE(ran);

  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
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
      obj, "unit_main", "drop", [&] { ran = true; }, [&] { dropped = true; });
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
  auto* trace = vm_owner_task_trace(1);
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  ASSERT_EQ(events->u.arr->size, 1);
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "task_type"), "heartbeat");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "state"), "scheduled");
  free_mapping(trace);

  auto* owner_status = vm_object_store_owner_status("owner/test/heartbeat");
  auto* status_record = find_string_in_mapping(owner_status, "status_record");
  auto* execution_shard = find_string_in_mapping(owner_status, "execution_shard");
  ASSERT_NE(status_record, nullptr);
  ASSERT_EQ(status_record->type, T_MAPPING);
  ASSERT_NE(execution_shard, nullptr);
  ASSERT_EQ(execution_shard->type, T_MAPPING);
  ASSERT_EQ(mapping_number(owner_status, "active_heartbeats"), 1);
  ASSERT_EQ(mapping_number(owner_status, "runnable_tasks"), 1);
  ASSERT_EQ(mapping_number(owner_status, "executor_ready"), 1);
  ASSERT_EQ(mapping_number(status_record->u.map, "heartbeats"), 1);
  ASSERT_EQ(mapping_number(execution_shard->u.map, "active_heartbeats"), 1);
  ASSERT_EQ(mapping_number(execution_shard->u.map, "runnable_tasks"), 1);
  free_mapping(owner_status);

  set_heart_beat(obj, 0);
  owner_status = vm_object_store_owner_status("owner/test/heartbeat");
  execution_shard = find_string_in_mapping(owner_status, "execution_shard");
  ASSERT_NE(execution_shard, nullptr);
  ASSERT_EQ(execution_shard->type, T_MAPPING);
  ASSERT_EQ(mapping_number(owner_status, "active_heartbeats"), 0);
  ASSERT_EQ(mapping_number(owner_status, "runnable_tasks"), 0);
  ASSERT_EQ(mapping_number(owner_status, "executor_ready"), 0);
  ASSERT_EQ(mapping_number(execution_shard->u.map, "active_heartbeats"), 0);
  ASSERT_EQ(mapping_number(execution_shard->u.map, "runnable_tasks"), 0);
  free_mapping(owner_status);
  vm_owner_clear_id(obj);
}

TEST_F(DriverTest, TestVmObjectStoreTracksPendingCallouts) {
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };

  object_t* obj = find_object("single/master.c");
  ASSERT_NE(obj, nullptr);
  vm_owner_set_id(obj, "owner/test/callout-store");
  vm_object_store_record_callout(obj, 12345);

  auto* owner_status = vm_object_store_owner_status("owner/test/callout-store");
  auto* status_record = find_string_in_mapping(owner_status, "status_record");
  auto* execution_shard = find_string_in_mapping(owner_status, "execution_shard");
  ASSERT_NE(status_record, nullptr);
  ASSERT_EQ(status_record->type, T_MAPPING);
  ASSERT_NE(execution_shard, nullptr);
  ASSERT_EQ(execution_shard->type, T_MAPPING);
  ASSERT_EQ(mapping_number(owner_status, "pending_callouts"), 1);
  ASSERT_EQ(mapping_number(owner_status, "runnable_tasks"), 1);
  ASSERT_EQ(mapping_number(owner_status, "executor_ready"), 1);
  ASSERT_EQ(mapping_number(status_record->u.map, "callouts"), 1);
  ASSERT_EQ(mapping_number(execution_shard->u.map, "pending_callouts"), 1);
  ASSERT_EQ(mapping_number(execution_shard->u.map, "runnable_tasks"), 1);
  free_mapping(owner_status);

  vm_object_store_remove_callout("owner/test/callout-store", 12345);
  owner_status = vm_object_store_owner_status("owner/test/callout-store");
  execution_shard = find_string_in_mapping(owner_status, "execution_shard");
  ASSERT_NE(execution_shard, nullptr);
  ASSERT_EQ(execution_shard->type, T_MAPPING);
  ASSERT_EQ(mapping_number(owner_status, "pending_callouts"), 0);
  ASSERT_EQ(mapping_number(owner_status, "runnable_tasks"), 0);
  ASSERT_EQ(mapping_number(owner_status, "executor_ready"), 0);
  ASSERT_EQ(mapping_number(execution_shard->u.map, "pending_callouts"), 0);
  ASSERT_EQ(mapping_number(execution_shard->u.map, "runnable_tasks"), 0);
  free_mapping(owner_status);
  vm_owner_clear_id(obj);
}

TEST_F(DriverTest, TestVmObjectStoreTracksPendingOwnerMessages) {
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };

  auto assert_pending_messages = [&](const char* owner_id, long expected) {
    auto* owner_status = vm_object_store_owner_status(owner_id);
    auto* status_record = find_string_in_mapping(owner_status, "status_record");
    auto* execution_shard = find_string_in_mapping(owner_status, "execution_shard");
    ASSERT_NE(status_record, nullptr);
    ASSERT_EQ(status_record->type, T_MAPPING);
    ASSERT_NE(execution_shard, nullptr);
    ASSERT_EQ(execution_shard->type, T_MAPPING);
    ASSERT_EQ(mapping_number(owner_status, "pending_messages"), expected);
    ASSERT_EQ(mapping_number(owner_status, "runnable_tasks"), expected);
    ASSERT_EQ(mapping_number(owner_status, "executor_ready"), expected > 0 ? 1 : 0);
    ASSERT_EQ(mapping_number(status_record->u.map, "messages"), 1);
    ASSERT_EQ(mapping_number(execution_shard->u.map, "pending_messages"), expected);
    ASSERT_EQ(mapping_number(execution_shard->u.map, "runnable_tasks"), expected);
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

  auto* scheduled = vm_owner_submit_message("owner/test/message/source", "owner/test/message/schedule",
                                           "message", "payload/schedule");
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
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
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
  ASSERT_STREQ(mapping_string(after, "heartbeat_owner_executor_route"), "owner_main_queue_callback_adapter");
  ASSERT_STREQ(mapping_string(after, "heartbeat_owner_executor_fallback_route"),
               "");
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
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
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
  auto before_cleanup_dispatched = mapping_number(before, "executor_callback_main_cleanup_dispatched");
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
  ASSERT_EQ(mapping_number(after, "executor_callback_main_cleanup_dispatched"), before_cleanup_dispatched);
  ASSERT_GE(mapping_number(after, "main_queued"), before_main_queued + 1);
  ASSERT_EQ(mapping_number(after, "callout_owner_executor_ready"), 1);
  ASSERT_STREQ(mapping_string(after, "callout_owner_executor_task_type"), "call_out");
  ASSERT_STREQ(mapping_string(after, "callout_owner_executor_route"), "owner_main_queue_callback_adapter");
  ASSERT_STREQ(mapping_string(after, "callout_owner_executor_fallback_route"),
               "");
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
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
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
  auto before_cleanup_dispatched = mapping_number(before, "executor_callback_main_cleanup_dispatched");
  auto before_main_queued = mapping_number(before, "main_queued");
  free_mapping(before);

  vm_owner_thread_start(1);
  ASSERT_TRUE(vm_owner_executor_available());
  ASSERT_TRUE(vm_async_test_support_dispatch_read_callback(obj, "async_callback_probe", "async owner result"));
  ASSERT_TRUE(vm_dns_test_support_dispatch_callback(obj, "dns_callback_probe", 4242));

  for (int i = 0; i < 100 && !trace_has_state("async_callback", "main_executor_callback_completed"); i++) {
    vm_owner_drain_main_tasks(64);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  for (int i = 0; i < 100 && !trace_has_state("dns_callback", "main_executor_callback_completed"); i++) {
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
  ASSERT_EQ(mapping_number(after, "executor_callback_main_cleanup_dispatched"), before_cleanup_dispatched);
  ASSERT_GE(mapping_number(after, "main_queued"), before_main_queued + 2);
  ASSERT_EQ(mapping_number(after, "async_owner_executor_ready"), 1);
  ASSERT_STREQ(mapping_string(after, "async_owner_executor_task_type"), "async_callback");
  ASSERT_STREQ(mapping_string(after, "async_owner_executor_route"), "owner_main_queue_callback_adapter");
  ASSERT_STREQ(mapping_string(after, "async_owner_executor_fallback_route"),
               "");
  ASSERT_STREQ(mapping_string(after, "async_owner_executor_policy"),
               "main_thread_callback_adapter_after_owner_admission");
  ASSERT_STREQ(mapping_string(after, "async_owner_executor_result_policy"), "frozen_deep_copy_result");
  ASSERT_EQ(mapping_number(after, "async_owner_executor_cleanup_main_ready"), 1);
  ASSERT_EQ(mapping_number(after, "async_owner_executor_drop_cleanup_ready"), 1);
  ASSERT_EQ(mapping_number(after, "dns_owner_executor_ready"), 1);
  ASSERT_STREQ(mapping_string(after, "dns_owner_executor_task_type"), "dns_callback");
  ASSERT_STREQ(mapping_string(after, "dns_owner_executor_route"), "owner_main_queue_callback_adapter");
  ASSERT_STREQ(mapping_string(after, "dns_owner_executor_fallback_route"),
               "");
  ASSERT_STREQ(mapping_string(after, "dns_owner_executor_policy"),
               "main_thread_callback_adapter_after_owner_admission");
  ASSERT_STREQ(mapping_string(after, "dns_owner_executor_result_policy"), "frozen_deep_copy_result");
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
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
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
  ASSERT_TRUE(vm_async_test_support_dispatch_read_callback(obj, "async_callback_probe", "stale async result"));
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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
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
            EXPECT_STREQ(mapping_string(event, "payload_policy"), "frozen_payload_or_owner_handle_only");
            EXPECT_STREQ(mapping_string(event, "cleanup_policy"), "main_thread_drop_cleanup");
            EXPECT_STREQ(mapping_string(event, "reply_future_policy"), "main_reply_or_cleanup_queue");
            EXPECT_STREQ(mapping_string(event, "admission_policy"), "owner_epoch_payload_allowlist_deadline_guard");
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
  auto before_cleanup_dispatched = mapping_number(before, "executor_callback_main_cleanup_dispatched");
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
  ASSERT_EQ(mapping_number(after, "executor_callback_main_cleanup_dispatched"), before_cleanup_dispatched);
  ASSERT_GE(mapping_number(after, "main_queued"), before_main_queued + 1);
  ASSERT_EQ(mapping_number(after, "socket_owner_executor_ready"), 1);
  ASSERT_STREQ(mapping_string(after, "socket_owner_executor_task_type"), "socket_callback");
  ASSERT_STREQ(mapping_string(after, "socket_owner_executor_route"), "owner_main_queue_callback_adapter");
  ASSERT_STREQ(mapping_string(after, "socket_owner_executor_fallback_route"),
               "");
  ASSERT_STREQ(mapping_string(after, "socket_owner_executor_policy"),
               "main_thread_callback_adapter_after_owner_admission");
  ASSERT_STREQ(mapping_string(after, "socket_owner_executor_result_policy"), "frozen_deep_copy_args");
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
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
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
  ASSERT_GE(mapping_number(after, "owner_executor_admission_dropped"), before_admission_dropped + 1);
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
  auto* trace = vm_owner_access_trace(1);
  ASSERT_STREQ(mapping_string(trace, "trace_kind"), "owner_access_trace");
  ASSERT_STREQ(mapping_string(trace, "trace_model"), "cross_owner_access_policy_trace");
  auto* events = find_string_in_mapping(trace, "events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->type, T_ARRAY);
  ASSERT_EQ(events->u.arr->size, 1);
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "trace_model"), "cross_owner_access_policy_event");
  ASSERT_EQ(mapping_number(events->u.arr->item[0].u.map, "cross_owner"), 1);
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"), "owner/test/access/source");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"), "owner/test/access/target");
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"), "owner/test/access/shared");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"), "owner/test/access/target");
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = map ? find_string_in_mapping(map, key) : nullptr;
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };

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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
    ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"), "owner/test/scope/target");
    ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"), "owner/test/scope/target");
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
    ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"), "owner/test/scope/target");
    ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"), "owner/test/scope/target");
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"), "owner/test/inventory/shared");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"), "owner/test/inventory/target");
  free_mapping(trace);

  vm_owner_clear_id(source);
  vm_owner_clear_id(target);
}

TEST_F(DriverTest, TestPresentRecordsCrossOwnerAccessTrace) {
  object_t* source = find_object("single/master.c");
  object_t* target = find_object("single/simul_efun.c");
  ASSERT_NE(source, nullptr);
  ASSERT_NE(target, nullptr);

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"), "owner/test/present/shared");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"), "owner/test/present/target");
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"), "owner/test/move/shared");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"), "owner/test/move/dest");
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };

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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"), "owner/test/destruct/shared");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"), "owner/test/destruct/target");
  free_mapping(trace);

  vm_owner_clear_id(source);
}

TEST_F(DriverTest, TestDestructEnforcedModeBlocksCrossOwnerDestruct) {
  object_t* source = find_object("single/master.c");
  object_t* target = load_object_for_test("single/on_destruct_good");
  ASSERT_NE(source, nullptr);
  ASSERT_NE(target, nullptr);

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };

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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "source_owner_id"), "owner/test/call/shared");
  ASSERT_STREQ(mapping_string(events->u.arr->item[0].u.map, "target_owner_id"), "owner/test/call/target");
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

  auto* submitted = vm_owner_submit_object_message(source_owner, handle, "owner_async_non_frozen_result",
                                                   "cross-owner/non-frozen/v1");
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
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
  ASSERT_STREQ(mapping_string(events->u.arr->item[events->u.arr->size - 1].u.map, "state"), "thread_dispatched");
  ASSERT_EQ(mapping_number(events->u.arr->item[events->u.arr->size - 1].u.map, "task_id"),
            static_cast<long>(task_id));
  ASSERT_STREQ(mapping_string(events->u.arr->item[events->u.arr->size - 1].u.map, "owner_id"), owner);
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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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

TEST_F(DriverTest, TestVmOwnerExecutorCommandFrameRestoreDispatchesWithoutLpc) {
  const char* owner = "owner/test/executor/command-frame-restore";

  vm_owner_thread_stop();
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
  auto mapping_entry = [](mapping_t* map, const char* key) -> mapping_t* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_MAPPING);
    return value && value->type == T_MAPPING ? value->u.map : nullptr;
  };

  auto* before = vm_owner_thread_status();
  auto before_frame_restore = mapping_number(before, "executor_command_frame_restore_entry_executed");
  auto before_safe = mapping_number(before, "executor_safe_task_dispatched");
  auto before_execution_cleared = mapping_number(before, "thread_execution_cleared");
  auto before_eval_stack_owner_bound = mapping_number(before, "thread_eval_stack_owner_bound");
  auto before_eval_stack_cleared = mapping_number(before, "thread_eval_stack_cleared");
  auto before_eval_stack_leaks = mapping_number(before, "thread_eval_stack_leak_detected");
  auto before_control_stack_owner_bound = mapping_number(before, "thread_control_stack_owner_bound");
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
    auto frame_restore_done = mapping_number(status, "executor_command_frame_restore_entry_executed");
    free_mapping(status);
    if (frame_restore_done >= before_frame_restore + 1) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  auto* running = vm_owner_thread_status();
  ASSERT_GE(mapping_number(running, "executor_command_frame_restore_entry_executed"), before_frame_restore + 1);
  ASSERT_GE(mapping_number(running, "executor_safe_task_dispatched"), before_safe + 1);
  ASSERT_GE(mapping_number(running, "thread_execution_cleared"), before_execution_cleared + 1);
  ASSERT_GE(mapping_number(running, "thread_eval_stack_owner_bound"), before_eval_stack_owner_bound + 1);
  ASSERT_GE(mapping_number(running, "thread_eval_stack_cleared"), before_eval_stack_cleared + 1);
  ASSERT_EQ(mapping_number(running, "thread_eval_stack_leak_detected"), before_eval_stack_leaks);
  ASSERT_GE(mapping_number(running, "thread_control_stack_owner_bound"), before_control_stack_owner_bound + 1);
  ASSERT_GE(mapping_number(running, "thread_control_stack_cleared"), before_control_stack_cleared + 1);
  ASSERT_EQ(mapping_number(running, "thread_control_stack_leak_detected"), before_control_stack_leaks);
  ASSERT_GE(mapping_number(running, "thread_value_stack_owner_bound"), before_value_stack_owner_bound + 1);
  ASSERT_GE(mapping_number(running, "thread_value_stack_cleared"), before_value_stack_cleared + 1);
  ASSERT_EQ(mapping_number(running, "thread_value_stack_leak_detected"), before_value_stack_leaks);
  ASSERT_GE(mapping_number(running, "thread_apply_return_owner_bound"), before_apply_return_owner_bound + 1);
  ASSERT_GE(mapping_number(running, "thread_apply_return_cleared"), before_apply_return_cleared + 1);
  ASSERT_EQ(mapping_number(running, "thread_apply_return_leak_detected"), before_apply_return_leaks);
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
    VMOwnerCallbackCleanupRecord *record;
    bool *called;
  };
  bool called = false;
  auto *record = new VMOwnerCallbackCleanupRecord();
  CleanupProbe probe{record, &called};
  record->prepare("owner/test/prepared-cleanup", 1, "socket_callback", "cleanup",
                  [](void *context) {
                    auto *probe = static_cast<CleanupProbe *>(context);
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
    first_saw_owner_scope.store(vm_context().owner.current_owner_id == owner && vm_context_is_main_thread() ? 1 : 0,
                                std::memory_order_release);
    first_saw_execution_scope.store(vm_context().execution.current_object == probe ? 1 : 0,
                                    std::memory_order_release);
  });
  ASSERT_GT(first_task, 0u);
  ASSERT_GE(vm_owner_drain_main_tasks(8), 1);
  ASSERT_EQ(first_saw_owner_scope.load(std::memory_order_acquire), 1);
  ASSERT_EQ(first_saw_execution_scope.load(std::memory_order_acquire), 1);

  auto stale_task = vm_owner_enqueue_executor_task(
      probe, "call_out", "unit-stale", [&] { second_ran.store(1, std::memory_order_release); },
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
  ASSERT_GE(mapping_number(after, "executor_callback_main_cleanup_queued"), before_cleanup_queued + 1);
  ASSERT_GE(mapping_number(after, "executor_callback_main_cleanup_dispatched"), before_cleanup + 1);
  ASSERT_EQ(mapping_number(after, "executor_callback_task_boundary_ready"), 1);
  ASSERT_EQ(mapping_number(after, "executor_callback_allowlist_ready"), 1);
  ASSERT_EQ(mapping_number(after, "executor_callback_allowlist_count"), 6);
  ASSERT_EQ(mapping_number(after, "owner_callback_diagnostics_ready"), 1);
  ASSERT_STREQ(mapping_string(after, "owner_callback_diagnostics_schema"), "owner_callback_diagnostics_v1");
  ASSERT_STREQ(mapping_string(after, "owner_callback_failure_code_schema"), "owner_callback_failure_code_v1");
  ASSERT_STREQ(mapping_string(after, "owner_callback_drop_reason_schema"), "owner_callback_drop_reason_v1");
  ASSERT_EQ(mapping_number(after, "owner_callback_allowlist_complete"), 1);
  ASSERT_STREQ(mapping_string(after, "owner_callback_supported_kinds"),
               "heartbeat,call_out,async_callback,dns_callback,socket_callback,gateway_command_execute,ed_callback");
  ASSERT_STREQ(mapping_string(after, "executor_callback_payload_policy"), "frozen_payload_or_owner_handle_only");
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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

  auto* submitted = vm_owner_submit_message("owner/test/thread/source", owner, "message", "payload/thread");
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
  auto* before = vm_owner_thread_status();
  auto before_probe = mapping_number(before, "executor_probe_executed");
  auto before_safe = mapping_number(before, "executor_safe_task_dispatched");
  auto before_skipped = mapping_number(before, "executor_main_required_skipped");
  auto before_main_queued = mapping_number(before, "main_queued");
  auto before_main_dispatched = mapping_number(before, "main_dispatched");
  free_mapping(before);

  auto* submitted = vm_owner_submit_object_message(source_owner, handle, "owner_lpc_probe", "executor-target/payload");
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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
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
  auto before_target_message_fallback = mapping_number(before_runtime, "target_owner_message_main_fallback");
  free_mapping(before_runtime);

  auto main_required_task = vm_owner_enqueue_test_main_required_message(owner, "blocked-object-message");
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
  ASSERT_EQ(mapping_number(after_runtime, "normal_path_main_fallback_count"), before_normal_fallback);
  ASSERT_EQ(mapping_number(after_runtime, "target_owner_message_main_fallback"), before_target_message_fallback);
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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
    if (std::string(mapping_string(event, "task_key")) !=
        "backpressure-over-limit") {
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

  auto mapping_number = [](mapping_t* map, const char* key) -> LPC_INT {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, &const0u) << key;
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  const auto lpc_future_id =
      static_cast<uint64_t>(mapping_number(lpc_submission, "future_id"));
  ASSERT_EQ(mapping_number(lpc_submission, "success"), 0);
  ASSERT_GT(lpc_future_id, 0u);
  free_mapping(lpc_submission);

  auto* ordinary_submission =
      vm_owner_ordinary_lpc_task(target, owner, "owner_task_player", 1);
  const auto ordinary_future_id =
      static_cast<uint64_t>(mapping_number(ordinary_submission, "future_id"));
  ASSERT_EQ(mapping_number(ordinary_submission, "success"), 0);
  ASSERT_STREQ(mapping_string(ordinary_submission, "state"), "failed");
  ASSERT_STREQ(mapping_string(ordinary_submission, "error"),
               "owner scheduler backpressure");
  ASSERT_GT(ordinary_future_id, 0u);
  free_mapping(ordinary_submission);

  constexpr uint64_t worker_task_id = 987654322u;
  const auto compute_future_id = vm_owner_register_compute_future(
      owner, worker_task_id, "bench", "submission-backpressure-compute");
  ASSERT_GT(compute_future_id, 0u);
  ASSERT_EQ(vm_owner_enqueue_compute_result(
                owner, worker_task_id, "bench", "completed", "bench", ""),
            0u);

  auto assert_failed_and_take = [&](uint64_t future_id) {
    auto* failed = vm_owner_future_poll(future_id);
    EXPECT_EQ(mapping_number(failed, "success"), 1);
    EXPECT_STREQ(mapping_string(failed, "state"), "failed");
    EXPECT_STREQ(mapping_string(failed, "error"),
                 "owner scheduler backpressure");
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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = map ? find_string_in_mapping(map, key) : nullptr;
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = map ? find_string_in_mapping(map, key) : nullptr;
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
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
  auto assert_production_gate_contract = [&](mapping_t* contract) {
    ASSERT_EQ(mapping_number(contract, "mudlib_audit_required"), 1);
    ASSERT_EQ(mapping_number(contract, "mudlib_cross_owner_hotspots_ready"), 1);
    ASSERT_STREQ(mapping_string(contract, "mudlib_cross_owner_hotspots_blocker"),
                 "");
    ASSERT_STREQ(mapping_string(contract, "mudlib_cross_owner_hotspots_evidence"),
                 "xkx_5513c8a12_multicore_mudlib_audit_2026_06_25_zero_delayed_object_payloads");
    ASSERT_EQ(mapping_number(contract, "production_gate_ready"), 1);
    ASSERT_STREQ(mapping_string(contract, "production_gate_blocker"), "");
    ASSERT_STREQ(mapping_string(contract, "production_gate_required_users"), "1,3,10");
    ASSERT_STREQ(mapping_string(contract, "production_gate_required_durations"), "smoke,30m");
    ASSERT_EQ(mapping_number(contract, "production_gate_pressure_evidence_ready"), 1);
    ASSERT_STREQ(mapping_string(contract, "production_gate_pressure_evidence"),
                 "xkx_audit_10_users_30m_2026_06_25_zero_timeouts_zero_gateway_errors");
    ASSERT_STREQ(mapping_string(contract, "production_gate_required_modes"), "off,audit,enforced");
    ASSERT_STREQ(mapping_string(contract, "production_gate_required_scenarios"),
                 "login,create,move,chat,inventory,shop,quest,combat,skills,mail,reconnect,"
                 "gateway_callback,socket_callback,heartbeat,callout");
    ASSERT_STREQ(mapping_string(contract, "production_gate_evidence_schema"),
                 "multicore_production_gate_evidence_v1");
    ASSERT_EQ(mapping_number(contract, "production_gate_evidence_required"), 1);
    ASSERT_EQ(mapping_number(contract, "production_gate_short_smoke_sufficient"), 0);
    ASSERT_STREQ(mapping_string(contract, "production_gate_minimum_ready_evidence"),
                 "accepted_30m_pressure_scope_final_audit_and_socket_release_handshake");
    ASSERT_EQ(mapping_number(contract, "production_gate_unclassified_hotspots_required_zero"), 1);
    ASSERT_EQ(mapping_number(contract, "production_gate_direct_cross_owner_writes_required_zero"), 1);
    ASSERT_EQ(mapping_number(contract, "production_gate_context_leaks_required_zero"), 1);
    ASSERT_EQ(mapping_number(contract, "production_gate_future_backlog_required_zero"), 1);
    ASSERT_EQ(mapping_number(contract, "production_gate_same_owner_claim_conflict_required_zero"), 1);
    ASSERT_EQ(mapping_number(contract, "production_gate_gateway_error_delta_required_zero"), 1);
    ASSERT_STREQ(mapping_string(contract, "production_gate_socket_release_policy"),
                 "owner_safe_synchronous_release_acquire_handshake");
    ASSERT_EQ(mapping_number(contract, "production_gate_socket_release_handshake_ready"), 1);
    ASSERT_STREQ(mapping_string(contract, "production_gate_socket_release_handshake_evidence"),
                 "socket_release_owner_epoch_handshake_contract_v1");
    ASSERT_STREQ(mapping_string(contract, "production_gate_report_schema"),
                 "xkx_gateway_loadtest_report_v1");
    ASSERT_STREQ(mapping_string(contract, "production_gate_report_required_fields"),
                 "schema,run_id,mode,users_requested,duration_seconds,scenario,commands_ok,timeouts,"
                 "gateway_metrics_delta,production_gate_observations");
  };

  auto assert_contract = [&](mapping_t* status) {
    ASSERT_STREQ(mapping_string(status, "executor_contract_version"), "owner_executor_v2");
    ASSERT_STREQ(mapping_string(status, "executor_model"), "owner_executor");
    ASSERT_STREQ(mapping_string(status, "executor_dispatch_model"), "descriptor_manifest");
    ASSERT_STREQ(mapping_string(status, "executor_lpc_model"), "default_closed_explicit_open");
    ASSERT_STREQ(mapping_string(status, "ordinary_lpc_default_policy"), "default_closed_explicit_open");
    ASSERT_EQ(mapping_number(status, "ordinary_lpc_default_closed"), 1);
    ASSERT_EQ(mapping_number(status, "ordinary_lpc_activation_policy_ready"), 1);
    ASSERT_EQ(mapping_number(status, "ordinary_lpc_dispatch_path_ready"), 1);
    ASSERT_EQ(mapping_number(status, "ordinary_lpc_explicit_open_required"), 1);
    ASSERT_STREQ(mapping_string(status, "ordinary_lpc_activation_policy"), "default_closed_explicit_open");
    ASSERT_STREQ(mapping_string(status, "ordinary_lpc_next_blocker"), "");
    ASSERT_EQ(mapping_number(status, "owner_runtime_split_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_runtime_split_model"),
                 "runtime_v4_modules_with_owner_runtime_coordinator");
    ASSERT_EQ(mapping_number(status, "owner_runtime_v4_hardening_ready"), 1);
    ASSERT_EQ(mapping_number(status, "owner_runtime_benchmark_smoke_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_runtime_benchmark_schema"), "owner_runtime_bench_v1");
    ASSERT_EQ(mapping_number(status, "owner_runtime_stress_profile_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_runtime_stress_entry"), "tools/owner-runtime-v4-stress.sh");
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
    ASSERT_STREQ(mapping_string(status, "strict_owner_policy"), "strict_owner_owner_safe_payloads_v1");
    ASSERT_EQ(mapping_number(status, "lpcc_owner_audit_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "lpcc_owner_audit_schema"), "lpcc_owner_audit_v1");
    ASSERT_EQ(mapping_number(status, "lpcc_owner_audit_cli_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "lpcc_owner_audit_cli"), "lpcc --owner-audit --format=json");
    ASSERT_EQ(mapping_number(status, "lpcc_owner_audit_static_scanner_ready"), 1);
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
    ASSERT_STREQ(mapping_string(status, "lpc_value_object_model"), "frozen_snapshot_value_object_v1");
    ASSERT_EQ(mapping_number(status, "lpc_value_object_live_lifecycle_member"), 0);
    ASSERT_EQ(mapping_number(status, "lpc_value_object_cross_owner_payload_safe"), 1);
    ASSERT_EQ(mapping_number(status, "owner_snapshot_persistence_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_snapshot_persistence_model"),
                 "owner_snapshot_serialized_payload_v1");
    ASSERT_STREQ(mapping_string(status, "owner_snapshot_persistence_adapter"), "main_thread_file_adapter");
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
    ASSERT_STREQ(mapping_string(status, "owner_callback_diagnostics_schema"), "owner_callback_diagnostics_v1");
    ASSERT_STREQ(mapping_string(status, "owner_callback_failure_code_schema"), "owner_callback_failure_code_v1");
    ASSERT_STREQ(mapping_string(status, "owner_callback_drop_reason_schema"), "owner_callback_drop_reason_v1");
    ASSERT_EQ(mapping_number(status, "owner_callback_allowlist_complete"), 1);
    ASSERT_STREQ(mapping_string(status, "owner_callback_supported_kinds"),
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
  ASSERT_STREQ(mapping_string(status, "gateway_latency_probe_fields"),
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
    ASSERT_STREQ(mapping_string(status, "owner_callback_failure_codes"),
                 "owner_scheduler_backpressure,callback_not_allowlisted,callback_invalid_target,"
                 "owner_epoch_mismatch,target_destructed,target_stale,admission_rejected,task_dropped");
    ASSERT_STREQ(mapping_string(status, "owner_callback_drop_reasons"),
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
    ASSERT_STREQ(mapping_string(status, "executor_callback_payload_policy"), "frozen_payload_or_owner_handle_only");
    ASSERT_EQ(mapping_number(status, "heartbeat_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "heartbeat_owner_executor_task_type"), "heartbeat");
    ASSERT_STREQ(mapping_string(status, "heartbeat_owner_executor_route"), "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(status, "heartbeat_owner_executor_fallback_route"),
                 "");
    ASSERT_STREQ(mapping_string(status, "heartbeat_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_EQ(mapping_number(status, "heartbeat_owner_executor_fallback_main_ready"), 1);
    ASSERT_EQ(mapping_number(status, "heartbeat_current_object_thread_local"), 0);
    ASSERT_EQ(mapping_number(status, "callout_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "callout_owner_executor_task_type"), "call_out");
    ASSERT_STREQ(mapping_string(status, "callout_owner_executor_route"), "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(status, "callout_owner_executor_fallback_route"),
                 "");
    ASSERT_STREQ(mapping_string(status, "callout_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_EQ(mapping_number(status, "callout_owner_executor_expired_handle_detach_ready"), 1);
    ASSERT_EQ(mapping_number(status, "callout_owner_executor_cleanup_main_ready"), 1);
    ASSERT_EQ(mapping_number(status, "callout_owner_executor_drop_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(status, "callout_owner_executor_fallback_main_ready"), 1);
    ASSERT_EQ(mapping_number(status, "async_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "async_owner_executor_task_type"), "async_callback");
    ASSERT_STREQ(mapping_string(status, "async_owner_executor_route"), "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(status, "async_owner_executor_fallback_route"),
                 "");
    ASSERT_STREQ(mapping_string(status, "async_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_STREQ(mapping_string(status, "async_owner_executor_result_policy"), "frozen_deep_copy_result");
    ASSERT_EQ(mapping_number(status, "async_owner_executor_cleanup_main_ready"), 1);
    ASSERT_EQ(mapping_number(status, "async_owner_executor_drop_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(status, "dns_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "dns_owner_executor_task_type"), "dns_callback");
    ASSERT_STREQ(mapping_string(status, "dns_owner_executor_route"), "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(status, "dns_owner_executor_fallback_route"), "");
    ASSERT_STREQ(mapping_string(status, "dns_owner_executor_policy"), "main_thread_callback_adapter_after_owner_admission");
    ASSERT_STREQ(mapping_string(status, "dns_owner_executor_result_policy"), "frozen_deep_copy_result");
    ASSERT_EQ(mapping_number(status, "dns_owner_executor_owner_epoch_capture_ready"), 1);
    ASSERT_EQ(mapping_number(status, "dns_owner_executor_cleanup_main_ready"), 1);
    ASSERT_EQ(mapping_number(status, "dns_owner_executor_drop_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(status, "socket_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "socket_owner_executor_task_type"), "socket_callback");
    ASSERT_STREQ(mapping_string(status, "socket_owner_executor_route"), "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(status, "socket_owner_executor_fallback_route"),
                 "");
    ASSERT_STREQ(mapping_string(status, "socket_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_STREQ(mapping_string(status, "socket_owner_executor_result_policy"), "frozen_deep_copy_args");
    ASSERT_EQ(mapping_number(status, "socket_owner_executor_cleanup_main_ready"), 1);
    ASSERT_EQ(mapping_number(status, "socket_owner_executor_drop_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(status, "socket_release_main_required"), 0);
    ASSERT_EQ(mapping_number(status, "socket_release_owner_safe_handshake_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "socket_release_owner_safe_handshake_policy"),
                 "synchronous_release_acquire_owner_epoch_guard");
    ASSERT_EQ(mapping_number(status, "socket_release_owner_epoch_guard_ready"), 1);
    ASSERT_EQ(mapping_number(status, "gateway_command_execute_ready"), 1);
    ASSERT_STREQ(mapping_string(status, "gateway_command_execute_task_type"), "gateway_command_execute");
    ASSERT_STREQ(mapping_string(status, "gateway_command_execute_route"), "owner_main_queue_io_adapter");
    ASSERT_STREQ(mapping_string(status, "gateway_command_execute_fallback_route"),
                 "");
    ASSERT_STREQ(mapping_string(status, "gateway_command_execute_policy"),
                 "main_thread_io_adapter_until_interactive_detached");
    ASSERT_STREQ(mapping_string(status, "gateway_command_execute_payload_policy"), "owner_private_command_snapshot");
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
    ASSERT_STREQ(mapping_string(vm_context_contract, "execution_state_model"), "vm_context_execution_snapshot");
    ASSERT_STREQ(mapping_string(vm_context_contract, "owner_state_model"), "vm_context_owner_scope");
    ASSERT_STREQ(mapping_string(vm_context_contract, "error_state_model"), "vm_context_error_snapshot");
    ASSERT_STREQ(mapping_string(vm_context_contract, "object_store_model"), "owner_local_object_store");
    ASSERT_STREQ(mapping_string(vm_context_contract, "object_store_off_main_policy"), "owner_local_lookup_only");
    ASSERT_EQ(mapping_number(vm_context_contract, "ordinary_lpc_ready"), 1);
    ASSERT_STREQ(mapping_string(vm_context_contract, "ordinary_lpc_blocker"), "");
    ASSERT_EQ(mapping_number(vm_context_contract, "controlled_lpc_ready"), 1);
    ASSERT_STREQ(mapping_string(vm_context_contract, "controlled_lpc_policy"), "descriptor_manifest_only");
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
    ASSERT_STREQ(mapping_string(vm_context_contract, "object_refs_model"), "object_handle_boundary");
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
    ASSERT_EQ(mapping_number(frozen_payload_contract, "top_level_owner_payload_must_be_mapping"), 1);
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
    ASSERT_STREQ(mapping_string(frozen_path("owner_call_async"), "result_policy"), "frozen_result_required");
    ASSERT_EQ(mapping_number(frozen_path("owner_call_async"), "frozen_result_required"), 1);
    ASSERT_STREQ(mapping_string(frozen_path("owner_publish_snapshot"), "result_policy"), "snapshot_only");
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
    ASSERT_STREQ(mapping_string(boundary_contract, "implementation_state"), "compilation_unit_active");
    ASSERT_STREQ(mapping_string(boundary_contract, "class_name"), "OwnerExecutor");
    ASSERT_EQ(mapping_number(boundary_contract, "class_extracted"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "module_extracted"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "module_file"), "vm/internal/owner_executor.h");
    ASSERT_EQ(mapping_number(boundary_contract, "compilation_unit_extracted"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "compilation_unit_file"), "vm/internal/owner_executor.cc");
    ASSERT_EQ(mapping_number(boundary_contract, "depends_on_owner_cc_internal_state"), 0);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_split_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_runtime_split_model"),
                 "runtime_v4_modules_with_owner_runtime_coordinator");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_v4_hardening_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_benchmark_smoke_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_runtime_benchmark_schema"), "owner_runtime_bench_v1");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_stress_profile_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_runtime_stress_entry"),
                 "tools/owner-runtime-v4-stress.sh");
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_modern_runtime_stress_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "lpc_modern_runtime_stress_entry"),
                 "tools/lpc-modern-runtime-stress.sh");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_layering_guard_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_coordinator_module_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_runtime_coordinator_file"),
                 "vm/internal/owner_runtime_coordinator.cc");
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_runtime_store_owner"), "OwnerRuntimeCoordinator");
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_cc_runtime_role"),
                 "runtime_status_facade_and_legacy_glue");
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_modern_profile_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "lpc_modern_profile_schema"), "lpc_modern_profile_v1");
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
    ASSERT_STREQ(mapping_string(boundary_contract, "object_store_benchmark_schema"), "object_store_bench_v1");
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
    ASSERT_STREQ(mapping_string(boundary_contract, "strict_owner_policy"), "strict_owner_owner_safe_payloads_v1");
    ASSERT_EQ(mapping_number(boundary_contract, "lpcc_owner_audit_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "lpcc_owner_audit_schema"), "lpcc_owner_audit_v1");
    ASSERT_EQ(mapping_number(boundary_contract, "lpcc_owner_audit_cli_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "lpcc_owner_audit_cli"), "lpcc --owner-audit --format=json");
    ASSERT_EQ(mapping_number(boundary_contract, "lpcc_owner_audit_static_scanner_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "lpc_source_encoding_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "lpc_source_encoding_schema"), "lpc_source_encoding_v1");
    ASSERT_STREQ(mapping_string(boundary_contract, "vm_internal_string_encoding"), "utf-8");
    ASSERT_EQ(mapping_number(boundary_contract, "session_encoding_contract_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "gateway_encoding_boundary_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "encoding_audit_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "legacy_lpc_default_closed"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "lpc_modern_profile_module_file"),
                 "compiler/internal/lpc_modern_profile.cc");
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
    ASSERT_EQ(mapping_number(boundary_contract, "owner_snapshot_direct_save_hot_path_audit_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_commit_api_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_commit_model"), "owner_commit_boundary_record");
    ASSERT_STREQ(mapping_string(boundary_contract, "lpc_modern_api_file"), "packages/core/vm_owner.cc");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_task_manifest_module_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_task_manifest_module_file"),
                 "vm/internal/owner_task_manifest.cc");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_trace_store_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_trace_store_file"), "vm/internal/owner_trace_store.cc");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_future_store_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_future_store_file"), "vm/internal/owner_future_store.cc");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_scheduler_state_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_scheduler_state_file"),
                 "vm/internal/owner_scheduler_state.cc");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_metrics_store_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_metrics_store_file"),
                 "vm/internal/owner_runtime_metrics.cc");
    ASSERT_EQ(mapping_number(boundary_contract, "object_store_owner_fast_path_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "object_store_global_fallback_on_owner_fast_path"), 0);
    ASSERT_EQ(mapping_number(boundary_contract, "object_handle_capability_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "object_handle_capability_model"),
                 "object_handle_capability_v1");
    ASSERT_STREQ(mapping_string(boundary_contract, "object_handle_capability_file"), "vm/object_handle.h");
    ASSERT_STREQ(mapping_string(boundary_contract, "object_handle_permission_intent_default"),
                 "owner_runtime");
    ASSERT_EQ(mapping_number(boundary_contract, "owner_scheduler_backpressure_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_scheduler_backpressure_strategy"),
                 "observe_then_reject_new_tasks");
    ASSERT_GT(mapping_number(boundary_contract, "owner_scheduler_max_owner_queue_depth"), 0);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_scheduler_fairness_guard_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_future_timeout_cancel_drop_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "dependency_manifest_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "runtime_dependency_contract_version"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "dependency_domains"),
                 "owner_scheduler_state,owner_task_manifest,owner_trace_store,owner_future_store,"
                 "owner_runtime_metrics,task_dispatch,vm_context");
    ASSERT_EQ(mapping_number(boundary_contract, "scheduler_state_dependency"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "mailbox_state_dependency"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "task_dispatch_dependency"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "vm_context_dependency"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "metric_counter_dependency"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "future_completion_dependency"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_facade_required"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_runtime_facade_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_runtime_facade_model"),
                 "owner_executor_runtime_facade_v1");
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_runtime_facade_file"), "vm/internal/owner.cc");
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
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_callback_supported_kinds"),
                 "heartbeat,call_out,async_callback,dns_callback,socket_callback,gateway_command_execute,ed_callback");
    ASSERT_EQ(mapping_number(boundary_contract, "executor_callback_cleanup_main_required"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "executor_callback_main_adapter_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "executor_callback_allowlist_count"), 6);
    ASSERT_STREQ(mapping_string(boundary_contract, "executor_callback_allowlist"),
                 "heartbeat,call_out,async_callback,dns_callback,socket_callback,ed_callback");
    ASSERT_EQ(mapping_number(boundary_contract, "heartbeat_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "heartbeat_owner_executor_task_type"), "heartbeat");
    ASSERT_STREQ(mapping_string(boundary_contract, "heartbeat_owner_executor_route"), "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(boundary_contract, "heartbeat_owner_executor_fallback_route"),
                 "");
    ASSERT_STREQ(mapping_string(boundary_contract, "heartbeat_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_EQ(mapping_number(boundary_contract, "heartbeat_owner_executor_fallback_main_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "heartbeat_current_object_thread_local"), 0);
    ASSERT_EQ(mapping_number(boundary_contract, "callout_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "callout_owner_executor_task_type"), "call_out");
    ASSERT_STREQ(mapping_string(boundary_contract, "callout_owner_executor_route"), "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(boundary_contract, "callout_owner_executor_fallback_route"),
                 "");
    ASSERT_STREQ(mapping_string(boundary_contract, "callout_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_EQ(mapping_number(boundary_contract, "callout_owner_executor_expired_handle_detach_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "callout_owner_executor_cleanup_main_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "callout_owner_executor_drop_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "callout_owner_executor_fallback_main_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "async_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "async_owner_executor_task_type"), "async_callback");
    ASSERT_STREQ(mapping_string(boundary_contract, "async_owner_executor_route"), "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(boundary_contract, "async_owner_executor_fallback_route"),
                 "");
    ASSERT_STREQ(mapping_string(boundary_contract, "async_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_STREQ(mapping_string(boundary_contract, "async_owner_executor_result_policy"),
                 "frozen_deep_copy_result");
    ASSERT_EQ(mapping_number(boundary_contract, "async_owner_executor_cleanup_main_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "async_owner_executor_drop_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "dns_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "dns_owner_executor_task_type"), "dns_callback");
    ASSERT_STREQ(mapping_string(boundary_contract, "dns_owner_executor_route"), "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(boundary_contract, "dns_owner_executor_fallback_route"),
                 "");
    ASSERT_STREQ(mapping_string(boundary_contract, "dns_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_STREQ(mapping_string(boundary_contract, "dns_owner_executor_result_policy"), "frozen_deep_copy_result");
    ASSERT_EQ(mapping_number(boundary_contract, "dns_owner_executor_owner_epoch_capture_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "dns_owner_executor_cleanup_main_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "dns_owner_executor_drop_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "socket_owner_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "socket_owner_executor_task_type"), "socket_callback");
    ASSERT_STREQ(mapping_string(boundary_contract, "socket_owner_executor_route"), "owner_main_queue_callback_adapter");
    ASSERT_STREQ(mapping_string(boundary_contract, "socket_owner_executor_fallback_route"),
                 "");
    ASSERT_STREQ(mapping_string(boundary_contract, "socket_owner_executor_policy"),
                 "main_thread_callback_adapter_after_owner_admission");
    ASSERT_STREQ(mapping_string(boundary_contract, "socket_owner_executor_result_policy"), "frozen_deep_copy_args");
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
    ASSERT_STREQ(mapping_string(boundary_contract, "gateway_command_execute_task_type"), "gateway_command_execute");
    ASSERT_STREQ(mapping_string(boundary_contract, "gateway_command_execute_route"), "owner_main_queue_io_adapter");
    ASSERT_STREQ(mapping_string(boundary_contract, "gateway_command_execute_fallback_route"),
                 "");
    ASSERT_STREQ(mapping_string(boundary_contract, "gateway_command_execute_policy"),
                 "main_thread_io_adapter_until_interactive_detached");
    ASSERT_STREQ(mapping_string(boundary_contract, "gateway_command_execute_payload_policy"),
                 "owner_private_command_snapshot");
    ASSERT_EQ(mapping_number(boundary_contract, "gateway_command_execute_reply_queue_main_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "gateway_command_execute_stale_drop_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "gateway_command_execute_context_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "gateway_command_execute_session_revalidate_ready"), 1);
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
    ASSERT_EQ(mapping_number(boundary_contract, "owner_service_registry_lpc_domain_alignment_ready"), 1);
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
    ASSERT_EQ(mapping_number(boundary_contract, "owner_scheduler_tick_group_backpressure_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_scheduler_starvation_guard_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "target_owner_message_executor_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "normal_path_main_fallback_count"), 0);
    ASSERT_EQ(mapping_number(boundary_contract, "normal_path_main_fallback_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "main_fallback_policy_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "main_fallback_classification"), "explicit_policy");
    ASSERT_EQ(mapping_number(boundary_contract, "session_fifo_contract_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "gateway_io_adapter_only_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "gateway_io_boundary"), "main_thread_io_adapter");
    ASSERT_EQ(mapping_number(boundary_contract, "gateway_low_overhead_latency_probe_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "gateway_latency_probe_source"), "gateway_status_internal");
  ASSERT_STREQ(mapping_string(boundary_contract, "gateway_latency_probe_fields"),
               "receive_decode,receive_payload_copy,receive_enqueue_to_dispatch,receive_apply,"
               "command_enqueue_to_dispatch,command_execute,"
               "receive_main_queue_depth,deferred_main_drain_wait,"
               "reply_enqueue_to_dispatch,reply_execute,output_enqueue_to_dispatch,output_execute,main_drain");
    ASSERT_EQ(mapping_number(boundary_contract, "callback_payload_strict_ready"), 1);
    ASSERT_EQ(mapping_number(boundary_contract, "owner_callback_payload_strict_diagnostics_ready"), 1);
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_callback_payload_policy_schema"),
                 "owner_callback_payload_policy_v1");
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_callback_payload_policy"),
                 "frozen_payload_or_owner_handle_only");
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_callback_failure_codes"),
                 "owner_scheduler_backpressure,callback_not_allowlisted,callback_invalid_target,"
                 "owner_epoch_mismatch,target_destructed,target_stale,admission_rejected,task_dropped");
    ASSERT_STREQ(mapping_string(boundary_contract, "owner_callback_drop_reasons"),
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
    ASSERT_STREQ(mapping_string(boundary_contract, "next_refactor_target"), "");
    assert_production_gate_contract(boundary_contract);

    auto* gateway_contract = mapping_entry(status, "gateway_owner_task_contract");
    ASSERT_NE(gateway_contract, nullptr);
    ASSERT_EQ(mapping_number(gateway_contract, "contract_version"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "input_model"), "owner_executor_with_main_fallback");
    ASSERT_STREQ(mapping_string(gateway_contract, "executor_migration_state"), "owner_executor_active");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_payload_model"),
                 "gateway_command_buffer_metadata_v1");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_input_source"), "interactive_text_buffer");
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
    ASSERT_EQ(mapping_number(gateway_contract, "command_input_callback_apply_frame_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "command_input_callback_mode_delta_model"),
                 "owner_command_frame_input_callback_mode_delta");
    ASSERT_EQ(mapping_number(gateway_contract, "command_input_callback_mode_delta_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "command_input_callback_mode_delta_executor_ready"), 1);
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
    ASSERT_EQ(mapping_number(gateway_contract, "process_input_add_action_parser_frame_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "process_input_add_action_parser_blocker"), "");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_executor_blocker"),
                 "interactive_command_requires_main_thread_io_adapter");
    ASSERT_EQ(mapping_number(gateway_contract, "gateway_command_execute_ready"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "gateway_command_execute_task_type"), "gateway_command_execute");
    ASSERT_STREQ(mapping_string(gateway_contract, "gateway_command_execute_route"), "owner_main_queue_io_adapter");
    ASSERT_STREQ(mapping_string(gateway_contract, "gateway_command_execute_fallback_route"),
                 "");
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
    ASSERT_STREQ(mapping_string(gateway_contract, "command_reply_queue_task_type"), "command_reply");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_reply_queue_task_key"),
                 "prompt_telnet_reschedule_io");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_reply_queue_side_effects"),
                 "prompt_telnet_reschedule_io");
    ASSERT_EQ(mapping_number(gateway_contract, "command_reply_queue_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "command_reply_queue_main_required"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "command_reply_write_prompt_apply_frame_model"),
                 "owner_command_frame_write_prompt_apply");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_reply_write_prompt_apply_frame_task_type"),
                 "command_reply");
    ASSERT_EQ(mapping_number(gateway_contract, "command_reply_write_prompt_apply_frame_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "command_reply_write_prompt_apply_frame_executor_ready"), 0);
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
    ASSERT_EQ(mapping_number(gateway_contract, "interactive_mode_localecho_restore_executor_ready"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "command_mode_delta_terminal_mode_task_type"),
                 "command_mode_delta");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_mode_delta_terminal_mode_task_keys"),
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
    ASSERT_EQ(mapping_number(gateway_contract, "interactive_mode_mxp_tag_filter_executor_ready"), 1);
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
    ASSERT_STREQ(mapping_string(gateway_contract, "command_stale_guard"), "owner_epoch_target_handle_guard");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_stale_trace_state"), "main_stale");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_stale_target_status"), "owner_epoch_mismatch");
    ASSERT_EQ(mapping_number(gateway_contract, "gateway_command_execute_stale_drop_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "gateway_command_execute_context_cleanup_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "gateway_command_execute_session_revalidate_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_contract, "gateway_command_execute_reply_queue_main_ready"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "command_executor_readiness_gate_model"),
                 "all_gates_required_before_owner_executor");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_executor_next_gate"), "");
    ASSERT_STREQ(mapping_string(gateway_contract, "command_executor_next_blocker"), "");
    assert_production_gate_contract(gateway_contract);
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
    auto* command_executor_gates = mapping_array(gateway_contract, "command_executor_readiness_gates");
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
    ASSERT_EQ(mapping_number(command_executor_gate("owner_epoch_target_handle_guard"), "satisfied"), 1);
    ASSERT_EQ(mapping_number(command_executor_gate("owner_owned_command_snapshot"), "satisfied"), 1);
    ASSERT_STREQ(mapping_string(command_executor_gate("owner_owned_command_snapshot"), "blocker"), "");
    ASSERT_EQ(mapping_number(command_executor_gate("owner_owned_command_consume"), "satisfied"), 1);
    ASSERT_STREQ(mapping_string(command_executor_gate("owner_owned_command_consume"), "blocker"), "");
    ASSERT_EQ(mapping_number(command_executor_gate("owner_executor_command_consume_entry"), "satisfied"), 1);
    ASSERT_STREQ(mapping_string(command_executor_gate("owner_executor_command_consume_entry"), "blocker"), "");
    ASSERT_EQ(mapping_number(command_executor_gate("owner_executor_frame_restore"), "satisfied"), 1);
    ASSERT_STREQ(mapping_string(command_executor_gate("owner_executor_frame_restore"), "blocker"), "");
    ASSERT_EQ(mapping_number(command_executor_gate("ordinary_lpc_ready"), "satisfied"), 1);
    ASSERT_STREQ(mapping_string(command_executor_gate("ordinary_lpc_ready"), "blocker"), "");
    ASSERT_EQ(mapping_number(command_executor_gate("gateway_command_executor_activation"), "satisfied"), 1);
    ASSERT_STREQ(mapping_string(command_executor_gate("gateway_command_executor_activation"), "blocker"),
                 "interactive_command_requires_main_thread_io_adapter");
    auto* command_side_effect_gates = mapping_array(gateway_contract, "command_side_effect_readiness_gates");
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
    ASSERT_EQ(mapping_number(command_side_effect_gate("interactive_buffer_consume"), "satisfied"), 1);
    ASSERT_EQ(mapping_number(command_side_effect_gate("interactive_buffer_consume"), "blocks_activation"), 0);
    ASSERT_STREQ(mapping_string(command_side_effect_gate("interactive_buffer_consume"), "blocker"), "");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("interactive_buffer_consume"), "state_owner"),
                 "owner_command_snapshot");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("interactive_buffer_consume"), "migration_boundary"),
                 "main_thread_consume_before_executor_activation");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("interactive_buffer_consume"), "side_effect_class"),
                 "input_buffer_consume");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("interactive_buffer_consume"), "snapshot_policy"),
                 "owner_private_command_text_snapshot_v1");
    ASSERT_EQ(mapping_number(command_side_effect_gate("input_to_get_char_state"), "satisfied"), 1);
    ASSERT_EQ(mapping_number(command_side_effect_gate("input_to_get_char_state"), "blocks_activation"), 0);
    ASSERT_STREQ(mapping_string(command_side_effect_gate("input_to_get_char_state"), "blocker"), "");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("input_to_get_char_state"), "state_owner"),
                 "owner_command_frame");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("input_to_get_char_state"), "migration_boundary"),
                 "owner_command_frame_input_callback_executor");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("input_to_get_char_state"), "side_effect_class"),
                 "input_callback_state");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("input_to_get_char_state"), "snapshot_policy"),
                 "redacted_input_to_get_char_state_v1");
    ASSERT_EQ(mapping_number(command_side_effect_gate("process_input_add_action_parser"), "satisfied"), 1);
    ASSERT_EQ(mapping_number(command_side_effect_gate("process_input_add_action_parser"), "blocks_activation"), 0);
    ASSERT_STREQ(mapping_string(command_side_effect_gate("process_input_add_action_parser"), "blocker"), "");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("process_input_add_action_parser"), "state_owner"),
                 "owner_command_frame");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("process_input_add_action_parser"), "migration_boundary"),
                 "owner_command_parser_context_executor");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("process_input_add_action_parser"), "side_effect_class"),
                 "parser_command_giver_state");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("process_input_add_action_parser"), "snapshot_policy"),
                 "redacted_process_input_add_action_parser_state_v1");
    ASSERT_EQ(mapping_number(command_side_effect_gate("prompt_telnet_reschedule_io"), "satisfied"), 1);
    ASSERT_EQ(mapping_number(command_side_effect_gate("prompt_telnet_reschedule_io"), "blocks_activation"), 0);
    ASSERT_STREQ(mapping_string(command_side_effect_gate("prompt_telnet_reschedule_io"), "blocker"), "");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("prompt_telnet_reschedule_io"), "state_owner"),
                 "main_reply_queue_and_network_io");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("prompt_telnet_reschedule_io"), "migration_boundary"),
                 "main_reply_queue_after_owner_command");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("prompt_telnet_reschedule_io"), "side_effect_class"),
                 "prompt_telnet_reschedule_io");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("prompt_telnet_reschedule_io"), "snapshot_policy"),
                 "redacted_prompt_telnet_reschedule_io_v1");
    ASSERT_EQ(mapping_number(command_side_effect_gate("interactive_mode_flags"), "satisfied"), 1);
    ASSERT_EQ(mapping_number(command_side_effect_gate("interactive_mode_flags"), "blocks_activation"), 0);
    ASSERT_STREQ(mapping_string(command_side_effect_gate("interactive_mode_flags"), "blocker"), "");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("interactive_mode_flags"), "state_owner"), "owner_command_frame");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("interactive_mode_flags"), "migration_boundary"),
                 "owner_command_frame_mode_delta_executor");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("interactive_mode_flags"), "side_effect_class"),
                 "echo_mxp_ed_mode_flags");
    ASSERT_STREQ(mapping_string(command_side_effect_gate("interactive_mode_flags"), "snapshot_policy"),
                 "redacted_interactive_mode_flags_v1");
    ASSERT_EQ(mapping_number(gateway_contract, "ordinary_lpc_ready_required"), 0);
    ASSERT_EQ(mapping_number(gateway_contract, "main_required"), 0);
    ASSERT_EQ(mapping_number(gateway_contract, "fallback_main_required"), 1);
    ASSERT_STREQ(mapping_string(gateway_contract, "next_blocker"), "");
    ASSERT_STREQ(mapping_string(gateway_contract, "next_blocker_chain"),
                 "production_gate_complete");
    assert_production_gate_contract(gateway_contract);
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
    auto assert_gateway_task = [&](const char* task_key, const char* executor_mode, const char* route,
                                   long requires_main_queue, const char* owner_scope_model,
                                   const char* stale_policy) {
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
                        "owner_scope_current_interactive_command_giver", "owner_epoch_target_guard");
    ASSERT_STREQ(mapping_string(gateway_task("process_user_command"), "payload_key"), "gateway_command_input");
    ASSERT_STREQ(mapping_string(gateway_task("process_user_command"), "input_payload_policy"),
                 "buffer_metadata_no_raw_command_text");
    ASSERT_STREQ(mapping_string(gateway_task("process_user_command"), "command_consume_model"),
                 "owner_owned_snapshot_main_thread_consume");
    ASSERT_EQ(mapping_number(gateway_task("process_user_command"), "command_consume_snapshot_ready"), 1);
    ASSERT_EQ(mapping_number(gateway_task("process_user_command"), "command_consume_executor_ready"), 0);
    ASSERT_STREQ(mapping_string(gateway_task("process_user_command"), "command_consume_blocker"),
                 "interactive_command_requires_main_thread_io_adapter");
    ASSERT_STREQ(mapping_string(gateway_task("process_user_command"), "execution_frame_model"),
                 "gateway_command_execution_frame_v1");
    ASSERT_STREQ(mapping_string(gateway_task("process_user_command"), "execution_frame_policy"),
                 "owner_scope_current_interactive_command_giver");
    ASSERT_STREQ(mapping_string(gateway_task("process_user_command"), "execution_frame_restore_policy"),
                 "main_thread_vmcontext_scope");
    ASSERT_EQ(mapping_number(gateway_task("process_user_command"), "execution_frame_restore_ready"), 1);
    ASSERT_STREQ(mapping_string(gateway_task("process_user_command"), "execution_frame_restore_blocker"), "");
    ASSERT_EQ(mapping_number(gateway_task("process_user_command"), "execution_frame_executor_ready"), 0);
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
      found_top_level_player_domain = found_top_level_player_domain || method == "owner_task_player";
      found_top_level_economy_domain = found_top_level_economy_domain || method == "owner_task_economy";
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
      ASSERT_STREQ(mapping_string(entry, "admission_policy"), "owner_epoch_payload_allowlist_deadline_guard");
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
    auto assert_dispatch = [&](const char* task_type, const char* contract_key, const char* dispatch_kind,
                               const char* executor_mode, long executor_runnable, long executor_safe,
                               long rejected, const char* route = "owner_executor", long main_required = 0) {
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
    assert_dispatch("lpc_task", "lpc_task_allowlist", "lpc_task", "executor_safe_allowlist", 1, 1, 0);
    assert_dispatch("ordinary_lpc", "ordinary_lpc_dispatch", "ordinary_lpc", "executor_safe_explicit_open", 1, 1, 0);
    assert_dispatch("owner_message", "owner_message_mailbox", "owner_message", "executor_safe", 1, 1, 0);
    assert_dispatch("command_consume", "owner_executor_command_consumer", "command_consume", "executor_safe", 1,
                    1, 0);
    assert_dispatch("command_frame_restore", "owner_executor_command_frame_restore", "command_frame_restore",
                    "executor_safe", 1, 1, 0);
    assert_dispatch("gateway_command", "gateway_command_executor_activation", "gateway_command", "executor_safe", 1, 1,
                    0);
    assert_dispatch("heartbeat", "owner_executor_callback", "executor_callback", "main_required_callback", 0, 0, 0,
                    "owner_main_queue_callback_adapter", 1);
    assert_dispatch("call_out", "owner_executor_callback", "executor_callback", "main_required_callback", 0, 0, 0,
                    "owner_main_queue_callback_adapter", 1);
    assert_dispatch("async_callback", "owner_executor_callback", "executor_callback", "main_required_callback", 0, 0,
                    0, "owner_main_queue_callback_adapter", 1);
    assert_dispatch("dns_callback", "owner_executor_callback", "executor_callback", "main_required_callback", 0, 0, 0,
                    "owner_main_queue_callback_adapter", 1);
    assert_dispatch("socket_callback", "owner_executor_callback", "executor_callback", "main_required_callback", 0, 0,
                    0, "owner_main_queue_callback_adapter", 1);
    assert_dispatch("gateway_command_execute", "gateway_command_main_queue", "main_thread",
                    "main_required", 1, 0, 0, "owner_main_queue", 1);
    assert_dispatch("ed_callback", "owner_executor_callback", "executor_callback", "main_required_callback", 0, 0, 0,
                    "owner_main_queue_callback_adapter", 1);
    assert_dispatch("room_output_projection", "room_output_projection", "executor_callback",
                    "executor_safe", 1, 1, 0);
    assert_dispatch("compute_result", "compute_result", "compute_result", "executor_safe", 1, 1, 0);
    assert_dispatch("lpc", "lpc", "reject_lpc", "rejected", 1, 0, 1);
    assert_dispatch("owner_state", "owner_state", "guard_owner_state", "rejected", 1, 0, 1);
    ASSERT_STREQ(mapping_string(dispatch_entry("gateway_command_execute"), "tick_group"), "gateway_command");
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
    ASSERT_STREQ(mapping_string(nested_lpc_contracts->item[0].u.map, "method"), "owner_task_readonly");
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

  auto cancel_future_id =
      vm_owner_register_compute_future("owner/test/future-cancel", 9101, "manifest_cancel", "v2_cancel");
  auto timeout_future_id =
      vm_owner_register_compute_future("owner/test/future-timeout", 9102, "manifest_timeout", "v2_timeout");
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
  ASSERT_GE(mapping_number(after_future_status, "owner_executor_future_cancelled"), before_cancelled + 1);
  ASSERT_GE(mapping_number(after_future_status, "owner_executor_future_timeout"), before_timeout + 1);
  ASSERT_GE(mapping_number(after_future_status, "futures_failed"), before_failed + 2);
  ASSERT_EQ(mapping_number(after_future_status, "owner_executor_future_pending_backlog"),
            mapping_number(after_future_status, "pending_futures"));
  free_mapping(after_future_status);
}

TEST_F(DriverTest, TestOwnerRuntimeLayeringGuardKeepsStoresOutOfOwnerCc) {
  const auto owner_cc = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto coordinator_cc = read_source_file_for_test("../src/vm/internal/owner_runtime_coordinator.cc");
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
    ASSERT_EQ(owner_cc.find(declaration), std::string::npos) << "owner.cc must not own runtime store: "
                                                             << declaration;
  }

  ASSERT_NE(coordinator_cc.find("OwnerRuntimeCoordinator &owner_runtime_coordinator()"), std::string::npos);
  ASSERT_NE(coordinator_cc.find("OwnerRuntimeMetrics &owner_runtime_metrics_instance()"), std::string::npos);
  ASSERT_NE(coordinator_cc.find("OwnerFutureStore &owner_future_store_instance()"), std::string::npos);
  ASSERT_NE(coordinator_cc.find("OwnerSchedulerState &owner_scheduler_state_instance()"), std::string::npos);
  ASSERT_NE(coordinator_cc.find("OwnerTraceStore &owner_trace_store_instance()"), std::string::npos);
}

TEST_F(DriverTest, TestVmOwnerExecutorBudgetYieldsAndRequeuesSameOwnerBacklog) {
  const char* owner = "owner/test/executor/budget-yield";

  vm_owner_thread_stop();
  // Reset the global yield-observation baseline under the runtime lock so
  // the test does not inherit observations from earlier tests.
  vm_owner_test_support_reset_budget_yield_observations();
  test_set_env("FLUFFOS_OWNER_EXECUTOR_PROBE_DELAY_MS", "5");
  struct ProbeDelayGuard {
    ~ProbeDelayGuard() { test_unset_env("FLUFFOS_OWNER_EXECUTOR_PROBE_DELAY_MS"); }
  } probe_delay_guard;
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
    auto last_yield_safe_backlog = mapping_number(status, "executor_last_budget_yield_safe_backlog");
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
  ASSERT_STREQ(mapping_string(executor_trace, "owner_task_manifest_schema"), "owner_task_manifest_v2");
  ASSERT_STREQ(mapping_string(executor_trace, "admission_policy"), "owner_epoch_payload_allowlist_deadline_guard");
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
    ASSERT_STREQ(mapping_string(event, "admission_policy"), "owner_epoch_payload_allowlist_deadline_guard");
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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
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
  auto before_last_yield_safe_backlog = mapping_number(before, "executor_last_budget_yield_safe_backlog");
  auto before_last_yield_owner = std::string(mapping_string(before, "executor_last_budget_yield_owner"));
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
  ASSERT_STREQ(mapping_string(running, "executor_last_budget_yield_owner"), before_last_yield_owner.c_str());
  ASSERT_EQ(mapping_number(running, "executor_last_budget_yield_backlog"), before_last_yield_backlog);
  ASSERT_EQ(mapping_number(running, "executor_last_budget_yield_safe_backlog"), before_last_yield_safe_backlog);
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, &const0u) << key;
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER) << key;
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr) << key;
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING) << key;
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };

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
    if (probe_done >= before_probe + 2 && releases_done >= before_releases + 2 && active_owners == 0) {
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
  ASSERT_GE(mapping_number(running, "executor_max_parallel_owners"), std::max<long>(2, before_max_parallel));
  ASSERT_GE(mapping_number(running, "executor_max_owner_parallel"), std::max<long>(1, before_max_owner_parallel));
  ASSERT_LE(mapping_number(running, "executor_max_owner_parallel"), 1);
  ASSERT_EQ(mapping_number(running, "executor_same_owner_claim_conflicts"), 0);
  ASSERT_EQ(mapping_number(running, "active_owners"), 0);
  free_mapping(running);

  vm_owner_thread_stop();
}

TEST_F(DriverTest, TestVmOwnerStartCannotCrossAnInProgressStop) {
  constexpr const char *kOwner = "owner/test/executor/stop-start-guard";
  vm_owner_thread_stop();
  auto mapping_number = [](mapping_t *map, const char *key) -> long {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };

  auto *before = vm_owner_thread_status();
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
    auto *status = vm_owner_thread_status();
    saw_active_claim =
        mapping_number(status, "active_owners") == 1 &&
        mapping_number(status, "executor_active_claims") == 1;
    free_mapping(status);
    if (saw_active_claim) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_TRUE(saw_active_claim);

  std::thread stopper([] { vm_owner_thread_stop(); });
  bool saw_stop_owned_threads = false;
  for (int i = 0; i < 200; ++i) {
    auto *status = vm_owner_thread_status();
    saw_stop_owned_threads =
        mapping_number(status, "stopping") == 1 &&
        mapping_number(status, "enabled") == 0;
    free_mapping(status);
    if (saw_stop_owned_threads) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  // This start must be rejected while the first stop owns the swapped thread
  // handles. The second stop must likewise leave the first stop's latch intact.
  vm_owner_thread_start(1);
  auto *during = vm_owner_thread_status();
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

  auto *stopped = vm_owner_thread_status();
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
    auto *status = vm_owner_thread_status();
    const auto restarted =
        mapping_number(status, "executor_probe_executed") >= before_probe + 3;
    free_mapping(status);
    if (restarted) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  vm_owner_thread_stop();
  auto *restarted = vm_owner_thread_status();
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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
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
  auto before_admission_accepted = mapping_number(before_runtime, "owner_executor_admission_accepted");
  auto before_admission_rejected = mapping_number(before_runtime, "owner_executor_admission_rejected");
  auto before_admission_dropped = mapping_number(before_runtime, "owner_executor_admission_dropped");
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
  auto same_elapsed_us =
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - same_start).count();

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
  auto parallel_elapsed_us =
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - parallel_start).count();

  auto* after_parallel = vm_owner_thread_status();
  auto parallel_completed = mapping_number(after_parallel, "executor_probe_executed") - before_parallel_probe;
  ASSERT_GE(parallel_completed, different_owner_tasks * 2);
  ASSERT_EQ(mapping_number(after_parallel, "executor_owner_claims") - before_parallel_claims,
            mapping_number(after_parallel, "executor_owner_releases") - before_parallel_releases);
  ASSERT_EQ(mapping_number(after_parallel, "executor_same_owner_claim_conflicts"), before_conflicts);
  ASSERT_EQ(mapping_number(after_parallel, "active_owners"), 0);
  ASSERT_EQ(mapping_number(after_parallel, "claimed_owners"), 0);
  ASSERT_GE(mapping_number(after_parallel, "executor_max_parallel_owners"), std::max<long>(2, before_max_parallel));
  ASSERT_LE(mapping_number(after_parallel, "executor_max_owner_parallel"), 1);
  RecordProperty("different_owner_tasks", different_owner_tasks * 2);
  RecordProperty("different_owner_completed", parallel_completed);
  RecordProperty("different_owner_elapsed_us", static_cast<long>(parallel_elapsed_us));
  RecordProperty("different_owner_max_parallel", mapping_number(after_parallel, "executor_max_parallel_owners"));
  RecordProperty("owner_claim_delta", mapping_number(after_parallel, "executor_owner_claims") - before_claims);
  RecordProperty("owner_release_delta", mapping_number(after_parallel, "executor_owner_releases") - before_releases);
  free_mapping(after_parallel);

  auto cancel_future_id =
      vm_owner_register_compute_future("owner/test/runtime-v3/future-cancel", 9201, "runtime_v3_cancel",
                                       "probe_cancel");
  auto timeout_future_id =
      vm_owner_register_compute_future("owner/test/runtime-v3/future-timeout", 9202, "runtime_v3_timeout",
                                       "probe_timeout");
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
  auto resolve_elapsed_us =
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - resolve_start).count();
  RecordProperty("object_resolve_iterations", object_resolve_iterations);
  RecordProperty("object_resolve_elapsed_us", static_cast<long>(resolve_elapsed_us));

  guard.callback_probe = load_object_for_test("single/void");
  ASSERT_NE(guard.callback_probe, nullptr);
  vm_owner_set_id(guard.callback_probe, callback_owner);
  ASSERT_FALSE(vm_owner_executor_available());
  ASSERT_EQ(vm_owner_enqueue_executor_task(guard.callback_probe, "ordinary_lpc", "runtime-v3-rejected", [] {}), 0u);

  std::atomic<int> adapter_ran{0};
  std::atomic<int> stale_ran{0};
  std::atomic<int> stale_drop_cleanup{0};
  vm_owner_thread_start(1);
  ASSERT_TRUE(vm_owner_executor_available());
  ASSERT_EQ(vm_owner_enqueue_executor_task(guard.callback_probe, "legacy_lpc", "runtime-v3-legacy-rejected", [] {}),
            0u);
  auto adapter_task = vm_owner_enqueue_executor_task(guard.callback_probe, "heartbeat", "runtime-v3-main-adapter", [&] {
    adapter_ran.store(vm_context_is_main_thread() ? 1 : -1, std::memory_order_release);
  });
  ASSERT_GT(adapter_task, 0u);
  ASSERT_EQ(vm_owner_drain_main_tasks(16), 1);
  ASSERT_EQ(adapter_ran.load(std::memory_order_acquire), 1);

  auto stale_task = vm_owner_enqueue_executor_task(
      guard.callback_probe, "call_out", "runtime-v3-stale", [&] {
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
  ASSERT_EQ(mapping_number(final_runtime, "normal_path_main_fallback_count"), before_normal_fallback);
  ASSERT_EQ(mapping_number(final_runtime, "normal_path_main_fallback_count"), 0);
  ASSERT_EQ(mapping_number(final_runtime, "owner_executor_context_cleanup_leaks"), 0);
  ASSERT_GE(mapping_number(final_runtime, "owner_executor_future_cancelled"), before_future_cancelled + 1);
  ASSERT_GE(mapping_number(final_runtime, "owner_executor_future_timeout"), before_future_timeout + 1);
  ASSERT_GE(mapping_number(final_runtime, "owner_executor_admission_accepted"), before_admission_accepted + 2);
  ASSERT_GE(mapping_number(final_runtime, "owner_executor_admission_rejected"), before_admission_rejected + 1);
  ASSERT_GE(mapping_number(final_runtime, "owner_executor_admission_dropped"), before_admission_dropped + 1);
  RecordProperty("future_cancel_delta",
                 mapping_number(final_runtime, "owner_executor_future_cancelled") - before_future_cancelled);
  RecordProperty("future_timeout_delta",
                 mapping_number(final_runtime, "owner_executor_future_timeout") - before_future_timeout);
  RecordProperty("admission_accept_delta",
                 mapping_number(final_runtime, "owner_executor_admission_accepted") - before_admission_accepted);
  RecordProperty("admission_reject_delta",
                 mapping_number(final_runtime, "owner_executor_admission_rejected") - before_admission_rejected);
  RecordProperty("admission_drop_delta",
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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

  auto* before = vm_owner_thread_status();
  auto before_runnable = mapping_number(before, "executor_runnable_task_dispatched");
  auto before_safe = mapping_number(before, "executor_safe_task_dispatched");
  free_mapping(before);

  auto lpc_task = vm_owner_enqueue_task(owner, "lpc", "off-main-dummy");
  auto state_task = vm_owner_enqueue_task(owner, "owner_state", "single-owner-state");
  auto gateway_command_task = vm_owner_enqueue_task(owner, "gateway_command", "player-command-activation");
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
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
  ASSERT_GE(mapping_number(running, "thread_lpc_canary_flag_cleared"), before_canary_flag_cleared + 1);
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

TEST_F(DriverTest, TestVmOwnerThreadRunsRegisteredReadonlyLpcTaskWithMultipleWorkers) {
  const char* owner = "owner/test/thread/lpc-task";
  const char* other_owner = "owner/test/thread/lpc-task-other";
  ASSERT_TRUE(vm_context_is_main_thread());

  vm_owner_thread_stop();
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);
  auto owner_epoch = vm_owner_epoch(probe);

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
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
  ASSERT_STREQ(mapping_string(submitted, "ordinary_lpc_activation_policy"), "default_closed_explicit_open");
  ASSERT_STREQ(mapping_string(submitted, "ordinary_lpc_next_blocker"), "");
  ASSERT_EQ(mapping_number(submitted, "direct_cross_owner_write"), 0);
  ASSERT_EQ(mapping_number(submitted, "owner_epoch"), static_cast<long>(owner_epoch));
  ASSERT_STREQ(mapping_string(submitted, "task_type"), "lpc_task");
  ASSERT_STREQ(mapping_string(submitted, "method"), "owner_task_readonly");
  ASSERT_STREQ(mapping_string(submitted, "executor_mode"), "executor_safe_allowlist");
  ASSERT_STREQ(mapping_string(submitted, "route"), "owner_executor");
  ASSERT_STREQ(mapping_string(submitted, "result_policy"), "frozen_result_required");
  ASSERT_STREQ(mapping_string(submitted, "contract_reason"), "registered readonly owner task with frozen result");
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
  ASSERT_GE(mapping_number(running, "thread_lpc_canary_flag_cleared"), before_controlled_lpc_cleared + 1);
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  const auto baseline_terminal =
      owner_future_store_instance().terminal_record_count();
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, &const0u) << key;
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER) << key;
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr) << key;
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING) << key;
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
      record.state = "completed";
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
  const auto string_submission = vm_owner_submit_frozen_string_task(
      probe, "room_output_projection", "capacity-reject-string",
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
  auto* ordinary_submission =
      vm_owner_ordinary_lpc_task(probe, owner, "owner_task_player", 1);
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
  ASSERT_EQ(owner_future_store_instance().terminal_record_count(),
            baseline_terminal);
  destruct_object(probe);
}

TEST_F(DriverTest, TestVmOwnerOrdinaryLpcRequiresExplicitOpen) {
  const char* owner = "owner/test/thread/ordinary-lpc-closed";
  const char* other_owner = "owner/test/thread/ordinary-lpc-other";

  vm_owner_thread_stop();
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
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
  const char* methods[] = {"owner_task_player",      "owner_task_room",     "owner_task_session",
                           "owner_task_item",        "owner_task_economy",  "owner_task_combat",
                           "owner_task_mail",        "owner_task_reward",   "owner_task_world",
                           "owner_task_persistence", "owner_task_team",     "owner_task_guild",
                           "owner_task_sect",        "owner_task_quest",    "owner_task_rank",
                           "owner_task_crafting",    "owner_task_life_skill"};
  const int method_count = static_cast<int>(sizeof(methods) / sizeof(methods[0]));
  ASSERT_TRUE(vm_context_is_main_thread());

  vm_owner_thread_stop();
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  vm_owner_set_id(probe, owner);

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
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
    ASSERT_NE(std::string(mapping_string(submitted, "contract_reason")).find("registered"), std::string::npos) << method;
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
  ASSERT_EQ(mapping_number(pending_runtime, "pending_futures"), before_pending_futures + method_count);
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
  ASSERT_GE(mapping_number(completed_runtime, "futures_completed"), before_future_completions + method_count);
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  ASSERT_STREQ(mapping_string(drained_tasks->u.arr->item[0].u.map, "task_key"), "owner_task_readonly");
  ASSERT_STREQ(mapping_string(drained_tasks->u.arr->item[0].u.map, "executor_mode"), "executor_safe");
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

  auto* schedule_submitted = vm_owner_lpc_task(schedule_probe, schedule_owner, "owner_task_readonly");
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
  ASSERT_STREQ(mapping_string(scheduled_tasks->u.arr->item[0].u.map, "task_key"), "owner_task_readonly");
  ASSERT_STREQ(mapping_string(scheduled_tasks->u.arr->item[0].u.map, "executor_mode"), "executor_safe");
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };

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
  ASSERT_GE(mapping_number(running, "thread_lpc_canary_flag_cleared"), before_canary_flag_cleared + 2);
  ASSERT_EQ(mapping_number(running, "thread_context_leak_detected"), before_context_leaks);
  free_mapping(running);

  vm_owner_thread_stop();
  destruct_object(probe);
}

TEST_F(DriverTest, TestVmOwnerMessageAndCommitTracesAreSpecOnly) {
  const char* source_owner = "owner/test/message/source";
  const char* target_owner = "owner/test/message/target";

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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

  auto* commit = vm_owner_record_commit_boundary(source_owner, target_owner, "move_object", message_id, "prepared");
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

  free_mapping(vm_owner_purge_mailbox(target_owner));
  auto* submitted = vm_owner_submit_message(source_owner, target_owner, "future_method", "future/payload");
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

  free_mapping(vm_owner_purge_mailbox(target_owner));
  auto* submitted = vm_owner_submit_message(source_owner, target_owner, "future_take", "future/take");
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

  free_mapping(vm_owner_purge_mailbox(target_owner));
  auto* submitted = vm_owner_submit_message(source_owner, target_owner, "future_take", "future/take-failed");
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

  free_mapping(vm_owner_purge_mailbox(target_owner));
  auto* submitted = vm_owner_submit_message(source_owner, target_owner, "future_method", "future/purge");
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, &const0u) << key;
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER) << key;
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };

  vm_owner_thread_start(1);
  // Drive handle + message workloads on the worker: submit messages with a
  // target handle, let the worker dispatch them, drain everything.
  for (int i = 0; i < 8; i++) {
    auto* submitted = vm_owner_submit_object_message(
        "owner/test/worker/ref-probe-source", handle, "object_method", "ref/probe");
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

  for (const char *invalid : {"127.0.0.1/33", "example.com/32", "127.0.0.1/32,",
                              "::ffff:192.0.2.0/95"}) {
    cidrs.clear();
    error.clear();
    EXPECT_FALSE(websocket_parse_trusted_proxy_cidrs(invalid, &cidrs, &error)) << invalid;
    EXPECT_TRUE(cidrs.empty()) << invalid;
    EXPECT_FALSE(error.empty()) << invalid;
  }
}

TEST_F(DriverTest, TestTlsClientIdentityAndMinimumProtocol) {
  auto *ctx = tls_client_init();
  ASSERT_NE(ctx, nullptr);
  EXPECT_EQ(SSL_CTX_get_min_proto_version(ctx), kTlsMinimumProtocolVersion);

  auto *ssl = SSL_new(ctx);
  ASSERT_NE(ssl, nullptr);
  EXPECT_TRUE(tls_configure_client_identity(ssl, nullptr, 0, "example.com", true));
  EXPECT_TRUE(tls_configure_client_identity(ssl, nullptr, 0, "example.com", false));
  EXPECT_FALSE(tls_configure_client_identity(ssl, nullptr, 0, "", true));
  EXPECT_FALSE(tls_configure_client_identity(ssl, nullptr, 0, nullptr, true));

  auto *server = tls_server_init(external_port[3].tls_cert, external_port[3].tls_key);
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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = map ? find_string_in_mapping(map, key) : nullptr;
    EXPECT_NE(value, nullptr) << key;
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER) << key;
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = map ? find_string_in_mapping(map, key) : nullptr;
    EXPECT_NE(value, nullptr) << key;
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING) << key;
    return value && value->type == T_STRING ? value->u.string : "";
  };
  object_t* probe = load_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);
  ASSERT_NE(master_ob, nullptr);

  auto set_authorized = [](int allowed) {
    push_number(allowed);
    auto* ret = safe_apply("set_sys_reload_tls_allowed", master_ob, 1,
                           ORIGIN_DRIVER);
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
  EXPECT_NE(call_reload(4).find("Failed to reload TLS context"),
            std::string::npos);
  EXPECT_EQ(tls_port.ssl, saved_ctx) << "failed reload must keep the old SSL_CTX";
  tls_port.tls_cert = saved_cert;
  tls_port.tls_key = "/nonexistent/sys-reload-tls-key.pem";
  EXPECT_NE(call_reload(4).find("Failed to reload TLS context"),
            std::string::npos);
  EXPECT_EQ(tls_port.ssl, saved_ctx)
      << "failed private-key load must keep the old SSL_CTX";
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
    auto* submitted =
        vm_owner_ordinary_lpc_task(probe2, owner, "call_sys_reload_tls_probe", 1);
    auto future_id =
        static_cast<uint64_t>(mapping_number(submitted, "future_id"));
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
    EXPECT_NE(std::string(result->u.string).find("main thread"),
              std::string::npos)
        << "worker + authorized must be rejected for the thread: "
        << result->u.string;
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

  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = map ? find_string_in_mapping(map, key) : nullptr;
    EXPECT_NE(value, nullptr) << key;
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER) << key;
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = map ? find_string_in_mapping(map, key) : nullptr;
    EXPECT_NE(value, nullptr) << key;
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING) << key;
    return value && value->type == T_STRING ? value->u.string : "";
  };
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
  auto* submitted =
      vm_owner_ordinary_lpc_task(probe, owner, "owner_nested_submission", 1);
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
  const char* kProbeKeys[] = {"owner_call_async", "owner_async",
                              "vm_owner_lpc_task",
                              "vm_owner_ordinary_lpc_task"};
  for (const char* key : kProbeKeys) {
    auto* nested = mapping_mapping(result, key);
    ASSERT_NE(nested, nullptr) << key;
    EXPECT_EQ(mapping_number(nested, "success"), 0) << key;
    EXPECT_STREQ(mapping_string(nested, "error"),
                 "main_thread_admission_required")
        << key;
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
  ASSERT_STREQ(vm_object_handle_resolve_status_name(object_id_mismatch_status.status), "object_id_mismatch");
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
  ASSERT_STREQ(vm_object_handle_resolve_status_name(object_not_found_status.status), "object_not_found");
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
  ASSERT_EQ(object_not_found_status.global_record_fallback_reason, "global_record_bridge_retirement_ready");
  ASSERT_FALSE(object_not_found_status.diagnosed_via_global_index);
  ASSERT_FALSE(object_not_found_status.resolved_via_global_index);

  vm_owner_clear_id(obj);
  destruct_object(obj);
}

TEST_F(DriverTest, TestVmObjectStoreReportsPointerBridgeSkippedWhenRecordBridgeRetired) {
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  vm_owner_set_id(obj, "owner/test/pointer-bridge/record-ready");
  vm_object_store_register(obj);
  auto handle = vm_object_handle(obj);
  ASSERT_TRUE(handle.valid);
  ASSERT_TRUE(
      vm_object_store_test_support_remove_live_object_ref_for_bridge_readiness(handle.owner_id.c_str(),
                                                                              handle.object_id));

  auto* store_status = vm_object_store_status();
  ASSERT_EQ(mapping_number(store_status, "owner_local_record_index_ready"), 1);
  ASSERT_EQ(mapping_number(store_status, "owner_local_canonical_record_ready"), 0);
  ASSERT_EQ(mapping_number(store_status, "global_record_bridge_retirement_ready"), 1);
  ASSERT_EQ(mapping_number(store_status, "global_live_object_bridge_retirement_ready"), 0);
  free_mapping(store_status);

  std::string bridge_path = "test/pointer_bridge_live_only";
  object_t live_only_object{};
  live_only_object.obname = bridge_path.c_str();
  live_only_object.flags = 0;
  ASSERT_TRUE(ObjectTable::instance().insert(bridge_path, &live_only_object));
  struct ObjectTableEntryGuard {
    std::string path;
    ~ObjectTableEntryGuard() { ObjectTable::instance().remove(path); }
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
  ASSERT_EQ(handle_status.global_record_pointer_bridge_skip_reason, "global_record_bridge_retirement_ready");
  ASSERT_TRUE(handle_status.global_record_bridge_retirement_ready);
  ASSERT_TRUE(handle_status.global_record_fallback_skipped);
  ASSERT_EQ(handle_status.global_record_fallback_reason, "global_record_bridge_retirement_ready");
  ASSERT_FALSE(handle_status.global_record_found);
  ASSERT_FALSE(handle_status.diagnosed_via_global_index);
  ASSERT_FALSE(handle_status.resolved_via_global_index);

  // Worker-safe resolution is owner-local only: the same compatibility-only
  // object is rejected without touching either global bridge.
  auto owner_local_status =
      vm_object_handle_resolve_owner_local_status(bridge_handle);
  ASSERT_EQ(owner_local_status.object, nullptr);
  ASSERT_EQ(owner_local_status.status,
            VMObjectHandleResolveStatus::kObjectNotFound);
  ASSERT_FALSE(owner_local_status.global_live_object_found);
  ASSERT_TRUE(owner_local_status.global_live_object_source.empty());
  ASSERT_FALSE(owner_local_status.global_record_found);
  ASSERT_FALSE(owner_local_status.global_record_id_scan_bridge_used);
  ASSERT_FALSE(owner_local_status.global_record_pointer_bridge_used);
  ASSERT_FALSE(owner_local_status.diagnosed_via_global_index);
  ASSERT_FALSE(owner_local_status.resolved_via_global_index);

  auto* path_lookup =
      vm_object_store_owner_path_lookup_status("owner/test/pointer-bridge/missing", bridge_path.c_str());
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
    ~MulticoreModeGuard() { CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode; }
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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
  auto expect_owner_local_store_complete_contract = [&](mapping_t* map) {
    ASSERT_EQ(mapping_number(map, "owner_local_store_complete"), 1);
    ASSERT_STREQ(mapping_string(map, "owner_local_store_complete_blocker"), "");
    ASSERT_EQ(mapping_number(map, "uses_global_object_table"), 0);
    ASSERT_EQ(mapping_number(map, "global_index_bridge"), 0);
    ASSERT_EQ(mapping_number(map, "global_live_object_bridge_ready"), 0);
    ASSERT_STREQ(mapping_string(map, "global_live_object_bridge_source"), "");
    ASSERT_EQ(mapping_number(map, "global_record_bridge_ready"), 0);
    ASSERT_STREQ(mapping_string(map, "global_record_bridge_source"), "");
    ASSERT_EQ(mapping_number(map, "owner_local_lifecycle_contract_version"), 1);
    ASSERT_EQ(mapping_number(map, "owner_local_lookup_resolve_ready"), 1);
    ASSERT_EQ(mapping_number(map, "owner_local_create_canonical_ready"), 1);
    ASSERT_EQ(mapping_number(map, "owner_local_move_canonical_ready"), 1);
    ASSERT_EQ(mapping_number(map, "owner_local_destruct_canonical_ready"), 1);
    ASSERT_EQ(mapping_number(map, "owner_local_deferred_destruct_ready"), 1);
    ASSERT_STREQ(mapping_string(map, "owner_local_deferred_destruct_blocker"), "");
    ASSERT_EQ(mapping_number(map, "global_index_physical_retirement_ready"), 1);
    ASSERT_STREQ(mapping_string(map, "global_index_physical_retirement_blocker"), "");
    ASSERT_EQ(mapping_number(map, "owner_local_lifecycle_ready"), 1);
    ASSERT_STREQ(mapping_string(map, "owner_local_lifecycle_blocker"), "");
  };
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
  ASSERT_STREQ(mapping_string(before, "store_kind"), "vm_object_store");
  ASSERT_STREQ(mapping_string(before, "status_model"), "object_store_status");
  ASSERT_STREQ(mapping_string(before, "directory_model"), "owner_local_object_directory");
  ASSERT_STREQ(mapping_string(before, "storage_model"), "owner_local_store");
  ASSERT_EQ(mapping_number(before, "object_store_owner_fast_path_ready"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_fast_path_ready"), 1);
  ASSERT_STREQ(mapping_string(before, "owner_local_fast_path_lock_model"), "shared_mutex_read_lock");
  ASSERT_STREQ(mapping_string(before, "owner_local_lifecycle_write_model"),
               "owner_shard_canonical_with_global_compat_mirror");
  ASSERT_STREQ(mapping_string(before, "global_record_write_model"), "compatibility_mirror");
  ASSERT_EQ(mapping_number(before, "global_record_canonical_write"), 0);
  ASSERT_EQ(mapping_number(before, "owner_local_global_bridge_consistent"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_to_global_bridge_consistent"), 1);
  ASSERT_EQ(mapping_number(before, "global_to_owner_local_bridge_consistent"), 1);
  ASSERT_STREQ(mapping_string(before, "owner_local_global_bridge_check"), "bidirectional");
  ASSERT_STREQ(mapping_string(before, "owner_local_global_bridge_source"), "vm_object_shard");
  expect_owner_local_store_complete_contract(before);
  ASSERT_EQ(mapping_number(before, "owner_local_orphan_record_total"), 0);
  ASSERT_EQ(mapping_number(before, "owner_local_to_global_mismatch_record_total"), 0);
  ASSERT_EQ(mapping_number(before, "global_to_owner_local_record_mismatch_record_total"), 0);
  ASSERT_EQ(mapping_number(before, "global_to_owner_local_mismatch_record_total"), 0);
  ASSERT_EQ(mapping_number(before, "owner_local_record_index_ready"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(before, "global_record_bridge_consistent"), 1);
  ASSERT_EQ(mapping_number(before, "global_record_bridge_retirement_ready"), 1);
  ASSERT_EQ(mapping_number(before, "global_live_object_bridge_retirement_ready"), 1);
  ASSERT_EQ(mapping_number(before, "global_record_total"), mapping_number(before, "registered_objects"));
  ASSERT_GE(mapping_number(before, "global_live_record_total"), 1);
  free_mapping(before);

  auto handle = vm_object_handle(obj);
  ASSERT_NE(handle.object_path.find('#'), std::string::npos);
  ASSERT_EQ(vm_object_store_owner_resolve("owner/test/migration/a", handle.object_id), obj);
  ASSERT_EQ(vm_object_store_owner_path_resolve("owner/test/migration/a", handle.object_path.c_str()), obj);
  auto* old_lookup_before = vm_object_store_owner_lookup_status("owner/test/migration/a", handle.object_id);
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
  ASSERT_STREQ(mapping_string(old_lookup_before, "owner_local_object_ref_source"), "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_object_ref_index_found"), 1);
  ASSERT_STREQ(mapping_string(old_lookup_before, "owner_local_object_ref_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_object_pointer_index_found"), 1);
  ASSERT_STREQ(mapping_string(old_lookup_before, "owner_local_object_pointer_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_resolve_found"), 1);
  ASSERT_STREQ(mapping_string(old_lookup_before, "owner_local_resolve_source"), "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_path_index_found"), 1);
  ASSERT_EQ(mapping_number(old_lookup_before, "owner_local_destructed_path_index_found"), 0);
  ASSERT_STREQ(mapping_string(old_lookup_before, "owner_local_path_index_source"), "vm_object_shard.object_path_index");
  ASSERT_STREQ(mapping_string(old_lookup_before, "owner_local_record_source"), "vm_object_shard.local_records");
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
  auto* old_path_lookup_before =
      vm_object_store_owner_path_lookup_status("owner/test/migration/a", handle.object_path.c_str());
  ASSERT_EQ(mapping_number(old_path_lookup_before, "success"), 1);
  ASSERT_EQ(mapping_number(old_path_lookup_before, "record_found"), 1);
  ASSERT_EQ(mapping_number(old_path_lookup_before, "found"), 1);
  ASSERT_EQ(mapping_number(old_path_lookup_before, "object_id"), static_cast<long>(handle.object_id));
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
  ASSERT_STREQ(vm_object_handle_resolve_status_name(migrated_handle_status.status), "owner_mismatch");
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
  ASSERT_STREQ(mapping_string(status, "store_kind"), "vm_object_store");
  ASSERT_STREQ(mapping_string(status, "status_model"), "object_store_status");
  ASSERT_STREQ(mapping_string(status, "directory_model"), "owner_local_object_directory");
  ASSERT_STREQ(mapping_string(status, "storage_model"), "owner_local_store");
  ASSERT_EQ(mapping_number(status, "object_store_global_fallback_on_owner_fast_path"), 0);
  ASSERT_STREQ(mapping_string(status, "object_store_owner_fast_path_scope"), "same_owner_handle_resolve");
  ASSERT_EQ(mapping_number(status, "owner_local_global_bridge_consistent"), 1);
  ASSERT_EQ(mapping_number(status, "owner_local_to_global_bridge_consistent"), 1);
  ASSERT_EQ(mapping_number(status, "global_to_owner_local_bridge_consistent"), 1);
  ASSERT_STREQ(mapping_string(status, "owner_local_global_bridge_check"), "bidirectional");
  ASSERT_STREQ(mapping_string(status, "owner_local_global_bridge_source"), "vm_object_shard");
  expect_owner_local_store_complete_contract(status);
  ASSERT_GE(mapping_number(status, "owner_local_record_total"), 1);
  ASSERT_GE(mapping_number(status, "owner_local_object_ref_total"), 1);
  ASSERT_GE(mapping_number(status, "owner_local_object_ref_index_total"), 1);
  ASSERT_GE(mapping_number(status, "owner_local_path_index_total"), 1);
  ASSERT_EQ(mapping_number(status, "owner_local_orphan_record_total"), 0);
  ASSERT_EQ(mapping_number(status, "owner_local_to_global_mismatch_record_total"), 0);
  ASSERT_EQ(mapping_number(status, "global_to_owner_local_record_mismatch_record_total"), 0);
  ASSERT_EQ(mapping_number(status, "global_to_owner_local_mismatch_record_total"), 0);
  ASSERT_EQ(mapping_number(status, "owner_local_record_index_ready"), 1);
  ASSERT_EQ(mapping_number(status, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(status, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(status, "owner_local_store_complete"), 1);
  ASSERT_EQ(mapping_number(status, "uses_global_object_table"), 0);
  ASSERT_EQ(mapping_number(status, "global_index_bridge"), 0);
  ASSERT_EQ(mapping_number(status, "global_record_bridge_consistent"), 1);
  ASSERT_EQ(mapping_number(status, "global_record_bridge_retirement_ready"), 1);
  ASSERT_EQ(mapping_number(status, "global_record_total"), mapping_number(status, "registered_objects"));
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
  ASSERT_EQ(mapping_number(old_owner, "owner_local_object_ref_index_consistent"), 1);
  ASSERT_EQ(mapping_number(old_owner, "owner_local_path_index_count"), 0);
  ASSERT_EQ(mapping_number(old_owner, "owner_local_destructed_path_index_count"), 0);
  free_mapping(old_owner);
  auto* old_lookup_after = vm_object_store_owner_lookup_status("owner/test/migration/a", handle.object_id);
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
  ASSERT_EQ(vm_object_store_owner_path_resolve("owner/test/migration/a", handle.object_path.c_str()), nullptr);
  auto* old_path_lookup_after =
      vm_object_store_owner_path_lookup_status("owner/test/migration/a", handle.object_path.c_str());
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
  ASSERT_STREQ(mapping_string(old_path_lookup_after, "owner_local_object_pointer_index_source"), "");
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
  ASSERT_EQ(mapping_number(new_owner, "owner_local_live_index_consistent"), 1);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_object_ref_index_consistent"), 1);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_live_path_index_consistent"), 1);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_destructed_path_index_consistent"), 1);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_directory_ready"), 1);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_path_index_ready"), 1);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(new_owner, "uses_global_object_table"), 0);
  ASSERT_EQ(mapping_number(new_owner, "owner_local_store_complete"), 1);
  ASSERT_EQ(mapping_number(new_owner, "global_index_bridge"), 0);
  expect_owner_local_store_complete_contract(new_owner);
  auto* shard_contract = find_string_in_mapping(new_owner, "vm_object_shard");
  ASSERT_NE(shard_contract, nullptr);
  ASSERT_EQ(shard_contract ? shard_contract->type : T_INVALID, T_MAPPING);
  ASSERT_STREQ(mapping_string(shard_contract->u.map, "shard_kind"), "vm_object_shard");
  ASSERT_STREQ(mapping_string(shard_contract->u.map, "status_model"), "owner_status_record");
  ASSERT_STREQ(mapping_string(shard_contract->u.map, "execution_model"), "owner_execution_shard");
  ASSERT_STREQ(mapping_string(shard_contract->u.map, "directory_model"), "owner_local_object_directory");
  ASSERT_STREQ(mapping_string(shard_contract->u.map, "storage_model"), "owner_local_store");
  ASSERT_EQ(mapping_number(shard_contract->u.map, "object_directory_count"), 1);
  ASSERT_EQ(mapping_number(shard_contract->u.map, "owner_local_record_count"), 1);
  ASSERT_EQ(mapping_number(shard_contract->u.map, "owner_local_destructed_record_count"), 0);
  ASSERT_EQ(mapping_number(shard_contract->u.map, "owner_local_object_ref_count"), 1);
  ASSERT_STREQ(mapping_string(shard_contract->u.map, "owner_local_object_ref_source"),
               "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(shard_contract->u.map, "owner_local_object_ref_index_count"), 1);
  ASSERT_STREQ(mapping_string(shard_contract->u.map, "owner_local_object_ref_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(shard_contract->u.map, "owner_local_path_index_count"), 1);
  ASSERT_EQ(mapping_number(shard_contract->u.map, "owner_local_destructed_path_index_count"), 0);
  ASSERT_EQ(mapping_number(shard_contract->u.map, "owner_local_live_index_consistent"), 1);
  ASSERT_EQ(mapping_number(shard_contract->u.map, "owner_local_object_ref_index_consistent"), 1);
  ASSERT_EQ(mapping_number(shard_contract->u.map, "owner_local_live_path_index_consistent"), 1);
  ASSERT_EQ(mapping_number(shard_contract->u.map, "owner_local_destructed_path_index_consistent"), 1);
  ASSERT_EQ(mapping_number(shard_contract->u.map, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(shard_contract->u.map, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(shard_contract->u.map, "owner_local_path_index_ready"), 1);
  ASSERT_STREQ(mapping_string(shard_contract->u.map, "owner_local_path_index_source"),
               "vm_object_shard.object_path_index");
  ASSERT_STREQ(mapping_string(shard_contract->u.map, "owner_local_destructed_path_index_source"),
               "vm_object_shard.destructed_path_index");
  ASSERT_EQ(mapping_number(shard_contract->u.map, "owner_local_directory_ready"), 1);
  ASSERT_EQ(mapping_number(shard_contract->u.map, "owner_local_directory_from_shard"), 1);
  ASSERT_EQ(mapping_number(shard_contract->u.map, "owner_local_store_complete"), 1);
  ASSERT_EQ(mapping_number(shard_contract->u.map, "global_index_bridge"), 0);
  expect_owner_local_store_complete_contract(shard_contract->u.map);
  auto* directory = find_string_in_mapping(new_owner, "object_directory");
  ASSERT_NE(directory, nullptr);
  ASSERT_EQ(directory->type, T_ARRAY);
  ASSERT_EQ(directory->u.arr->size, 1);
  ASSERT_EQ(mapping_number(new_owner, "object_directory_count"),
            mapping_number(new_owner, "owner_local_directory_count"));
  auto* directory_record = directory->u.arr->item[0].u.map;
  ASSERT_EQ(mapping_number(directory_record, "object_id"), static_cast<long>(handle.object_id));
  ASSERT_EQ(mapping_number(directory_record, "owner_epoch"), static_cast<long>(vm_owner_epoch(obj)));
  ASSERT_EQ(mapping_number(directory_record, "destructed"), 0);
  ASSERT_EQ(mapping_number(directory_record, "live"), 1);
  ASSERT_EQ(mapping_number(directory_record, "owner_local_directory_entry"), 1);
  ASSERT_STREQ(mapping_string(directory_record, "owner_local_directory_source"), "vm_object_shard.object_directory");
  ASSERT_EQ(mapping_number(directory_record, "owner_local_record_snapshot"), 1);
  ASSERT_STREQ(mapping_string(directory_record, "owner_local_record_source"), "vm_object_shard.local_records");
  ASSERT_EQ(mapping_number(directory_record, "owner_local_object_ref_entry"), 1);
  ASSERT_STREQ(mapping_string(directory_record, "owner_local_object_ref_source"), "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(directory_record, "owner_local_path_index_entry"), 1);
  ASSERT_STREQ(mapping_string(directory_record, "owner_local_path_index_source"), "vm_object_shard.object_path_index");
  ASSERT_EQ(mapping_number(directory_record, "resolved_via_owner_local_store"), 1);
  ASSERT_EQ(mapping_number(directory_record, "resolved_via_global_index"), 0);
  ASSERT_STREQ(mapping_string(directory_record, "owner_id"), "owner/test/migration/b");
  ASSERT_STREQ(mapping_string(directory_record, "object_path"), handle.object_path.c_str());
  free_mapping(new_owner);
  auto* new_lookup_after = vm_object_store_owner_lookup_status("owner/test/migration/b", handle.object_id);
  ASSERT_EQ(mapping_number(new_lookup_after, "success"), 1);
  ASSERT_EQ(mapping_number(new_lookup_after, "record_found"), 1);
  ASSERT_EQ(mapping_number(new_lookup_after, "found"), 1);
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_directory_entry"), 1);
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_record_found"), 1);
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_destructed_record_found"), 0);
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_record_destructed"), 0);
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_object_ref_found"), 1);
  ASSERT_STREQ(mapping_string(new_lookup_after, "owner_local_object_ref_source"), "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_object_ref_index_found"), 1);
  ASSERT_STREQ(mapping_string(new_lookup_after, "owner_local_object_ref_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_object_pointer_index_found"), 1);
  ASSERT_STREQ(mapping_string(new_lookup_after, "owner_local_object_pointer_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_resolve_found"), 1);
  ASSERT_STREQ(mapping_string(new_lookup_after, "owner_local_resolve_source"), "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_path_index_found"), 1);
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_destructed_path_index_found"), 0);
  expect_no_lookup_global_live_object(new_lookup_after);
  expect_no_lookup_global_record_id_scan(new_lookup_after);
  expect_no_lookup_global_record_pointer(new_lookup_after);
  expect_no_lookup_global_record_scan(new_lookup_after);
  ASSERT_STREQ(mapping_string(new_lookup_after, "owner_local_record_source"), "vm_object_shard.local_records");
  ASSERT_STREQ(mapping_string(new_lookup_after, "owner_local_path_index_source"), "vm_object_shard.object_path_index");
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_local_store_complete"), 1);
  ASSERT_STREQ(mapping_string(new_lookup_after, "owner_local_store_complete_blocker"), "");
  ASSERT_EQ(mapping_number(new_lookup_after, "owner_mismatch"), 0);
  ASSERT_STREQ(mapping_string(new_lookup_after, "record_owner_id"), "owner/test/migration/b");
  ASSERT_STREQ(mapping_string(new_lookup_after, "object_path"), handle.object_path.c_str());
  free_mapping(new_lookup_after);
  ASSERT_EQ(vm_object_store_owner_resolve("owner/test/migration/b", handle.object_id), obj);
  ASSERT_EQ(vm_object_store_owner_path_resolve("owner/test/migration/b", handle.object_path.c_str()), obj);
  auto* new_path_lookup_after =
      vm_object_store_owner_path_lookup_status("owner/test/migration/b", handle.object_path.c_str());
  ASSERT_EQ(mapping_number(new_path_lookup_after, "success"), 1);
  ASSERT_EQ(mapping_number(new_path_lookup_after, "record_found"), 1);
  ASSERT_EQ(mapping_number(new_path_lookup_after, "found"), 1);
  ASSERT_EQ(mapping_number(new_path_lookup_after, "object_id"), static_cast<long>(handle.object_id));
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
  ASSERT_EQ(mapping_number(missing_object_lookup, "owner_local_global_live_object_fallback_skipped"), 0);
  ASSERT_STREQ(mapping_string(missing_object_lookup, "owner_local_global_live_object_fallback_reason"),
               "");
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
  ASSERT_EQ(mapping_number(missing_path_lookup, "owner_local_global_live_object_fallback_skipped"), 1);
  ASSERT_STREQ(mapping_string(missing_path_lookup, "owner_local_global_live_object_fallback_reason"),
               "global_live_object_bridge_retirement_ready");
  free_mapping(missing_path_lookup);

  vm_owner_clear_id(obj);
  destruct_object(obj);
}

TEST_F(DriverTest, TestVmObjectStoreShardRemovesDestructedObject) {
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
  auto expect_owner_local_store_complete_contract = [&](mapping_t* map) {
    ASSERT_EQ(mapping_number(map, "owner_local_store_complete"), 1);
    ASSERT_STREQ(mapping_string(map, "owner_local_store_complete_blocker"), "");
    ASSERT_EQ(mapping_number(map, "uses_global_object_table"), 0);
    ASSERT_EQ(mapping_number(map, "global_index_bridge"), 0);
    ASSERT_EQ(mapping_number(map, "global_live_object_bridge_ready"), 0);
    ASSERT_STREQ(mapping_string(map, "global_live_object_bridge_source"), "");
    ASSERT_EQ(mapping_number(map, "global_record_bridge_ready"), 0);
    ASSERT_STREQ(mapping_string(map, "global_record_bridge_source"), "");
    ASSERT_EQ(mapping_number(map, "owner_local_lifecycle_contract_version"), 1);
    ASSERT_EQ(mapping_number(map, "owner_local_lookup_resolve_ready"), 1);
    ASSERT_EQ(mapping_number(map, "owner_local_create_canonical_ready"), 1);
    ASSERT_EQ(mapping_number(map, "owner_local_move_canonical_ready"), 1);
    ASSERT_EQ(mapping_number(map, "owner_local_destruct_canonical_ready"), 1);
    ASSERT_EQ(mapping_number(map, "owner_local_deferred_destruct_ready"), 1);
    ASSERT_STREQ(mapping_string(map, "owner_local_deferred_destruct_blocker"), "");
    ASSERT_EQ(mapping_number(map, "global_index_physical_retirement_ready"), 1);
    ASSERT_STREQ(mapping_string(map, "global_index_physical_retirement_blocker"), "");
    ASSERT_EQ(mapping_number(map, "owner_local_lifecycle_ready"), 1);
    ASSERT_STREQ(mapping_string(map, "owner_local_lifecycle_blocker"), "");
  };
  auto expect_no_lookup_global_live_object = [&](mapping_t* map) {
    ASSERT_EQ(mapping_number(map, "owner_local_global_live_object_found"), 0);
    ASSERT_STREQ(mapping_string(map, "owner_local_global_live_object_source"), "");
  };

  auto* missing_owner = vm_object_store_owner_status("owner/test/store/missing");
  ASSERT_EQ(mapping_number(missing_owner, "objects"), 0);
  ASSERT_EQ(mapping_number(missing_owner, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(missing_owner, "owner_local_store_complete"), 1);
  ASSERT_EQ(mapping_number(missing_owner, "global_index_bridge"), 0);
  expect_owner_local_store_complete_contract(missing_owner);
  auto* missing_owner_shard_contract = find_string_in_mapping(missing_owner, "vm_object_shard");
  ASSERT_NE(missing_owner_shard_contract, nullptr);
  ASSERT_EQ(missing_owner_shard_contract ? missing_owner_shard_contract->type : T_INVALID, T_MAPPING);
  ASSERT_EQ(mapping_number(missing_owner_shard_contract->u.map, "owner_local_store_ready"), 1);
  expect_owner_local_store_complete_contract(missing_owner_shard_contract->u.map);
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
  ASSERT_EQ(vm_object_store_owner_path_resolve("owner/test/store/destruct", handle.object_path.c_str()), obj);

  auto* before = vm_object_store_owner_status("owner/test/store/destruct");
  ASSERT_EQ(mapping_number(before, "objects"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_record_count"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_destructed_record_count"), 0);
  ASSERT_EQ(mapping_number(before, "owner_local_object_ref_count"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_object_ref_index_count"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_path_index_count"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_destructed_path_index_count"), 0);
  ASSERT_EQ(mapping_number(before, "owner_local_live_index_consistent"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_object_ref_index_consistent"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_live_path_index_consistent"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_destructed_path_index_consistent"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(before, "owner_local_store_complete"), 1);
  expect_owner_local_store_complete_contract(before);
  auto before_destructed = mapping_number(before, "destructed");
  free_mapping(before);
  auto* lookup_before = vm_object_store_owner_lookup_status("owner/test/store/destruct", handle.object_id);
  ASSERT_EQ(mapping_number(lookup_before, "success"), 1);
  ASSERT_EQ(mapping_number(lookup_before, "record_found"), 1);
  ASSERT_EQ(mapping_number(lookup_before, "found"), 1);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_directory_entry"), 1);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_record_found"), 1);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_destructed_record_found"), 0);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_record_destructed"), 0);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_object_ref_found"), 1);
  ASSERT_STREQ(mapping_string(lookup_before, "owner_local_object_ref_source"), "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_object_ref_index_found"), 1);
  ASSERT_STREQ(mapping_string(lookup_before, "owner_local_object_ref_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_object_pointer_index_found"), 1);
  ASSERT_STREQ(mapping_string(lookup_before, "owner_local_object_pointer_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_resolve_found"), 1);
  ASSERT_STREQ(mapping_string(lookup_before, "owner_local_resolve_source"), "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_path_index_found"), 1);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_destructed_path_index_found"), 0);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(lookup_before, "owner_local_store_complete"), 1);
  ASSERT_STREQ(mapping_string(lookup_before, "owner_local_store_complete_blocker"), "");
  expect_no_lookup_global_live_object(lookup_before);
  ASSERT_STREQ(mapping_string(lookup_before, "owner_local_path_index_source"), "vm_object_shard.object_path_index");
  ASSERT_STREQ(mapping_string(lookup_before, "owner_local_record_source"), "vm_object_shard.local_records");
  ASSERT_EQ(mapping_number(lookup_before, "destructed"), 0);
  free_mapping(lookup_before);
  auto* path_lookup_before =
      vm_object_store_owner_path_lookup_status("owner/test/store/destruct", handle.object_path.c_str());
  ASSERT_EQ(mapping_number(path_lookup_before, "success"), 1);
  ASSERT_EQ(mapping_number(path_lookup_before, "record_found"), 1);
  ASSERT_EQ(mapping_number(path_lookup_before, "found"), 1);
  ASSERT_EQ(mapping_number(path_lookup_before, "object_id"), static_cast<long>(handle.object_id));
  ASSERT_EQ(mapping_number(path_lookup_before, "owner_local_object_ref_found"), 1);
  ASSERT_STREQ(mapping_string(path_lookup_before, "owner_local_object_ref_source"), "vm_object_shard.local_objects");
  ASSERT_EQ(mapping_number(path_lookup_before, "owner_local_object_ref_index_found"), 1);
  ASSERT_STREQ(mapping_string(path_lookup_before, "owner_local_object_ref_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(path_lookup_before, "owner_local_object_pointer_index_found"), 1);
  ASSERT_STREQ(mapping_string(path_lookup_before, "owner_local_object_pointer_index_source"),
               "vm_object_shard.local_object_index");
  ASSERT_EQ(mapping_number(path_lookup_before, "owner_local_resolve_found"), 1);
  ASSERT_STREQ(mapping_string(path_lookup_before, "owner_local_resolve_source"), "vm_object_shard.local_objects");
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
  ASSERT_STREQ(vm_object_handle_resolve_status_name(destructed_handle_status.status), "record_destructed");
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
  ASSERT_STREQ(mapping_string(store_after_destruct, "store_kind"), "vm_object_store");
  ASSERT_STREQ(mapping_string(store_after_destruct, "status_model"), "object_store_status");
  ASSERT_STREQ(mapping_string(store_after_destruct, "directory_model"), "owner_local_object_directory");
  ASSERT_STREQ(mapping_string(store_after_destruct, "storage_model"), "owner_local_store");
  ASSERT_EQ(mapping_number(store_after_destruct, "owner_local_global_bridge_consistent"), 1);
  ASSERT_EQ(mapping_number(store_after_destruct, "owner_local_to_global_bridge_consistent"), 1);
  ASSERT_EQ(mapping_number(store_after_destruct, "global_to_owner_local_bridge_consistent"), 1);
  ASSERT_STREQ(mapping_string(store_after_destruct, "owner_local_global_bridge_check"), "bidirectional");
  ASSERT_STREQ(mapping_string(store_after_destruct, "owner_local_global_bridge_source"), "vm_object_shard");
  expect_owner_local_store_complete_contract(store_after_destruct);
  ASSERT_GE(mapping_number(store_after_destruct, "owner_local_destructed_record_total"), 1);
  ASSERT_EQ(mapping_number(store_after_destruct, "owner_local_object_ref_total"),
            mapping_number(store_after_destruct, "owner_local_object_ref_index_total"));
  ASSERT_GE(mapping_number(store_after_destruct, "owner_local_destructed_path_index_total"), 1);
  ASSERT_EQ(mapping_number(store_after_destruct, "owner_local_orphan_record_total"), 0);
  ASSERT_EQ(mapping_number(store_after_destruct, "owner_local_to_global_mismatch_record_total"), 0);
  ASSERT_EQ(mapping_number(store_after_destruct, "global_to_owner_local_record_mismatch_record_total"), 0);
  ASSERT_EQ(mapping_number(store_after_destruct, "global_to_owner_local_mismatch_record_total"), 0);
  ASSERT_EQ(mapping_number(store_after_destruct, "owner_local_record_index_ready"), 1);
  ASSERT_EQ(mapping_number(store_after_destruct, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(store_after_destruct, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(store_after_destruct, "owner_local_store_complete"), 1);
  ASSERT_EQ(mapping_number(store_after_destruct, "uses_global_object_table"), 0);
  ASSERT_EQ(mapping_number(store_after_destruct, "global_index_bridge"), 0);
  ASSERT_EQ(mapping_number(store_after_destruct, "global_record_bridge_consistent"), 1);
  ASSERT_EQ(mapping_number(store_after_destruct, "global_record_bridge_retirement_ready"), 1);
  ASSERT_EQ(mapping_number(store_after_destruct, "global_live_object_bridge_retirement_ready"), 1);
  ASSERT_EQ(mapping_number(store_after_destruct, "global_record_total"),
            mapping_number(store_after_destruct, "registered_objects"));
  ASSERT_GE(mapping_number(store_after_destruct, "global_destructed_record_total"), 1);
  free_mapping(store_after_destruct);
  ASSERT_EQ(vm_object_store_owner_resolve("owner/test/store/destruct", handle.object_id), nullptr);
  ASSERT_EQ(vm_object_store_owner_path_resolve("owner/test/store/destruct", handle.object_path.c_str()), nullptr);

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
  ASSERT_EQ(mapping_number(after, "owner_local_live_index_consistent"), 1);
  ASSERT_EQ(mapping_number(after, "owner_local_object_ref_index_consistent"), 1);
  ASSERT_EQ(mapping_number(after, "owner_local_live_path_index_consistent"), 1);
  ASSERT_EQ(mapping_number(after, "owner_local_destructed_path_index_consistent"), 1);
  ASSERT_EQ(mapping_number(after, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(after, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(after, "owner_local_store_complete"), 1);
  expect_owner_local_store_complete_contract(after);
  ASSERT_EQ(mapping_number(after, "destructed"), before_destructed + 1);
  free_mapping(after);
  auto* lookup_after = vm_object_store_owner_lookup_status("owner/test/store/destruct", handle.object_id);
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
  ASSERT_STREQ(mapping_string(lookup_after, "owner_local_record_source"), "vm_object_shard.destructed_records");
  ASSERT_STREQ(mapping_string(lookup_after, "owner_local_path_index_source"),
               "vm_object_shard.destructed_path_index");
  ASSERT_EQ(mapping_number(lookup_after, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(lookup_after, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(lookup_after, "owner_local_store_complete"), 1);
  ASSERT_STREQ(mapping_string(lookup_after, "owner_local_store_complete_blocker"), "");
  ASSERT_EQ(mapping_number(lookup_after, "destructed"), 1);
  free_mapping(lookup_after);
  auto* path_lookup_after =
      vm_object_store_owner_path_lookup_status("owner/test/store/destruct", handle.object_path.c_str());
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
  ASSERT_EQ(vm_object_store_owner_resolve("owner/test/store/destruct-after", handle.object_id), nullptr);
  ASSERT_EQ(vm_object_store_owner_path_resolve("owner/test/store/destruct-after", handle.object_path.c_str()), nullptr);
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
  ASSERT_EQ(mapping_number(moved_after_destruct, "owner_local_live_index_consistent"), 1);
  ASSERT_EQ(mapping_number(moved_after_destruct, "owner_local_object_ref_index_consistent"), 1);
  ASSERT_EQ(mapping_number(moved_after_destruct, "owner_local_live_path_index_consistent"), 1);
  ASSERT_EQ(mapping_number(moved_after_destruct, "owner_local_destructed_path_index_consistent"), 1);
  ASSERT_EQ(mapping_number(moved_after_destruct, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(moved_after_destruct, "owner_local_store_complete"), 1);
  expect_owner_local_store_complete_contract(moved_after_destruct);
  auto* empty_shard_contract = find_string_in_mapping(moved_after_destruct, "vm_object_shard");
  ASSERT_NE(empty_shard_contract, nullptr);
  ASSERT_EQ(empty_shard_contract ? empty_shard_contract->type : T_INVALID, T_MAPPING);
  ASSERT_STREQ(mapping_string(empty_shard_contract->u.map, "directory_model"), "owner_local_object_directory");
  ASSERT_EQ(mapping_number(empty_shard_contract->u.map, "object_directory_count"), 0);
  ASSERT_EQ(mapping_number(empty_shard_contract->u.map, "owner_local_record_count"), 0);
  ASSERT_EQ(mapping_number(empty_shard_contract->u.map, "owner_local_destructed_record_count"), 1);
  ASSERT_EQ(mapping_number(empty_shard_contract->u.map, "owner_local_object_ref_count"), 0);
  ASSERT_EQ(mapping_number(empty_shard_contract->u.map, "owner_local_object_ref_index_count"), 0);
  ASSERT_EQ(mapping_number(empty_shard_contract->u.map, "owner_local_path_index_count"), 0);
  ASSERT_EQ(mapping_number(empty_shard_contract->u.map, "owner_local_destructed_path_index_count"), 1);
  ASSERT_EQ(mapping_number(empty_shard_contract->u.map, "owner_local_live_index_consistent"), 1);
  ASSERT_EQ(mapping_number(empty_shard_contract->u.map, "owner_local_object_ref_index_consistent"), 1);
  ASSERT_EQ(mapping_number(empty_shard_contract->u.map, "owner_local_live_path_index_consistent"), 1);
  ASSERT_EQ(mapping_number(empty_shard_contract->u.map, "owner_local_destructed_path_index_consistent"), 1);
  ASSERT_EQ(mapping_number(empty_shard_contract->u.map, "owner_local_canonical_record_ready"), 1);
  ASSERT_EQ(mapping_number(empty_shard_contract->u.map, "owner_local_directory_ready"), 1);
  ASSERT_EQ(mapping_number(empty_shard_contract->u.map, "owner_local_directory_from_shard"), 1);
  ASSERT_EQ(mapping_number(empty_shard_contract->u.map, "owner_local_store_ready"), 1);
  ASSERT_EQ(mapping_number(empty_shard_contract->u.map, "owner_local_store_complete"), 1);
  ASSERT_EQ(mapping_number(empty_shard_contract->u.map, "global_index_bridge"), 0);
  expect_owner_local_store_complete_contract(empty_shard_contract->u.map);
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
  ASSERT_EQ(mapping_number(moved_lookup_after_destruct, "owner_local_destructed_path_index_found"), 1);
  expect_no_lookup_global_live_object(moved_lookup_after_destruct);
  ASSERT_STREQ(mapping_string(moved_lookup_after_destruct, "owner_local_record_source"),
               "vm_object_shard.destructed_records");
  ASSERT_STREQ(mapping_string(moved_lookup_after_destruct, "owner_local_path_index_source"),
               "vm_object_shard.destructed_path_index");
  ASSERT_EQ(mapping_number(moved_lookup_after_destruct, "destructed"), 1);
  free_mapping(moved_lookup_after_destruct);
  auto* moved_path_lookup_after_destruct =
      vm_object_store_owner_path_lookup_status("owner/test/store/destruct-after", handle.object_path.c_str());
  ASSERT_EQ(mapping_number(moved_path_lookup_after_destruct, "success"), 1);
  ASSERT_EQ(mapping_number(moved_path_lookup_after_destruct, "record_found"), 1);
  ASSERT_EQ(mapping_number(moved_path_lookup_after_destruct, "found"), 0);
  ASSERT_EQ(mapping_number(moved_path_lookup_after_destruct, "object_id"), static_cast<long>(handle.object_id));
  ASSERT_EQ(mapping_number(moved_path_lookup_after_destruct, "owner_local_object_ref_found"), 0);
  ASSERT_STREQ(mapping_string(moved_path_lookup_after_destruct, "owner_local_object_ref_source"), "");
  ASSERT_EQ(mapping_number(moved_path_lookup_after_destruct, "owner_local_path_index_found"), 0);
  ASSERT_EQ(mapping_number(moved_path_lookup_after_destruct, "owner_local_destructed_path_index_found"), 1);
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
  auto task_id = vm_worker_submit_snapshot_digest("actor/async", "{\"hp\":100,\"room\":\"test\"}", 16);
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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

  const char* owner = "actor/owner-future-empty-result";
  const uint64_t worker_task_id = 987654321u;
  vm_owner_thread_stop();
  free_mapping(vm_owner_purge_mailbox(owner));

  auto future_id = vm_owner_register_compute_future(owner, worker_task_id, "empty_result", "worker_compute");
  ASSERT_GT(future_id, 0u);
  auto* pending = vm_owner_future_poll(future_id);
  ASSERT_EQ(mapping_number(pending, "success"), 1);
  ASSERT_STREQ(mapping_string(pending, "state"), "pending");
  ASSERT_EQ(mapping_number(pending, "target_task_id"), static_cast<long>(worker_task_id));
  ASSERT_EQ(mapping_number(pending, "frozen_result"), 0);
  free_mapping(pending);

  auto result_task_id = vm_owner_enqueue_compute_result(owner, worker_task_id, "empty_result", "completed",
                                                       "empty_result", "");
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
  auto task_id = vm_worker_submit_snapshot_digest_v2("actor/envelope", "{\"hp\":100}", 8, 1000, 5000);
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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  ASSERT_EQ(mapping_number(result_map->u.map, "checksum"), static_cast<long>(result.bench.checksum));
  free_mapping(completed);
}

TEST_F(DriverTest, TestVmWorkerV2TimeoutFailsPendingTask) {
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
  auto mapping_number = [](mapping_t* map, const char* key) -> long {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t* map, const char* key) -> const char* {
    auto* value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

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
