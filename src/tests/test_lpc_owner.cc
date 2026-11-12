#include "test_lpc_support.h"

#ifdef PACKAGE_ASYNC

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
        object, "owner_message", "verify_cleanup", nullptr, nullptr, [] {
          ADD_FAILURE() << "Message must execute the target lfun";
        });
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
    ~RecompileConfigGuard() {
      CONFIG_INT(__RECOMPILE_OBJECT_ENABLED__) = saved;
    }
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
    if (tick_event_queue_size_for_test() == 0 && walltime_event_queue_size_for_test() == 0 &&
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
  ASSERT_STREQ(promise->result.u.string, "*async function owner was destructed while suspended");
  promise->handled = true;

  for (int pass = 0; pass < 16; pass++) {
    if (tick_event_queue_size_for_test() != 0) {
      ASSERT_GT(run_tick_events_for_test(), 0u);
    }
    if (walltime_event_queue_size_for_test() != 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      ASSERT_EQ(event_base_loop(g_event_base, EVLOOP_NONBLOCK), 0);
    }
    if (tick_event_queue_size_for_test() == 0 && walltime_event_queue_size_for_test() == 0) {
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

  mapping_t* m = allocate_mapping(4);
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
  ASSERT_GE(store.terminal_payload_bytes(), static_cast<int64_t>(key.size() + value.size()));
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
  ASSERT_EQ(completion->record.state, OwnerFutureState::kFailed);
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
  ASSERT_EQ(completion->record.state, OwnerFutureState::kFailed);
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
    array_t* current = allocate_array(1);
    for (int i = 1; i < depth; i++) {
      array_t* next = allocate_array(1);
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

  auto* cycle = allocate_array(1);
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

  auto* shared_child = allocate_array(1);
  shared_child->item[0].type = T_STRING;
  shared_child->item[0].subtype = STRING_SHARED;
  shared_child->item[0].u.string = make_shared_string("dag");
  auto* dag = allocate_array(2);
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

  auto* partial = allocate_array(2);
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

  auto* partial_map = allocate_mapping(1);
  svalue_t string_key = const0u;
  string_key.type = T_STRING;
  string_key.subtype = STRING_SHARED;
  string_key.u.string = make_shared_string("partial-map");
  auto* partial_map_slot = find_for_insert(partial_map, &string_key, 1);
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

  auto* bad_map = allocate_mapping(1);
  svalue_t numeric_key = const0u;
  numeric_key.type = T_NUMBER;
  numeric_key.u.number = 42;
  auto* bad_slot = find_for_insert(bad_map, &numeric_key, 1);
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
    array_t* prev = allocate_array(1);
    for (int d = 1; d < depth; d++) {
      array_t* cur = allocate_array(1);
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
  array_t* wide = allocate_array(kMaps);
  for (int i = 0; i < kMaps; i++) {
    mapping_t* sub = allocate_mapping(4);
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
  ASSERT_EQ(wide_completion->record.state, OwnerFutureState::kFailed);
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
  store.set_clock_for_test([&fake_now_ns]() {
    return fake_now_ns;
  });
  store.admit_pending(owner_future_store_test_record(1, 101));
  auto completion = store.complete(1, "completed", "result", "", std::make_shared<VMFrozenValue>());
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
    auto* array = allocate_empty_array(static_cast<int>(values.size()));
    int index = 0;
    for (const LPC_INT value : values) {
      array->item[index] = const0u;
      array->item[index++].u.number = value;
    }
    return array;
  };

  auto* difference = subtract_array(make_number_array({std::numeric_limits<LPC_INT>::min()}),
                                    make_number_array({0}));
  ASSERT_EQ(difference->size, 1);
  ASSERT_EQ(difference->item[0].type, T_NUMBER);
  ASSERT_EQ(difference->item[0].u.number, std::numeric_limits<LPC_INT>::min());
  free_array(difference);

  // The set operators consume both operands. Assert the array pool returns to
  // its baseline, otherwise a consumed operand whose refcount hit zero is
  // leaked silently (ASan flags the same thing as a 92-byte leak).
  const auto arrays_before = num_arrays;

  auto* intersection = intersect_array(make_number_array({0}),
                                       make_number_array({std::numeric_limits<LPC_INT>::min()}));
  ASSERT_EQ(intersection, &the_null_array);

  auto* array_union =
      union_array(make_number_array({0}), make_number_array({std::numeric_limits<LPC_INT>::min()}));
  ASSERT_EQ(array_union->size, 2);
  bool found_zero = false;
  bool found_min = false;
  for (int index = 0; index < array_union->size; ++index) {
    found_zero = found_zero || array_union->item[index].u.number == 0;
    found_min =
        found_min || array_union->item[index].u.number == std::numeric_limits<LPC_INT>::min();
  }
  ASSERT_TRUE(found_zero);
  ASSERT_TRUE(found_min);
  free_array(array_union);
  ASSERT_EQ(num_arrays, arrays_before);
}

TEST_F(DriverTest, TestSourceEncodingPragmaDetectionAndTranscoding) {
  // The pragma is read by the live compile path (lpc_source_encoding.cc), so this
  // covers the parser and the ICU conversion, not a scanner copy.
  const std::string source =
      "#pragma source_encoding(\"GBK\")\n"
      "void f() {}\n";
  EXPECT_EQ(lpc_source_encoding_from_source(source), "GBK");

  auto decoded = lpc_source_decode_to_utf8(source);
  EXPECT_EQ(decoded.encoding, "GBK");
  EXPECT_TRUE(decoded.transcoded);
  EXPECT_EQ(decoded.invalid_sequence_count, 0);
  EXPECT_NE(decoded.data.find("#pragma source_encoding"), std::string::npos);

  // No pragma: the canonical VM encoding is assumed and nothing is transcoded.
  auto plain = lpc_source_decode_to_utf8("void f() {}\n");
  EXPECT_EQ(plain.encoding, "utf-8");
  EXPECT_FALSE(plain.transcoded);
}

TEST_F(DriverTest, TestCompileFileAcceptsGbkSourceEncodingPragma) {
  std::string source = "#pragma source_encoding(\"GBK\")\nstring value() { return \"";
  source.append("\xD6\xD0\xCE\xC4", 4);
  source += "\"; }\n";

  std::istringstream stream_source(source);
  auto stream = std::make_unique<IStreamLexStream>(stream_source);
  auto* compiled = compile_file(std::move(stream), "gbk_source_encoding_test");

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
  program_t* prog = compile_file(std::move(stream), "lpc_vm_profile_apply_cache_test");
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

TEST_F(DriverTest, TestOwnerFuturePendingCountUsesTransitionCounter) {
  const auto header = read_source_file_for_test("../src/vm/internal/owner_future_store.h");
  const auto source = read_source_file_for_test("../src/vm/internal/owner_future_store.cc");
  const auto count_start = source.find("int64_t OwnerFutureStore::pending_count()");
  const auto count_end = source.find("int64_t OwnerFutureStore::size()", count_start);

  ASSERT_NE(count_start, std::string::npos);
  ASSERT_NE(count_end, std::string::npos);
  const auto count_body = source.substr(count_start, count_end - count_start);
  EXPECT_NE(header.find("std::atomic<int64_t> pending_"), std::string::npos);
  EXPECT_NE(count_body.find("pending_.load(std::memory_order_relaxed)"), std::string::npos);
  EXPECT_EQ(count_body.find("for ("), std::string::npos);
}

TEST_F(DriverTest, TestOwnerFutureCountChainUsesFixedWidthTypes) {
  const auto header = read_source_file_for_test("../src/vm/internal/owner_future_store.h");
  const auto source = read_source_file_for_test("../src/vm/internal/owner_future_store.cc");
  const auto owner = read_source_file_for_test("../src/vm/internal/owner.cc");

  EXPECT_NE(header.find("int64_t pending_count() const;"), std::string::npos);
  EXPECT_EQ(header.find("long pending_count() const;"), std::string::npos);
  EXPECT_NE(header.find("int64_t size() const;"), std::string::npos);
  EXPECT_EQ(header.find("long size() const;"), std::string::npos);
  EXPECT_NE(header.find("std::atomic<int64_t> pending_{0};"), std::string::npos);
  EXPECT_EQ(header.find("std::atomic<long> pending_{0};"), std::string::npos);

  EXPECT_NE(source.find("int64_t OwnerFutureStore::pending_count() const"), std::string::npos);
  EXPECT_NE(source.find("int64_t OwnerFutureStore::size() const"), std::string::npos);
  EXPECT_NE(source.find("static_cast<int64_t>(futures_.size())"), std::string::npos);
  EXPECT_EQ(source.find("static_cast<long>(futures_.size())"), std::string::npos);

  EXPECT_NE(owner.find("int64_t owner_pending_future_count()"), std::string::npos);
  EXPECT_EQ(owner.find("long owner_pending_future_count()"), std::string::npos);
  EXPECT_NE(owner.find("int64_t pending_futures{0};"), std::string::npos);
  EXPECT_EQ(owner.find("long pending_futures{0};"), std::string::npos);
}

TEST_F(DriverTest, TestOwnerFutureStoreResolvesTargetsAfterTerminalLockScope) {
  const auto source = read_source_file_for_test("../src/vm/internal/owner_future_store.cc");
  auto assert_resolved_after_unlock = [&](const char* start_marker, const char* end_marker,
                                          const char* resolution) {
    const auto start = source.find(start_marker);
    const auto end = source.find(end_marker, start);
    ASSERT_NE(start, std::string::npos) << start_marker;
    ASSERT_NE(end, std::string::npos) << end_marker;
    const auto body = source.substr(start, end - start);
    const auto lock_pos = body.find("std::unique_lock<std::mutex> lock(mutex_);");
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
  assert_resolved_after_unlock("OwnerFutureTerminalResult OwnerFutureStore::fail_terminal(",
                               "int64_t OwnerFutureStore::pending_count()",
                               "result.target_status = target_status(result.record);");

  const auto helper_start = source.find("OwnerFutureCompletion OwnerFutureStore::complete_record(");
  ASSERT_NE(helper_start, std::string::npos);
  const auto helper_body = source.substr(helper_start);
  EXPECT_EQ(helper_body.find("target_status(record)"), std::string::npos);
}

TEST_F(DriverTest, TestGatewayStatusReportsSessionFifoContract) {
  const auto gateway_status_source =
      read_source_file_for_test("../src/packages/gateway/gateway.cc");
  ASSERT_NE(gateway_status_source.find("GATEWAY_TOTAL_US(map, receive_enqueue_to_dispatch)"),
            std::string::npos);

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
  ASSERT_EQ(mapping_number(status, "gateway_background_dispatch_max_interval_us"), 2000);
  ASSERT_EQ(mapping_number(status, "gateway_background_dispatch_max_callbacks"), 8);
  ASSERT_EQ(mapping_number(status, "gateway_background_dispatch_min_priority"), 1);
  ASSERT_EQ(mapping_number(status, "gateway_backend_tick_slice_budget"), 64);
  ASSERT_EQ(mapping_number(status, "gateway_backend_tick_slice_wall_budget_us"), 4000);
  ASSERT_EQ(mapping_number(status, "gateway_backend_tick_continuation_delay_us"), 1000);
  ASSERT_EQ(mapping_number(status, "gateway_backend_owner_main_slice_budget"), 64);
  ASSERT_EQ(mapping_number(status, "gateway_backend_owner_main_slice_wall_budget_us"), 8000);
  ASSERT_EQ(mapping_number(status, "gateway_backend_owner_main_continuation_delay_us"), 1000);
  ASSERT_GE(mapping_number(status, "gateway_backend_tick_slice_runs"), 0);
  ASSERT_GE(mapping_number(status, "gateway_backend_tick_slice_callbacks_total"), 0);
  ASSERT_GE(mapping_number(status, "gateway_backend_tick_slice_callbacks_max"), 0);
  ASSERT_GE(mapping_number(status, "gateway_backend_tick_slice_budget_yields"), 0);
  ASSERT_GE(mapping_number(status, "gateway_backend_tick_slice_wall_yields"), 0);
  ASSERT_GE(mapping_number(status, "gateway_backend_tick_continuations_scheduled"), 0);
  ASSERT_GE(mapping_number(status, "gateway_backend_tick_continuations_executed"), 0);
  ASSERT_GE(mapping_number(status, "gateway_backend_owner_main_slice_runs"), 0);
  ASSERT_GE(mapping_number(status, "gateway_backend_owner_main_slice_tasks_total"), 0);
  ASSERT_GE(mapping_number(status, "gateway_backend_owner_main_slice_tasks_max"), 0);
  ASSERT_GE(mapping_number(status, "gateway_backend_owner_main_slice_task_budget_yields"), 0);
  ASSERT_GE(mapping_number(status, "gateway_backend_owner_main_slice_wall_yields"), 0);
  ASSERT_GE(mapping_number(status, "gateway_backend_owner_main_tasks_exceeding_wall_budget"), 0);
  ASSERT_GE(mapping_number(status, "gateway_backend_owner_main_continuations_scheduled"), 0);
  ASSERT_GE(mapping_number(status, "gateway_backend_owner_main_continuations_executed"), 0);
  ASSERT_GE(mapping_number(status, "gateway_backend_owner_main_continuation_pending"), 0);
  ASSERT_EQ(mapping_number(status, "session_fifo_contract_ready"), 1);
  ASSERT_GE(mapping_number(status, "session_fifo_depth"), 0);
  ASSERT_GE(mapping_number(status, "session_fifo_pending_reservations"), 0);
  ASSERT_GE(mapping_number(status, "session_fifo_wire_bytes"), 0);
  ASSERT_GE(mapping_number(status, "session_fifo_wire_limit_bytes"), 64 * 1024);
  ASSERT_GE(mapping_number(status, "session_fifo_wire_aggregate_limit_bytes"), 64 * 1024);
  ASSERT_GE(mapping_number(status, "session_fifo_wire_detached_bytes"), 0);
  ASSERT_GE(mapping_number(status, "session_fifo_wire_bytes_rejected"), 0);
  ASSERT_GE(mapping_number(status, "session_fifo_enqueued"), 0);
  ASSERT_GE(mapping_number(status, "session_fifo_flushed"), 0);
  ASSERT_GE(mapping_number(status, "session_fifo_rejected"), 0);
  ASSERT_GE(mapping_number(status, "gateway_data_frames_received"), 0);
  ASSERT_GE(mapping_number(status, "gateway_data_frames_applied"), 0);
  ASSERT_GE(mapping_number(status, "gateway_data_frames_rejected"), 0);
  ASSERT_GE(mapping_number(status, "gateway_json_frames_rejected"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_dispatch_frame_length_rejected"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_dispatch_buffer_limit_rejected"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_dispatch_native_bytes_deferred"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_buffer_limit_bytes"), 1028);
  ASSERT_GE(mapping_number(status, "gateway_raw_write_backpressure_rejected"), 0);
  ASSERT_GE(mapping_number(status, "gateway_write_buffer_limit_bytes"), 64 * 1024);
  ASSERT_EQ(mapping_number(status, "gateway_packet_size_hard_limit_bytes"), 16 * 1024 * 1024);
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
  ASSERT_GE(mapping_number(status, "gateway_output_fifo_wire_bytes_rejected"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_fifo_aggregate_wire_bytes_rejected"), 0);
  ASSERT_GE(mapping_number(status, "gateway_output_fifo_rebucket_wire_bytes_dropped"), 0);
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
  for (const auto* prefix : {
           "gateway_room_output_projection_worker_thread_cpu",
           "gateway_room_output_projection_inline_thread_cpu",
       }) {
    for (const auto* suffix : {"_samples", "_total_us", "_avg_us", "_max_us"}) {
      const auto field = std::string(prefix) + suffix;
      ASSERT_NE(find_string_in_mapping(status, field.c_str()), nullptr) << field;
      ASSERT_GE(mapping_number(status, field.c_str()), 0);
    }
  }
  ASSERT_GE(mapping_number(status, "gateway_room_output_projection_worker_thread_cpu_unavailable"),
            0);
  ASSERT_GE(mapping_number(status, "gateway_room_output_projection_inline_thread_cpu_unavailable"),
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
  ASSERT_GE(mapping_number(status, "gateway_main_drain_deferred_main_tasks_exceeding_wall_budget"),
            0);
  ASSERT_EQ(mapping_number(status, "gateway_read_batch_drain_task_budget"), 32);
  ASSERT_EQ(mapping_number(status, "gateway_read_batch_drain_wall_budget_us"), 12000);
  ASSERT_GE(mapping_number(status, "gateway_read_batch_drain_runs"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_batch_drain_tasks_total"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_batch_drain_tasks_max"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_batch_drain_backlog_rescheduled"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_batch_drain_main_task_wall_max_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_read_batch_drain_main_tasks_exceeding_wall_budget"), 0);
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
  ASSERT_EQ(mapping_number(status, "gateway_main_drain_deferred_wait_timer_queue_only"), 1);
  ASSERT_GE(mapping_number(status, "gateway_receive_decode_samples"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_decode_avg_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_decode_max_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_payload_copy_samples"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_payload_copy_avg_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_payload_copy_max_us"), 0);
  ASSERT_GE(mapping_number(status, "gateway_receive_enqueue_to_dispatch_samples"), 0);
  ASSERT_NE(find_string_in_mapping(status, "gateway_receive_enqueue_to_dispatch_total_us"),
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
    std::atomic<uint64_t>& counter;
    uint64_t original;
    ~AtomicCounterRestore() {
      counter.store(original, std::memory_order_relaxed);
    }
  };

  auto& worker_total = g_gateway_runtime_counters.room_output_projection_worker_thread_cpu_ns_total;
  auto& worker_samples =
      g_gateway_runtime_counters.room_output_projection_worker_thread_cpu_samples;
  auto& inline_total = g_gateway_runtime_counters.room_output_projection_inline_thread_cpu_ns_total;
  auto& inline_samples =
      g_gateway_runtime_counters.room_output_projection_inline_thread_cpu_samples;
  AtomicCounterRestore worker_total_restore{worker_total,
                                            worker_total.load(std::memory_order_relaxed)};
  AtomicCounterRestore worker_samples_restore{worker_samples,
                                              worker_samples.load(std::memory_order_relaxed)};
  AtomicCounterRestore inline_total_restore{inline_total,
                                            inline_total.load(std::memory_order_relaxed)};
  AtomicCounterRestore inline_samples_restore{inline_samples,
                                              inline_samples.load(std::memory_order_relaxed)};

  worker_total.store(12000, std::memory_order_relaxed);
  worker_samples.store(3, std::memory_order_relaxed);
  inline_total.store(25000, std::memory_order_relaxed);
  inline_samples.store(5, std::memory_order_relaxed);

  auto* status = gateway_status_internal();
  ASSERT_NE(status, nullptr);
  auto* worker_avg =
      find_string_in_mapping(status, "gateway_room_output_projection_worker_thread_cpu_avg_us");
  auto* inline_avg =
      find_string_in_mapping(status, "gateway_room_output_projection_inline_thread_cpu_avg_us");
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
      g_gateway_runtime_counters.output_fifo_head_blocked_predecessors_total.load(
          std::memory_order_relaxed);
  ASSERT_EQ(gateway_fill_session_protocol_output_with_writer(&session, second_reservation_id,
                                                             "second", 6, writer),
            1);
  ASSERT_EQ(session.output_fifo.size(), 2u);
  ASSERT_FALSE(session.output_fifo.front().ready);
  ASSERT_TRUE(session.output_fifo.back().ready);
  ASSERT_TRUE(writes.empty());
  ASSERT_EQ(
      g_gateway_runtime_counters.output_fifo_head_blocked_fills.load(std::memory_order_relaxed),
      blocked_fills_before + 1);
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_head_blocked_predecessors_total.load(
                std::memory_order_relaxed),
            blocked_predecessors_before + 1);
  ASSERT_GE(g_gateway_runtime_counters.output_fifo_head_blocked_predecessors_max.load(
                std::memory_order_relaxed),
            1u);

  ASSERT_EQ(gateway_fill_session_protocol_output_with_writer(&session, first_reservation_id,
                                                             "first", 5, writer),
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
  auto writer = [](int, const char*, size_t) -> int {
    return 1;
  };

  GatewaySession session;
  session.session_id = "ready-successor-session";
  session.master_fd = 78;
  auto first_reservation_id = gateway_reserve_session_output(&session);
  auto second_reservation_id = gateway_reserve_session_output(&session);
  ASSERT_GT(first_reservation_id, 0u);
  ASSERT_GT(second_reservation_id, 0u);

  ASSERT_FALSE(
      gateway_session_pending_reservation_has_ready_successor(nullptr, first_reservation_id));
  ASSERT_FALSE(gateway_session_pending_reservation_has_ready_successor(&session, 0));
  ASSERT_FALSE(
      gateway_session_pending_reservation_has_ready_successor(&session, second_reservation_id + 1));
  ASSERT_FALSE(
      gateway_session_pending_reservation_has_ready_successor(&session, first_reservation_id));
  ASSERT_FALSE(
      gateway_session_pending_reservation_has_ready_successor(&session, second_reservation_id));

  ASSERT_EQ(gateway_fill_session_protocol_output_with_writer(&session, second_reservation_id,
                                                             "second", 6, writer),
            1);
  ASSERT_TRUE(
      gateway_session_pending_reservation_has_ready_successor(&session, first_reservation_id));
  ASSERT_FALSE(
      gateway_session_pending_reservation_has_ready_successor(&session, second_reservation_id));
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
  ASSERT_EQ(gateway_enqueue_session_protocol_output(&session, "after-release",
                                                    sizeof("after-release") - 1),
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

  ASSERT_EQ(gateway_enqueue_session_protocol_output(&session, frame.data(), frame.size()), 1);
  ASSERT_EQ(session.output_fifo.size(), 1u);
  ASSERT_TRUE(session.output_fifo.front().ready);
  const auto wire = nlohmann::json::parse(session.output_fifo.front().wire_bytes);
  ASSERT_EQ(session.output_fifo_wire_bytes, session.output_fifo.front().wire_bytes.size());
  ASSERT_EQ(wire["type"], "output");
  ASSERT_EQ(wire["cid"], "ordinary-session");
  ASSERT_EQ(wire["data"].get<std::string>(), frame);
}

TEST_F(DriverTest, TestGatewayMasterOutputBufferRejectsSlowPeerBeforeUnboundedGrowth) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() {
      g_gateway_max_packet_size = original;
    }
  } packet_size_guard;

  g_gateway_max_packet_size = 1024;
  constexpr int master_fd = 1708;
  bufferevent* pair[2] = {nullptr, nullptr};
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  ASSERT_NE(gateway_register_master_for_test(master_fd, pair[0]), nullptr);

  const auto output_limit = gateway_write_buffer_limit_for_test();
  const auto rejected_before =
      g_gateway_runtime_counters.raw_write_backpressure_rejected.load(std::memory_order_relaxed);
  const std::string payload(128, 'x');
  size_t accepted = 0;
  while (gateway_send_raw_to_fd(master_fd, payload.data(), payload.size())) {
    accepted++;
    ASSERT_LE(evbuffer_get_length(bufferevent_get_output(pair[0])), output_limit);
  }

  ASSERT_GT(accepted, 0u);
  ASSERT_LT(evbuffer_get_length(bufferevent_get_output(pair[0])),
            output_limit + payload.size() + sizeof(uint32_t));
  ASSERT_EQ(
      g_gateway_runtime_counters.raw_write_backpressure_rejected.load(std::memory_order_relaxed),
      rejected_before + 1);
  ASSERT_EQ(gateway_send_raw_to_fd(master_fd, nullptr, payload.size()), 0);
  ASSERT_EQ(gateway_send_raw_to_fd(master_fd, payload.data(), g_gateway_max_packet_size + 1), 0);
  ASSERT_EQ(
      g_gateway_runtime_counters.raw_write_backpressure_rejected.load(std::memory_order_relaxed),
      rejected_before + 1);

  gateway_remove_master_for_test(master_fd);
  pair[0] = nullptr;
  bufferevent_free(pair[1]);
}

TEST_F(DriverTest, TestGatewayWireFactoryRejectsOversizeBeforeFifoMutation) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() {
      g_gateway_max_packet_size = original;
    }
  } guard;
  static std::vector<std::string> writes;
  writes.clear();
  auto writer = [](int, const char* data, size_t len) -> int {
    writes.emplace_back(data, len);
    return 1;
  };
  g_gateway_max_packet_size = 96;
  const std::string oversized_payload(128, 'x');

  GatewaySession ordinary;
  ordinary.session_id = "oversize-ordinary";
  ordinary.master_fd = -1;
  ASSERT_EQ(gateway_enqueue_session_protocol_output(&ordinary, oversized_payload.data(),
                                                    oversized_payload.size()),
            0);
  ASSERT_TRUE(ordinary.output_fifo.empty());

  GatewaySession reserved;
  reserved.session_id = "oversize-reserved";
  reserved.master_fd = 77;
  const auto reservation_id = gateway_reserve_session_output(&reserved);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_EQ(
      gateway_fill_session_protocol_output_with_writer(
          &reserved, reservation_id, oversized_payload.data(), oversized_payload.size(), writer),
      0);
  ASSERT_TRUE(writes.empty());
  ASSERT_EQ(reserved.output_fifo.size(), 1u);
  ASSERT_EQ(reserved.output_fifo.front().reservation_id, reservation_id);
  ASSERT_FALSE(reserved.output_fifo.front().ready);
  ASSERT_TRUE(reserved.output_fifo.front().wire_bytes.empty());
}

TEST_F(DriverTest, TestGatewaySessionFifoWireByteLimitRejectsWithoutQueueMutation) {
  const auto rejected_before =
      g_gateway_runtime_counters.output_fifo_wire_bytes_rejected.load(std::memory_order_relaxed);

  GatewaySession ordinary;
  ordinary.session_id = "wire-byte-limit-ordinary";
  ordinary.master_fd = -1;
  const auto ordinary_wire =
      gateway_encode_output_envelope_for_test(ordinary.session_id, "first", 5);
  ordinary.output_fifo_max_wire_bytes = ordinary_wire.size();
  ASSERT_EQ(gateway_enqueue_session_protocol_output(&ordinary, "first", 5), 1);
  ASSERT_EQ(ordinary.output_fifo_wire_bytes, ordinary_wire.size());
  ASSERT_EQ(gateway_enqueue_session_protocol_output(&ordinary, "second", 6), 0);
  ASSERT_EQ(ordinary.output_fifo.size(), 1u);
  ASSERT_EQ(ordinary.output_fifo_wire_bytes, ordinary_wire.size());
  ASSERT_EQ(ordinary.output_fifo_wire_bytes_rejected, 1u);

  GatewaySession reserved;
  reserved.session_id = "wire-byte-limit-reserved";
  reserved.master_fd = -1;
  const auto retained_wire =
      gateway_encode_output_envelope_for_test(reserved.session_id, "retained", 8);
  reserved.output_fifo_max_wire_bytes = retained_wire.size();
  ASSERT_EQ(gateway_enqueue_session_protocol_output(&reserved, "retained", 8), 1);
  const auto reservation_id = gateway_reserve_session_output(&reserved);
  ASSERT_GT(reservation_id, 0u);
  reserved.master_fd = 77;
  ASSERT_EQ(
      gateway_fill_session_protocol_output_with_writer(&reserved, reservation_id, "blocked", 7,
                                                       [](int, const char*, size_t) -> int {
                                                         return 1;
                                                       }),
      0);
  ASSERT_EQ(reserved.output_fifo.size(), 2u);
  ASSERT_TRUE(reserved.output_fifo.front().ready);
  ASSERT_FALSE(reserved.output_fifo.back().ready);
  ASSERT_TRUE(reserved.output_fifo.back().wire_bytes.empty());
  ASSERT_EQ(reserved.output_fifo_wire_bytes, retained_wire.size());
  ASSERT_EQ(reserved.output_fifo_wire_bytes_rejected, 1u);
  ASSERT_EQ(
      g_gateway_runtime_counters.output_fifo_wire_bytes_rejected.load(std::memory_order_relaxed),
      rejected_before + 2);
}

TEST_F(DriverTest, TestGatewayFlushDropsReadyWireMadeOversizeByLimitReduction) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() {
      g_gateway_max_packet_size = original;
    }
  } guard;
  static std::vector<size_t> attempted_lengths;
  static std::vector<std::string> writes;
  attempted_lengths.clear();
  writes.clear();
  auto writer = [](int fd, const char* data, size_t len) -> int {
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
  ASSERT_EQ(gateway_enqueue_session_protocol_output(&session, oversized_payload.data(),
                                                    oversized_payload.size()),
            1);
  ASSERT_EQ(gateway_enqueue_session_protocol_output(&session, "successor", sizeof("successor") - 1),
            1);
  ASSERT_EQ(session.output_fifo.size(), 2u);
  ASSERT_TRUE(session.output_fifo.front().ready);
  ASSERT_EQ(session.output_fifo.front().reservation_id, 0u);
  ASSERT_GT(session.output_fifo.front().wire_bytes.size(), 1024u);
  ASSERT_LT(session.output_fifo.back().wire_bytes.size(), 1024u);
  ASSERT_EQ(session.output_fifo_wire_bytes, session.output_fifo.front().wire_bytes.size() +
                                                session.output_fifo.back().wire_bytes.size());
  const auto oversize_dropped_before =
      g_gateway_runtime_counters.output_fifo_oversize_dropped.load(std::memory_order_relaxed);
  const auto released_before =
      g_gateway_runtime_counters.output_fifo_released.load(std::memory_order_relaxed);
  const auto writer_failures_before =
      g_gateway_runtime_counters.output_fifo_writer_failures.load(std::memory_order_relaxed);

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
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_oversize_dropped.load(std::memory_order_relaxed),
            oversize_dropped_before + 1);
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_released.load(std::memory_order_relaxed),
            released_before);
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_writer_failures.load(std::memory_order_relaxed),
            writer_failures_before);
}

TEST_F(DriverTest, TestGatewayFlushClassifiesReadyReservationOversizeExactlyOnce) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() {
      g_gateway_max_packet_size = original;
    }
  } guard;
  static std::vector<size_t> attempted_lengths;
  static std::vector<std::string> writes;
  attempted_lengths.clear();
  writes.clear();
  auto writer = [](int fd, const char* data, size_t len) -> int {
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
  ASSERT_EQ(
      gateway_fill_session_protocol_output_with_writer(
          &session, reservation_id, oversized_payload.data(), oversized_payload.size(), writer),
      1);
  ASSERT_EQ(gateway_enqueue_session_protocol_output(&session, "successor", sizeof("successor") - 1),
            1);
  ASSERT_EQ(session.output_fifo.size(), 2u);
  ASSERT_TRUE(session.output_fifo.front().ready);
  ASSERT_EQ(session.output_fifo.front().reservation_id, reservation_id);
  ASSERT_GT(session.output_fifo.front().wire_bytes.size(), 1024u);
  ASSERT_LT(session.output_fifo.back().wire_bytes.size(), 1024u);
  ASSERT_EQ(session.output_fifo_wire_bytes, session.output_fifo.front().wire_bytes.size() +
                                                session.output_fifo.back().wire_bytes.size());
  const auto oversize_dropped_before =
      g_gateway_runtime_counters.output_fifo_oversize_dropped.load(std::memory_order_relaxed);
  const auto released_before =
      g_gateway_runtime_counters.output_fifo_released.load(std::memory_order_relaxed);
  const auto writer_failures_before =
      g_gateway_runtime_counters.output_fifo_writer_failures.load(std::memory_order_relaxed);

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
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_oversize_dropped.load(std::memory_order_relaxed),
            oversize_dropped_before + 1);
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_released.load(std::memory_order_relaxed),
            released_before);
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_writer_failures.load(std::memory_order_relaxed),
            writer_failures_before);
}

TEST_F(DriverTest, TestGatewayWireReadyQueueRejectsMalformedSouthboundJson) {
  GatewaySession session;
  session.session_id = "wire-ready-session";
  session.master_fd = -1;

  ASSERT_EQ(
      gateway_enqueue_session_wire_json_for_test(
          &session, R"({"type":"output","cid":"wire-ready-session","data":"\u001bXKMSGE{}"})"),
      1);
  ASSERT_EQ(session.output_fifo.size(), 1u);
  const auto valid_wire = session.output_fifo.front().wire_bytes;

  for (const auto& invalid : {
           R"({"cid":"wire-ready-session","data":"frame"})",
           R"({"type":"sys","cid":"wire-ready-session","data":"frame"})",
           R"({"type":"output","cid":"other-session","data":"frame"})",
           R"({"type":"output","cid":"wire-ready-session"})",
           R"({"type":"output","cid":"wire-ready-session","data":{"frame":1}})",
           R"({"type":"output","cid":"wire-ready-session","data":"frame","extra":1})",
           R"(["output","wire-ready-session","frame"])",
       }) {
    EXPECT_EQ(gateway_enqueue_session_wire_json_for_test(&session, invalid), 0) << invalid;
    EXPECT_EQ(session.output_fifo.size(), 1u) << invalid;
    EXPECT_EQ(session.output_fifo.front().wire_bytes, valid_wire) << invalid;
  }
}

TEST_F(DriverTest, TestGatewayProjectedWireBatchRejectsUnboundRecipientsAtomically) {
  static std::vector<std::string> writes;
  writes.clear();
  auto writer = [](int, const char* data, size_t len) -> int {
    writes.emplace_back(data, len);
    return 1;
  };

  const auto assert_rejected = [&](const std::vector<std::string>& wire_json) {
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

    EXPECT_FALSE(gateway_fill_projected_wires_for_test({&first, &second}, {first_id, second_id},
                                                       wire_json, writer));
    EXPECT_TRUE(writes.empty());
    ASSERT_EQ(first.output_fifo.size(), 1u);
    ASSERT_EQ(second.output_fifo.size(), 1u);
    EXPECT_FALSE(first.output_fifo.front().ready);
    EXPECT_FALSE(second.output_fifo.front().ready);
  };

  const auto first_wire =
      gateway_encode_output_envelope_for_test("projected-first", "first-frame", 11);
  const auto second_wire =
      gateway_encode_output_envelope_for_test("projected-second", "second-frame", 12);
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
  ASSERT_TRUE(gateway_fill_projected_wires_for_test({&first, &second}, {first_id, second_id},
                                                    {first_wire, second_wire}, writer));
  ASSERT_EQ(writes.size(), 2u);
  EXPECT_EQ(nlohmann::json::parse(writes[0])["cid"], "projected-first");
  EXPECT_EQ(nlohmann::json::parse(writes[1])["cid"], "projected-second");
}

TEST_F(DriverTest, TestGatewayProjectedWireBatchRejectsOversizeAtomically) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() {
      g_gateway_max_packet_size = original;
    }
  } guard;
  static std::vector<std::string> writes;
  writes.clear();
  auto writer = [](int, const char* data, size_t len) -> int {
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
  const auto first_wire = gateway_encode_output_envelope_for_test(first.session_id, "ok", 2);
  const std::string oversized_payload(256, 'y');
  const auto second_wire = gateway_encode_output_envelope_for_test(
      second.session_id, oversized_payload.data(), oversized_payload.size());
  ASSERT_LT(first_wire.size(), g_gateway_max_packet_size);
  ASSERT_GT(second_wire.size(), g_gateway_max_packet_size);

  ASSERT_FALSE(gateway_fill_projected_wires_for_test({&first, &second}, {first_id, second_id},
                                                     {first_wire, second_wire}, writer));
  ASSERT_TRUE(writes.empty());
  ASSERT_EQ(first.output_fifo.size(), 1u);
  ASSERT_EQ(second.output_fifo.size(), 1u);
  ASSERT_FALSE(first.output_fifo.front().ready);
  ASSERT_FALSE(second.output_fifo.front().ready);
  ASSERT_TRUE(first.output_fifo.front().wire_bytes.empty());
  ASSERT_TRUE(second.output_fifo.front().wire_bytes.empty());
}

TEST_F(DriverTest, TestGatewayProjectedWireBatchRejectsWireByteLimitAtomically) {
  static std::vector<std::string> writes;
  writes.clear();
  auto writer = [](int, const char* data, size_t len) -> int {
    writes.emplace_back(data, len);
    return 1;
  };

  GatewaySession first;
  GatewaySession second;
  first.session_id = "projected-byte-first";
  second.session_id = "projected-byte-second";
  first.master_fd = -1;
  second.master_fd = 82;
  const auto retained_wire =
      gateway_encode_output_envelope_for_test(first.session_id, "retained", 8);
  first.output_fifo_max_wire_bytes = retained_wire.size();
  ASSERT_EQ(gateway_enqueue_session_protocol_output(&first, "retained", 8), 1);
  const auto first_id = gateway_reserve_session_output(&first);
  const auto second_id = gateway_reserve_session_output(&second);
  ASSERT_GT(first_id, 0u);
  ASSERT_GT(second_id, 0u);
  first.master_fd = 81;
  const auto first_wire = gateway_encode_output_envelope_for_test(first.session_id, "first", 5);
  const auto second_wire = gateway_encode_output_envelope_for_test(second.session_id, "second", 6);

  ASSERT_FALSE(gateway_fill_projected_wires_for_test({&first, &second}, {first_id, second_id},
                                                     {first_wire, second_wire}, writer));
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
  const auto assert_payload = [](const std::string& payload_json, const nlohmann::json& expected) {
    GatewaySession session;
    session.session_id = "mixed-session";
    session.master_fd = -1;
    ASSERT_EQ(gateway_enqueue_session_payload_json_for_test(&session, payload_json), 1);
    ASSERT_EQ(session.output_fifo.size(), 1u);
    EXPECT_EQ(nlohmann::json::parse(session.output_fifo.front().wire_bytes), expected);
  };

  assert_payload("7", {{"type", "output"}, {"cid", "mixed-session"}, {"data", 7}});
  assert_payload(
      R"([1,"two"])",
      {{"type", "output"}, {"cid", "mixed-session"}, {"data", nlohmann::json::array({1, "two"})}});
  assert_payload(R"({"type":"custom","data":{"answer":42}})",
                 {{"type", "custom"}, {"cid", "mixed-session"}, {"data", {{"answer", 42}}}});
}

TEST_F(DriverTest, TestGatewayWriterFailureRetainsOneReadyFrameForExactRetry) {
  static std::vector<std::string> writes;
  writes.clear();
  auto failing_writer = [](int, const char*, size_t) -> int {
    return 0;
  };
  auto succeeding_writer = [](int, const char* data, size_t len) -> int {
    writes.emplace_back(data, len);
    return 1;
  };

  GatewaySession session;
  session.session_id = "writer-retry-session";
  session.master_fd = 71;
  const auto reservation_id = gateway_reserve_session_output(&session);
  ASSERT_GT(reservation_id, 0u);
  const auto failures_before =
      g_gateway_runtime_counters.output_fifo_writer_failures.load(std::memory_order_relaxed);

  ASSERT_EQ(gateway_fill_session_protocol_output_with_writer(&session, reservation_id,
                                                             "retry-frame", 11, failing_writer),
            1);
  ASSERT_EQ(session.output_fifo.size(), 1u);
  ASSERT_TRUE(session.output_fifo.front().ready);
  ASSERT_EQ(session.output_fifo.front().reservation_id, reservation_id);
  ASSERT_EQ(session.output_fifo_wire_bytes, session.output_fifo.front().wire_bytes.size());
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_writer_failures.load(std::memory_order_relaxed),
            failures_before + 1);

  ASSERT_EQ(gateway_flush_session_output_fifo_with_writer(&session, succeeding_writer), 1);
  ASSERT_EQ(gateway_flush_session_output_fifo_with_writer(&session, succeeding_writer), 0);
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
  ASSERT_TRUE(gateway_reserve_session_outputs({&first, &second}, {first_id, 0}, &result));
  ASSERT_EQ(result.reservation_ids.size(), 2u);
  ASSERT_EQ(result.reused, (std::vector<bool>{true, false}));
  ASSERT_EQ(result.reservation_ids[0], first_id);
  ASSERT_GT(result.reservation_ids[1], 0u);
  ASSERT_EQ(first.output_fifo.size(), 1u);
  ASSERT_EQ(second.output_fifo.size(), 1u);

  ASSERT_EQ(
      gateway_enqueue_session_protocol_output(&first, "after-pending", sizeof("after-pending") - 1),
      1);
  GatewaySessionBatchReservationResult after_intervening;
  ASSERT_TRUE(gateway_reserve_session_outputs({&first}, {first_id}, &after_intervening));
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
  ASSERT_FALSE(gateway_reserve_session_outputs({&first, &full}, {first_id, 0}, &result));
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
  ASSERT_FALSE(gateway_reserve_session_outputs({&session, &session}, {0, 0}, &result));
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

  const std::vector<GatewaySession*> sessions{&first, &second};
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
  const auto writer = [](int, const char* data, size_t len) {
    writes.emplace_back(data, len);
    return 1;
  };
  ASSERT_EQ(gateway_fill_pending_message_event_batch_with_writer(&first, first_id, "player-one", 7,
                                                                 writer),
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
  ASSERT_EQ(gateway_fill_pending_message_event_batch_with_writer(&second, second_id, "player-two",
                                                                 8, writer),
            1);
  ASSERT_EQ(writes.size(), 2u);
  const auto second_wire = nlohmann::json::parse(writes[1]);
  ASSERT_EQ(second_wire["type"], "output");
  ASSERT_EQ(second_wire["cid"], "session-two");
  ASSERT_EQ(second_wire["data"].get<std::string>().substr(0, 7), "\x1bXKBACH");
  ASSERT_EQ(g_gateway_runtime_counters.message_event_template_cache_hits.load(),
            cache_hits_after_append);
  ASSERT_EQ(g_gateway_runtime_counters.message_event_template_cache_misses.load(),
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
      {&first, &second}, {first_id, second_id}, {101, 201}, {102, 202}, {3, 4}, {1001, 2001},
      stable, "player", 123456, 128));

  static std::vector<std::string> writes;
  writes.clear();
  const auto writer = [](int, const char* data, size_t len) {
    writes.emplace_back(data, len);
    return 1;
  };
  GatewayPendingMessageEventBatchFillResult result;

  ASSERT_FALSE(gateway_fill_pending_message_event_batches_with_writer(
      {&first, &second}, {first_id, second_id}, {"player-one", ""}, {7, 8}, writer, &result));
  ASSERT_TRUE(writes.empty());
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 1u);
  ASSERT_EQ(gateway_pending_message_event_count(&second, second_id), 1u);

  const auto inline_cpu_samples_before =
      g_gateway_runtime_counters.room_output_projection_inline_thread_cpu_samples.load(
          std::memory_order_relaxed);
  const auto inline_cpu_unavailable_before =
      g_gateway_runtime_counters.room_output_projection_inline_thread_cpu_unavailable.load(
          std::memory_order_relaxed);

  ASSERT_TRUE(gateway_fill_pending_message_event_batches_with_writer(
      {&first, &second}, {first_id, second_id}, {"player-one", "player-two"}, {7, 8}, writer,
      &result));
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
  ASSERT_NE(first_wire["data"].get<std::string>().find("player-one"), std::string::npos);
  ASSERT_NE(second_wire["data"].get<std::string>().find("player-two"), std::string::npos);
  const auto inline_cpu_samples_after =
      g_gateway_runtime_counters.room_output_projection_inline_thread_cpu_samples.load(
          std::memory_order_relaxed);
  const auto inline_cpu_unavailable_after =
      g_gateway_runtime_counters.room_output_projection_inline_thread_cpu_unavailable.load(
          std::memory_order_relaxed);
  ASSERT_EQ((inline_cpu_samples_after - inline_cpu_samples_before) +
                (inline_cpu_unavailable_after - inline_cpu_unavailable_before),
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
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave({&session}, {reservation_id}, {901},
                                                           {902}, {11}, {7001}, first_stable,
                                                           "player", 123456, 128));
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave({&session}, {reservation_id}, {901},
                                                           {904}, {12}, {7002}, second_stable,
                                                           "player", 123457, 128));

  GatewayPendingMessageEventProjectionSnapshot snapshot;
  GatewayPendingMessageEventProjectionColumns columns;
  ASSERT_TRUE(gateway_snapshot_pending_message_event_batch(
      &session, reservation_id, "player-\"owner\\scope", 13, &snapshot, &columns));
  ASSERT_EQ(snapshot.wave_table.size(), 2u);
  ASSERT_EQ(snapshot.event_wave_indices, (std::vector<size_t>{0, 1}));
  ASSERT_EQ(snapshot.wave_table[0]->message_template.stable_members.find("first-owner-event") !=
                std::string::npos,
            true);
  ASSERT_EQ(snapshot.wave_table[1]->message_template.stable_members.find("second-owner-event") !=
                std::string::npos,
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
  ASSERT_TRUE(gateway_encode_pending_message_event_projection(snapshot, columns, &owner_projected));
  const auto inline_inner = gateway_encode_preencoded_message_event_batch_for_test(
      {first_stable, second_stable}, {"player", "player"}, "player-\"owner\\scope", {7001, 7002},
      {902, 904}, {11, 12}, {123456, 123457}, 901, 13, 123456);
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

TEST_F(DriverTest, TestGatewayLocalProjectionSkipsFullValidationWhileUntrustedWireKeepsIt) {
  static std::vector<std::string> writes;
  writes.clear();
  const auto writer = [](int, const char* data, size_t len) {
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
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave({&local}, {local_id}, {101}, {102}, {3},
                                                           {1001}, stable, "player", 123456, 128));

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
  const auto valid_wire =
      gateway_encode_output_envelope_for_test(untrusted.session_id, "validated", 9);
  ASSERT_TRUE(
      gateway_fill_projected_wires_for_test({&untrusted}, {valid_id}, {valid_wire}, writer));
  EXPECT_EQ(gateway_projected_wire_full_validation_count_for_test(), 1u);

  const auto invalid_id = gateway_reserve_session_output(&untrusted);
  ASSERT_GT(invalid_id, 0u);
  const auto wrong_session_wire =
      gateway_encode_output_envelope_for_test("wrong-session", "rejected", 8);
  ASSERT_FALSE(gateway_fill_projected_wires_for_test({&untrusted}, {invalid_id},
                                                     {wrong_session_wire}, writer));
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
      {&first, &invalid}, {first_id, invalid_id}, {101, 201}, {102, 202}, {3, 4}, {1001, 2001},
      stable, "player", 123456, 128));

  static std::vector<std::string> writes;
  writes.clear();
  const auto writer = [](int, const char* data, size_t len) {
    writes.emplace_back(data, len);
    return 1;
  };
  GatewayPendingMessageEventBatchFillResult result;

  ASSERT_FALSE(gateway_fill_pending_message_event_batches_with_writer(
      {&first, &invalid}, {first_id, invalid_id}, {"player-one", "player-two"}, {7, 8}, writer,
      &result));
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
      {&session}, {first_id}, {501}, {502}, {7}, {5001}, first_stable, "player", 123456, 128));
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      {&session}, {second_id}, {503}, {504}, {7}, {5002}, second_stable, "player", 123457, 128));

  static std::vector<std::string> writes;
  writes.clear();
  const auto writer = [](int, const char* data, size_t len) {
    writes.emplace_back(data, len);
    return 1;
  };
  GatewayPendingMessageEventBatchFillResult result;
  ASSERT_TRUE(gateway_fill_pending_message_event_batches_with_writer(
      {&session, &session}, {second_id, first_id}, {"player-one", "player-one"}, {9, 9}, writer,
      &result));
  ASSERT_EQ(result.filled, (std::vector<bool>{true, true}));
  ASSERT_EQ(writes.size(), 2u);
  const auto first_wire = nlohmann::json::parse(writes[0]);
  const auto second_wire = nlohmann::json::parse(writes[1]);
  const auto expected_first_frame = gateway_encode_preencoded_message_event_batch_for_test(
      {first_stable}, {"player"}, "player-one", {5001}, {502}, {7}, {123456}, 501, 9, 123456);
  const auto expected_second_frame = gateway_encode_preencoded_message_event_batch_for_test(
      {second_stable}, {"player"}, "player-one", {5002}, {504}, {7}, {123457}, 503, 9, 123457);
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
      {&session}, {first_id}, {601}, {602}, {8}, {6001}, stable, "player", 123456, 128));
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      {&session}, {second_id}, {603}, {604}, {8}, {6002}, stable, "player", 123457, 128));

  static std::vector<std::string> writes;
  writes.clear();
  const auto writer = [](int, const char* data, size_t len) {
    writes.emplace_back(data, len);
    return 1;
  };
  const std::string invalid_scope("invalid-\xc3\x28", 10);
  GatewayPendingMessageEventBatchFillResult result;
  ASSERT_FALSE(gateway_fill_pending_message_event_batches_with_writer(
      {&session, &session}, {first_id, first_id}, {"player-one", "player-one"}, {10, 10}, writer,
      &result));
  ASSERT_TRUE(writes.empty());
  ASSERT_EQ(gateway_pending_message_event_count(&session, first_id), 1u);
  ASSERT_EQ(gateway_pending_message_event_count(&session, second_id), 1u);

  ASSERT_FALSE(gateway_fill_pending_message_event_batches_with_writer(
      {&session, &session}, {first_id, second_id}, {"player-one", invalid_scope}, {10, 10}, writer,
      &result));
  ASSERT_TRUE(writes.empty());
  ASSERT_EQ(gateway_pending_message_event_count(&session, first_id), 1u);
  ASSERT_EQ(gateway_pending_message_event_count(&session, second_id), 1u);
  ASSERT_FALSE(session.output_fifo[0].ready);
  ASSERT_FALSE(session.output_fifo[1].ready);
}

TEST_F(DriverTest, TestGatewayPendingMessageEventBatchStagesWholeWaveBeforeFirstWrite) {
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
      {&first, &second}, {first_id, second_id}, {301, 401}, {302, 402}, {5, 6}, {3001, 4001},
      stable, "player", 123456, 128));

  static GatewaySession* session_to_release = nullptr;
  static uint64_t reservation_to_release = 0;
  static bool release_on_first_write = false;
  static int release_result = -1;
  static int writes = 0;
  session_to_release = &second;
  reservation_to_release = second_id;
  release_on_first_write = true;
  release_result = -1;
  writes = 0;
  const auto writer = [](int, const char*, size_t) {
    ++writes;
    if (release_on_first_write) {
      release_on_first_write = false;
      release_result = gateway_release_session_output(session_to_release, reservation_to_release);
    }
    return 1;
  };
  GatewayPendingMessageEventBatchFillResult result;

  ASSERT_TRUE(gateway_fill_pending_message_event_batches_with_writer(
      {&first, &second}, {first_id, second_id}, {"player-one", "player-two"}, {9, 10}, writer,
      &result));
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
      {&first, &second}, {first_id, second_id + 1}, {101, 201}, {102, 202}, {3, 4}, {1001, 2001},
      "{\"schema_version\":1,\"text\":\"valid\"}", "player", 123456, 128));
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 0u);
  ASSERT_EQ(gateway_pending_message_event_count(&second, second_id), 0u);

  ASSERT_FALSE(gateway_append_preencoded_message_event_wave(
      {&first, &second}, {first_id, second_id}, {101, 201}, {102, 202}, {3, 4}, {1001, 2001},
      "{\"schema_version\":1,\"text\":", "player", 123456, 128));
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
      {&session}, {reservation_id}, {101}, {102}, {3}, {1001}, stable, "player", 123456, 1));
  ASSERT_FALSE(gateway_append_preencoded_message_event_wave(
      {&session}, {reservation_id}, {103}, {104}, {3}, {1001}, stable, "player", 123457, 1));
  ASSERT_EQ(gateway_pending_message_event_count(&session, reservation_id), 1u);
  ASSERT_EQ(gateway_release_session_output_with_writer(&session, reservation_id,
                                                       [](int, const char*, size_t) -> int {
                                                         return 1;
                                                       }),
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
      {&reused_session}, {existing_id}, {101}, {102}, {3}, {1001}, stable, "player", 123456, 128));

  GatewaySessionBatchReservationResult result;
  ASSERT_TRUE(gateway_reserve_and_append_preencoded_message_event_wave(
      {&reused_session, &new_session}, {existing_id, 0}, 103, {3, 4}, {1002, 2001}, stable,
      "player", 123457, 128, &result));
  ASSERT_EQ(result.reused, (std::vector<bool>{true, false}));
  ASSERT_GT(result.wave_id, 0u);
  ASSERT_EQ(gateway_pending_message_event_count(&reused_session, existing_id), 2u);
  ASSERT_EQ(new_session.output_fifo.size(), 1u);
  ASSERT_EQ(reused_session.output_fifo.front().pending_message_batch->events.back().server_seq,
            104);
  ASSERT_EQ(new_session.output_fifo.front().pending_message_batch->slot_server_seq, 105);
  ASSERT_EQ(new_session.output_fifo.front().pending_message_batch->events.front().server_seq, 106);

  ASSERT_TRUE(gateway_rollback_preencoded_message_event_wave_with_writer(
      {&reused_session, &new_session}, result.reservation_ids, result.reused, result.wave_id,
      [](int, const char*, size_t) -> int {
        return 1;
      }));
  ASSERT_EQ(gateway_pending_message_event_count(&reused_session, existing_id), 1u);
  ASSERT_TRUE(new_session.output_fifo.empty());

  static std::vector<std::string> writes;
  writes.clear();
  ASSERT_EQ(gateway_fill_pending_message_event_batch_with_writer(
                &reused_session, existing_id, "player-one", 7,
                [](int, const char* data, size_t len) {
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
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave({&first}, {first_id}, {101}, {102}, {3},
                                                           {1001}, stable, "player", 123456, 128));

  GatewaySessionBatchReservationResult result;
  ASSERT_TRUE(gateway_reserve_and_append_preencoded_message_event_wave(
      {&first, &second}, {first_id, 0}, 103, {3, 4}, {1002, 2001}, stable, "player", 123457, 128,
      &result));
  ASSERT_EQ(result.reused, (std::vector<bool>{true, false}));

  ASSERT_FALSE(gateway_rollback_preencoded_message_event_wave_with_writer(
      {&first, &second}, result.reservation_ids, {true, true}, result.wave_id,
      [](int, const char*, size_t) -> int {
        return 1;
      }));
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 2u);
  ASSERT_EQ(gateway_pending_message_event_count(&second, result.reservation_ids[1]), 1u);

  ASSERT_FALSE(gateway_rollback_preencoded_message_event_wave_with_writer(
      {&first, &second}, result.reservation_ids, result.reused, result.wave_id + 1,
      [](int, const char*, size_t) -> int {
        return 1;
      }));
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 2u);
  ASSERT_EQ(gateway_pending_message_event_count(&second, result.reservation_ids[1]), 1u);

  ASSERT_FALSE(gateway_rollback_preencoded_message_event_wave_with_writer(
      {&first, &first}, {first_id, first_id}, {true, true}, result.wave_id,
      [](int, const char*, size_t) -> int {
        return 1;
      }));
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 2u);
  ASSERT_EQ(gateway_pending_message_event_count(&second, result.reservation_ids[1]), 1u);

  ASSERT_TRUE(gateway_rollback_preencoded_message_event_wave_with_writer(
      {&first, &second}, result.reservation_ids, result.reused, result.wave_id,
      [](int, const char*, size_t) -> int {
        return 1;
      }));
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
      {&first, &full}, {first_id, 0}, 101, {3, 4}, {1001, 2001}, stable, "player", 123456, 128,
      &result));
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 0u);
  ASSERT_EQ(first.output_fifo.size(), 1u);
  ASSERT_TRUE(full.output_fifo.empty());
  ASSERT_TRUE(result.reservation_ids.empty());
  ASSERT_TRUE(result.reused.empty());

  result.reservation_ids = {999};
  result.reused = {true};
  result.wave_id = 999;
  ASSERT_FALSE(gateway_reserve_and_append_preencoded_message_event_wave(
      {&first}, {first_id}, std::numeric_limits<LPC_INT>::max(), {3}, {1001}, stable, "player",
      123456, 128, &result));
  ASSERT_EQ(gateway_pending_message_event_count(&first, first_id), 0u);
  ASSERT_EQ(first.output_fifo.size(), 1u);
  ASSERT_TRUE(result.reservation_ids.empty());
  ASSERT_TRUE(result.reused.empty());
  ASSERT_EQ(result.wave_id, 0u);

  result.reservation_ids = {999};
  result.reused = {true};
  result.wave_id = 999;
  ASSERT_FALSE(gateway_reserve_and_append_preencoded_message_event_wave(
      {&first}, {first_id}, 101, {3}, {1001}, "{\"schema_version\":1,\"text\":", "player", 123456,
      128, &result));
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
      {&first, &second}, 101, {3, 4}, {1001, 2001}, stable, "player", 123456, 2, 7, &first_wave));
  ASSERT_EQ(first_wave.created_indices, (std::vector<size_t>{0, 1}));
  ASSERT_EQ(first_wave.created_reservation_ids.size(), 2u);
  ASSERT_EQ(first_wave.created_slot_server_seqs, (std::vector<LPC_INT>{101, 103}));
  ASSERT_TRUE(first_wave.full_indices.empty());
  ASSERT_EQ(first_wave.reused_count, 0u);
  ASSERT_GT(first_wave.wave_id, 0u);

  // A newly-created slot cannot be reused until LPC session/FIFO bookkeeping
  // has committed the sparse registration result.
  GatewaySessionCompactMessageEventWaveResult uncommitted_wave;
  ASSERT_FALSE(gateway_reserve_and_append_compact_preencoded_message_event_wave(
      {&first, &second}, 105, {3, 4}, {1002, 2002}, stable, "player", 123457, 2, 7,
      &uncommitted_wave));
  ASSERT_TRUE(
      gateway_commit_compact_preencoded_message_event_wave({&first, &second}, first_wave.wave_id));

  GatewaySessionCompactMessageEventWaveResult reused_wave;
  ASSERT_TRUE(gateway_reserve_and_append_compact_preencoded_message_event_wave(
      {&first, &second}, 105, {3, 4}, {1002, 2002}, stable, "player", 123457, 2, 11, &reused_wave));
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
      {&first, &second}, reused_wave.wave_id, [](int, const char*, size_t) -> int {
        return 1;
      }));
  ASSERT_TRUE(gateway_pending_message_event_batch_status(
      &first, first_wave.created_reservation_ids[0], &status));
  ASSERT_EQ(status.event_count, 1u);
  ASSERT_EQ(status.text_length_total, 7u);
  ASSERT_TRUE(status.registered);
  ASSERT_EQ(first.output_fifo.size(), 1u);
  ASSERT_EQ(second.output_fifo.size(), 1u);
}

TEST_F(DriverTest, TestGatewayPendingMessageEventBatchFillReturnsAuthoritativeTotals) {
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
      {&session}, 101, {3}, {1001}, stable, "player", 123456, 128, 7, &first_wave));
  ASSERT_TRUE(gateway_commit_compact_preencoded_message_event_wave({&session}, first_wave.wave_id));

  GatewaySessionCompactMessageEventWaveResult second_wave;
  ASSERT_TRUE(gateway_reserve_and_append_compact_preencoded_message_event_wave(
      {&session}, 103, {3}, {1002}, stable, "player", 123457, 128, 11, &second_wave));
  ASSERT_TRUE(second_wave.created_indices.empty());
  ASSERT_EQ(second_wave.reused_count, 1u);
  ASSERT_TRUE(
      gateway_commit_compact_preencoded_message_event_wave({&session}, second_wave.wave_id));

  GatewayPendingMessageEventBatchFillResult result;
  ASSERT_TRUE(gateway_fill_pending_message_event_batches_with_writer(
      {&session}, {first_wave.created_reservation_ids[0]}, {"player-one"}, {9},
      [](int, const char*, size_t) -> int {
        return 1;
      },
      &result));
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
      {&first, &second}, 201, {7, 8}, {3001, 4001}, stable, "player", 223456, 128, 7, &wave));
  ASSERT_EQ(wave.created_indices, (std::vector<size_t>{0, 1}));
  ASSERT_EQ(wave.reused_count, 0u);

  GatewayPendingMessageEventBatchStatus status;
  ASSERT_TRUE(
      gateway_pending_message_event_batch_status(&first, wave.created_reservation_ids[0], &status));
  ASSERT_FALSE(status.registered);
  ASSERT_EQ(status.event_count, 1u);
  ASSERT_TRUE(gateway_rollback_compact_preencoded_message_event_wave_with_writer(
      {&first, &second}, wave.wave_id, [](int, const char*, size_t) -> int {
        return 1;
      }));
  ASSERT_TRUE(first.output_fifo.empty());
  ASSERT_TRUE(second.output_fifo.empty());
  ASSERT_FALSE(
      gateway_pending_message_event_batch_status(&first, wave.created_reservation_ids[0], &status));
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
      {&reused}, 301, {9}, {5001}, stable, "player", 323456, 2, 5, &base));
  ASSERT_TRUE(gateway_commit_compact_preencoded_message_event_wave({&reused}, base.wave_id));

  GatewaySessionCompactMessageEventWaveResult mixed;
  ASSERT_TRUE(gateway_reserve_and_append_compact_preencoded_message_event_wave(
      {&reused, &created}, 303, {9, 10}, {5002, 6001}, stable, "player", 323457, 2, 6, &mixed));
  ASSERT_EQ(mixed.created_indices, (std::vector<size_t>{1}));
  ASSERT_EQ(mixed.created_slot_server_seqs, (std::vector<LPC_INT>{305}));
  ASSERT_EQ(mixed.full_indices, (std::vector<size_t>{0}));
  ASSERT_EQ(mixed.reused_count, 1u);

  GatewayPendingMessageEventBatchStatus status;
  ASSERT_FALSE(
      gateway_commit_compact_preencoded_message_event_wave({&reused, &created}, mixed.wave_id + 1));
  ASSERT_FALSE(gateway_rollback_compact_preencoded_message_event_wave_with_writer(
      {&reused, &created}, mixed.wave_id + 1, [](int, const char*, size_t) -> int {
        return 1;
      }));
  ASSERT_TRUE(gateway_pending_message_event_batch_status(&reused, base.created_reservation_ids[0],
                                                         &status));
  ASSERT_EQ(status.event_count, 2u);
  ASSERT_EQ(status.text_length_total, 11u);
  ASSERT_EQ(created.output_fifo.size(), 1u);

  ASSERT_TRUE(
      gateway_commit_compact_preencoded_message_event_wave({&reused, &created}, mixed.wave_id));
  ASSERT_TRUE(gateway_rollback_compact_preencoded_message_event_wave_with_writer(
      {&reused, &created}, mixed.wave_id, [](int, const char*, size_t) -> int {
        return 1;
      }));
  ASSERT_TRUE(gateway_pending_message_event_batch_status(&reused, base.created_reservation_ids[0],
                                                         &status));
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
      stable_children, 7, 101, 123456, "{\"meta\":{\"server_seq\":100,\"stream\":\"message\"}}");

  ASSERT_EQ(frame,
            "\x1bXKBACH{\"messages\":[{\"type\":\"CHAT\",\"payload\":{\"content\":\"first\","
            "\"direction\":\"incoming\",\"meta\":{\"stream\":\"message\",\"server_seq\":101,"
            "\"sent_at\":123456,\"priority\":\"normal\",\"epoch\":7,"
            "\"reliability\":\"important\"}}},{\"type\":\"CHAT\",\"payload\":{"
            "\"content\":\"第二条\",\"direction\":\"incoming\",\"meta\":{\"stream\":\"message\","
            "\"server_seq\":102,\"sent_at\":123456,\"priority\":\"normal\",\"epoch\":7,"
            "\"reliability\":\"important\"}}}],\"meta\":{\"server_seq\":100,"
            "\"stream\":\"message\"}}\x1b\n");
  ASSERT_TRUE(gateway_encode_preencoded_chat_batch_for_test({"[]"}, 7, 101, 123456, "{\"meta\":{}}")
                  .empty());
  ASSERT_TRUE(gateway_encode_preencoded_chat_batch_for_test(stable_children, -1, 101, 123456,
                                                            "{\"meta\":{}}")
                  .empty());
  ASSERT_TRUE(gateway_encode_preencoded_chat_batch_for_test(
                  stable_children, 7, std::numeric_limits<LPC_INT>::max(), 123456, "{\"meta\":{}}")
                  .empty());
  ASSERT_TRUE(
      gateway_encode_preencoded_chat_batch_for_test(stable_children, 7, 101, 123456, "[]").empty());
  ASSERT_TRUE(gateway_encode_preencoded_chat_batch_for_test({"{\"content\":\"bad\",}"}, 7, 101,
                                                            123456, "{\"meta\":{}}")
                  .empty());
  ASSERT_TRUE(gateway_encode_preencoded_chat_batch_for_test({"{\"content\":\"bad\",\"meta\":{}}"},
                                                            7, 101, 123456, "{\"meta\":{}}")
                  .empty());
  ASSERT_TRUE(gateway_encode_preencoded_chat_batch_for_test(stable_children, 7, 101, 123456,
                                                            "{\"messages\":[]}")
                  .empty());
  ASSERT_TRUE(gateway_encode_preencoded_chat_batch_for_test(stable_children, 7, 101, 123456,
                                                            "{\"meta\":\"bad\"}")
                  .empty());
  ASSERT_TRUE(gateway_encode_preencoded_chat_batch_for_test(stable_children, 7, 101, 123456,
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
      stable_children, {"player", "player"}, "observer-7", {11, 12}, {201, 202}, {7, 8},
      {1000, 1001}, 200, 0, 999);

  ASSERT_GE(frame.size(), 8u);
  ASSERT_EQ(frame.substr(0, 7), "\x1bXKBACH");
  ASSERT_EQ(frame.substr(frame.size() - 2), "\x1b\n");
  const auto payload = nlohmann::json::parse(frame.substr(7, frame.size() - 9));
  ASSERT_EQ(payload["meta"], (nlohmann::json{{"server_seq", 200},
                                             {"stream", "system"},
                                             {"epoch", 0},
                                             {"reliability", "important"},
                                             {"priority", "normal"},
                                             {"sent_at", 999}}));
  ASSERT_EQ(payload["messages"].size(), 2u);

  const auto& first = payload["messages"][0];
  ASSERT_EQ(first["type"], "MSGE");
  ASSERT_EQ(first["payload"]["id"], "msg_1000_11");
  ASSERT_EQ(first["payload"]["seq"], 11);
  ASSERT_EQ(first["payload"]["scope"], (nlohmann::json{{"type", "player"}, {"id", "observer-7"}}));
  ASSERT_EQ(first["payload"]["causation_id"], "cmd_11");
  ASSERT_EQ(first["payload"]["correlation_id"], "txn_11");
  ASSERT_EQ(first["payload"]["timestamp"], 1000);
  ASSERT_EQ(first["payload"]["text"], "first");
  ASSERT_EQ(first["payload"]["meta"], (nlohmann::json{{"server_seq", 201},
                                                      {"stream", "message"},
                                                      {"epoch", 7},
                                                      {"reliability", "important"},
                                                      {"priority", "normal"},
                                                      {"sent_at", 1000},
                                                      {"ttl_ms", 30000}}));

  const auto& second = payload["messages"][1]["payload"];
  ASSERT_EQ(second["id"], "fixed-id");
  ASSERT_EQ(second["scope"], (nlohmann::json{{"type", "room"}, {"id", "room-1"}}));
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
      stable_children, {"player"}, "observer-8", {13}, {301}, {9}, {2000}, 300, 0, 1999);

  ASSERT_EQ(frame.substr(0, 7), "\x1bXKMSGE");
  const auto payload = nlohmann::json::parse(frame.substr(7, frame.size() - 9));
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
  const auto encode = [&](const std::vector<std::string>& stable,
                          const std::vector<std::string>& scope_types,
                          const std::vector<LPC_INT>& message_seqs) {
    return gateway_encode_preencoded_message_event_batch_for_test(
        stable, scope_types, "observer-9", message_seqs, {401}, {10}, {3000}, 400, 0, 2999);
  };

  ASSERT_TRUE(encode({"[]"}, {"player"}, {14}).empty());
  ASSERT_TRUE(encode({valid}, {}, {14}).empty());
  ASSERT_TRUE(encode({valid}, {"player"}, {}).empty());
  ASSERT_TRUE(
      encode({valid.substr(0, valid.size() - 1) + ",\"seq\":999}"}, {"player"}, {14}).empty());
  ASSERT_TRUE(encode({valid.substr(0, valid.size() - 1) + ",\"meta\":{\"server_seq\":999}}"},
                     {"player"}, {14})
                  .empty());
  ASSERT_TRUE(gateway_encode_preencoded_message_event_batch_for_test(
                  {valid}, {"player"}, "observer-9", {14}, {401}, {10}, {3000}, 0, 0, 2999)
                  .empty());
}

TEST_F(DriverTest, TestGatewayPreencodedMessageEventTemplateCacheKeepsDynamicMetadataIsolated) {
  const std::string stable =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"normal\",\"reliability\":\"important\","
      "\"display_mode\":\"paced\",\"ttl_ms\":30000,\"collapse_key\":\"\","
      "\"text\":\"shared room fact\",\"payload\":{}}";

  gateway_clear_message_event_template_cache_for_test();
  const auto hits_before = g_gateway_runtime_counters.message_event_template_cache_hits.load();
  const auto misses_before = g_gateway_runtime_counters.message_event_template_cache_misses.load();

  const auto first_frame = gateway_encode_preencoded_message_event_batch_for_test(
      {stable}, {"player"}, "observer-a", {11}, {201}, {7}, {1000}, 200, 0, 999);
  const auto second_frame = gateway_encode_preencoded_message_event_batch_for_test(
      {stable}, {"player"}, "observer-b", {12}, {301}, {8}, {2000}, 300, 0, 1999);

  ASSERT_FALSE(first_frame.empty());
  ASSERT_FALSE(second_frame.empty());
  const auto first = nlohmann::json::parse(first_frame.substr(7, first_frame.size() - 9));
  const auto second = nlohmann::json::parse(second_frame.substr(7, second_frame.size() - 9));
  ASSERT_EQ(first["scope"]["id"], "observer-a");
  ASSERT_EQ(first["seq"], 11);
  ASSERT_EQ(first["meta"]["server_seq"], 200);
  ASSERT_EQ(second["scope"]["id"], "observer-b");
  ASSERT_EQ(second["seq"], 12);
  ASSERT_EQ(second["meta"]["server_seq"], 300);
  ASSERT_EQ(g_gateway_runtime_counters.message_event_template_cache_misses.load(),
            misses_before + 1);
  ASSERT_EQ(g_gateway_runtime_counters.message_event_template_cache_hits.load(), hits_before + 1);
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
      {stable}, {scope_type}, scope_id, {15}, {501}, {11}, {4000}, 500, 0, 3999);

  ASSERT_FALSE(frame.empty());
  const auto payload = nlohmann::json::parse(frame.substr(7, frame.size() - 9));
  ASSERT_EQ(payload["scope"]["type"], scope_type);
  ASSERT_EQ(payload["scope"]["id"], scope_id);
  ASSERT_EQ(payload["text"], "shared room fact");
}

TEST_F(DriverTest,
       TestGatewayPreencodedMessageEventTemplateCacheRejectsInvalidAndBypassesOversizedInput) {
  const std::string stable_prefix =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"normal\",\"reliability\":\"important\","
      "\"display_mode\":\"paced\",\"ttl_ms\":30000,\"collapse_key\":\"\","
      "\"text\":\"";
  const std::string stable_suffix = "\",\"payload\":{}}";
  const std::string invalid = stable_prefix + "invalid\"} ";
  const std::string oversized = stable_prefix + std::string(70 * 1024, 'x') + stable_suffix;

  gateway_clear_message_event_template_cache_for_test();
  const auto hits_before = g_gateway_runtime_counters.message_event_template_cache_hits.load();
  const auto misses_before = g_gateway_runtime_counters.message_event_template_cache_misses.load();
  const auto bypasses_before =
      g_gateway_runtime_counters.message_event_template_cache_bypasses.load();
  const auto encode = [](const std::string& stable) {
    return gateway_encode_preencoded_message_event_batch_for_test(
        {stable}, {"player"}, "observer-cache", {14}, {401}, {10}, {3000}, 400, 0, 2999);
  };

  ASSERT_TRUE(encode(invalid).empty());
  ASSERT_TRUE(encode(invalid).empty());
  ASSERT_FALSE(encode(oversized).empty());
  ASSERT_FALSE(encode(oversized).empty());
  ASSERT_EQ(g_gateway_runtime_counters.message_event_template_cache_hits.load(), hits_before);
  ASSERT_EQ(g_gateway_runtime_counters.message_event_template_cache_misses.load(),
            misses_before + 2);
  ASSERT_EQ(g_gateway_runtime_counters.message_event_template_cache_bypasses.load(),
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
  for (const auto& stable : stable_events) {
    ASSERT_FALSE(
        gateway_encode_preencoded_message_event_batch_for_test(
            {stable}, {"player"}, "observer-bound", {16}, {601}, {12}, {5000}, 600, 0, 4999)
            .empty());
  }
  ASSERT_EQ(g_gateway_runtime_counters.message_event_template_cache_evictions.load(),
            evictions_before + 256);

  const auto hits_before = g_gateway_runtime_counters.message_event_template_cache_hits.load();
  ASSERT_FALSE(gateway_encode_preencoded_message_event_batch_for_test(
                   {stable_events.back()}, {"player"}, "observer-bound", {17}, {602}, {12}, {5001},
                   601, 0, 5000)
                   .empty());
  ASSERT_EQ(g_gateway_runtime_counters.message_event_template_cache_hits.load(), hits_before + 1);
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
  const auto event_base_source = apply_source.substr(event_base_pos, fallback_pos - event_base_pos);
  ASSERT_EQ(event_base_source.find("gateway_drain_owner_main_tasks_later"), std::string::npos);
  ASSERT_EQ(event_base_source.find("gateway_drain_owner_main_tasks_now"), std::string::npos);
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
  ASSERT_NE(helper_source.find("gateway_drain_owner_main_tasks_with_budget("), std::string::npos);
  ASSERT_NE(helper_source.find("kGatewayReadBatchMainDrainBudget"), std::string::npos);
  ASSERT_NE(helper_source.find("kGatewayReadBatchMainDrainWallBudget"), std::string::npos);
  ASSERT_NE(helper_source.find("drain_result.remaining_main_tasks > 0"), std::string::npos);
  ASSERT_NE(helper_source.find("gateway_drain_owner_main_tasks_later()"), std::string::npos);
  ASSERT_NE(
      source.find("kGatewayDeferredMainDrainContinuationDelay = std::chrono::milliseconds(1)"),
      std::string::npos);
  ASSERT_NE(source.find("gateway_deferred_main_drain_wall_budget(backlog_before)"),
            std::string::npos);
  ASSERT_NE(source.find("kGatewayDeferredMainDrainBaseWallBudget = std::chrono::milliseconds(8)"),
            std::string::npos);
  ASSERT_NE(
      source.find("kGatewayDeferredMainDrainBacklogWallBudget = std::chrono::milliseconds(8)"),
      std::string::npos);
  ASSERT_NE(source.find("kGatewayDeferredMainDrainBacklogThreshold = 64"), std::string::npos);
  ASSERT_NE(source.find("kGatewayDeferredMainDrainWaitTimerQueueOnly = 1"), std::string::npos);

  const auto deferred_pos = source.find("void gateway_drain_owner_main_tasks_later()");
  const auto deferred_end =
      source.find("\nvoid gateway_service_admitted_receive_tasks", deferred_pos);
  ASSERT_NE(deferred_pos, std::string::npos);
  ASSERT_NE(deferred_end, std::string::npos);
  const auto deferred_source = source.substr(deferred_pos, deferred_end - deferred_pos);
  ASSERT_NE(deferred_source.find("add_walltime_event("), std::string::npos);
  ASSERT_NE(deferred_source.find("kGatewayDeferredMainDrainContinuationDelay"), std::string::npos);
  ASSERT_NE(deferred_source.find("BackendEventPriority::kGateway"), std::string::npos);
  const auto callback_started =
      deferred_source.find("auto callback_started_at = gateway_now_ns();");
  const auto drain_call = deferred_source.find("gateway_drain_owner_main_tasks_with_budget(");
  const auto wait_sample = deferred_source.find("callback_started_at - scheduled_at");
  ASSERT_NE(callback_started, std::string::npos);
  ASSERT_NE(drain_call, std::string::npos);
  ASSERT_NE(wait_sample, std::string::npos);
  ASSERT_LT(callback_started, drain_call);
  ASSERT_LT(drain_call, wait_sample);
  ASSERT_EQ(deferred_source.find("gateway_now_ns() - scheduled_at"), std::string::npos);

  const auto schedule_pos = source.find("void gateway_schedule_buffered_read(int fd) {");
  const auto readcb_pos = source.find("void gateway_readcb", schedule_pos);
  ASSERT_NE(schedule_pos, std::string::npos);
  ASSERT_NE(readcb_pos, std::string::npos);
  const auto schedule_source = source.substr(schedule_pos, readcb_pos - schedule_pos);
  const auto continuation_dispatch =
      schedule_source.find("gateway_dispatch_buffered_frames(scheduled_master,");
  const auto continuation_service =
      schedule_source.find("gateway_service_admitted_receive_tasks();", continuation_dispatch);
  ASSERT_NE(continuation_dispatch, std::string::npos);
  ASSERT_NE(continuation_service, std::string::npos);

  const auto listener_pos = source.find("\nvoid gateway_listener_cb", readcb_pos);
  ASSERT_NE(listener_pos, std::string::npos);
  const auto readcb_source = source.substr(readcb_pos, listener_pos - readcb_pos);
  const auto read_dispatch =
      readcb_source.find("gateway_dispatch_buffered_frames(master, kGatewayReadFrameBudget)");
  const auto read_service =
      readcb_source.find("gateway_service_admitted_receive_tasks();", read_dispatch);
  ASSERT_NE(read_dispatch, std::string::npos);
  ASSERT_NE(read_service, std::string::npos);

  for (const auto* field : {
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
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto count_pos = source.find("LPC_INT gateway_session_command_pending_count()");
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
  ASSERT_NE(source.find("g_gateway_command_input_pending_sessions.fetch_add("), std::string::npos);
  ASSERT_NE(source.find("g_gateway_command_task_pending_sessions.fetch_add("), std::string::npos);
  ASSERT_NE(count_pos, std::string::npos);
  ASSERT_NE(count_end, std::string::npos);
  const auto count_source = source.substr(count_pos, count_end - count_pos);
  ASSERT_NE(count_source.find("gateway_session_command_input_pending_count()"), std::string::npos);
  ASSERT_NE(count_source.find("gateway_session_command_task_pending_count()"), std::string::npos);
  ASSERT_EQ(count_source.find("g_gateway_sessions"), std::string::npos);

  ASSERT_NE(unbind_pos, std::string::npos);
  ASSERT_NE(unbind_end, std::string::npos);
  const auto unbind_source = source.substr(unbind_pos, unbind_end - unbind_pos);
  ASSERT_NE(unbind_source.find("gateway_release_command_input_pending(sess);"), std::string::npos);
  ASSERT_NE(unbind_source.find("gateway_release_command_task_pending(sess);"), std::string::npos);
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
  const auto header = read_source_file_for_test("../src/packages/gateway/gateway.h");
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway.cc");
  const auto session_source =
      read_source_file_for_test("../src/packages/gateway/gateway_session.cc");

  for (const auto* signature : {
           "LPC_INT gateway_session_command_input_pending_count();",
           "LPC_INT gateway_session_command_task_pending_count();",
           "LPC_INT gateway_session_command_pending_count();",
           "LPC_INT gateway_read_dispatch_pending_count();",
           "LPC_INT gateway_buffered_input_pending_count();",
           "LPC_INT gateway_command_pressure_count();",
       }) {
    EXPECT_NE(header.find(signature), std::string::npos) << signature;
  }
  EXPECT_EQ(header.find("long gateway_session_command_input_pending_count();"), std::string::npos);
  EXPECT_EQ(header.find("long gateway_read_dispatch_pending_count();"), std::string::npos);
  EXPECT_EQ(header.find("long gateway_command_pressure_count();"), std::string::npos);

  EXPECT_NE(session_source.find("std::atomic<LPC_INT> g_gateway_command_input_pending_sessions{0}"),
            std::string::npos);
  EXPECT_NE(session_source.find("std::atomic<LPC_INT> g_gateway_command_task_pending_sessions{0}"),
            std::string::npos);
  EXPECT_NE(
      session_source.find("void gateway_decrement_pending_counter(std::atomic<LPC_INT> &counter)"),
      std::string::npos);
  for (const auto* signature : {
           "LPC_INT gateway_session_command_input_pending_count()",
           "LPC_INT gateway_session_command_task_pending_count()",
           "LPC_INT gateway_session_command_pending_count()",
       }) {
    EXPECT_NE(session_source.find(signature), std::string::npos) << signature;
  }
  EXPECT_EQ(session_source.find("std::atomic<long> g_gateway_command_input_pending_sessions"),
            std::string::npos);
  EXPECT_EQ(session_source.find("long gateway_session_command_pending_count()"), std::string::npos);

  EXPECT_NE(source.find("std::atomic<LPC_INT> g_gateway_read_dispatch_pending_masters{0}"),
            std::string::npos);
  EXPECT_NE(source.find("LPC_INT gateway_main_queue_read_paused_count()"), std::string::npos);
  EXPECT_NE(source.find("LPC_INT gateway_read_dispatch_pending_count()"), std::string::npos);
  EXPECT_NE(source.find("LPC_INT gateway_buffered_input_pending_count()"), std::string::npos);
  EXPECT_NE(source.find("LPC_INT gateway_command_pressure_count()"), std::string::npos);
  EXPECT_NE(source.find("LPC_INT pending = 0;"), std::string::npos);
  EXPECT_EQ(source.find("std::atomic<long> g_gateway_read_dispatch_pending_masters"),
            std::string::npos);
  EXPECT_EQ(source.find("long gateway_buffered_input_pending_count()"), std::string::npos);
  EXPECT_EQ(source.find("long gateway_command_pressure_count()"), std::string::npos);
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
  ASSERT_NE(status_source.find("gateway_schedule_status_response(fd, msg)"), std::string::npos);
  ASSERT_EQ(status_source.find("gateway_status_to_json(&status)"), std::string::npos);
}

TEST_F(DriverTest, TestGatewayStatusWireIncludesMudlibRuntimeExtension) {
  constexpr int master_fd = 993;
  bufferevent* pair[2] = {nullptr, nullptr};

  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  ASSERT_EQ(bufferevent_enable(pair[0], EV_WRITE), 0);
  ASSERT_EQ(bufferevent_enable(pair[1], EV_READ), 0);
  ASSERT_NE(gateway_register_master_for_test(master_fd, pair[0]), nullptr);

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      master_fd, R"({"type":"sys","action":"status","ts":1700000000123456})"));

  auto* input = bufferevent_get_input(pair[1]);
  ASSERT_NE(input, nullptr);
  for (int attempt = 0; attempt < 16 && evbuffer_get_length(input) == 0; ++attempt) {
    event_base_loop(g_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK);
  }
  const auto framed_size = evbuffer_get_length(input);
  ASSERT_GT(framed_size, sizeof(uint32_t));
  std::string framed(framed_size, '\0');
  ASSERT_EQ(evbuffer_copyout(input, framed.data(), framed.size()), static_cast<int>(framed.size()));
  uint32_t network_length = 0;
  memcpy(&network_length, framed.data(), sizeof(network_length));
  const auto payload_length = ntohl(network_length);
  ASSERT_EQ(payload_length, framed.size() - sizeof(network_length));
  const auto response =
      nlohmann::json::parse(framed.substr(sizeof(network_length), payload_length));
  ASSERT_EQ(response.at("type"), "sys");
  ASSERT_EQ(response.at("action"), "status");
  ASSERT_EQ(response.at("ts"), 1700000000123456);
  const auto& data = response.at("data");
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
  bufferevent* pair[2] = {nullptr, nullptr};
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  ASSERT_EQ(bufferevent_enable(pair[0], EV_WRITE), 0);
  ASSERT_EQ(bufferevent_enable(pair[1], EV_READ), 0);
  native_master.bev = pair[0];
  ASSERT_EQ(bufferevent_enable(pair[0], EV_READ), 0);
  ASSERT_EQ(bufferevent_enable(pair[1], EV_WRITE), 0);
  auto* native_input = bufferevent_get_input(pair[0]);
  ASSERT_NE(native_input, nullptr);
  const auto pump_until = [&](size_t expected) {
    for (int attempt = 0; attempt < 8 && evbuffer_get_length(native_input) < expected; ++attempt) {
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
