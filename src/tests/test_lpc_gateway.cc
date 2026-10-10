#include "test_lpc_support.h"

namespace {

svalue_t *call_lpc_method(object_t *ob, const char *method, int num_args = 0) {
  save_command_giver(ob);
  set_eval(max_eval_cost);
  auto *ret = safe_apply(method, ob, num_args, ORIGIN_DRIVER);
  restore_command_giver();
  return ret;
}

object_t *create_gateway_session_for_test(const char *session_id, const char *login_file,
                                          int master_fd = -1) {
  svalue_t data{};
  data.type = T_MAPPING;
  data.u.map = allocate_mapping(1);
  add_mapping_string(data.u.map, "ip", "127.0.0.1");
  copy_and_push_string(login_file);
  safe_apply("set_test_login_ob", master_ob, 1, ORIGIN_DRIVER);
  auto *ob = gateway_create_session_internal(session_id, &data, "127.0.0.1", 6040, master_fd);
  safe_apply("reset_test_login_ob", master_ob, 0, ORIGIN_DRIVER);
  free_svalue(&data, "create_gateway_session_for_test");
  return ob;
}

TEST_F(DriverTest, TestGatewayQueryIpNumberUsesForwardedClientAddress) {
  const char *session_id = "gw-test-query-ip-number";
  auto *ob = create_gateway_session_for_test(session_id, "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayQueryIpNumberUsesForwardedClientAddress");

  const char *address = query_ip_number(ob);
  EXPECT_NE(address, nullptr);
  if (address) {
    EXPECT_STREQ(address, "127.0.0.1");
    free_string(address);
  }

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayQueryIpNumberUsesForwardedClientAddress");
}

TEST_F(DriverTest, TestGatewayMasterSessionFifoBytesAreAggregatelyBounded) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() { g_gateway_max_packet_size = original; }
  } packet_size_guard;

  g_gateway_max_packet_size = 1024;
  constexpr int master_fd = 1709;
  const char *first_session_id = "gw-test-master-fifo-budget-first";
  const char *second_session_id = "gw-test-master-fifo-budget-second";
  bufferevent *pair[2] = {nullptr, nullptr};
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  ASSERT_NE(gateway_register_master_for_test(master_fd, pair[0]), nullptr);

  auto *first_ob = create_gateway_session_for_test(
      first_session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(first_ob, nullptr);
  add_ref(first_ob, "TestGatewayMasterSessionFifoBytesAreAggregatelyBoundedFirst");
  auto *second_ob = create_gateway_session_for_test(
      second_session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(second_ob, nullptr);
  add_ref(second_ob,
          "TestGatewayMasterSessionFifoBytesAreAggregatelyBoundedSecond");
  auto *first = gateway_find_session(first_session_id);
  auto *second = gateway_find_session(second_session_id);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);

  const auto aggregate_limit = gateway_write_buffer_limit_for_test();
  const auto aggregate_rejected_before =
      g_gateway_runtime_counters.output_fifo_aggregate_wire_bytes_rejected.load(
          std::memory_order_relaxed);
  const std::string payload(512, 'x');
  bool rejected = false;
  for (int attempt = 0; attempt < 4096; ++attempt) {
    auto *session = attempt % 2 == 0 ? first : second;
    if (!gateway_enqueue_session_protocol_output(
            session, payload.data(), payload.size())) {
      rejected = true;
      break;
    }
  }

  ASSERT_TRUE(rejected);
  EXPECT_LE(first->output_fifo_wire_bytes + second->output_fifo_wire_bytes,
            aggregate_limit);
  EXPECT_EQ(
      g_gateway_runtime_counters.output_fifo_aggregate_wire_bytes_rejected.load(
          std::memory_order_relaxed),
      aggregate_rejected_before + 1);

  ASSERT_EQ(gateway_destroy_session_internal(first_session_id, "test_done", "done"),
            1);
  destruct_object(first_ob);
  free_object(
      &first_ob,
      "TestGatewayMasterSessionFifoBytesAreAggregatelyBoundedFirst");
  ASSERT_EQ(
      gateway_destroy_session_internal(second_session_id, "test_done", "done"),
      1);
  destruct_object(second_ob);
  free_object(
      &second_ob,
      "TestGatewayMasterSessionFifoBytesAreAggregatelyBoundedSecond");
  gateway_remove_master_for_test(master_fd);
  pair[0] = nullptr;
  bufferevent_free(pair[1]);
}

TEST_F(DriverTest,
       TestGatewayDetachedSessionFifoBytesRemainAggregatelyBounded) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() { g_gateway_max_packet_size = original; }
  } packet_size_guard;

  g_gateway_max_packet_size = 1024;
  constexpr int first_master_fd = 1711;
  constexpr int second_master_fd = 1712;
  const char *first_session_id = "gw-test-detached-fifo-budget-first";
  const char *second_session_id = "gw-test-detached-fifo-budget-second";
  auto *first_ob = create_gateway_session_for_test(
      first_session_id, "/clone/gateway_login_example", first_master_fd);
  ASSERT_NE(first_ob, nullptr);
  add_ref(
      first_ob,
      "TestGatewayDetachedSessionFifoBytesRemainAggregatelyBoundedFirst");
  auto *second_ob = create_gateway_session_for_test(
      second_session_id, "/clone/gateway_login_example", second_master_fd);
  ASSERT_NE(second_ob, nullptr);
  add_ref(
      second_ob,
      "TestGatewayDetachedSessionFifoBytesRemainAggregatelyBoundedSecond");
  auto *first = gateway_find_session(first_session_id);
  auto *second = gateway_find_session(second_session_id);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);

  const auto aggregate_limit = gateway_write_buffer_limit_for_test();
  const std::string payload(512, 'x');
  while (gateway_enqueue_session_protocol_output(
      first, payload.data(), payload.size())) {
    EXPECT_LE(first->output_fifo_wire_bytes, aggregate_limit);
  }
  while (gateway_enqueue_session_protocol_output(
      second, payload.data(), payload.size())) {
    EXPECT_LE(second->output_fifo_wire_bytes, aggregate_limit);
  }
  ASSERT_GT(first->output_fifo_wire_bytes, aggregate_limit / 2);
  ASSERT_GT(second->output_fifo_wire_bytes, aggregate_limit / 2);
  EXPECT_EQ(gateway_session_fifo_wire_detached_bytes(), 0u);
  const auto rebucket_dropped_before =
      g_gateway_runtime_counters.output_fifo_rebucket_wire_bytes_dropped.load(
          std::memory_order_relaxed);

  gateway_cleanup_master_sessions(first_master_fd);
  EXPECT_EQ(gateway_session_fifo_wire_detached_bytes(),
            first->output_fifo_wire_bytes);
  gateway_cleanup_master_sessions(second_master_fd);
  EXPECT_LE(gateway_session_fifo_wire_detached_bytes(), aggregate_limit);
  EXPECT_GT(
      g_gateway_runtime_counters.output_fifo_rebucket_wire_bytes_dropped.load(
          std::memory_order_relaxed),
      rebucket_dropped_before);

  ASSERT_EQ(
      gateway_destroy_session_internal(first_session_id, "test_done", "done"),
      1);
  destruct_object(first_ob);
  free_object(
      &first_ob,
      "TestGatewayDetachedSessionFifoBytesRemainAggregatelyBoundedFirst");
  ASSERT_EQ(gateway_destroy_session_internal(
                second_session_id, "test_done", "done"),
            1);
  destruct_object(second_ob);
  free_object(
      &second_ob,
      "TestGatewayDetachedSessionFifoBytesRemainAggregatelyBoundedSecond");
  EXPECT_EQ(gateway_session_fifo_wire_detached_bytes(), 0u);
}

TEST_F(DriverTest,
       TestGatewayProjectedWireBatchRejectsAggregateBudgetAtomically) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() { g_gateway_max_packet_size = original; }
  } packet_size_guard;
  static std::vector<std::string> writes;
  writes.clear();
  auto writer = [](int, const char *data, size_t len) -> int {
    writes.emplace_back(data, len);
    return 1;
  };

  g_gateway_max_packet_size = 1024;
  constexpr int master_fd = 1713;
  const char *prefill_session_id = "gw-test-aggregate-batch-prefill";
  const char *first_session_id = "gw-test-aggregate-batch-first";
  const char *second_session_id = "gw-test-aggregate-batch-second";
  auto *prefill_ob = create_gateway_session_for_test(
      prefill_session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(prefill_ob, nullptr);
  add_ref(prefill_ob,
          "TestGatewayProjectedWireBatchRejectsAggregateBudgetPrefill");
  auto *first_ob = create_gateway_session_for_test(
      first_session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(first_ob, nullptr);
  add_ref(first_ob,
          "TestGatewayProjectedWireBatchRejectsAggregateBudgetFirst");
  auto *second_ob = create_gateway_session_for_test(
      second_session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(second_ob, nullptr);
  add_ref(second_ob,
          "TestGatewayProjectedWireBatchRejectsAggregateBudgetSecond");
  auto *prefill = gateway_find_session(prefill_session_id);
  auto *first = gateway_find_session(first_session_id);
  auto *second = gateway_find_session(second_session_id);
  ASSERT_NE(prefill, nullptr);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);

  const auto first_reservation = gateway_reserve_session_output(first);
  const auto second_reservation = gateway_reserve_session_output(second);
  ASSERT_GT(first_reservation, 0u);
  ASSERT_GT(second_reservation, 0u);
  const std::string first_payload(256, 'a');
  const std::string second_payload(256, 'b');
  const auto first_wire = gateway_encode_output_envelope_for_test(
      first_session_id, first_payload.data(), first_payload.size());
  const auto second_wire = gateway_encode_output_envelope_for_test(
      second_session_id, second_payload.data(), second_payload.size());
  const auto largest_batch_wire = std::max(first_wire.size(), second_wire.size());
  const auto batch_wire_bytes = first_wire.size() + second_wire.size();
  const auto aggregate_limit = gateway_write_buffer_limit_for_test();
  const auto target_remaining = largest_batch_wire + 1;
  ASSERT_LT(target_remaining, batch_wire_bytes);
  ASSERT_LT(batch_wire_bytes, aggregate_limit);

  const std::string filler_payload(800, 'f');
  const auto filler_wire = gateway_encode_output_envelope_for_test(
      prefill_session_id, filler_payload.data(), filler_payload.size());
  const auto target_prefill = aggregate_limit - target_remaining;
  while (prefill->output_fifo_wire_bytes + filler_wire.size() <=
         target_prefill) {
    ASSERT_EQ(gateway_enqueue_session_protocol_output(
                  prefill, filler_payload.data(), filler_payload.size()),
              1);
  }
  const auto minimum_wire = gateway_encode_output_envelope_for_test(
      prefill_session_id, "", 0);
  const auto padding_wire_bytes =
      target_prefill - prefill->output_fifo_wire_bytes;
  if (padding_wire_bytes >= minimum_wire.size()) {
    const std::string padding_payload(
        padding_wire_bytes - minimum_wire.size(), 'p');
    ASSERT_EQ(gateway_enqueue_session_protocol_output(
                  prefill, padding_payload.data(), padding_payload.size()),
              1);
  }
  const auto remaining =
      aggregate_limit - prefill->output_fifo_wire_bytes;
  ASSERT_GT(remaining, largest_batch_wire);
  ASSERT_LT(remaining, batch_wire_bytes);

  const auto aggregate_rejected_before =
      g_gateway_runtime_counters.output_fifo_aggregate_wire_bytes_rejected.load(
          std::memory_order_relaxed);
  ASSERT_FALSE(gateway_fill_projected_wires_for_test(
      {first, second}, {first_reservation, second_reservation},
      {first_wire, second_wire}, writer));
  EXPECT_TRUE(writes.empty());
  EXPECT_FALSE(first->output_fifo.front().ready);
  EXPECT_TRUE(first->output_fifo.front().wire_bytes.empty());
  EXPECT_FALSE(second->output_fifo.front().ready);
  EXPECT_TRUE(second->output_fifo.front().wire_bytes.empty());
  EXPECT_EQ(first->output_fifo_wire_bytes, 0u);
  EXPECT_EQ(second->output_fifo_wire_bytes, 0u);
  EXPECT_EQ(
      g_gateway_runtime_counters.output_fifo_aggregate_wire_bytes_rejected.load(
          std::memory_order_relaxed),
      aggregate_rejected_before + 1);

  ASSERT_EQ(gateway_destroy_session_internal(
                prefill_session_id, "test_done", "done"),
            1);
  destruct_object(prefill_ob);
  free_object(
      &prefill_ob,
      "TestGatewayProjectedWireBatchRejectsAggregateBudgetPrefill");
  ASSERT_EQ(
      gateway_destroy_session_internal(first_session_id, "test_done", "done"),
      1);
  destruct_object(first_ob);
  free_object(&first_ob,
              "TestGatewayProjectedWireBatchRejectsAggregateBudgetFirst");
  ASSERT_EQ(gateway_destroy_session_internal(
                second_session_id, "test_done", "done"),
            1);
  destruct_object(second_ob);
  free_object(&second_ob,
              "TestGatewayProjectedWireBatchRejectsAggregateBudgetSecond");
}

TEST_F(DriverTest, TestGatewayMasterWriteCallbackRetriesBackpressuredSessionFifo) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() { g_gateway_max_packet_size = original; }
  } packet_size_guard;

  g_gateway_max_packet_size = 1024;
  constexpr int master_fd = 1709;
  const char *session_id = "gw-test-write-backpressure-retry";
  bufferevent *pair[2] = {nullptr, nullptr};
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  ASSERT_NE(gateway_register_master_for_test(master_fd, pair[0]), nullptr);
  auto *ob = create_gateway_session_for_test(
      session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(ob, nullptr);
  add_ref(ob,
          "TestGatewayMasterWriteCallbackRetriesBackpressuredSessionFifo");
  auto *sess = gateway_find_session(session_id);
  ASSERT_NE(sess, nullptr);
  const std::string payload(512, 'x');

  for (int attempt = 0; attempt < 1024 && sess->output_fifo.empty(); ++attempt) {
    ASSERT_EQ(gateway_enqueue_session_protocol_output(
                  sess, payload.data(), payload.size()),
              1);
  }
  ASSERT_FALSE(sess->output_fifo.empty());
  ASSERT_TRUE(sess->output_fifo.front().ready);
  ASSERT_LE(evbuffer_get_length(bufferevent_get_output(pair[0])),
            gateway_write_buffer_limit_for_test());

  ASSERT_EQ(bufferevent_enable(pair[0], EV_WRITE), 0);
  ASSERT_EQ(bufferevent_enable(pair[1], EV_READ), 0);
  auto *peer_input = bufferevent_get_input(pair[1]);
  ASSERT_NE(peer_input, nullptr);
  for (int attempt = 0; attempt < 256 && !sess->output_fifo.empty(); ++attempt) {
    event_base_loop(g_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK);
    const auto received = evbuffer_get_length(peer_input);
    if (received > 0) {
      ASSERT_EQ(evbuffer_drain(peer_input, received), 0);
    }
  }
  ASSERT_TRUE(sess->output_fifo.empty());

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(
      &ob, "TestGatewayMasterWriteCallbackRetriesBackpressuredSessionFifo");
  gateway_remove_master_for_test(master_fd);
  pair[0] = nullptr;
  bufferevent_free(pair[1]);
}

TEST_F(DriverTest,
       TestGatewayMasterWriteCallbackDoesNotRescheduleWithoutProgress) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() { g_gateway_max_packet_size = original; }
  } packet_size_guard;

  g_gateway_max_packet_size = 1024;
  constexpr int master_fd = 1710;
  const char *session_id = "gw-test-write-no-progress";
  bufferevent *pair[2] = {nullptr, nullptr};
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  auto *master = gateway_register_master_for_test(master_fd, pair[0]);
  ASSERT_NE(master, nullptr);
  auto *output = bufferevent_get_output(pair[0]);
  ASSERT_NE(output, nullptr);
  const auto output_limit = gateway_write_buffer_limit_for_test();
  ASSERT_GT(output_limit, 32u);
  const std::string filler(output_limit - 32, 'f');
  ASSERT_EQ(evbuffer_add(output, filler.data(), filler.size()), 0);

  auto *ob = create_gateway_session_for_test(
      session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(ob, nullptr);
  add_ref(ob,
          "TestGatewayMasterWriteCallbackDoesNotRescheduleWithoutProgress");
  auto *sess = gateway_find_session(session_id);
  ASSERT_NE(sess, nullptr);
  const std::string payload(512, 'x');
  ASSERT_EQ(gateway_enqueue_session_protocol_output(
                sess, payload.data(), payload.size()),
            1);
  ASSERT_EQ(sess->output_fifo.size(), 1u);
  ASSERT_TRUE(sess->output_fifo.front().ready);
  ASSERT_FALSE(master->write_flush_scheduled);
  const auto scheduled_before = walltime_event_queue_size_for_test();

  gateway_invoke_master_write_callback_for_test(master);

  ASSERT_EQ(sess->output_fifo.size(), 1u);
  ASSERT_FALSE(master->write_flush_scheduled);
  ASSERT_EQ(walltime_event_queue_size_for_test(), scheduled_before);

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(
      &ob,
      "TestGatewayMasterWriteCallbackDoesNotRescheduleWithoutProgress");
  gateway_remove_master_for_test(master_fd);
  pair[0] = nullptr;
  bufferevent_free(pair[1]);
}

// F10: with budget + N ready sessions, exactly one continuation is recorded
// per flush and the FIFO drains fully in order across scheduled flushes.
TEST_F(DriverTest, TestGatewayMasterOutputContinuationCountsOverBudget) {
  constexpr int master_fd = 1711;
  bufferevent *pair[2] = {nullptr, nullptr};
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  auto *master = gateway_register_master_for_test(master_fd, pair[0]);
  ASSERT_NE(master, nullptr);

  auto before_remaining =
      g_gateway_runtime_counters.master_output_ready_remaining_total.load(
          std::memory_order_relaxed);
  auto before_continuations =
      g_gateway_runtime_counters.master_output_continuation_needed_total.load(
          std::memory_order_relaxed);

  // Create budget + N ready sessions on the same master. FIFO entries are
  // filled through a failing writer so they stay queued until the explicit
  // master flush below (no automatic enqueue-time flush).
  const size_t budget = 8;
  const size_t extra = 4;
  auto failing_writer = [](int /*fd*/, const char* /*data*/, size_t /*len*/) -> int { return 0; };
  std::vector<std::string> session_ids;
  std::vector<object_t *> objects;
  std::vector<GatewaySession *> sessions;
  for (size_t i = 0; i < budget + extra; i++) {
    auto id = "gw-test-continuation-" + std::to_string(i);
    auto *ob = create_gateway_session_for_test(
        id.c_str(), "/clone/gateway_login_example", master_fd);
    ASSERT_NE(ob, nullptr);
    add_ref(ob, "TestGatewayMasterOutputContinuationCountsOverBudget");
    auto *sess = gateway_find_session(id.c_str());
    ASSERT_NE(sess, nullptr);
    auto reservation = gateway_reserve_session_output(sess);
    ASSERT_GT(reservation, 0u);
    ASSERT_EQ(gateway_fill_session_protocol_output_with_writer(
                  sess, reservation, "ping", 4, failing_writer),
              1);
    ASSERT_FALSE(sess->output_fifo.empty());
    ASSERT_TRUE(sess->output_fifo.front().ready);
    session_ids.push_back(id);
    objects.push_back(ob);
    sessions.push_back(sess);
  }

  // Flush with a small budget: one continuation is recorded, remaining_ready
  // counts the left-behind sessions.
  const int flushed = gateway_flush_master_output_fifos(master_fd, budget);
  ASSERT_EQ(flushed, static_cast<int>(budget));
  ASSERT_EQ(
      g_gateway_runtime_counters.master_output_ready_remaining_total.load(
          std::memory_order_relaxed),
      before_remaining + extra);
  ASSERT_EQ(
      g_gateway_runtime_counters.master_output_continuation_needed_total.load(
          std::memory_order_relaxed),
      before_continuations + 1);

  // Exactly `extra` sessions still carry their FIFO entries (registry
  // traversal order is not insertion order).
  size_t remaining_fifos = 0;
  for (auto *sess : sessions) {
    if (!sess->output_fifo.empty()) {
      remaining_fifos++;
    }
  }
  ASSERT_EQ(remaining_fifos, extra);
  // A second flush with a big budget drains everything with no continuation.
  const auto continuations_after_first =
      g_gateway_runtime_counters.master_output_continuation_needed_total.load(
          std::memory_order_relaxed);
  const int drained = gateway_flush_master_output_fifos(master_fd, 4096);
  ASSERT_EQ(drained, static_cast<int>(extra));
  for (auto *sess : sessions) {
    ASSERT_TRUE(sess->output_fifo.empty());
  }
  ASSERT_EQ(
      g_gateway_runtime_counters.master_output_continuation_needed_total.load(
          std::memory_order_relaxed),
      continuations_after_first);

  for (size_t i = 0; i < session_ids.size(); i++) {
    ASSERT_EQ(gateway_destroy_session_internal(session_ids[i].c_str(), "test_done", "done"), 1);
    destruct_object(objects[i]);
    free_object(&objects[i], "TestGatewayMasterOutputContinuationCountsOverBudget");
  }
  gateway_remove_master_for_test(master_fd);
  pair[0] = nullptr;
  bufferevent_free(pair[1]);
}

// R2-F10: schedule/coalesce/callback counters each map to one deterministic
// event: a request while a continuation is already scheduled is a coalesced
// request (never a new schedule), and the callback itself is counted
// separately when it actually runs.
TEST_F(DriverTest, TestGatewayMasterOutputContinuationScheduleSemantics) {
  constexpr int master_fd = 1713;
  bufferevent *pair[2] = {nullptr, nullptr};
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  auto *master = gateway_register_master_for_test(master_fd, pair[0]);
  ASSERT_NE(master, nullptr);

  auto load = [](const std::atomic<uint64_t> &counter) -> uint64_t {
    return counter.load(std::memory_order_relaxed);
  };
  const auto b_attempts = load(
      g_gateway_runtime_counters.master_output_continuation_schedule_attempts_total);
  const auto b_scheduled = load(
      g_gateway_runtime_counters.master_output_continuation_scheduled_total);
  const auto b_coalesced = load(
      g_gateway_runtime_counters.master_output_continuation_coalesced_requests_total);
  const auto b_callbacks = load(
      g_gateway_runtime_counters.master_output_continuation_callbacks_executed_total);

  // One ready session with a pending FIFO entry (writer fails so the entry
  // stays queued).
  auto failing_writer = [](int /*fd*/, const char * /*data*/, size_t /*len*/) -> int { return 0; };
  const char *session_id = "gw-test-schedule-semantics";
  auto *ob = create_gateway_session_for_test(
      session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayMasterOutputContinuationScheduleSemantics");
  auto *sess = gateway_find_session(session_id);
  ASSERT_NE(sess, nullptr);
  auto reservation = gateway_reserve_session_output(sess);
  ASSERT_GT(reservation, 0u);
  ASSERT_EQ(gateway_fill_session_protocol_output_with_writer(
                sess, reservation, "ping", 4, failing_writer),
            1);
  ASSERT_FALSE(sess->output_fifo.empty());
  ASSERT_TRUE(sess->output_fifo.front().ready);

  // First request: one attempt, one successful schedule, no coalesce.
  gateway_schedule_write_flush_for_test(master_fd);
  ASSERT_EQ(load(g_gateway_runtime_counters
                     .master_output_continuation_schedule_attempts_total),
            b_attempts + 1);
  ASSERT_EQ(load(g_gateway_runtime_counters
                     .master_output_continuation_scheduled_total),
            b_scheduled + 1);
  ASSERT_EQ(load(g_gateway_runtime_counters
                     .master_output_continuation_coalesced_requests_total),
            b_coalesced);

  // Second request while a continuation is scheduled: coalesced, no new
  // attempt, no new schedule.
  gateway_schedule_write_flush_for_test(master_fd);
  ASSERT_EQ(load(g_gateway_runtime_counters
                     .master_output_continuation_schedule_attempts_total),
            b_attempts + 1);
  ASSERT_EQ(load(g_gateway_runtime_counters
                     .master_output_continuation_scheduled_total),
            b_scheduled + 1);
  ASSERT_EQ(load(g_gateway_runtime_counters
                     .master_output_continuation_coalesced_requests_total),
            b_coalesced + 1);

  // Let the 1ms continuation timer fire: the callback counter moves exactly
  // once (and the scheduled flush runs its scan). Drive the event loop until
  // the callback runs (bounded), then assert the deltas.
  const auto b_runs = load(g_gateway_runtime_counters.master_output_scan_runs_total);
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
  while (load(g_gateway_runtime_counters
                  .master_output_continuation_callbacks_executed_total) <
             b_callbacks + 1 &&
         std::chrono::steady_clock::now() < deadline) {
    event_base_loop(g_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_GE(load(g_gateway_runtime_counters
                     .master_output_continuation_callbacks_executed_total),
            b_callbacks + 1);
  ASSERT_GE(load(g_gateway_runtime_counters.master_output_scan_runs_total),
            b_runs + 1);

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayMasterOutputContinuationScheduleSemantics");
  gateway_remove_master_for_test(master_fd);
  pair[0] = nullptr;
  bufferevent_free(pair[1]);
}

TEST_F(DriverTest, TestGatewayProbeSuppressionIsSessionScopedAndOneShot) {
  auto *ob = create_gateway_session_for_test("gw-test-probe-suppress",
                                             "/clone/gateway_login_example", 89);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayProbeSuppressionIsSessionScopedAndOneShot");

  ASSERT_FALSE(gateway_probe_suppressed_for_object(ob));
  ASSERT_EQ(gateway_probe_suppress_once_for_object(ob), 1);
  ASSERT_TRUE(gateway_probe_suppressed_for_object(ob));
  gateway_probe_finish_suppressed_command_for_object(ob);
  ASSERT_FALSE(gateway_probe_suppressed_for_object(ob));

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-probe-suppress", "test_done", "done"), 1);
  ASSERT_EQ(gateway_probe_suppress_once_for_object(ob), 0);
  ASSERT_FALSE(gateway_probe_suppressed_for_object(ob));

  destruct_object(ob);
  free_object(&ob, "TestGatewayProbeSuppressionIsSessionScopedAndOneShot");
}

TEST_F(DriverTest, TestGatewayInjectInputEfunPreservesArgumentLifetimeAndStack) {
  constexpr const char *kSessionId = "gw-test-inject-input-efun";
  auto *ob = create_gateway_session_for_test(
      kSessionId, "/clone/gateway_login_example", 90);
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  add_ref(ob, "TestGatewayInjectInputEfunPreservesArgumentLifetimeAndStack");

  const std::string input(128, 'x');
  copy_and_push_string(input.c_str());
  auto *result = call_lpc_method(ob, "inject_gateway_input_via_efun", 1);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_NUMBER);
  ASSERT_EQ(result->u.number, 1);

  const auto expected = input + "-efun\n";
  ASSERT_EQ(ob->interactive->text_end,
            static_cast<int>(expected.size()));
  ASSERT_EQ(std::string(ob->interactive->text,
                        static_cast<size_t>(ob->interactive->text_end)),
            expected);

  ASSERT_EQ(gateway_destroy_session_internal(kSessionId, "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(&ob,
              "TestGatewayInjectInputEfunPreservesArgumentLifetimeAndStack");
}

TEST_F(DriverTest, TestGatewayCoreEfunsPreserveReturnStackContracts) {
  constexpr const char *kSessionId = "gw-test-core-efun-stack-contract";
  auto *ob = create_gateway_session_for_test(
      kSessionId, "/clone/gateway_login_example", 91);
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  add_ref(ob, "TestGatewayCoreEfunsPreserveReturnStackContracts");

  auto *pending = call_lpc_method(ob, "query_gateway_pending_counters_via_efun");
  ASSERT_NE(pending, nullptr);
  ASSERT_EQ(pending->type, T_NUMBER);
  ASSERT_GE(pending->u.number, 0);

  auto *listen = call_lpc_method(ob, "listen_gateway_via_efun");
  ASSERT_NE(listen, nullptr);
  ASSERT_EQ(listen->type, T_NUMBER);
  ASSERT_EQ(listen->u.number, 0);

  auto *empty_session =
      call_lpc_method(ob, "create_empty_gateway_session_via_efun");
  ASSERT_NE(empty_session, nullptr);
  ASSERT_EQ(empty_session->type, T_NUMBER);
  ASSERT_EQ(empty_session->u.number, 0);

  auto *set_heartbeat =
      call_lpc_method(ob, "set_gateway_heartbeat_via_efun");
  ASSERT_NE(set_heartbeat, nullptr);
  ASSERT_EQ(set_heartbeat->type, T_NUMBER);
  ASSERT_EQ(set_heartbeat->u.number, 1);

  auto *check_timeout =
      call_lpc_method(ob, "check_gateway_timeout_via_efun");
  ASSERT_NE(check_timeout, nullptr);
  ASSERT_EQ(check_timeout->type, T_NUMBER);
  ASSERT_EQ(check_timeout->u.number, 1);

  auto *non_session = clone_object_for_test("clone/gateway_login_example");
  ASSERT_NE(non_session, nullptr);
  auto *info =
      call_lpc_method(non_session, "gateway_session_info_non_session_via_efun");
  ASSERT_NE(info, nullptr);
  ASSERT_EQ(info->type, T_NUMBER);
  ASSERT_EQ(info->u.number, 0);
  destruct_object_for_test(non_session);

  ASSERT_EQ(gateway_destroy_session_internal(kSessionId, "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayCoreEfunsPreserveReturnStackContracts");
}

LPC_INT gateway_test_mapping_number(mapping_t *map, const char *key) {
  auto *value = find_string_in_mapping(map, key);
  EXPECT_NE(value, nullptr) << key;
  EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER) << key;
  return value && value->type == T_NUMBER ? value->u.number : 0;
}

const char *gateway_test_mapping_string(mapping_t *map, const char *key) {
  auto *value = find_string_in_mapping(map, key);
  EXPECT_NE(value, nullptr) << key;
  EXPECT_EQ(value ? value->type : T_INVALID, T_STRING) << key;
  return value && value->type == T_STRING ? value->u.string : "";
}

LPC_INT gateway_test_call_number(const char *method, object_t *target) {
  auto *ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
  EXPECT_NE(ret, nullptr) << method;
  EXPECT_EQ(ret ? ret->type : T_INVALID, T_NUMBER) << method;
  return ret && ret->type == T_NUMBER ? ret->u.number : -1;
}

const char *gateway_test_call_string(const char *method, object_t *target) {
  auto *ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
  EXPECT_NE(ret, nullptr) << method;
  EXPECT_EQ(ret ? ret->type : T_INVALID, T_STRING) << method;
  return ret && ret->type == T_STRING ? ret->u.string : "";
}

const std::string &gateway_test_owner_room_stable_payload() {
  static const std::string payload =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"owner-room-output\",\"payload\":{}}";
  return payload;
}

bool gateway_test_seed_owner_room_event(GatewaySession *session,
                                        uint64_t reservation_id) {
  return gateway_append_preencoded_message_event_wave(
      {session}, {reservation_id}, {1201}, {1202}, {17}, {8101},
      gateway_test_owner_room_stable_payload(), "player", 222333, 128);
}

bool gateway_test_encode_owner_room_wire(GatewaySession *session,
                                         uint64_t reservation_id,
                                         std::string *wire_bytes,
                                         const std::string &scope_id =
                                             "owner-room-player",
                                         LPC_INT slot_epoch = 19) {
  GatewayPendingMessageEventProjectionSnapshot snapshot;
  GatewayPendingMessageEventProjectionColumns columns;
  return wire_bytes && gateway_snapshot_pending_message_event_batch(
                           session, reservation_id, scope_id, slot_epoch,
                           &snapshot, &columns) &&
      gateway_encode_pending_message_event_projection(snapshot, columns,
                                                      wire_bytes);
}

bool gateway_test_drain_owner_room_mailbox(object_t *target,
                                           const char *task_key) {
  auto drained = std::make_shared<std::atomic<int>>(0);
  if (vm_owner_enqueue_executor_task(
          target, "room_output_projection", task_key,
          [drained] { drained->store(1, std::memory_order_release); }) == 0) {
    return false;
  }
  for (int i = 0; i < 2000 &&
                  drained->load(std::memory_order_acquire) == 0;
       ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return drained->load(std::memory_order_acquire) == 1;
}

}  // namespace

TEST_F(DriverTest, TestGatewayOutputReservationPublicWrappersAreMainOnly) {
  auto *ob = create_gateway_session_for_test("gw-test-reservation-main-only",
                                             "/clone/gateway_login_example", 77);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayOutputReservationPublicWrappersAreMainOnly");

  auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);

  std::atomic<uint64_t> off_main_reservation{1};
  std::atomic<int> off_main_fill{1};
  std::atomic<int> off_main_release{1};
  std::thread worker([&] {
    VMContext worker_context;
    VMContextThreadScope scope(worker_context);
    off_main_reservation.store(gateway_reserve_session_output_for_object(ob),
                               std::memory_order_release);
    off_main_fill.store(gateway_fill_session_output_for_object(ob, reservation_id,
                                                               "stale", 5),
                        std::memory_order_release);
    off_main_release.store(gateway_release_session_output_for_object(ob, reservation_id),
                           std::memory_order_release);
  });
  worker.join();

  ASSERT_EQ(off_main_reservation.load(std::memory_order_acquire), 0u);
  ASSERT_EQ(off_main_fill.load(std::memory_order_acquire), 0);
  ASSERT_EQ(off_main_release.load(std::memory_order_acquire), 0);
  ASSERT_EQ(gateway_release_session_output_for_object(ob, reservation_id), 1);
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-reservation-main-only", "test_done", "done"), 1);

  destruct_object(ob);
  free_object(&ob, "TestGatewayOutputReservationPublicWrappersAreMainOnly");
}

TEST_F(DriverTest, TestGatewayOutputReservationRejectsStaleTokenAfterSessionDestroy) {
  auto *ob = create_gateway_session_for_test("gw-test-reservation-stale",
                                             "/clone/gateway_login_example", 88);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayOutputReservationRejectsStaleTokenAfterSessionDestroy");

  auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-reservation-stale", "test_done", "done"), 1);
  ASSERT_EQ(gateway_fill_session_output_for_object(ob, reservation_id, "late", 4), 0);
  ASSERT_EQ(gateway_release_session_output_for_object(ob, reservation_id), 0);

  destruct_object(ob);
  free_object(&ob, "TestGatewayOutputReservationRejectsStaleTokenAfterSessionDestroy");
}

TEST_F(DriverTest, TestGatewayFutureWatchKeepsPendingFutureAndReservation) {
  auto *ob = create_gateway_session_for_test("gw-test-future-watch-pending",
                                             "/clone/gateway_login_example", 93);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayFutureWatchKeepsPendingFutureAndReservation");
  vm_owner_set_id(ob, "owner/test/gateway/future-pending");

  auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  push_number(41);
  auto *submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_GT(future_id, 0);

  push_number(static_cast<LPC_INT>(reservation_id));
  push_number(future_id);
  push_number(1000);
  auto *watched = call_lpc_method(ob, "watch_gateway_owner_future", 3);
  ASSERT_NE(watched, nullptr);
  ASSERT_EQ(watched->type, T_NUMBER);
  ASSERT_EQ(watched->u.number, 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 1);
  ASSERT_EQ(gateway_process_session_future_watches_at(0), 0);
  ASSERT_EQ(gateway_session_future_watch_count(), 1);

  auto *pending = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_STREQ(gateway_test_mapping_string(pending, "state"), "pending");
  free_mapping(pending);
  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  ASSERT_EQ(session->output_fifo.size(), 1u);
  ASSERT_FALSE(session->output_fifo.front().ready);

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-future-watch-pending", "test_done", "done"), 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_future_cancelled_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_future_cancelled_future_id", ob),
            future_id);
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_future_cancelled_reason", ob),
               "gateway session destroyed");
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_future_cancelled_callback_off_main", ob),
            0);
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_future_cancelled_release_result", ob),
            1);
  auto *consumed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_STREQ(gateway_test_mapping_string(consumed, "state"), "unknown");
  free_mapping(consumed);

  destruct_object(ob);
  free_object(&ob, "TestGatewayFutureWatchKeepsPendingFutureAndReservation");
}

TEST_F(DriverTest, TestGatewayFutureWatchDispatchesCompletedFutureOnMain) {
  auto before_owner_metrics = owner_runtime_metrics_instance().snapshot();
  auto before_gateway_completion_cpu_total =
      g_gateway_runtime_counters.future_watch_main_completion_thread_cpu_ns_total.load(
          std::memory_order_relaxed);
  auto before_gateway_completion_cpu_unavailable =
      g_gateway_runtime_counters.future_watch_main_completion_thread_cpu_unavailable.load(
          std::memory_order_relaxed);
  auto *before_owner = vm_owner_thread_status();
  auto before_owner_queue_samples =
      gateway_test_mapping_number(before_owner, "owner_async_queue_wait_samples");
  auto before_owner_execute_samples =
      gateway_test_mapping_number(before_owner, "owner_async_lpc_execute_samples");
  auto before_owner_completion_samples =
      gateway_test_mapping_number(before_owner, "owner_async_result_completion_samples");
  free_mapping(before_owner);
  auto *before_gateway = gateway_status_internal();
  auto before_reserve_samples =
      gateway_test_mapping_number(before_gateway, "gateway_output_reserve_samples");
  auto before_register_samples =
      gateway_test_mapping_number(before_gateway, "gateway_future_watch_register_samples");
  auto before_terminal_lag_samples =
      gateway_test_mapping_number(before_gateway, "gateway_future_watch_terminal_lag_samples");
  auto before_take_samples =
      gateway_test_mapping_number(before_gateway, "gateway_future_watch_take_samples");
  auto before_callback_samples =
      gateway_test_mapping_number(before_gateway, "gateway_future_watch_callback_samples");
  auto before_end_to_end_samples =
      gateway_test_mapping_number(before_gateway, "gateway_future_watch_end_to_end_samples");
  auto before_completion_notifications = gateway_test_mapping_number(
      before_gateway, "gateway_future_watch_completion_notifications");
  auto before_completion_wakeups = gateway_test_mapping_number(
      before_gateway, "gateway_future_watch_completion_wakeups");
  free_mapping(before_gateway);

  auto *ob = create_gateway_session_for_test("gw-test-future-watch-completed",
                                             "/clone/gateway_login_example", 94);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayFutureWatchDispatchesCompletedFutureOnMain");
  vm_owner_set_id(ob, "owner/test/gateway/future-completed");

  auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  push_number(41);
  auto *submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_GT(future_id, 0);
  ASSERT_EQ(gateway_watch_session_future_for_object(
                ob, reservation_id, static_cast<uint64_t>(future_id), 1000),
            1);

  vm_owner_thread_start(1);
  for (int i = 0; i < 200; i++) {
    auto *future = vm_owner_future_poll(static_cast<uint64_t>(future_id));
    auto completed = std::string(gateway_test_mapping_string(future, "state")) == "completed";
    free_mapping(future);
    if (completed) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  vm_owner_thread_stop();

  for (int i = 0; i < 200; i++) {
    event_base_loop(g_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK);
    if (gateway_test_call_number("query_last_owner_future_reservation_id", ob) ==
        static_cast<long>(reservation_id)) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_callback_off_main", ob), 0);
  auto *future = call_lpc_method(ob, "query_last_owner_future");
  ASSERT_NE(future, nullptr);
  ASSERT_EQ(future->type, T_MAPPING);
  ASSERT_STREQ(gateway_test_mapping_string(future->u.map, "state"), "completed");
  ASSERT_GT(gateway_test_mapping_number(future->u.map, "terminal_at_ns"), 0);
  auto *result = find_string_in_mapping(future->u.map, "result");
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_MAPPING);
  ASSERT_EQ(gateway_test_mapping_number(result->u.map, "value"), 42);
  auto *consumed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_STREQ(gateway_test_mapping_string(consumed, "state"), "unknown");
  free_mapping(consumed);

  auto *after_owner = vm_owner_thread_status();
  ASSERT_GE(gateway_test_mapping_number(after_owner, "owner_async_queue_wait_samples"),
            before_owner_queue_samples + 1);
  ASSERT_GE(gateway_test_mapping_number(after_owner, "owner_async_lpc_execute_samples"),
            before_owner_execute_samples + 1);
  ASSERT_GE(gateway_test_mapping_number(after_owner, "owner_async_result_completion_samples"),
            before_owner_completion_samples + 1);
  auto after_owner_metrics = owner_runtime_metrics_instance().snapshot();
  ASSERT_TRUE(
      after_owner_metrics.owner_async_lpc_execute_thread_cpu_ns_total >
          before_owner_metrics.owner_async_lpc_execute_thread_cpu_ns_total ||
      after_owner_metrics.owner_async_lpc_execute_thread_cpu_unavailable >
          before_owner_metrics.owner_async_lpc_execute_thread_cpu_unavailable);
  ASSERT_TRUE(
      after_owner_metrics.owner_async_result_completion_thread_cpu_ns_total >
          before_owner_metrics.owner_async_result_completion_thread_cpu_ns_total ||
      after_owner_metrics.owner_async_result_completion_thread_cpu_unavailable >
          before_owner_metrics.owner_async_result_completion_thread_cpu_unavailable);
  free_mapping(after_owner);
  auto *after_gateway = gateway_status_internal();
  ASSERT_GE(gateway_test_mapping_number(after_gateway, "gateway_output_reserve_samples"),
            before_reserve_samples + 1);
  ASSERT_GE(gateway_test_mapping_number(after_gateway, "gateway_future_watch_register_samples"),
            before_register_samples + 1);
  ASSERT_GE(gateway_test_mapping_number(after_gateway, "gateway_future_watch_terminal_lag_samples"),
            before_terminal_lag_samples + 1);
  ASSERT_GE(gateway_test_mapping_number(after_gateway, "gateway_future_watch_take_samples"),
            before_take_samples + 1);
  ASSERT_GE(gateway_test_mapping_number(after_gateway, "gateway_future_watch_callback_samples"),
            before_callback_samples + 1);
  ASSERT_GE(gateway_test_mapping_number(after_gateway, "gateway_future_watch_end_to_end_samples"),
            before_end_to_end_samples + 1);
  auto after_completion_notifications = gateway_test_mapping_number(
      after_gateway, "gateway_future_watch_completion_notifications");
  ASSERT_GE(after_completion_notifications, before_completion_notifications + 1);
  ASSERT_GE(gateway_test_mapping_number(after_gateway, "gateway_future_watch_completion_wakeups"),
            before_completion_wakeups + 1);
  ASSERT_TRUE(
      g_gateway_runtime_counters.future_watch_main_completion_thread_cpu_ns_total.load(
          std::memory_order_relaxed) > before_gateway_completion_cpu_total ||
      g_gateway_runtime_counters.future_watch_main_completion_thread_cpu_unavailable.load(
          std::memory_order_relaxed) > before_gateway_completion_cpu_unavailable);
  free_mapping(after_gateway);

  push_number(99);
  auto *unwatched = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(unwatched, nullptr);
  ASSERT_EQ(unwatched->type, T_MAPPING);
  auto unwatched_future_id = gateway_test_mapping_number(unwatched->u.map, "future_id");
  ASSERT_GT(unwatched_future_id, 0);
  vm_owner_thread_start(1);
  for (int i = 0; i < 200; i++) {
    auto *polled = vm_owner_future_poll(static_cast<uint64_t>(unwatched_future_id));
    auto completed = std::string(gateway_test_mapping_string(polled, "state")) == "completed";
    free_mapping(polled);
    if (completed) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  vm_owner_thread_stop();
  auto *after_unwatched = gateway_status_internal();
  ASSERT_EQ(gateway_test_mapping_number(after_unwatched,
                                        "gateway_future_watch_completion_notifications"),
            after_completion_notifications);
  free_mapping(after_unwatched);
  auto *unwatched_consumed = vm_owner_future_take(static_cast<uint64_t>(unwatched_future_id));
  ASSERT_STREQ(gateway_test_mapping_string(unwatched_consumed, "state"), "completed");
  free_mapping(unwatched_consumed);

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-future-watch-completed", "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayFutureWatchDispatchesCompletedFutureOnMain");
}

TEST_F(DriverTest, TestGatewayEncodedOutputFutureFillsReservationWithoutLpcCallback) {
  auto *ob = create_gateway_session_for_test("gw-test-future-output-completed",
                                             "/clone/gateway_login_example", 96);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayEncodedOutputFutureFillsReservationWithoutLpcCallback");
  vm_owner_set_id(ob, "owner/test/gateway/future-output-completed");

  auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  copy_and_push_string("native-frame-payload");
  auto *submitted = call_lpc_method(ob, "submit_gateway_owner_frame_string", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_GT(future_id, 0);
  push_number(static_cast<LPC_INT>(reservation_id));
  push_number(future_id);
  push_number(1000);
  auto *watched = call_lpc_method(ob, "watch_gateway_owner_future_output", 3);
  ASSERT_NE(watched, nullptr);
  ASSERT_EQ(watched->type, T_NUMBER);
  ASSERT_EQ(watched->u.number, 1);

  vm_owner_thread_start(1);
  for (int i = 0; i < 200; i++) {
    auto state = vm_owner_future_state(static_cast<uint64_t>(future_id));
    if (state == VM_OWNER_FUTURE_COMPLETED || state == VM_OWNER_FUTURE_FAILED) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  vm_owner_thread_stop();
  ASSERT_EQ(vm_owner_future_state(static_cast<uint64_t>(future_id)),
            VM_OWNER_FUTURE_COMPLETED);
  std::atomic<int> off_main_take_found{1};
  std::thread off_main_take([&] {
    VMContext worker_context;
    VMContextThreadScope scope(worker_context);
    auto taken = vm_owner_future_take_string(static_cast<uint64_t>(future_id));
    off_main_take_found.store(taken.found ? 1 : 0, std::memory_order_release);
  });
  off_main_take.join();
  ASSERT_EQ(off_main_take_found.load(std::memory_order_acquire), 0);
  ASSERT_EQ(vm_owner_future_state(static_cast<uint64_t>(future_id)),
            VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(gateway_process_session_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            1);

  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  ASSERT_EQ(session->output_fifo.size(), 1u);
  ASSERT_TRUE(session->output_fifo.front().ready);
  const auto wire = nlohmann::json::parse(session->output_fifo.front().wire_bytes);
  ASSERT_EQ(wire["type"], "output");
  ASSERT_EQ(wire["cid"], "gw-test-future-output-completed");
  ASSERT_EQ(wire["data"], "native-frame-payload");
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_reservation_id", ob), 0);
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_output_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_future_output_state", ob),
               "completed");
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_output_callback_off_main", ob),
            0);
  ASSERT_EQ(vm_owner_future_state(static_cast<uint64_t>(future_id)),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-future-output-completed",
                                             "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayEncodedOutputFutureFillsReservationWithoutLpcCallback");
}

TEST_F(DriverTest, TestGatewayEncodedOutputFutureReleasesNonStringResult) {
  auto *ob = create_gateway_session_for_test("gw-test-future-output-non-string",
                                             "/clone/gateway_login_example", 97);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayEncodedOutputFutureReleasesNonStringResult");
  vm_owner_set_id(ob, "owner/test/gateway/future-output-non-string");

  auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  push_number(41);
  auto *submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_EQ(gateway_watch_session_future_output_for_object(
                ob, reservation_id, static_cast<uint64_t>(future_id), 1000),
            1);
  vm_owner_thread_start(1);
  for (int i = 0; i < 200; i++) {
    auto state = vm_owner_future_state(static_cast<uint64_t>(future_id));
    if (state == VM_OWNER_FUTURE_COMPLETED || state == VM_OWNER_FUTURE_FAILED) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  vm_owner_thread_stop();
  ASSERT_EQ(gateway_process_session_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            1);
  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->output_fifo.empty());
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_output_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_future_output_state", ob),
               "released");
  ASSERT_EQ(vm_owner_future_state(static_cast<uint64_t>(future_id)),
            VM_OWNER_FUTURE_UNKNOWN);

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-future-output-non-string",
                                             "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayEncodedOutputFutureReleasesNonStringResult");
}

TEST_F(DriverTest, TestGatewayEncodedOutputFutureTimeoutReleasesReservation) {
  auto *ob = create_gateway_session_for_test("gw-test-future-output-timeout",
                                             "/clone/gateway_login_example", 98);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayEncodedOutputFutureTimeoutReleasesReservation");
  vm_owner_set_id(ob, "owner/test/gateway/future-output-timeout");

  auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  copy_and_push_string("never-completed-frame");
  auto *submitted = call_lpc_method(ob, "submit_gateway_owner_frame_string", 1);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_EQ(gateway_watch_session_future_output_for_object(
                ob, reservation_id, static_cast<uint64_t>(future_id), 1),
            1);
  ASSERT_EQ(gateway_process_session_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            1);
  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->output_fifo.empty());
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_output_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_future_output_state", ob),
               "released");
  ASSERT_EQ(vm_owner_future_state(static_cast<uint64_t>(future_id)),
            VM_OWNER_FUTURE_UNKNOWN);

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-future-output-timeout",
                                             "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayEncodedOutputFutureTimeoutReleasesReservation");
}

TEST_F(DriverTest, TestVmOwnerNativeStringFuturePollReportsFrozenResult) {
  auto *ob = create_gateway_session_for_test(
      "gw-test-native-string-frozen-result", "/clone/gateway_login_example", 110);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestVmOwnerNativeStringFuturePollReportsFrozenResult");
  vm_owner_set_id(ob, "owner/test/gateway/native-string-frozen-result");

  std::atomic<int> projector_runs{0};
  vm_owner_thread_start(1);
  const auto submission = vm_owner_submit_frozen_string_task(
      ob, "room_output_projection", "unit-native-string-frozen-result",
      [&projector_runs](std::string *output) {
        projector_runs.fetch_add(1, std::memory_order_relaxed);
        *output = "native-string-result";
        return true;
      });
  ASSERT_TRUE(submission.queued);
  ASSERT_GT(submission.future_id, 0u);
  for (int index = 0; index < 200 &&
                      vm_owner_future_state(submission.future_id) ==
                          VM_OWNER_FUTURE_PENDING;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(vm_owner_future_state(submission.future_id),
            VM_OWNER_FUTURE_COMPLETED);

  auto *polled = vm_owner_future_poll(submission.future_id);
  ASSERT_EQ(gateway_test_mapping_number(polled, "frozen_result"), 1);
  free_mapping(polled);
  const auto taken = vm_owner_future_take_string(submission.future_id);
  ASSERT_TRUE(taken.found);
  ASSERT_TRUE(taken.consumed);
  ASSERT_TRUE(taken.string_result);
  ASSERT_EQ(taken.value, "native-string-result");
  ASSERT_EQ(projector_runs.load(std::memory_order_relaxed), 1);

  vm_owner_thread_stop();
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-native-string-frozen-result", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(&ob, "TestVmOwnerNativeStringFuturePollReportsFrozenResult");
}

TEST_F(DriverTest, TestVmOwnerRepeatedTimeoutReportsTerminalChangeOnce) {
  const auto future_id = vm_owner_register_compute_future(
      "owner/test/future-terminal-change", 900001,
      "unit-terminal-change", "unit-terminal-change");
  ASSERT_GT(future_id, 0u);

  auto *first = vm_owner_future_timeout(future_id, "unit timeout");
  ASSERT_STREQ(gateway_test_mapping_string(first, "state"), "failed");
  ASSERT_EQ(gateway_test_mapping_number(first, "terminal_changed"), 1);
  free_mapping(first);

  auto *repeated = vm_owner_future_timeout(future_id, "repeated unit timeout");
  ASSERT_STREQ(gateway_test_mapping_string(repeated, "state"), "failed");
  ASSERT_EQ(gateway_test_mapping_number(repeated, "terminal_changed"), 0);
  free_mapping(repeated);

  auto *taken = vm_owner_future_take(future_id);
  ASSERT_EQ(gateway_test_mapping_number(taken, "consumed"), 1);
  free_mapping(taken);
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_UNKNOWN);
}

TEST_F(DriverTest,
       TestVmOwnerQueuedFrozenStringCancellationRemovesTaskBeforeProjection) {
  auto *ob = create_gateway_session_for_test(
      "gw-test-queued-string-cancel", "/clone/gateway_login_example", 109);
  ASSERT_NE(ob, nullptr);
  add_ref(ob,
          "TestVmOwnerQueuedFrozenStringCancellationRemovesTaskBeforeProjection");
  vm_owner_set_id(ob, "owner/test/gateway/queued-string-cancel");

  std::atomic<int> blocker_started{0};
  std::atomic<int> release_blocker{0};
  std::atomic<int> projector_runs{0};
  vm_owner_thread_start(1);
  ASSERT_GT(vm_owner_enqueue_executor_task(
                ob, "room_output_projection", "unit-queued-cancel-blocker",
                [&] {
                  blocker_started.store(1, std::memory_order_release);
                  while (release_blocker.load(std::memory_order_acquire) == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
                }),
            0u);
  for (int index = 0; index < 200 &&
                      blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  auto *before_cancel = vm_owner_thread_status();
  const auto callback_cancelled_before = gateway_test_mapping_number(
      before_cancel, "executor_callback_cancelled_before_dispatch");
  const auto owner_message_cancelled_before = gateway_test_mapping_number(
      before_cancel, "owner_message_cancelled_before_dispatch");
  const auto callback_dropped_before = gateway_test_mapping_number(
      before_cancel, "executor_callback_dropped");
  free_mapping(before_cancel);

  const auto submission = vm_owner_submit_frozen_string_task(
      ob, "room_output_projection", "unit-queued-cancel-projector",
      [&projector_runs](std::string *output) {
        projector_runs.fetch_add(1, std::memory_order_relaxed);
        *output = "must-not-run";
        return true;
      });
  ASSERT_TRUE(submission.queued);
  ASSERT_EQ(vm_owner_future_state(submission.future_id),
            VM_OWNER_FUTURE_PENDING);

  auto *cancelled = vm_owner_future_cancel_queued_task(
      submission.future_id, "unit queued cancellation");
  ASSERT_STREQ(gateway_test_mapping_string(cancelled, "state"), "failed");
  ASSERT_EQ(gateway_test_mapping_number(cancelled, "terminal_changed"), 1);
  ASSERT_EQ(gateway_test_mapping_number(cancelled, "queued_task_removed"), 1);
  ASSERT_STREQ(gateway_test_mapping_string(cancelled, "queued_task_kind"),
               "executor_callback");
  free_mapping(cancelled);
  auto *taken = vm_owner_future_take(submission.future_id);
  ASSERT_EQ(gateway_test_mapping_number(taken, "consumed"), 1);
  free_mapping(taken);

  release_blocker.store(1, std::memory_order_release);
  vm_owner_thread_stop();
  ASSERT_EQ(projector_runs.load(std::memory_order_relaxed), 0);
  ASSERT_EQ(vm_owner_future_state(submission.future_id),
            VM_OWNER_FUTURE_UNKNOWN);
  auto *status = vm_owner_thread_status();
  ASSERT_EQ(gateway_test_mapping_number(status, "queue_depth"), 0);
  ASSERT_EQ(gateway_test_mapping_number(
                status, "executor_callback_cancelled_before_dispatch"),
            callback_cancelled_before + 1);
  ASSERT_EQ(gateway_test_mapping_number(
                status, "owner_message_cancelled_before_dispatch"),
            owner_message_cancelled_before);
  ASSERT_EQ(gateway_test_mapping_number(status, "executor_callback_dropped"),
            callback_dropped_before + 1);
  free_mapping(status);

  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-queued-string-cancel", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(
      &ob,
      "TestVmOwnerQueuedFrozenStringCancellationRemovesTaskBeforeProjection");
}

TEST_F(DriverTest,
       TestVmOwnerQueuedMessageCancellationDoesNotCountAsCallbackDrop) {
  auto *blocker = create_gateway_session_for_test(
      "gw-test-queued-message-blocker", "/clone/gateway_login_example", 130);
  auto *target = create_gateway_session_for_test(
      "gw-test-queued-message-target", "/clone/gateway_login_example", 131);
  ASSERT_NE(blocker, nullptr);
  ASSERT_NE(target, nullptr);
  add_ref(blocker,
          "TestVmOwnerQueuedMessageCancellationDoesNotCountAsCallbackDrop");
  add_ref(target,
          "TestVmOwnerQueuedMessageCancellationDoesNotCountAsCallbackDrop");
  vm_owner_set_id(blocker, "owner/test/gateway/queued-message-blocker");
  vm_owner_set_id(target, "owner/test/gateway/queued-message-target");

  std::atomic<int> blocker_started{0};
  std::atomic<int> release_blocker{0};
  vm_owner_thread_start(1);
  struct OwnerThreadGuard {
    std::atomic<int> &release;
    bool active{true};
    ~OwnerThreadGuard() {
      if (active) {
        release.store(1, std::memory_order_release);
        vm_owner_thread_stop();
      }
    }
    void stop() {
      release.store(1, std::memory_order_release);
      vm_owner_thread_stop();
      active = false;
    }
  } owner_thread_guard{release_blocker};
  ASSERT_GT(vm_owner_enqueue_executor_task(
                blocker, "room_output_projection",
                "unit-queued-message-blocker", [&] {
                  blocker_started.store(1, std::memory_order_release);
                  while (release_blocker.load(std::memory_order_acquire) == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
                }),
            0u);
  for (int index = 0; index < 200 &&
                      blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  auto *before_cancel = vm_owner_thread_status();
  const auto callback_cancelled_before = gateway_test_mapping_number(
      before_cancel, "executor_callback_cancelled_before_dispatch");
  const auto owner_message_cancelled_before = gateway_test_mapping_number(
      before_cancel, "owner_message_cancelled_before_dispatch");
  const auto callback_dropped_before = gateway_test_mapping_number(
      before_cancel, "executor_callback_dropped");
  free_mapping(before_cancel);

  push_number(41);
  auto *submitted = call_lpc_method(target, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  const auto future_id = static_cast<uint64_t>(
      gateway_test_mapping_number(submitted->u.map, "future_id"));
  ASSERT_GT(future_id, 0u);
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_PENDING);

  auto *cancelled = vm_owner_future_cancel_queued_task(
      future_id, "unit queued owner message cancellation");
  ASSERT_STREQ(gateway_test_mapping_string(cancelled, "state"), "failed");
  ASSERT_EQ(gateway_test_mapping_number(cancelled, "terminal_changed"), 1);
  ASSERT_EQ(gateway_test_mapping_number(cancelled, "queued_task_removed"), 1);
  ASSERT_STREQ(gateway_test_mapping_string(cancelled, "queued_task_kind"),
               "owner_message");
  free_mapping(cancelled);
  auto *taken = vm_owner_future_take(future_id);
  ASSERT_EQ(gateway_test_mapping_number(taken, "consumed"), 1);
  free_mapping(taken);

  owner_thread_guard.stop();
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_UNKNOWN);
  auto *status = vm_owner_thread_status();
  ASSERT_EQ(gateway_test_mapping_number(
                status, "executor_callback_cancelled_before_dispatch"),
            callback_cancelled_before);
  ASSERT_EQ(gateway_test_mapping_number(
                status, "owner_message_cancelled_before_dispatch"),
            owner_message_cancelled_before + 1);
  ASSERT_EQ(gateway_test_mapping_number(status, "executor_callback_dropped"),
            callback_dropped_before);
  free_mapping(status);

  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-queued-message-blocker", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-queued-message-target", "test_done", "done"),
            1);
  destruct_object(blocker);
  destruct_object(target);
  free_object(
      &blocker,
      "TestVmOwnerQueuedMessageCancellationDoesNotCountAsCallbackDrop");
  free_object(
      &target,
      "TestVmOwnerQueuedMessageCancellationDoesNotCountAsCallbackDrop");
}

TEST_F(DriverTest,
       TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric) {
  auto *blocker = create_gateway_session_for_test(
      "gw-test-quiesce-all-blocker", "/clone/gateway_login_example", 132);
  auto *mapping_target = create_gateway_session_for_test(
      "gw-test-quiesce-all-mapping", "/clone/gateway_login_example", 133);
  auto *protocol_target = create_gateway_session_for_test(
      "gw-test-quiesce-all-protocol", "/clone/gateway_login_example", 134);
  auto *room_target = create_gateway_session_for_test(
      "gw-test-quiesce-all-room", "/clone/gateway_login_example", 135);
  auto *generic_target = clone_object_for_test("clone/gateway_login_example");
  ASSERT_NE(blocker, nullptr);
  ASSERT_NE(mapping_target, nullptr);
  ASSERT_NE(protocol_target, nullptr);
  ASSERT_NE(room_target, nullptr);
  ASSERT_NE(generic_target, nullptr);
  add_ref(blocker,
          "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  add_ref(mapping_target,
          "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  add_ref(protocol_target,
          "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  add_ref(room_target,
          "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  add_ref(generic_target,
          "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  vm_owner_set_id(blocker, "owner/test/gateway/quiesce-all-blocker");
  vm_owner_set_id(mapping_target, "owner/test/gateway/quiesce-all-mapping");
  vm_owner_set_id(protocol_target, "owner/test/gateway/quiesce-all-protocol");
  vm_owner_set_id(room_target, "owner/test/gateway/quiesce-all-room");
  vm_owner_set_id(generic_target, "owner/test/gateway/quiesce-all-generic");

  auto *mapping_session = gateway_find_session_by_object(mapping_target);
  auto *protocol_session = gateway_find_session_by_object(protocol_target);
  auto *room_session = gateway_find_session_by_object(room_target);
  ASSERT_NE(mapping_session, nullptr);
  ASSERT_NE(protocol_session, nullptr);
  ASSERT_NE(room_session, nullptr);
  const auto mapping_reservation =
      gateway_reserve_session_output_for_object(mapping_target);
  const auto protocol_reservation =
      gateway_reserve_session_output_for_object(protocol_target);
  const auto room_reservation =
      gateway_reserve_session_output_for_object(room_target);
  ASSERT_GT(mapping_reservation, 0u);
  ASSERT_GT(protocol_reservation, 0u);
  ASSERT_GT(room_reservation, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(room_session, room_reservation));

  std::atomic<int> blocker_started{0};
  std::atomic<int> release_blocker{0};
  vm_owner_thread_start(1);
  struct OwnerThreadGuard {
    std::atomic<int> &release;
    bool active{true};
    ~OwnerThreadGuard() {
      if (active) {
        release.store(1, std::memory_order_release);
        vm_owner_thread_stop();
      }
    }
    void stop() {
      release.store(1, std::memory_order_release);
      vm_owner_thread_stop();
      active = false;
    }
  } owner_thread_guard{release_blocker};
  ASSERT_GT(vm_owner_enqueue_executor_task(
                blocker, "room_output_projection", "unit-quiesce-all-blocker",
                [&] {
                  blocker_started.store(1, std::memory_order_release);
                  while (release_blocker.load(std::memory_order_acquire) == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
                }),
            0u);
  for (int index = 0; index < 200 &&
                      blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  push_number(41);
  auto *mapping_submitted =
      call_lpc_method(mapping_target, "submit_gateway_owner_future", 1);
  ASSERT_NE(mapping_submitted, nullptr);
  ASSERT_EQ(mapping_submitted->type, T_MAPPING);
  const auto mapping_future = static_cast<uint64_t>(
      gateway_test_mapping_number(mapping_submitted->u.map, "future_id"));
  ASSERT_GT(mapping_future, 0u);
  ASSERT_EQ(gateway_watch_session_future_for_object(
                mapping_target, mapping_reservation, mapping_future, 1000),
            1);

  copy_and_push_string("quiesce-protocol-frame");
  auto *protocol_submitted = call_lpc_method(
      protocol_target, "submit_gateway_owner_frame_string", 1);
  ASSERT_NE(protocol_submitted, nullptr);
  ASSERT_EQ(protocol_submitted->type, T_MAPPING);
  const auto protocol_future = static_cast<uint64_t>(
      gateway_test_mapping_number(protocol_submitted->u.map, "future_id"));
  ASSERT_GT(protocol_future, 0u);
  ASSERT_EQ(gateway_watch_session_future_output_for_object(
                protocol_target, protocol_reservation, protocol_future, 1000),
            1);

  GatewayPendingMessageEventBatchOwnerSubmitResult room_submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {room_target}, {room_reservation}, {"quiesce-room"}, {23}, 1000,
      &room_submitted));
  ASSERT_EQ(room_submitted.submitted, (std::vector<bool>{true}));
  ASSERT_EQ(room_submitted.future_ids.size(), 1u);
  const auto room_future = room_submitted.future_ids[0];

  push_number(51);
  auto *generic_submitted =
      call_lpc_method(generic_target, "submit_gateway_owner_future", 1);
  ASSERT_NE(generic_submitted, nullptr);
  ASSERT_EQ(generic_submitted->type, T_MAPPING);
  const auto generic_future = static_cast<uint64_t>(
      gateway_test_mapping_number(generic_submitted->u.map, "future_id"));
  ASSERT_GT(generic_future, 0u);
  ASSERT_EQ(gateway_watch_future_for_object(
                generic_target, 901, generic_future, 1000),
            1);
  ASSERT_EQ(gateway_session_future_watch_count(), 3);
  ASSERT_EQ(gateway_future_watch_count(), 1);

  auto *before = vm_owner_thread_status();
  const auto callback_cancelled_before = gateway_test_mapping_number(
      before, "executor_callback_cancelled_before_dispatch");
  const auto owner_message_cancelled_before = gateway_test_mapping_number(
      before, "owner_message_cancelled_before_dispatch");
  const auto callback_dropped_before = gateway_test_mapping_number(
      before, "executor_callback_dropped");
  free_mapping(before);

  auto *quiesced = gateway_owner_output_quiesce("unit quiesce all watches");
  ASSERT_NE(quiesced, nullptr);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "success"), 1);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "session_watches_before"),
            3);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "session_watches_after"),
            0);
  ASSERT_EQ(
      gateway_test_mapping_number(quiesced, "session_reservations_before"),
      3);
  ASSERT_EQ(
      gateway_test_mapping_number(quiesced, "session_reservations_after"),
      0);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "cancelled_futures"), 3);
  free_mapping(quiesced);

  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_future_watch_count(), 1);
  ASSERT_EQ(vm_owner_future_state(mapping_future), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(vm_owner_future_state(protocol_future), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(vm_owner_future_state(room_future), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(vm_owner_future_state(generic_future), VM_OWNER_FUTURE_PENDING);
  ASSERT_TRUE(mapping_session->output_fifo.empty());
  ASSERT_TRUE(protocol_session->output_fifo.empty());
  ASSERT_TRUE(room_session->output_fifo.empty());
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_future_cancelled_reservation_id",
                mapping_target),
            static_cast<long>(mapping_reservation));
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_future_cancelled_future_id",
                mapping_target),
            static_cast<long>(mapping_future));
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_future_cancelled_release_result",
                mapping_target),
            1);
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_future_output_state", protocol_target),
               "released");
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_room_output_state", room_target),
               "released");

  auto *after = vm_owner_thread_status();
  ASSERT_EQ(gateway_test_mapping_number(
                after, "executor_callback_cancelled_before_dispatch"),
            callback_cancelled_before + 1);
  ASSERT_EQ(gateway_test_mapping_number(
                after, "owner_message_cancelled_before_dispatch"),
            owner_message_cancelled_before + 2);
  ASSERT_EQ(gateway_test_mapping_number(after, "executor_callback_dropped"),
            callback_dropped_before + 1);
  free_mapping(after);

  owner_thread_guard.stop();
  ASSERT_EQ(vm_owner_future_state(generic_future), VM_OWNER_FUTURE_PENDING);
  vm_owner_thread_start(1);
  for (int index = 0; index < 200 &&
                      vm_owner_future_state(generic_future) ==
                          VM_OWNER_FUTURE_PENDING;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  vm_owner_thread_stop();
  ASSERT_EQ(vm_owner_future_state(generic_future), VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(gateway_process_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            1);
  ASSERT_EQ(gateway_future_watch_count(), 0);
  ASSERT_EQ(gateway_test_call_number(
                "query_last_generic_owner_future_context_id", generic_target),
            901);
  ASSERT_EQ(vm_owner_future_state(generic_future), VM_OWNER_FUTURE_UNKNOWN);

  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-quiesce-all-blocker", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-quiesce-all-mapping", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-quiesce-all-protocol", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-quiesce-all-room", "test_done", "done"),
            1);
  destruct_object(blocker);
  destruct_object(mapping_target);
  destruct_object(protocol_target);
  destruct_object(room_target);
  destruct_object(generic_target);
  free_object(
      &blocker,
      "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  free_object(
      &mapping_target,
      "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  free_object(
      &protocol_target,
      "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  free_object(
      &room_target,
      "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  free_object(
      &generic_target,
      "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
}

TEST_F(DriverTest,
       TestGatewayMappingFutureStaleOwnerNotifiesCancellationAndClosesReservation) {
  auto *ob = create_gateway_session_for_test(
      "gw-test-mapping-stale-owner", "/clone/gateway_login_example", 136);
  ASSERT_NE(ob, nullptr);
  add_ref(
      ob,
      "TestGatewayMappingFutureStaleOwnerNotifiesCancellationAndClosesReservation");
  vm_owner_set_id(ob, "owner/test/gateway/mapping-stale-a");

  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  push_number(41);
  auto *submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  const auto future_id = static_cast<uint64_t>(
      gateway_test_mapping_number(submitted->u.map, "future_id"));
  ASSERT_GT(future_id, 0u);
  ASSERT_EQ(gateway_watch_session_future_for_object(
                ob, reservation_id, future_id, 1000),
            1);

  vm_owner_set_id(ob, "owner/test/gateway/mapping-stale-b");
  ASSERT_EQ(gateway_process_session_future_watches_at(0), 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_future_cancelled_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_future_cancelled_future_id", ob),
            static_cast<long>(future_id));
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_future_cancelled_reason", ob),
               "gateway session stale");
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_future_cancelled_release_result", ob),
            1);
  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->output_fifo.empty());

  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-mapping-stale-owner", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(
      &ob,
      "TestGatewayMappingFutureStaleOwnerNotifiesCancellationAndClosesReservation");
}

TEST_F(DriverTest,
       TestGatewayMappingFutureCancellationCallbackMayReenterSessionDestroy) {
  constexpr const char *kSessionId = "gw-test-mapping-cancel-reentry";
  auto *ob = create_gateway_session_for_test(
      kSessionId, "/clone/gateway_login_example", 137);
  ASSERT_NE(ob, nullptr);
  add_ref(
      ob,
      "TestGatewayMappingFutureCancellationCallbackMayReenterSessionDestroy");
  vm_owner_set_id(ob, "owner/test/gateway/mapping-cancel-reentry");

  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  push_number(41);
  auto *submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  const auto future_id = static_cast<uint64_t>(
      gateway_test_mapping_number(submitted->u.map, "future_id"));
  ASSERT_GT(future_id, 0u);
  ASSERT_EQ(gateway_watch_session_future_for_object(
                ob, reservation_id, future_id, 1000),
            1);

  copy_and_push_string(kSessionId);
  auto *configured = call_lpc_method(
      ob, "configure_owner_future_cancelled_reentry", 1);
  ASSERT_NE(configured, nullptr);
  auto *quiesced = gateway_owner_output_quiesce(
      "unit mapping cancellation reentry");
  ASSERT_NE(quiesced, nullptr);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "success"), 1);
  free_mapping(quiesced);

  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_future_cancelled_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_future_cancelled_future_id", ob),
            static_cast<long>(future_id));
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_future_cancelled_reentry_result", ob),
            1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_find_session(kSessionId), nullptr);

  destruct_object(ob);
  free_object(
      &ob,
      "TestGatewayMappingFutureCancellationCallbackMayReenterSessionDestroy");
}

TEST_F(DriverTest,
       TestGatewayMappingFutureReservationStaleStillNotifiesCancellation) {
  auto *ob = create_gateway_session_for_test(
      "gw-test-mapping-reservation-stale", "/clone/gateway_login_example",
      138);
  ASSERT_NE(ob, nullptr);
  add_ref(
      ob,
      "TestGatewayMappingFutureReservationStaleStillNotifiesCancellation");
  vm_owner_set_id(ob, "owner/test/gateway/mapping-reservation-stale");

  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  push_number(41);
  auto *submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  const auto future_id = static_cast<uint64_t>(
      gateway_test_mapping_number(submitted->u.map, "future_id"));
  ASSERT_GT(future_id, 0u);
  ASSERT_EQ(gateway_watch_session_future_for_object(
                ob, reservation_id, future_id, 1000),
            1);
  ASSERT_EQ(gateway_release_session_output_for_object(ob, reservation_id), 1);

  ASSERT_EQ(gateway_process_session_future_watches_at(0), 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_future_cancelled_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_future_cancelled_reason", ob),
               "gateway reservation stale");
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_future_cancelled_release_result", ob),
            0);

  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-mapping-reservation-stale", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(
      &ob,
      "TestGatewayMappingFutureReservationStaleStillNotifiesCancellation");
}

TEST_F(DriverTest,
       TestGatewayMappingFutureCompletionCallbackFailureRunsCancellationCleanup) {
  auto *ob = create_gateway_session_for_test(
      "gw-test-mapping-callback-failure", "/clone/gateway_login_example",
      139);
  ASSERT_NE(ob, nullptr);
  add_ref(
      ob,
      "TestGatewayMappingFutureCompletionCallbackFailureRunsCancellationCleanup");
  vm_owner_set_id(ob, "owner/test/gateway/mapping-callback-failure");

  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  push_number(1);
  auto *configured = call_lpc_method(
      ob, "configure_owner_future_completed_failure", 1);
  ASSERT_NE(configured, nullptr);
  push_number(41);
  auto *submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  const auto future_id = static_cast<uint64_t>(
      gateway_test_mapping_number(submitted->u.map, "future_id"));
  ASSERT_GT(future_id, 0u);
  ASSERT_EQ(gateway_watch_session_future_for_object(
                ob, reservation_id, future_id, 1000),
            1);

  vm_owner_thread_start(1);
  for (int index = 0; index < 200 &&
                      vm_owner_future_state(future_id) ==
                          VM_OWNER_FUTURE_PENDING;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  vm_owner_thread_stop();
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(gateway_process_session_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            1);

  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_future_cancelled_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_future_cancelled_reason", ob),
               "gateway owner future callback failed");
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_future_cancelled_release_result", ob),
            1);
  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->output_fifo.empty());

  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-mapping-callback-failure", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(
      &ob,
      "TestGatewayMappingFutureCompletionCallbackFailureRunsCancellationCleanup");
}

TEST_F(DriverTest, TestGatewayOwnerOutputQuiesceClosesStoppedWorkerWave) {
  auto *blocker = create_gateway_session_for_test(
      "gw-test-owner-quiesce-blocker", "/clone/gateway_login_example", 131);
  auto *first = create_gateway_session_for_test(
      "gw-test-owner-quiesce-a", "/clone/gateway_login_example", 132);
  auto *second = create_gateway_session_for_test(
      "gw-test-owner-quiesce-b", "/clone/gateway_login_example", 133);
  ASSERT_NE(blocker, nullptr);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  add_ref(blocker, "TestGatewayOwnerOutputQuiesceClosesStoppedWorkerWave");
  add_ref(first, "TestGatewayOwnerOutputQuiesceClosesStoppedWorkerWave");
  add_ref(second, "TestGatewayOwnerOutputQuiesceClosesStoppedWorkerWave");
  vm_owner_set_id(blocker, "owner/test/gateway/quiesce-blocker");
  vm_owner_set_id(first, "owner/test/gateway/quiesce-a");
  vm_owner_set_id(second, "owner/test/gateway/quiesce-b");
  auto *first_session = gateway_find_session_by_object(first);
  auto *second_session = gateway_find_session_by_object(second);
  ASSERT_NE(first_session, nullptr);
  ASSERT_NE(second_session, nullptr);
  const auto first_reservation = gateway_reserve_session_output_for_object(first);
  const auto second_reservation = gateway_reserve_session_output_for_object(second);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(first_session, first_reservation));
  ASSERT_TRUE(gateway_test_seed_owner_room_event(second_session, second_reservation));

  std::atomic<int> blocker_started{0};
  std::atomic<int> release_blocker{0};
  vm_owner_thread_start(1);
  ASSERT_GT(vm_owner_enqueue_executor_task(
                blocker, "room_output_projection", "unit-quiesce-blocker",
                [&] {
                  blocker_started.store(1, std::memory_order_release);
                  while (release_blocker.load(std::memory_order_acquire) == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
                }),
            0u);
  for (int index = 0; index < 200 &&
                      blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {first, second}, {first_reservation, second_reservation},
      {"owner-quiesce-a", "owner-quiesce-b"}, {21, 22}, 1000,
      &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true, true}));
  ASSERT_EQ(gateway_session_future_watch_count(), 2);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 2);

  std::thread stopper([] { vm_owner_thread_stop(); });
  for (int index = 0; index < 200; ++index) {
    auto *status = vm_owner_thread_status();
    const auto stopping = gateway_test_mapping_number(status, "stopping");
    free_mapping(status);
    if (stopping == 1) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  release_blocker.store(1, std::memory_order_release);
  stopper.join();
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_PENDING);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_PENDING);

  auto *quiesced = call_lpc_method(first, "quiesce_gateway_owner_output");
  ASSERT_NE(quiesced, nullptr);
  ASSERT_EQ(quiesced->type, T_MAPPING);
  ASSERT_EQ(gateway_test_mapping_number(quiesced->u.map, "success"), 1);
  ASSERT_EQ(gateway_test_mapping_number(quiesced->u.map, "room_waves_before"),
            1);
  ASSERT_EQ(gateway_test_mapping_number(quiesced->u.map, "room_pending_before"),
            2);
  ASSERT_EQ(gateway_test_mapping_number(quiesced->u.map, "room_watches_after"),
            0);
  ASSERT_EQ(gateway_test_mapping_number(quiesced->u.map, "room_reservations_after"),
            0);
  ASSERT_EQ(gateway_test_mapping_number(quiesced->u.map, "room_pending_after"),
            0);
  ASSERT_EQ(gateway_test_mapping_number(quiesced->u.map,
                                        "requires_immediate_owner_stop"),
            1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_TRUE(first_session->output_fifo.empty());
  ASSERT_TRUE(second_session->output_fifo.empty());
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_UNKNOWN);
  auto *status = vm_owner_thread_status();
  ASSERT_EQ(gateway_test_mapping_number(status, "queue_depth"), 0);
  free_mapping(status);

  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-quiesce-blocker", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-quiesce-a", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-quiesce-b", "test_done", "done"),
            1);
  destruct_object(blocker);
  destruct_object(first);
  destruct_object(second);
  free_object(&blocker,
              "TestGatewayOwnerOutputQuiesceClosesStoppedWorkerWave");
  free_object(&first, "TestGatewayOwnerOutputQuiesceClosesStoppedWorkerWave");
  free_object(&second,
              "TestGatewayOwnerOutputQuiesceClosesStoppedWorkerWave");
}

TEST_F(DriverTest,
       TestGatewayOwnerOutputQuiesceCountsDetachedWaveReservation) {
  auto *first = create_gateway_session_for_test(
      "gw-test-quiesce-detached-a", "/clone/gateway_login_example", 136);
  auto *second = create_gateway_session_for_test(
      "gw-test-quiesce-detached-b", "/clone/gateway_login_example", 137);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  add_ref(first,
          "TestGatewayOwnerOutputQuiesceCountsDetachedWaveReservation");
  add_ref(second,
          "TestGatewayOwnerOutputQuiesceCountsDetachedWaveReservation");
  vm_owner_set_id(first, "owner/test/gateway/quiesce-detached-a");
  vm_owner_set_id(second, "owner/test/gateway/quiesce-detached-b");
  auto *first_session = gateway_find_session_by_object(first);
  auto *second_session = gateway_find_session_by_object(second);
  ASSERT_NE(first_session, nullptr);
  ASSERT_NE(second_session, nullptr);
  const auto first_reservation = gateway_reserve_session_output_for_object(first);
  const auto second_reservation =
      gateway_reserve_session_output_for_object(second);
  ASSERT_GT(first_reservation, 0u);
  ASSERT_GT(second_reservation, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(first_session,
                                                 first_reservation));
  ASSERT_TRUE(gateway_test_seed_owner_room_event(second_session,
                                                 second_reservation));

  std::atomic<int> blocker_started{0};
  std::atomic<int> release_blocker{0};
  vm_owner_thread_start(2);
  struct OwnerThreadGuard {
    std::atomic<int> &release;
    bool active{true};
    ~OwnerThreadGuard() {
      if (active) {
        release.store(1, std::memory_order_release);
        vm_owner_thread_stop();
      }
    }
    void stop() {
      release.store(1, std::memory_order_release);
      vm_owner_thread_stop();
      active = false;
    }
  } owner_thread_guard{release_blocker};
  ASSERT_GT(vm_owner_enqueue_executor_task(
                second, "room_output_projection",
                "unit-quiesce-detached-blocker", [&] {
                  blocker_started.store(1, std::memory_order_release);
                  while (release_blocker.load(std::memory_order_acquire) == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
                }),
            0u);
  for (int index = 0; index < 200 &&
                      blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {first, second}, {first_reservation, second_reservation},
      {"quiesce-detached-a", "quiesce-detached-b"}, {24, 25}, 1000,
      &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true, true}));
  ASSERT_EQ(submitted.future_ids.size(), 2u);
  for (int index = 0; index < 200; ++index) {
    if (vm_owner_future_state(submitted.future_ids[0]) ==
        VM_OWNER_FUTURE_COMPLETED) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_PENDING);
  ASSERT_EQ(gateway_process_session_future_watches_at(0), 1);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_session_future_watch_count(), 1);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 1);
  ASSERT_EQ(first_session->output_fifo.size(), 1u);
  ASSERT_EQ(second_session->output_fifo.size(), 1u);
  ASSERT_EQ(first_session->output_fifo.front().reservation_id,
            first_reservation);
  ASSERT_EQ(second_session->output_fifo.front().reservation_id,
            second_reservation);
  ASSERT_FALSE(first_session->output_fifo.front().ready);
  ASSERT_FALSE(second_session->output_fifo.front().ready);

  auto *quiesced =
      gateway_owner_output_quiesce("unit detached reservation quiesce");
  ASSERT_NE(quiesced, nullptr);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "success"), 1);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "room_watches_before"), 1);
  ASSERT_EQ(
      gateway_test_mapping_number(quiesced, "room_reservations_before"),
      2);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "released_reservations"),
            2);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "cancelled_futures"), 1);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "room_reservations_after"),
            0);
  free_mapping(quiesced);

  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_TRUE(first_session->output_fifo.empty());
  ASSERT_TRUE(second_session->output_fifo.empty());

  owner_thread_guard.stop();
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-quiesce-detached-a", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-quiesce-detached-b", "test_done", "done"),
            1);
  destruct_object(first);
  destruct_object(second);
  free_object(&first,
              "TestGatewayOwnerOutputQuiesceCountsDetachedWaveReservation");
  free_object(&second,
              "TestGatewayOwnerOutputQuiesceCountsDetachedWaveReservation");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputProjectionUsesSameWireAndFifoSlot) {
  auto *ob = create_gateway_session_for_test(
      "gw-test-owner-room-output", "/clone/gateway_login_example", 111);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayOwnerRoomOutputProjectionUsesSameWireAndFifoSlot");
  vm_owner_set_id(ob, "owner/test/gateway/room-output");
  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  const auto worker_wall_samples_before =
      g_gateway_runtime_counters.room_output_projection_worker_samples.load(
          std::memory_order_relaxed);
  const auto worker_cpu_samples_before =
      g_gateway_runtime_counters
          .room_output_projection_worker_thread_cpu_samples.load(
              std::memory_order_relaxed);
  const auto worker_cpu_unavailable_before =
      g_gateway_runtime_counters
          .room_output_projection_worker_thread_cpu_unavailable.load(
              std::memory_order_relaxed);

  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(session, reservation_id));
  std::string expected_wire;
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(
      session, reservation_id, &expected_wire));

  vm_owner_thread_start(2);
  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {ob}, {reservation_id}, {"owner-room-player"}, {19}, 1000,
      &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true}));
  ASSERT_EQ(submitted.filled_inline, (std::vector<bool>{false}));
  ASSERT_EQ(submitted.event_counts, (std::vector<LPC_INT>{1}));
  ASSERT_EQ(submitted.text_length_totals, (std::vector<LPC_INT>{0}));
  ASSERT_EQ(submitted.slot_server_seqs, (std::vector<LPC_INT>{1201}));
  ASSERT_EQ(submitted.future_ids.size(), 1u);
  ASSERT_GT(submitted.future_ids[0], 0u);
  ASSERT_FALSE(gateway_append_preencoded_message_event_wave(
      {session}, {reservation_id}, {1203}, {1204}, {17}, {8102},
      gateway_test_owner_room_stable_payload(), "player", 222334, 128));
  ASSERT_EQ(gateway_pending_message_event_count(session, reservation_id), 1u);
  for (int i = 0; i < 200; ++i) {
    const auto state = vm_owner_future_state(submitted.future_ids[0]);
    if (state == VM_OWNER_FUTURE_COMPLETED ||
        state == VM_OWNER_FUTURE_FAILED) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(gateway_process_session_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            1);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_room_output_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_room_output_state", ob),
               "completed");
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_room_output_event_count", ob),
            1);
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_room_output_slot_server_seq", ob),
            1201);
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_room_output_callback_off_main", ob),
            0);
  ASSERT_EQ(session->output_fifo.size(), 1u);
  ASSERT_TRUE(session->output_fifo.front().ready);
  ASSERT_EQ(session->output_fifo.front().reservation_id, reservation_id);
  ASSERT_EQ(session->output_fifo.front().wire_bytes, expected_wire);
  const auto wire = nlohmann::json::parse(expected_wire);
  ASSERT_EQ(wire["type"], "output");
  ASSERT_EQ(wire["cid"], "gw-test-owner-room-output");
  ASSERT_TRUE(wire["data"].is_string());
  ASSERT_GT(g_gateway_runtime_counters.room_output_projection_submitted.load(
                std::memory_order_relaxed),
            0u);
  ASSERT_GT(g_gateway_runtime_counters.room_output_projection_worker_samples.load(
                std::memory_order_relaxed),
            0u);
  const auto worker_wall_samples_after =
      g_gateway_runtime_counters.room_output_projection_worker_samples.load(
          std::memory_order_relaxed);
  const auto worker_cpu_samples_after =
      g_gateway_runtime_counters
          .room_output_projection_worker_thread_cpu_samples.load(
              std::memory_order_relaxed);
  const auto worker_cpu_unavailable_after =
      g_gateway_runtime_counters
          .room_output_projection_worker_thread_cpu_unavailable.load(
              std::memory_order_relaxed);
  ASSERT_EQ(worker_wall_samples_after - worker_wall_samples_before,
            (worker_cpu_samples_after - worker_cpu_samples_before) +
                (worker_cpu_unavailable_after -
                 worker_cpu_unavailable_before));
  ASSERT_GT(g_gateway_runtime_counters.room_output_projection_publish_samples.load(
                std::memory_order_relaxed),
            0u);

  vm_owner_thread_stop();

  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-output", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(&ob,
              "TestGatewayOwnerRoomOutputProjectionUsesSameWireAndFifoSlot");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputMissingWaveReleasesExactlyOnce) {
  const auto cancelled_before =
      g_gateway_runtime_counters.future_watches_cancelled.load();
  const auto released_before =
      g_gateway_runtime_counters.room_output_projection_released.load();
  auto *ob = create_gateway_session_for_test(
      "gw-test-owner-room-missing-wave", "/clone/gateway_login_example", 120);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayOwnerRoomOutputMissingWaveReleasesExactlyOnce");
  vm_owner_set_id(ob, "owner/test/gateway/room-missing-wave");
  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(session, reservation_id));

  vm_owner_thread_start(1);
  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {ob}, {reservation_id}, {"owner-room-player"}, {19}, 1000,
      &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true}));
  ASSERT_EQ(submitted.future_ids.size(), 1u);
  ASSERT_TRUE(gateway_drop_room_output_wave_for_test(reservation_id));
  ASSERT_EQ(gateway_process_session_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            1);

  EXPECT_TRUE(session->output_fifo.empty());
  EXPECT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_UNKNOWN);
  EXPECT_EQ(gateway_session_future_watch_count(), 0);
  EXPECT_EQ(gateway_test_call_number(
                "query_last_owner_room_output_reservation_id", ob),
            static_cast<long>(reservation_id));
  EXPECT_STREQ(gateway_test_call_string(
                   "query_last_owner_room_output_state", ob),
               "released");
  EXPECT_EQ(g_gateway_runtime_counters.future_watches_cancelled.load() -
                cancelled_before,
            1u);
  EXPECT_EQ(g_gateway_runtime_counters.room_output_projection_released.load() -
                released_before,
            1u);

  vm_owner_thread_stop();
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-missing-wave", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(&ob,
              "TestGatewayOwnerRoomOutputMissingWaveReleasesExactlyOnce");
}

TEST_F(DriverTest,
       TestGatewayOwnerRoomOutputRejectsInvalidUtf8BeforeOwnerSubmit) {
  auto *ob = create_gateway_session_for_test(
      "gw-test-owner-room-invalid-utf8", "/clone/gateway_login_example", 141);
  ASSERT_NE(ob, nullptr);
  add_ref(ob,
          "TestGatewayOwnerRoomOutputRejectsInvalidUtf8BeforeOwnerSubmit");
  vm_owner_set_id(ob, "owner/test/gateway/room-invalid-utf8");
  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(session, reservation_id));

  vm_owner_thread_start(1);
  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  const std::string invalid_utf8_scope(1, static_cast<char>(0xc3));
  const auto accepted = gateway_submit_pending_message_event_batches_for_objects(
      {ob}, {reservation_id}, {invalid_utf8_scope}, {19}, 1000, &submitted);
  if (accepted) {
    auto *quiesced = gateway_owner_output_quiesce(
        "unit invalid utf8 pre-submit cleanup");
    ASSERT_NE(quiesced, nullptr);
    free_mapping(quiesced);
  }
  vm_owner_thread_stop();

  EXPECT_FALSE(accepted);
  EXPECT_EQ(gateway_session_future_watch_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-invalid-utf8", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(
      &ob, "TestGatewayOwnerRoomOutputRejectsInvalidUtf8BeforeOwnerSubmit");
}

TEST_F(DriverTest,
       TestGatewayOwnerRoomOutputRejectsOversizedWireBeforeOwnerSubmit) {
  auto *ob = create_gateway_session_for_test(
      "gw-test-owner-room-oversized", "/clone/gateway_login_example", 142);
  ASSERT_NE(ob, nullptr);
  add_ref(ob,
          "TestGatewayOwnerRoomOutputRejectsOversizedWireBeforeOwnerSubmit");
  vm_owner_set_id(ob, "owner/test/gateway/room-oversized");
  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  const std::string stable_prefix =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"normal\",\"reliability\":\"important\","
      "\"display_mode\":\"paced\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"";
  const std::string stable_suffix = "\",\"payload\":{}}";
  const std::string oversized_event =
      stable_prefix + std::string(70 * 1024, 'x') + stable_suffix;
  for (LPC_INT index = 0; index < 16; ++index) {
    ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
        {session}, {reservation_id}, {1201 + index * 2},
        {1202 + index * 2}, {17}, {8101 + index}, oversized_event,
        "player", 222333 + index, 128));
  }

  vm_owner_thread_start(1);
  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  const auto accepted = gateway_submit_pending_message_event_batches_for_objects(
      {ob}, {reservation_id}, {"owner-room-player"}, {19}, 1000,
      &submitted);
  if (accepted) {
    auto *quiesced = gateway_owner_output_quiesce(
        "unit oversized wire pre-submit cleanup");
    ASSERT_NE(quiesced, nullptr);
    free_mapping(quiesced);
  }
  vm_owner_thread_stop();

  EXPECT_FALSE(accepted);
  EXPECT_EQ(gateway_session_future_watch_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-oversized", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(
      &ob, "TestGatewayOwnerRoomOutputRejectsOversizedWireBeforeOwnerSubmit");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputWaitsForWholeWaveBeforePublish) {
  const auto submitted_before =
      g_gateway_runtime_counters.room_output_projection_submitted.load();
  const auto completed_before =
      g_gateway_runtime_counters.room_output_projection_completed.load();
  const auto released_before =
      g_gateway_runtime_counters.room_output_projection_released.load();
  auto *first = create_gateway_session_for_test(
      "gw-test-owner-room-wave-a", "/clone/gateway_login_example", 121);
  auto *second = create_gateway_session_for_test(
      "gw-test-owner-room-wave-b", "/clone/gateway_login_example", 122);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  add_ref(first, "TestGatewayOwnerRoomOutputWaitsForWholeWaveBeforePublish");
  add_ref(second, "TestGatewayOwnerRoomOutputWaitsForWholeWaveBeforePublish");
  vm_owner_set_id(first, "owner/test/gateway/room-wave-a");
  vm_owner_set_id(second, "owner/test/gateway/room-wave-b");
  auto *first_session = gateway_find_session_by_object(first);
  auto *second_session = gateway_find_session_by_object(second);
  ASSERT_NE(first_session, nullptr);
  ASSERT_NE(second_session, nullptr);
  const auto first_reservation = gateway_reserve_session_output_for_object(first);
  const auto second_reservation = gateway_reserve_session_output_for_object(second);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(first_session, first_reservation));
  ASSERT_TRUE(gateway_test_seed_owner_room_event(second_session, second_reservation));

  std::atomic<int> blocker_started{0};
  std::atomic<int> blocker_finished{0};
  std::atomic<int> release_blocker{0};
  vm_owner_thread_start(2);
  struct OwnerThreadBlockerGuard {
    std::atomic<int> &release;
    bool active{true};

    ~OwnerThreadBlockerGuard() {
      if (!active) {
        return;
      }
      release.store(1, std::memory_order_release);
      vm_owner_thread_stop();
    }

    void stop() {
      release.store(1, std::memory_order_release);
      vm_owner_thread_stop();
      active = false;
    }
  } owner_thread_guard{release_blocker};
  ASSERT_GT(vm_owner_enqueue_executor_task(
                second, "room_output_projection", "unit-wave-blocker", [&] {
                  blocker_started.store(1, std::memory_order_release);
                  while (release_blocker.load(std::memory_order_acquire) == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
                  blocker_finished.store(1, std::memory_order_release);
                }),
            0u);
  for (int index = 0;
       index < 200 && blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {first, second}, {first_reservation, second_reservation},
      {"owner-room-player-a", "owner-room-player-b"}, {19, 20}, 1000,
      &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true, true}));
  ASSERT_EQ(submitted.future_ids.size(), 2u);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 2);
  ASSERT_EQ(g_gateway_runtime_counters.room_output_projection_submitted.load(),
            submitted_before + 2);
  for (int index = 0; index < 200; ++index) {
    if (vm_owner_future_state(submitted.future_ids[0]) ==
        VM_OWNER_FUTURE_COMPLETED) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_PENDING);

  ASSERT_EQ(gateway_process_session_future_watches_at(0), 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 1);
  ASSERT_EQ(first_session->output_fifo.size(), 1u);
  ASSERT_EQ(second_session->output_fifo.size(), 1u);
  ASSERT_FALSE(first_session->output_fifo.front().ready);
  ASSERT_FALSE(second_session->output_fifo.front().ready);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 1);

  release_blocker.store(1, std::memory_order_release);
  ASSERT_TRUE(gateway_test_drain_owner_room_mailbox(second, "unit-wave-drain"));
  ASSERT_EQ(blocker_finished.load(std::memory_order_acquire), 1);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(gateway_process_session_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_TRUE(first_session->output_fifo.front().ready);
  ASSERT_TRUE(second_session->output_fifo.front().ready);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_EQ(g_gateway_runtime_counters.room_output_projection_completed.load(),
            completed_before + 2);
  ASSERT_EQ(g_gateway_runtime_counters.room_output_projection_released.load(),
            released_before);
  owner_thread_guard.stop();

  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-wave-a", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-wave-b", "test_done", "done"),
            1);
  destruct_object(first);
  destruct_object(second);
  free_object(&first, "TestGatewayOwnerRoomOutputWaitsForWholeWaveBeforePublish");
  free_object(&second, "TestGatewayOwnerRoomOutputWaitsForWholeWaveBeforePublish");
}

TEST_F(DriverTest,
       TestGatewayOwnerRoomOutputSupportsSameSessionMultipleReservations) {
  const auto inline_fallbacks_before =
      g_gateway_runtime_counters.room_output_projection_inline_fallbacks.load();
  auto *ob = create_gateway_session_for_test(
      "gw-test-owner-room-same-session", "/clone/gateway_login_example", 133);
  ASSERT_NE(ob, nullptr);
  add_ref(
      ob,
      "TestGatewayOwnerRoomOutputSupportsSameSessionMultipleReservations");
  vm_owner_set_id(ob, "owner/test/gateway/room-same-session");
  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);

  const auto first_reservation = gateway_reserve_session_output_for_object(ob);
  const auto second_reservation = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(first_reservation, 0u);
  ASSERT_GT(second_reservation, 0u);
  ASSERT_NE(first_reservation, second_reservation);
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      {session}, {first_reservation}, {1201}, {1202}, {17}, {8101},
      gateway_test_owner_room_stable_payload(), "player", 222333, 128));
  ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
      {session}, {second_reservation}, {1203}, {1204}, {17}, {8102},
      gateway_test_owner_room_stable_payload(), "player", 222334, 128));
  std::string first_expected;
  std::string second_expected;
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(
      session, first_reservation, &first_expected,
      "owner-room-player", 19));
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(
      session, second_reservation, &second_expected,
      "owner-room-player", 19));

  vm_owner_thread_start(2);
  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {ob, ob}, {first_reservation, second_reservation},
      {"owner-room-player", "owner-room-player"}, {19, 19}, 1000,
      &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true, true}));
  ASSERT_EQ(submitted.filled_inline, (std::vector<bool>{false, false}));
  ASSERT_EQ(submitted.future_ids.size(), 2u);
  ASSERT_NE(submitted.future_ids[0], submitted.future_ids[1]);
  ASSERT_TRUE(gateway_test_drain_owner_room_mailbox(
      ob, "unit-same-session-drain"));
  ASSERT_EQ(gateway_process_session_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            2);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(session->output_fifo.size(), 2u);
  ASSERT_TRUE(session->output_fifo[0].ready);
  ASSERT_TRUE(session->output_fifo[1].ready);
  ASSERT_EQ(session->output_fifo[0].reservation_id, first_reservation);
  ASSERT_EQ(session->output_fifo[1].reservation_id, second_reservation);
  ASSERT_EQ(session->output_fifo[0].wire_bytes, first_expected);
  ASSERT_EQ(session->output_fifo[1].wire_bytes, second_expected);
  ASSERT_EQ(
      g_gateway_runtime_counters.room_output_projection_inline_fallbacks.load(),
      inline_fallbacks_before);

  vm_owner_thread_stop();
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-same-session", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(
      &ob,
      "TestGatewayOwnerRoomOutputSupportsSameSessionMultipleReservations");
}

TEST_F(DriverTest,
       TestGatewayOwnerRoomOutputPublishRetryIsBoundedAndPreservesFifo) {
  const auto retry_enqueued_before =
      g_gateway_runtime_counters.room_output_projection_retry_enqueued.load();
  const auto retry_attempted_before =
      g_gateway_runtime_counters.room_output_projection_retry_attempted.load();
  auto *ob = create_gateway_session_for_test(
      "gw-test-owner-room-publish-retry", "/clone/gateway_login_example",
      145);
  ASSERT_NE(ob, nullptr);
  add_ref(ob,
          "TestGatewayOwnerRoomOutputPublishRetryIsBoundedAndPreservesFifo");
  vm_owner_set_id(ob, "owner/test/gateway/room-publish-retry");
  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(session, reservation_id));
  std::string expected_wire;
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(
      session, reservation_id, &expected_wire));

  vm_owner_thread_start(1);
  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {ob}, {reservation_id}, {"owner-room-player"}, {19}, 1000,
      &submitted));
  for (int index = 0; index < 200; ++index) {
    if (vm_owner_future_state(submitted.future_ids[0]) ==
        VM_OWNER_FUTURE_COMPLETED) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_COMPLETED);

  struct GatewayPacketLimitGuard {
    size_t original;
    ~GatewayPacketLimitGuard() { g_gateway_max_packet_size = original; }
  } packet_limit_guard{g_gateway_max_packet_size};
  g_gateway_max_packet_size = 1;
  ASSERT_EQ(gateway_process_session_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            1);
  EXPECT_EQ(gateway_session_future_watch_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_pending_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_wave_count(), 1);
  EXPECT_EQ(gateway_room_output_projection_reservation_count(), 1);
  EXPECT_EQ(gateway_room_output_projection_retry_count(), 1);
  ASSERT_EQ(session->output_fifo.size(), 1u);
  EXPECT_FALSE(session->output_fifo.front().ready);

  g_gateway_max_packet_size = packet_limit_guard.original;
  std::this_thread::sleep_for(std::chrono::milliseconds(3));
  EXPECT_EQ(gateway_process_session_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            0);
  EXPECT_EQ(gateway_room_output_projection_wave_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_reservation_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_retry_count(), 0);
  ASSERT_EQ(session->output_fifo.size(), 1u);
  EXPECT_TRUE(session->output_fifo.front().ready);
  EXPECT_EQ(session->output_fifo.front().wire_bytes, expected_wire);
  EXPECT_EQ(
      g_gateway_runtime_counters.room_output_projection_retry_enqueued.load(),
      retry_enqueued_before + 1);
  EXPECT_EQ(
      g_gateway_runtime_counters.room_output_projection_retry_attempted.load(),
      retry_attempted_before + 1);

  vm_owner_thread_stop();
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-publish-retry", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(
      &ob,
      "TestGatewayOwnerRoomOutputPublishRetryIsBoundedAndPreservesFifo");
}

TEST_F(DriverTest, TestGatewayRoomOutputRetrySchedulerHasHardBudgets) {
  const auto source =
      read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto watch_start =
      source.find("int gateway_process_session_future_watches_at");
  const auto watch_end =
      source.find("gateway_session_future_watch_count()", watch_start);
  ASSERT_NE(watch_start, std::string::npos);
  ASSERT_NE(watch_end, std::string::npos);
  const auto watch_body = source.substr(watch_start, watch_end - watch_start);
  EXPECT_NE(watch_body.find(
                "gateway_process_room_output_publish_retries("),
            std::string::npos);
  EXPECT_EQ(watch_body.find(
                "for (const auto &[wave_id, wave] : "
                "g_gateway_room_output_waves)"),
            std::string::npos);

  const auto retry_start =
      source.find("void gateway_process_room_output_publish_retries");
  const auto retry_end =
      source.find("void gateway_cancel_room_output_wave_session_items",
                  retry_start);
  ASSERT_NE(retry_start, std::string::npos);
  ASSERT_NE(retry_end, std::string::npos);
  const auto retry_body = source.substr(retry_start, retry_end - retry_start);
  EXPECT_NE(retry_body.find("kGatewayRoomOutputRetryBudget"),
            std::string::npos);
  EXPECT_NE(retry_body.find("kGatewayRoomOutputRetryWallBudgetNs"),
            std::string::npos);
  EXPECT_NE(retry_body.find("kGatewayRoomOutputRetryMaxHoldMs"),
            std::string::npos);
  EXPECT_NE(source.find("kGatewayRoomOutputRetryMaxAttempts = 12"),
            std::string::npos);
  EXPECT_NE(source.find("GatewayRoomOutputRetrySchedule"),
            std::string::npos);
}

TEST_F(DriverTest, TestGatewayRoomOutputPreparesFlushSessionsBeforeFifoStage) {
  const auto source =
      read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto publish_start = source.find(
      "bool gateway_publish_room_output_wave(uint64_t wave_id) {");
  const auto publish_end = source.find(
      "void gateway_process_room_output_publish_retries", publish_start);
  ASSERT_NE(publish_start, std::string::npos);
  ASSERT_NE(publish_end, std::string::npos);
  const auto publish_body =
      source.substr(publish_start, publish_end - publish_start);
  const auto flush_insert = publish_body.find("flush_sessions.insert");
  const auto fifo_stage =
      publish_body.find("gateway_stage_session_wire_outputs");
  ASSERT_NE(flush_insert, std::string::npos);
  ASSERT_NE(fifo_stage, std::string::npos);
  EXPECT_LT(flush_insert, fifo_stage);
  EXPECT_EQ(publish_body.find("flush_sessions.insert", fifo_stage),
            std::string::npos);
}

TEST_F(DriverTest,
       TestGatewayRoomOutputPartialWatchRollbackCountsCancellation) {
  const auto source =
      read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto submit_start = source.find(
      "bool gateway_submit_pending_message_event_batches_for_objects");
  const auto submit_end = source.find(
      "int gateway_fill_pending_message_event_batch_for_object", submit_start);
  ASSERT_NE(submit_start, std::string::npos);
  ASSERT_NE(submit_end, std::string::npos);
  const auto submit_body =
      source.substr(submit_start, submit_end - submit_start);
  const auto rollback_start = submit_body.find(
      "for (size_t index = 0; index < registered_count; ++index)");
  const auto rollback_end = submit_body.find(
      "for (size_t index = registered_count; index < submissions.size(); "
      "++index)",
      rollback_start);
  ASSERT_NE(rollback_start, std::string::npos);
  ASSERT_NE(rollback_end, std::string::npos);
  const auto rollback_body =
      submit_body.substr(rollback_start, rollback_end - rollback_start);
  EXPECT_NE(rollback_body.find("future_watches_cancelled.fetch_add"),
            std::string::npos);
}

TEST_F(DriverTest, TestGatewayFutureWatchQueueMutationIsTransactional) {
  const auto source =
      read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto session_register =
      source.find("bool gateway_register_session_future_watch_state");
  const auto session_requeue =
      source.find("bool gateway_requeue_session_future_watch");
  const auto generic_register =
      source.find("bool gateway_register_generic_future_watch_state");
  const auto generic_requeue =
      source.find("bool gateway_requeue_generic_future_watch");
  ASSERT_NE(session_register, std::string::npos);
  ASSERT_NE(session_requeue, std::string::npos);
  ASSERT_NE(generic_register, std::string::npos);
  ASSERT_NE(generic_requeue, std::string::npos);

  const auto watch_register_start = source.find(
      "int gateway_watch_session_future_for_object_internal");
  const auto watch_register_end = source.find(
      "int gateway_watch_session_future_for_object(", watch_register_start);
  ASSERT_NE(watch_register_start, std::string::npos);
  ASSERT_NE(watch_register_end, std::string::npos);
  const auto watch_register_body = source.substr(
      watch_register_start, watch_register_end - watch_register_start);
  EXPECT_NE(watch_register_body.find(
                "gateway_register_session_future_watch_state"),
            std::string::npos);
  EXPECT_EQ(watch_register_body.find(
                "g_gateway_session_future_watches.emplace"),
            std::string::npos);
  EXPECT_EQ(watch_register_body.find(
                "g_gateway_future_watch_queue.push_back"),
            std::string::npos);

  const auto generic_watch_start =
      source.find("int gateway_watch_future_for_object");
  const auto generic_watch_end =
      source.find("int gateway_process_future_watches_at", generic_watch_start);
  ASSERT_NE(generic_watch_start, std::string::npos);
  ASSERT_NE(generic_watch_end, std::string::npos);
  const auto generic_watch_body = source.substr(
      generic_watch_start, generic_watch_end - generic_watch_start);
  EXPECT_NE(generic_watch_body.find(
                "gateway_register_generic_future_watch_state"),
            std::string::npos);
  EXPECT_EQ(generic_watch_body.find("g_gateway_future_watches.emplace"),
            std::string::npos);
  EXPECT_EQ(generic_watch_body.find(
                "g_gateway_generic_future_watch_queue.push_back"),
            std::string::npos);

  const auto process_session_start =
      source.find("int gateway_process_session_future_watches_at");
  const auto process_session_end =
      source.find("gateway_session_future_watch_count()", process_session_start);
  ASSERT_NE(process_session_start, std::string::npos);
  ASSERT_NE(process_session_end, std::string::npos);
  const auto process_session_body = source.substr(
      process_session_start, process_session_end - process_session_start);
  EXPECT_NE(process_session_body.find(
                "gateway_requeue_session_future_watch"),
            std::string::npos);

  const auto process_generic_start =
      source.find("int gateway_process_future_watches_at");
  const auto process_generic_end =
      source.find("gateway_future_watch_count()", process_generic_start);
  ASSERT_NE(process_generic_start, std::string::npos);
  ASSERT_NE(process_generic_end, std::string::npos);
  const auto process_generic_body = source.substr(
      process_generic_start, process_generic_end - process_generic_start);
  EXPECT_NE(process_generic_body.find(
                "gateway_requeue_generic_future_watch"),
            std::string::npos);
}

TEST_F(DriverTest, TestGatewayFutureWatchLimitIsSharedAcrossWatchKinds) {
  const auto source =
      read_source_file_for_test("../src/packages/gateway/gateway_session.cc");

  const auto limit_start =
      source.find("bool gateway_future_watch_limit_reached()");
  const auto limit_end =
      source.find("void gateway_future_watch_timer_cb", limit_start);
  ASSERT_NE(limit_start, std::string::npos);
  ASSERT_NE(limit_end, std::string::npos);
  const auto limit_body = source.substr(limit_start, limit_end - limit_start);
  EXPECT_NE(limit_body.find("g_gateway_session_future_watches.size()"),
            std::string::npos);
  EXPECT_NE(limit_body.find("g_gateway_future_watches.size()"),
            std::string::npos);
  EXPECT_NE(limit_body.find("kGatewayMaxFutureWatches"), std::string::npos);

  const auto session_start = source.find(
      "int gateway_watch_session_future_for_object_internal");
  const auto session_end = source.find(
      "int gateway_watch_session_future_for_object(", session_start);
  ASSERT_NE(session_start, std::string::npos);
  ASSERT_NE(session_end, std::string::npos);
  const auto session_body =
      source.substr(session_start, session_end - session_start);
  EXPECT_NE(session_body.find("gateway_future_watch_limit_reached()"),
            std::string::npos);

  const auto generic_start =
      source.find("int gateway_watch_future_for_object");
  const auto generic_end =
      source.find("int gateway_process_future_watches_at", generic_start);
  ASSERT_NE(generic_start, std::string::npos);
  ASSERT_NE(generic_end, std::string::npos);
  const auto generic_body =
      source.substr(generic_start, generic_end - generic_start);
  EXPECT_NE(generic_body.find("gateway_future_watch_limit_reached()"),
            std::string::npos);
}

TEST_F(DriverTest,
       TestGatewayOwnerStaleNotificationDoesNotReuseWaveAfterCallback) {
  const auto source =
      read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto process_start = source.find(
      "int gateway_process_room_output_wave_watch(\n"
      "    const GatewaySessionFutureWatch &watch, uint64_t now_ms) {");
  const auto process_end = source.find("}  // namespace", process_start);
  ASSERT_NE(process_start, std::string::npos);
  ASSERT_NE(process_end, std::string::npos);
  const auto process_body =
      source.substr(process_start, process_end - process_start);
  const auto stale_start = process_body.find("if (!owner_current)");
  const auto stale_end = process_body.find("auto future_state", stale_start);
  ASSERT_NE(stale_start, std::string::npos);
  ASSERT_NE(stale_end, std::string::npos);
  const auto stale_body =
      process_body.substr(stale_start, stale_end - stale_start);
  const auto terminal_snapshot =
      stale_body.find("gateway_room_output_wave_all_terminal");
  const auto notification =
      stale_body.find("gateway_notify_room_output_wave_item");
  ASSERT_NE(terminal_snapshot, std::string::npos);
  ASSERT_NE(notification, std::string::npos);
  EXPECT_LT(terminal_snapshot, notification);
  EXPECT_EQ(stale_body.find("wave_it->", notification), std::string::npos);
}

TEST_F(DriverTest,
       TestGatewayOwnerRoomOutputReservationMismatchIsolatesRecipient) {
  auto *first = create_gateway_session_for_test(
      "gw-test-owner-room-mismatch-wave-a", "/clone/gateway_login_example",
      143);
  auto *second = create_gateway_session_for_test(
      "gw-test-owner-room-mismatch-wave-b", "/clone/gateway_login_example",
      144);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  add_ref(first,
          "TestGatewayOwnerRoomOutputReservationMismatchIsolatesRecipient");
  add_ref(second,
          "TestGatewayOwnerRoomOutputReservationMismatchIsolatesRecipient");
  vm_owner_set_id(first, "owner/test/gateway/room-mismatch-wave-a");
  vm_owner_set_id(second, "owner/test/gateway/room-mismatch-wave-b");
  auto *first_session = gateway_find_session_by_object(first);
  auto *second_session = gateway_find_session_by_object(second);
  ASSERT_NE(first_session, nullptr);
  ASSERT_NE(second_session, nullptr);
  const auto first_reservation = gateway_reserve_session_output_for_object(first);
  const auto second_reservation =
      gateway_reserve_session_output_for_object(second);
  ASSERT_TRUE(
      gateway_test_seed_owner_room_event(first_session, first_reservation));
  ASSERT_TRUE(
      gateway_test_seed_owner_room_event(second_session, second_reservation));
  std::string second_expected_wire;
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(
      second_session, second_reservation, &second_expected_wire,
      "owner-room-player-b", 20));

  std::atomic<int> blocker_started{0};
  std::atomic<int> release_blocker{0};
  vm_owner_thread_start(2);
  ASSERT_GT(vm_owner_enqueue_executor_task(
                second, "room_output_projection",
                "unit-reservation-mismatch-wave-blocker", [&] {
                  blocker_started.store(1, std::memory_order_release);
                  while (release_blocker.load(std::memory_order_acquire) == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
                }),
            0u);
  for (int index = 0;
       index < 200 && blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {first, second}, {first_reservation, second_reservation},
      {"owner-room-player-a", "owner-room-player-b"}, {19, 20}, 1000,
      &submitted));
  for (int index = 0; index < 200; ++index) {
    if (vm_owner_future_state(submitted.future_ids[0]) ==
        VM_OWNER_FUTURE_COMPLETED) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_PENDING);
  ASSERT_EQ(gateway_release_session_output_for_object(
                first, first_reservation),
            1);

  ASSERT_EQ(gateway_process_session_future_watches_at(0), 1);
  const auto watches_after_mismatch = gateway_session_future_watch_count();
  const auto pending_after_mismatch =
      gateway_room_output_projection_pending_count();
  EXPECT_EQ(watches_after_mismatch, 1);
  EXPECT_EQ(pending_after_mismatch, 1);
  EXPECT_EQ(gateway_room_output_projection_wave_count(), 1);
  EXPECT_EQ(gateway_room_output_projection_reservation_count(), 1);
  EXPECT_TRUE(first_session->output_fifo.empty());
  ASSERT_EQ(second_session->output_fifo.size(), 1u);
  EXPECT_FALSE(second_session->output_fifo.front().ready);

  release_blocker.store(1, std::memory_order_release);
  for (int index = 0; index < 200; ++index) {
    if (vm_owner_future_state(submitted.future_ids[1]) ==
        VM_OWNER_FUTURE_COMPLETED) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(gateway_process_session_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            1);
  vm_owner_thread_stop();

  EXPECT_EQ(gateway_session_future_watch_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_pending_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_wave_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_reservation_count(), 0);
  EXPECT_TRUE(first_session->output_fifo.empty());
  ASSERT_EQ(second_session->output_fifo.size(), 1u);
  EXPECT_TRUE(second_session->output_fifo.front().ready);
  EXPECT_EQ(second_session->output_fifo.front().wire_bytes,
            second_expected_wire);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-mismatch-wave-a", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-mismatch-wave-b", "test_done", "done"),
            1);
  destruct_object(first);
  destruct_object(second);
  free_object(
      &first, "TestGatewayOwnerRoomOutputReservationMismatchIsolatesRecipient");
  free_object(
      &second, "TestGatewayOwnerRoomOutputReservationMismatchIsolatesRecipient");
}

TEST_F(DriverTest,
       TestGatewayOwnerRoomOutputStaleRecipientFallsBackWithSlowPeer) {
  auto *stale = create_gateway_session_for_test(
      "gw-test-owner-room-stale-wave-a", "/clone/gateway_login_example", 127);
  auto *slow = create_gateway_session_for_test(
      "gw-test-owner-room-stale-wave-b", "/clone/gateway_login_example", 128);
  ASSERT_NE(stale, nullptr);
  ASSERT_NE(slow, nullptr);
  add_ref(
      stale,
      "TestGatewayOwnerRoomOutputStaleRecipientFallsBackWithSlowPeer");
  add_ref(
      slow,
      "TestGatewayOwnerRoomOutputStaleRecipientFallsBackWithSlowPeer");
  vm_owner_set_id(stale, "owner/test/gateway/room-stale-wave-a");
  vm_owner_set_id(slow, "owner/test/gateway/room-stale-wave-b");
  auto *stale_session = gateway_find_session_by_object(stale);
  auto *slow_session = gateway_find_session_by_object(slow);
  ASSERT_NE(stale_session, nullptr);
  ASSERT_NE(slow_session, nullptr);
  const auto stale_reservation =
      gateway_reserve_session_output_for_object(stale);
  const auto slow_reservation = gateway_reserve_session_output_for_object(slow);
  ASSERT_TRUE(
      gateway_test_seed_owner_room_event(stale_session, stale_reservation));
  ASSERT_TRUE(
      gateway_test_seed_owner_room_event(slow_session, slow_reservation));
  std::string stale_expected;
  std::string slow_expected;
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(
      stale_session, stale_reservation, &stale_expected,
      "owner-room-player-a", 19));
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(
      slow_session, slow_reservation, &slow_expected,
      "owner-room-player-b", 20));

  std::atomic<int> blocker_started{0};
  std::atomic<int> release_blocker{0};
  vm_owner_thread_start(2);
  struct OwnerThreadStaleGuard {
    std::atomic<int> &release;
    bool active{true};

    ~OwnerThreadStaleGuard() {
      if (active) {
        release.store(1, std::memory_order_release);
        vm_owner_thread_stop();
      }
    }

    void stop() {
      release.store(1, std::memory_order_release);
      vm_owner_thread_stop();
      active = false;
    }
  } owner_thread_guard{release_blocker};
  ASSERT_GT(vm_owner_enqueue_executor_task(
                slow, "room_output_projection", "unit-stale-wave-blocker",
                [&] {
                  blocker_started.store(1, std::memory_order_release);
                  while (release_blocker.load(std::memory_order_acquire) == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
                }),
            0u);
  for (int index = 0;
       index < 200 && blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {stale, slow}, {stale_reservation, slow_reservation},
      {"owner-room-player-a", "owner-room-player-b"}, {19, 20}, 1000,
      &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true, true}));
  ASSERT_EQ(submitted.future_ids.size(), 2u);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_PENDING);

  vm_owner_set_id(stale, "owner/test/gateway/room-stale-wave-rebound");
  ASSERT_EQ(gateway_process_session_future_watches_at(0), 1);

  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_PENDING);
  ASSERT_EQ(gateway_session_future_watch_count(), 1);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 1);
  ASSERT_EQ(stale_session->output_fifo.size(), 1u);
  ASSERT_FALSE(stale_session->output_fifo.front().ready);
  ASSERT_EQ(slow_session->output_fifo.size(), 1u);
  ASSERT_FALSE(slow_session->output_fifo.front().ready);
  ASSERT_STRNE(gateway_test_call_string(
                   "query_last_owner_room_output_state", stale),
               "released");

  release_blocker.store(1, std::memory_order_release);
  ASSERT_TRUE(gateway_test_drain_owner_room_mailbox(
      slow, "unit-stale-wave-drain"));
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(gateway_process_session_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_EQ(stale_session->output_fifo.size(), 1u);
  ASSERT_TRUE(stale_session->output_fifo.front().ready);
  ASSERT_EQ(stale_session->output_fifo.front().wire_bytes, stale_expected);
  ASSERT_EQ(slow_session->output_fifo.size(), 1u);
  ASSERT_TRUE(slow_session->output_fifo.front().ready);
  ASSERT_EQ(slow_session->output_fifo.front().wire_bytes, slow_expected);
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_room_output_state", stale),
               "completed");
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_room_output_state", slow),
               "completed");

  owner_thread_guard.stop();
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-stale-wave-a", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-stale-wave-b", "test_done", "done"),
            1);
  destruct_object(stale);
  destruct_object(slow);
  free_object(
      &stale,
      "TestGatewayOwnerRoomOutputStaleRecipientFallsBackWithSlowPeer");
  free_object(
      &slow,
      "TestGatewayOwnerRoomOutputStaleRecipientFallsBackWithSlowPeer");
}

TEST_F(DriverTest,
       TestGatewayOwnerRoomOutputFailedRecipientFallsBackWithSlowPeer) {
  auto *failed = create_gateway_session_for_test(
      "gw-test-owner-room-failed-wave-a", "/clone/gateway_login_example", 129);
  auto *slow = create_gateway_session_for_test(
      "gw-test-owner-room-failed-wave-b", "/clone/gateway_login_example", 130);
  ASSERT_NE(failed, nullptr);
  ASSERT_NE(slow, nullptr);
  add_ref(
      failed,
      "TestGatewayOwnerRoomOutputFailedRecipientFallsBackWithSlowPeer");
  add_ref(
      slow,
      "TestGatewayOwnerRoomOutputFailedRecipientFallsBackWithSlowPeer");
  vm_owner_set_id(failed, "owner/test/gateway/room-failed-wave-a");
  vm_owner_set_id(slow, "owner/test/gateway/room-failed-wave-b");
  auto *failed_session = gateway_find_session_by_object(failed);
  auto *slow_session = gateway_find_session_by_object(slow);
  ASSERT_NE(failed_session, nullptr);
  ASSERT_NE(slow_session, nullptr);
  const auto failed_reservation =
      gateway_reserve_session_output_for_object(failed);
  const auto slow_reservation = gateway_reserve_session_output_for_object(slow);
  ASSERT_TRUE(
      gateway_test_seed_owner_room_event(failed_session, failed_reservation));
  ASSERT_TRUE(
      gateway_test_seed_owner_room_event(slow_session, slow_reservation));
  std::string failed_expected;
  std::string slow_expected;
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(
      failed_session, failed_reservation, &failed_expected,
      "owner-room-player-a", 19));
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(
      slow_session, slow_reservation, &slow_expected,
      "owner-room-player-b", 20));

  std::atomic<int> blockers_started{0};
  std::atomic<int> release_blockers{0};
  vm_owner_thread_start(2);
  struct OwnerThreadFailedGuard {
    std::atomic<int> &release;
    bool active{true};

    ~OwnerThreadFailedGuard() {
      if (active) {
        release.store(1, std::memory_order_release);
        vm_owner_thread_stop();
      }
    }

    void stop() {
      release.store(1, std::memory_order_release);
      vm_owner_thread_stop();
      active = false;
    }
  } owner_thread_guard{release_blockers};
  auto enqueue_blocker = [&](object_t *target, const char *task_key) {
    return vm_owner_enqueue_executor_task(
        target, "room_output_projection", task_key, [&] {
          blockers_started.fetch_add(1, std::memory_order_release);
          while (release_blockers.load(std::memory_order_acquire) == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
        });
  };
  ASSERT_GT(enqueue_blocker(failed, "unit-failed-wave-blocker-a"), 0u);
  ASSERT_GT(enqueue_blocker(slow, "unit-failed-wave-blocker-b"), 0u);
  for (int index = 0;
       index < 200 && blockers_started.load(std::memory_order_acquire) != 2;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blockers_started.load(std::memory_order_acquire), 2);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {failed, slow}, {failed_reservation, slow_reservation},
      {"owner-room-player-a", "owner-room-player-b"}, {19, 20}, 1000,
      &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true, true}));
  ASSERT_EQ(submitted.future_ids.size(), 2u);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_PENDING);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_PENDING);

  auto *cancelled = vm_owner_future_cancel(
      submitted.future_ids[0], "unit room output projection failure");
  ASSERT_STREQ(gateway_test_mapping_string(cancelled, "state"), "failed");
  ASSERT_EQ(gateway_test_mapping_number(cancelled, "cancelled"), 1);
  free_mapping(cancelled);
  ASSERT_EQ(gateway_process_session_future_watches_at(0), 1);

  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_PENDING);
  ASSERT_EQ(gateway_session_future_watch_count(), 1);
  ASSERT_EQ(failed_session->output_fifo.size(), 1u);
  ASSERT_FALSE(failed_session->output_fifo.front().ready);
  ASSERT_EQ(slow_session->output_fifo.size(), 1u);
  ASSERT_FALSE(slow_session->output_fifo.front().ready);
  ASSERT_STRNE(gateway_test_call_string(
                   "query_last_owner_room_output_state", failed),
               "released");

  release_blockers.store(1, std::memory_order_release);
  ASSERT_TRUE(gateway_test_drain_owner_room_mailbox(
      failed, "unit-failed-wave-drain-a"));
  ASSERT_TRUE(gateway_test_drain_owner_room_mailbox(
      slow, "unit-failed-wave-drain-b"));
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(gateway_process_session_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_EQ(failed_session->output_fifo.size(), 1u);
  ASSERT_TRUE(failed_session->output_fifo.front().ready);
  ASSERT_EQ(failed_session->output_fifo.front().wire_bytes, failed_expected);
  ASSERT_EQ(slow_session->output_fifo.size(), 1u);
  ASSERT_TRUE(slow_session->output_fifo.front().ready);
  ASSERT_EQ(slow_session->output_fifo.front().wire_bytes, slow_expected);
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_room_output_state", failed),
               "completed");
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_room_output_state", slow),
               "completed");

  owner_thread_guard.stop();
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-failed-wave-a", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-failed-wave-b", "test_done", "done"),
            1);
  destruct_object(failed);
  destruct_object(slow);
  free_object(
      &failed,
      "TestGatewayOwnerRoomOutputFailedRecipientFallsBackWithSlowPeer");
  free_object(
      &slow,
      "TestGatewayOwnerRoomOutputFailedRecipientFallsBackWithSlowPeer");
}

TEST_F(DriverTest,
       TestGatewayOwnerRoomOutputWaveTimeoutPreservesCompletedRecipient) {
  const auto completed_before =
      g_gateway_runtime_counters.room_output_projection_completed.load();
  const auto released_before =
      g_gateway_runtime_counters.room_output_projection_released.load();
  auto *first = create_gateway_session_for_test(
      "gw-test-owner-room-timeout-wave-a", "/clone/gateway_login_example", 123);
  auto *second = create_gateway_session_for_test(
      "gw-test-owner-room-timeout-wave-b", "/clone/gateway_login_example", 124);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  add_ref(first,
          "TestGatewayOwnerRoomOutputWaveTimeoutPreservesCompletedRecipient");
  add_ref(second,
          "TestGatewayOwnerRoomOutputWaveTimeoutPreservesCompletedRecipient");
  vm_owner_set_id(first, "owner/test/gateway/room-timeout-wave-a");
  vm_owner_set_id(second, "owner/test/gateway/room-timeout-wave-b");
  auto *first_session = gateway_find_session_by_object(first);
  auto *second_session = gateway_find_session_by_object(second);
  ASSERT_NE(first_session, nullptr);
  ASSERT_NE(second_session, nullptr);
  const auto first_reservation = gateway_reserve_session_output_for_object(first);
  const auto second_reservation = gateway_reserve_session_output_for_object(second);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(first_session, first_reservation));
  ASSERT_TRUE(gateway_test_seed_owner_room_event(second_session, second_reservation));
  std::string first_expected;
  std::string second_expected;
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(
      first_session, first_reservation, &first_expected,
      "owner-room-player-a", 19));
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(
      second_session, second_reservation, &second_expected,
      "owner-room-player-b", 20));

  std::atomic<int> blocker_started{0};
  std::atomic<int> release_blocker{0};
  vm_owner_thread_start(2);
  struct OwnerThreadTimeoutGuard {
    std::atomic<int> &release;
    bool active{true};

    ~OwnerThreadTimeoutGuard() {
      if (active) {
        release.store(1, std::memory_order_release);
        vm_owner_thread_stop();
      }
    }

    void stop() {
      release.store(1, std::memory_order_release);
      vm_owner_thread_stop();
      active = false;
    }
  } owner_thread_guard{release_blocker};
  ASSERT_GT(vm_owner_enqueue_executor_task(
                second, "room_output_projection", "unit-timeout-wave-blocker",
                [&] {
                  blocker_started.store(1, std::memory_order_release);
                  while (release_blocker.load(std::memory_order_acquire) == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
                }),
            0u);
  for (int index = 0;
       index < 200 && blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {first, second}, {first_reservation, second_reservation},
      {"owner-room-player-a", "owner-room-player-b"}, {19, 20}, 1,
      &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true, true}));
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 2);
  for (int index = 0; index < 200; ++index) {
    if (vm_owner_future_state(submitted.future_ids[0]) ==
        VM_OWNER_FUTURE_COMPLETED) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_PENDING);

  ASSERT_GE(gateway_process_session_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(first_session->output_fifo.size(), 1u);
  ASSERT_TRUE(first_session->output_fifo.front().ready);
  ASSERT_EQ(first_session->output_fifo.front().wire_bytes, first_expected);
  ASSERT_EQ(second_session->output_fifo.size(), 1u);
  ASSERT_TRUE(second_session->output_fifo.front().ready);
  ASSERT_EQ(second_session->output_fifo.front().wire_bytes, second_expected);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_EQ(g_gateway_runtime_counters.room_output_projection_completed.load(),
            completed_before + 2);
  ASSERT_EQ(g_gateway_runtime_counters.room_output_projection_released.load(),
            released_before);
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_room_output_state", first),
               "completed");
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_room_output_state", second),
               "completed");
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_room_output_event_count", first),
            1);
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_room_output_slot_server_seq", first),
            1201);

  release_blocker.store(1, std::memory_order_release);
  ASSERT_TRUE(gateway_test_drain_owner_room_mailbox(
      second, "unit-timeout-wave-drain"));
  owner_thread_guard.stop();
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-timeout-wave-a", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-timeout-wave-b", "test_done", "done"),
            1);
  destruct_object(first);
  destruct_object(second);
  free_object(
      &first, "TestGatewayOwnerRoomOutputWaveTimeoutPreservesCompletedRecipient");
  free_object(
      &second, "TestGatewayOwnerRoomOutputWaveTimeoutPreservesCompletedRecipient");
}

TEST_F(DriverTest,
       TestGatewayOwnerRoomOutputWaveDisconnectReleasesOtherRecipient) {
  const auto completed_before =
      g_gateway_runtime_counters.room_output_projection_completed.load();
  const auto released_before =
      g_gateway_runtime_counters.room_output_projection_released.load();
  auto *first = create_gateway_session_for_test(
      "gw-test-owner-room-disconnect-wave-a", "/clone/gateway_login_example",
      125);
  auto *second = create_gateway_session_for_test(
      "gw-test-owner-room-disconnect-wave-b", "/clone/gateway_login_example",
      126);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  add_ref(first,
          "TestGatewayOwnerRoomOutputWaveDisconnectReleasesOtherRecipient");
  add_ref(second,
          "TestGatewayOwnerRoomOutputWaveDisconnectReleasesOtherRecipient");
  vm_owner_set_id(first, "owner/test/gateway/room-disconnect-wave-a");
  vm_owner_set_id(second, "owner/test/gateway/room-disconnect-wave-b");
  auto *first_session = gateway_find_session_by_object(first);
  auto *second_session = gateway_find_session_by_object(second);
  ASSERT_NE(first_session, nullptr);
  ASSERT_NE(second_session, nullptr);
  const auto first_reservation = gateway_reserve_session_output_for_object(first);
  const auto second_reservation = gateway_reserve_session_output_for_object(second);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(first_session, first_reservation));
  ASSERT_TRUE(gateway_test_seed_owner_room_event(second_session, second_reservation));

  std::atomic<int> blocker_started{0};
  std::atomic<int> release_blocker{0};
  vm_owner_thread_start(2);
  struct OwnerThreadDisconnectGuard {
    std::atomic<int> &release;
    bool active{true};

    ~OwnerThreadDisconnectGuard() {
      if (active) {
        release.store(1, std::memory_order_release);
        vm_owner_thread_stop();
      }
    }

    void stop() {
      release.store(1, std::memory_order_release);
      vm_owner_thread_stop();
      active = false;
    }
  } owner_thread_guard{release_blocker};
  ASSERT_GT(vm_owner_enqueue_executor_task(
                second, "room_output_projection",
                "unit-disconnect-wave-blocker", [&] {
                  blocker_started.store(1, std::memory_order_release);
                  while (release_blocker.load(std::memory_order_acquire) == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
                }),
            0u);
  for (int index = 0;
       index < 200 && blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {first, second}, {first_reservation, second_reservation},
      {"owner-room-player-a", "owner-room-player-b"}, {19, 20}, 1000,
      &submitted));
  for (int index = 0; index < 200; ++index) {
    if (vm_owner_future_state(submitted.future_ids[0]) ==
        VM_OWNER_FUTURE_COMPLETED) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(gateway_process_session_future_watches_at(0), 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 1);
  ASSERT_FALSE(first_session->output_fifo.front().ready);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-disconnect-wave-b", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(first_session->output_fifo.size(), 1u);
  ASSERT_TRUE(first_session->output_fifo.front().ready);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_EQ(g_gateway_runtime_counters.room_output_projection_completed.load(),
            completed_before + 1);
  ASSERT_EQ(g_gateway_runtime_counters.room_output_projection_released.load(),
            released_before + 1);
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_room_output_state", first),
               "completed");
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_room_output_state", second),
               "released");

  release_blocker.store(1, std::memory_order_release);
  ASSERT_TRUE(gateway_test_drain_owner_room_mailbox(
      second, "unit-disconnect-wave-drain"));
  owner_thread_guard.stop();
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-disconnect-wave-a", "test_done", "done"),
            1);
  destruct_object(first);
  destruct_object(second);
  free_object(
      &first,
      "TestGatewayOwnerRoomOutputWaveDisconnectReleasesOtherRecipient");
  free_object(
      &second,
      "TestGatewayOwnerRoomOutputWaveDisconnectReleasesOtherRecipient");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputRejectsLiveSessionIdMismatch) {
  constexpr const char *kSessionId = "gw-test-owner-room-mismatch";
  auto *ob = create_gateway_session_for_test(
      kSessionId, "/clone/gateway_login_example", 116);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayOwnerRoomOutputRejectsLiveSessionIdMismatch");
  vm_owner_set_id(ob, "owner/test/gateway/room-output-mismatch");
  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(session, reservation_id));

  vm_owner_thread_start(1);
  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {ob}, {reservation_id}, {"owner-room-player"}, {19}, 1000,
      &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true}));
  for (int i = 0; i < 200; ++i) {
    const auto state = vm_owner_future_state(submitted.future_ids[0]);
    if (state == VM_OWNER_FUTURE_COMPLETED ||
        state == VM_OWNER_FUTURE_FAILED) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_COMPLETED);
  session->session_id = "live-session-id-mismatch";
  ASSERT_EQ(gateway_process_session_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            1);
  session->session_id = kSessionId;
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_TRUE(session->output_fifo.empty());
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_room_output_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_room_output_state", ob),
               "released");
  vm_owner_thread_stop();

  ASSERT_EQ(gateway_destroy_session_internal(
                kSessionId, "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayOwnerRoomOutputRejectsLiveSessionIdMismatch");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputFallsBackInlineByteEquivalent) {
  auto *ob = create_gateway_session_for_test(
      "gw-test-owner-room-inline", "/clone/gateway_login_example", 112);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayOwnerRoomOutputFallsBackInlineByteEquivalent");
  vm_owner_set_id(ob, "owner/test/gateway/room-output-inline");
  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);

  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(session, reservation_id));
  std::string expected_wire;
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(
      session, reservation_id, &expected_wire));

  ASSERT_FALSE(vm_owner_executor_available());
  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {ob}, {reservation_id}, {"owner-room-player"}, {19}, 1000,
      &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{false}));
  ASSERT_EQ(submitted.filled_inline, (std::vector<bool>{true}));
  ASSERT_EQ(submitted.future_ids, (std::vector<uint64_t>{0}));
  ASSERT_EQ(submitted.event_counts, (std::vector<LPC_INT>{1}));
  ASSERT_EQ(submitted.text_length_totals, (std::vector<LPC_INT>{0}));
  ASSERT_EQ(submitted.slot_server_seqs, (std::vector<LPC_INT>{1201}));
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(session->output_fifo.size(), 1u);
  ASSERT_TRUE(session->output_fifo.front().ready);
  ASSERT_EQ(session->output_fifo.front().reservation_id, reservation_id);
  ASSERT_EQ(session->output_fifo.front().wire_bytes, expected_wire);

  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-inline", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(&ob,
              "TestGatewayOwnerRoomOutputFallsBackInlineByteEquivalent");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputStagesInlineFallbackBatchBeforeFill) {
  auto *first = create_gateway_session_for_test(
      "gw-test-owner-room-inline-batch-a", "/clone/gateway_login_example", 117);
  auto *second = create_gateway_session_for_test(
      "gw-test-owner-room-inline-batch-b", "/clone/gateway_login_example", 118);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  add_ref(first, "TestGatewayOwnerRoomOutputStagesInlineFallbackBatchBeforeFill");
  add_ref(second, "TestGatewayOwnerRoomOutputStagesInlineFallbackBatchBeforeFill");
  vm_owner_set_id(first, "owner/test/gateway/room-output-inline-batch-a");
  vm_owner_set_id(second, "owner/test/gateway/room-output-inline-batch-b");
  auto *first_session = gateway_find_session_by_object(first);
  auto *second_session = gateway_find_session_by_object(second);
  ASSERT_NE(first_session, nullptr);
  ASSERT_NE(second_session, nullptr);
  const auto first_reservation = gateway_reserve_session_output_for_object(first);
  const auto second_reservation = gateway_reserve_session_output_for_object(second);
  ASSERT_GT(first_reservation, 0u);
  ASSERT_GT(second_reservation, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(first_session, first_reservation));
  ASSERT_TRUE(gateway_test_seed_owner_room_event(second_session, second_reservation));
  std::string first_expected;
  std::string second_expected;
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(
      first_session, first_reservation, &first_expected,
      "owner-room-player-a", 19));
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(
      second_session, second_reservation, &second_expected,
      "owner-room-player-b", 20));

  ASSERT_FALSE(vm_owner_executor_available());
  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {first, second}, {first_reservation, second_reservation},
      {"owner-room-player-a", "owner-room-player-b"}, {19, 20}, 1000,
      &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{false, false}));
  ASSERT_EQ(submitted.filled_inline, (std::vector<bool>{true, true}));
  ASSERT_EQ(submitted.future_ids, (std::vector<uint64_t>{0, 0}));
  ASSERT_EQ(first_session->output_fifo.size(), 1u);
  ASSERT_EQ(second_session->output_fifo.size(), 1u);
  ASSERT_TRUE(first_session->output_fifo.front().ready);
  ASSERT_TRUE(second_session->output_fifo.front().ready);
  ASSERT_EQ(first_session->output_fifo.front().wire_bytes, first_expected);
  ASSERT_EQ(second_session->output_fifo.front().wire_bytes, second_expected);

  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-inline-batch-a", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-inline-batch-b", "test_done", "done"),
            1);
  destruct_object(first);
  destruct_object(second);
  free_object(&first,
              "TestGatewayOwnerRoomOutputStagesInlineFallbackBatchBeforeFill");
  free_object(&second,
              "TestGatewayOwnerRoomOutputStagesInlineFallbackBatchBeforeFill");
}

TEST_F(DriverTest,
       TestGatewayOwnerRoomOutputInvalidInlineInputWritesNoRecipient) {
  auto *first = create_gateway_session_for_test(
      "gw-test-owner-room-inline-failure-a", "/clone/gateway_login_example",
      119);
  auto *second = create_gateway_session_for_test(
      "gw-test-owner-room-inline-failure-b", "/clone/gateway_login_example",
      120);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  add_ref(
      first,
      "TestGatewayOwnerRoomOutputInvalidInlineInputWritesNoRecipient");
  add_ref(
      second,
      "TestGatewayOwnerRoomOutputInvalidInlineInputWritesNoRecipient");
  vm_owner_set_id(first, "owner/test/gateway/room-output-inline-failure-a");
  vm_owner_set_id(second, "owner/test/gateway/room-output-inline-failure-b");
  auto *first_session = gateway_find_session_by_object(first);
  auto *second_session = gateway_find_session_by_object(second);
  ASSERT_NE(first_session, nullptr);
  ASSERT_NE(second_session, nullptr);
  const auto first_reservation = gateway_reserve_session_output_for_object(first);
  const auto second_reservation = gateway_reserve_session_output_for_object(second);
  ASSERT_GT(first_reservation, 0u);
  ASSERT_GT(second_reservation, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(first_session, first_reservation));
  ASSERT_TRUE(gateway_test_seed_owner_room_event(second_session, second_reservation));

  ASSERT_FALSE(vm_owner_executor_available());
  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  const std::string invalid_utf8_scope(1, static_cast<char>(0xc3));
  ASSERT_FALSE(gateway_submit_pending_message_event_batches_for_objects(
      {first, second}, {first_reservation, second_reservation},
      {"owner-room-player-a", invalid_utf8_scope}, {19, 20}, 1000,
      &submitted));
  ASSERT_TRUE(submitted.submitted.empty());
  ASSERT_TRUE(submitted.filled_inline.empty());
  ASSERT_TRUE(submitted.future_ids.empty());
  ASSERT_EQ(first_session->output_fifo.size(), 1u);
  ASSERT_EQ(second_session->output_fifo.size(), 1u);
  ASSERT_FALSE(first_session->output_fifo.front().ready);
  ASSERT_FALSE(second_session->output_fifo.front().ready);
  ASSERT_TRUE(first_session->output_fifo.front().wire_bytes.empty());
  ASSERT_TRUE(second_session->output_fifo.front().wire_bytes.empty());
  ASSERT_EQ(gateway_fill_pending_message_event_batch_for_object(
                first, first_reservation, "owner-room-player-a",
                std::strlen("owner-room-player-a"), 19),
            1);
  ASSERT_TRUE(first_session->output_fifo.front().ready);
  ASSERT_FALSE(first_session->output_fifo.front().wire_bytes.empty());
  ASSERT_EQ(gateway_release_session_output(second_session, second_reservation),
            1);

  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-inline-failure-a", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-inline-failure-b", "test_done", "done"),
            1);
  destruct_object(first);
  destruct_object(second);
  free_object(
      &first,
      "TestGatewayOwnerRoomOutputInvalidInlineInputWritesNoRecipient");
  free_object(
      &second,
      "TestGatewayOwnerRoomOutputInvalidInlineInputWritesNoRecipient");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputTimeoutFallsBackInline) {
  auto *ob = create_gateway_session_for_test(
      "gw-test-owner-room-timeout", "/clone/gateway_login_example", 113);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayOwnerRoomOutputTimeoutFallsBackInline");
  vm_owner_set_id(ob, "owner/test/gateway/room-output-timeout");
  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);

  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(session, reservation_id));
  std::string expected_wire;
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(
      session, reservation_id, &expected_wire));

  vm_owner_thread_start(1);
  std::atomic<int> blocker_started{0};
  ASSERT_GT(vm_owner_enqueue_executor_task(
                ob, "room_output_projection", "unit-timeout-blocker", [&] {
                  blocker_started.store(1, std::memory_order_release);
                  std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }),
            0u);
  for (int i = 0; i < 200 &&
                  blocker_started.load(std::memory_order_acquire) == 0;
       ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {ob}, {reservation_id}, {"owner-room-player"}, {19}, 1,
      &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true}));
  ASSERT_EQ(submitted.future_ids.size(), 1u);
  ASSERT_GT(submitted.future_ids[0], 0u);
  ASSERT_EQ(gateway_process_session_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            1);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(session->output_fifo.size(), 1u);
  ASSERT_TRUE(session->output_fifo.front().ready);
  ASSERT_EQ(session->output_fifo.front().wire_bytes, expected_wire);
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_room_output_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_room_output_state", ob),
               "completed");
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_room_output_event_count", ob),
            1);
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_room_output_slot_server_seq", ob),
            1201);
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_room_output_callback_off_main", ob),
            0);
  ASSERT_TRUE(gateway_test_drain_owner_room_mailbox(
      ob, "unit-timeout-drain"));
  vm_owner_thread_stop();

  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-timeout", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(
      &ob, "TestGatewayOwnerRoomOutputTimeoutFallsBackInline");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputDisconnectConsumesFuture) {
  auto *ob = create_gateway_session_for_test(
      "gw-test-owner-room-disconnect", "/clone/gateway_login_example", 114);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayOwnerRoomOutputDisconnectConsumesFuture");
  vm_owner_set_id(ob, "owner/test/gateway/room-output-disconnect");
  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);

  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(session, reservation_id));

  vm_owner_thread_start(1);
  std::atomic<int> blocker_started{0};
  ASSERT_GT(vm_owner_enqueue_executor_task(
                ob, "room_output_projection", "unit-disconnect-blocker", [&] {
                  blocker_started.store(1, std::memory_order_release);
                  std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }),
            0u);
  for (int i = 0; i < 200 &&
                  blocker_started.load(std::memory_order_acquire) == 0;
       ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {ob}, {reservation_id}, {"owner-room-player"}, {19}, 1000,
      &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true}));
  ASSERT_EQ(gateway_session_future_watch_count(), 1);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-disconnect", "test_done", "done"),
            1);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_find_session("gw-test-owner-room-disconnect"), nullptr);
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_room_output_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_room_output_state", ob),
               "released");
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_room_output_callback_off_main", ob),
            0);
  ASSERT_TRUE(gateway_test_drain_owner_room_mailbox(
      ob, "unit-disconnect-drain"));
  vm_owner_thread_stop();

  destruct_object(ob);
  free_object(&ob, "TestGatewayOwnerRoomOutputDisconnectConsumesFuture");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputStaleOwnerEpochFallsBackInline) {
  auto *ob = create_gateway_session_for_test(
      "gw-test-owner-room-stale", "/clone/gateway_login_example", 115);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayOwnerRoomOutputStaleOwnerEpochFallsBackInline");
  vm_owner_set_id(ob, "owner/test/gateway/room-output-stale-a");
  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);

  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(session, reservation_id));
  std::string expected_wire;
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(
      session, reservation_id, &expected_wire));

  vm_owner_thread_start(1);
  std::atomic<int> blocker_started{0};
  ASSERT_GT(vm_owner_enqueue_executor_task(
                ob, "room_output_projection", "unit-stale-blocker", [&] {
                  blocker_started.store(1, std::memory_order_release);
                  std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }),
            0u);
  for (int i = 0; i < 200 &&
                  blocker_started.load(std::memory_order_acquire) == 0;
       ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {ob}, {reservation_id}, {"owner-room-player"}, {19}, 1000,
      &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true}));
  vm_owner_set_id(ob, "owner/test/gateway/room-output-stale-b");
  ASSERT_EQ(gateway_process_session_future_watches_at(
                std::numeric_limits<uint64_t>::max()),
            1);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]),
            VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(session->output_fifo.size(), 1u);
  ASSERT_TRUE(session->output_fifo.front().ready);
  ASSERT_EQ(session->output_fifo.front().wire_bytes, expected_wire);
  ASSERT_EQ(gateway_test_call_number(
                "query_last_owner_room_output_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_STREQ(gateway_test_call_string(
                   "query_last_owner_room_output_state", ob),
               "completed");
  vm_owner_set_id(ob, "owner/test/gateway/room-output-stale-a");
  ASSERT_TRUE(gateway_test_drain_owner_room_mailbox(
      ob, "unit-stale-drain"));
  vm_owner_thread_stop();

  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-owner-room-stale", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayOwnerRoomOutputStaleOwnerEpochFallsBackInline");
}

TEST_F(DriverTest, TestGatewayFutureWatchTimesOutAndReleasesReservation) {
  auto *ob = create_gateway_session_for_test("gw-test-future-watch-timeout",
                                             "/clone/gateway_login_example", 95);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayFutureWatchTimesOutAndReleasesReservation");
  vm_owner_set_id(ob, "owner/test/gateway/future-timeout");

  auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  push_number(41);
  auto *submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_GT(future_id, 0);
  ASSERT_EQ(gateway_watch_session_future_for_object(
                ob, reservation_id, static_cast<uint64_t>(future_id), 1),
            1);

  ASSERT_EQ(gateway_process_session_future_watches_at(std::numeric_limits<uint64_t>::max()), 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  auto *future = call_lpc_method(ob, "query_last_owner_future");
  ASSERT_NE(future, nullptr);
  ASSERT_EQ(future->type, T_MAPPING);
  ASSERT_STREQ(gateway_test_mapping_string(future->u.map, "state"), "failed");
  ASSERT_EQ(gateway_test_mapping_number(future->u.map, "timed_out"), 1);
  auto *session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->output_fifo.empty());
  auto *consumed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_STREQ(gateway_test_mapping_string(consumed, "state"), "unknown");
  free_mapping(consumed);

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-future-watch-timeout", "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayFutureWatchTimesOutAndReleasesReservation");
}

TEST_F(DriverTest, TestGatewayFutureWatchRejectsAnotherSessionFuture) {
  auto *source = create_gateway_session_for_test("gw-test-future-watch-source",
                                                 "/clone/gateway_login_example", 96);
  auto *target = create_gateway_session_for_test("gw-test-future-watch-target",
                                                 "/clone/gateway_login_example", 97);
  ASSERT_NE(source, nullptr);
  ASSERT_NE(target, nullptr);
  add_ref(source, "TestGatewayFutureWatchRejectsAnotherSessionFuture/source");
  add_ref(target, "TestGatewayFutureWatchRejectsAnotherSessionFuture/target");
  vm_owner_set_id(source, "owner/test/gateway/future-source");
  vm_owner_set_id(target, "owner/test/gateway/future-target");

  push_number(41);
  auto *submitted = call_lpc_method(source, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_GT(future_id, 0);
  auto reservation_id = gateway_reserve_session_output_for_object(target);
  ASSERT_GT(reservation_id, 0u);

  ASSERT_EQ(gateway_watch_session_future_for_object(
                target, reservation_id, static_cast<uint64_t>(future_id), 1000),
            0);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_release_session_output_for_object(target, reservation_id), 1);
  auto *cancelled = vm_owner_future_cancel(static_cast<uint64_t>(future_id), "test cleanup");
  free_mapping(cancelled);
  auto *consumed = vm_owner_future_take(static_cast<uint64_t>(future_id));
  ASSERT_STREQ(gateway_test_mapping_string(consumed, "state"), "failed");
  free_mapping(consumed);

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-future-watch-source", "test_done", "done"), 1);
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-future-watch-target", "test_done", "done"), 1);
  destruct_object(source);
  destruct_object(target);
  free_object(&source, "TestGatewayFutureWatchRejectsAnotherSessionFuture/source");
  free_object(&target, "TestGatewayFutureWatchRejectsAnotherSessionFuture/target");
}

TEST_F(DriverTest, TestGatewayGenericFutureWatchDispatchesNonSessionObjectOnMain) {
  auto *ob = clone_object_for_test("clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayGenericFutureWatchDispatchesNonSessionObjectOnMain");
  ASSERT_FALSE(gateway_is_session(ob));
  vm_owner_set_id(ob, "owner/test/gateway/generic-future-completed");

  push_number(41);
  auto *submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_GT(future_id, 0);
  push_number(701);
  push_number(future_id);
  push_number(1000);
  auto *watched = call_lpc_method(ob, "watch_generic_owner_future", 3);
  ASSERT_NE(watched, nullptr);
  ASSERT_EQ(watched->type, T_NUMBER);
  ASSERT_EQ(watched->u.number, 1);
  ASSERT_EQ(gateway_future_watch_count(), 1);

  vm_owner_thread_start(1);
  for (int i = 0; i < 200; i++) {
    auto *future = vm_owner_future_poll(static_cast<uint64_t>(future_id));
    auto completed = std::string(gateway_test_mapping_string(future, "state")) == "completed";
    free_mapping(future);
    if (completed) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  vm_owner_thread_stop();

  for (int i = 0; i < 200; i++) {
    event_base_loop(g_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK);
    if (gateway_test_call_number("query_last_generic_owner_future_context_id", ob) == 701) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  ASSERT_EQ(gateway_future_watch_count(), 0);
  ASSERT_EQ(gateway_test_call_number("query_last_generic_owner_future_context_id", ob), 701);
  ASSERT_EQ(gateway_test_call_number("query_last_generic_owner_future_callback_off_main", ob), 0);
  auto *future = call_lpc_method(ob, "query_last_generic_owner_future");
  ASSERT_NE(future, nullptr);
  ASSERT_EQ(future->type, T_MAPPING);
  ASSERT_STREQ(gateway_test_mapping_string(future->u.map, "state"), "completed");
  auto *result = find_string_in_mapping(future->u.map, "result");
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_MAPPING);
  ASSERT_EQ(gateway_test_mapping_number(result->u.map, "value"), 42);
  auto *consumed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_STREQ(gateway_test_mapping_string(consumed, "state"), "unknown");
  free_mapping(consumed);

  destruct_object(ob);
  free_object(&ob, "TestGatewayGenericFutureWatchDispatchesNonSessionObjectOnMain");
}

TEST_F(DriverTest, TestGatewayGenericFutureWatchPollCountersReportBudgetedWork) {
  auto *ob = clone_object_for_test("clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayGenericFutureWatchPollCountersReportBudgetedWork");
  vm_owner_set_id(ob, "owner/test/gateway/generic-future-poll-counters");

  std::vector<uint64_t> future_ids;
  future_ids.reserve(65);
  for (int index = 0; index < 65; ++index) {
    push_number(index);
    auto *submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
    ASSERT_NE(submitted, nullptr);
    ASSERT_EQ(submitted->type, T_MAPPING);
    auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
    ASSERT_GT(future_id, 0);
    ASSERT_EQ(gateway_watch_future_for_object(
                  ob, static_cast<uint64_t>(800 + index),
                  static_cast<uint64_t>(future_id), 1000),
              1);
    future_ids.push_back(static_cast<uint64_t>(future_id));
  }
  ASSERT_EQ(gateway_future_watch_count(), 65);

  auto *before = gateway_status_internal();
  ASSERT_NE(before, nullptr);
  const auto poll_runs_before = gateway_test_mapping_number(
      before, "gateway_generic_future_watch_poll_runs");
  const auto poll_items_before = gateway_test_mapping_number(
      before, "gateway_generic_future_watch_poll_items");
  const auto budget_hits_before = gateway_test_mapping_number(
      before, "gateway_generic_future_watch_poll_budget_hits");
  free_mapping(before);

  ASSERT_EQ(gateway_process_future_watches_at(0), 0);
  ASSERT_EQ(gateway_future_watch_count(), 65);

  auto *after = gateway_status_internal();
  ASSERT_NE(after, nullptr);
  EXPECT_EQ(gateway_test_mapping_number(
                after, "gateway_generic_future_watch_poll_runs") -
                poll_runs_before,
            1);
  EXPECT_EQ(gateway_test_mapping_number(
                after, "gateway_generic_future_watch_poll_items") -
                poll_items_before,
            64);
  EXPECT_EQ(gateway_test_mapping_number(
                after, "gateway_generic_future_watch_poll_budget_hits") -
                budget_hits_before,
            1);
  free_mapping(after);

  destruct_object(ob);
  for (int attempt = 0;
       attempt < 4 && gateway_future_watch_count() != 0; ++attempt) {
    gateway_process_future_watches_at(std::numeric_limits<uint64_t>::max());
  }
  ASSERT_EQ(gateway_future_watch_count(), 0);
  for (const auto future_id : future_ids) {
    EXPECT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_UNKNOWN);
  }
  free_object(&ob, "TestGatewayGenericFutureWatchPollCountersReportBudgetedWork");
}

TEST_F(DriverTest, TestGatewaySessionWatchCancellationKeepsGenericWatchWakeups) {
  auto *generic_ob = clone_object_for_test("clone/gateway_login_example");
  auto *session_ob = create_gateway_session_for_test(
      "gw-test-mixed-future-watch", "/clone/gateway_login_example", 117);
  ASSERT_NE(generic_ob, nullptr);
  ASSERT_NE(session_ob, nullptr);
  add_ref(generic_ob,
          "TestGatewaySessionWatchCancellationKeepsGenericWatchWakeups/generic");
  add_ref(session_ob,
          "TestGatewaySessionWatchCancellationKeepsGenericWatchWakeups/session");
  vm_owner_set_id(generic_ob, "owner/test/gateway/mixed-watch-generic");
  vm_owner_set_id(session_ob, "owner/test/gateway/mixed-watch-session");

  push_number(41);
  auto *generic_submitted =
      call_lpc_method(generic_ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(generic_submitted, nullptr);
  ASSERT_EQ(generic_submitted->type, T_MAPPING);
  const auto generic_future_id =
      gateway_test_mapping_number(generic_submitted->u.map, "future_id");
  ASSERT_GT(generic_future_id, 0);
  ASSERT_EQ(gateway_watch_future_for_object(
                generic_ob, 706, static_cast<uint64_t>(generic_future_id), 1000),
            1);

  push_number(51);
  auto *session_submitted =
      call_lpc_method(session_ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(session_submitted, nullptr);
  ASSERT_EQ(session_submitted->type, T_MAPPING);
  const auto session_future_id =
      gateway_test_mapping_number(session_submitted->u.map, "future_id");
  ASSERT_GT(session_future_id, 0);
  const auto reservation_id =
      gateway_reserve_session_output_for_object(session_ob);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_EQ(gateway_watch_session_future_for_object(
                session_ob, reservation_id,
                static_cast<uint64_t>(session_future_id), 1000),
            1);
  ASSERT_EQ(gateway_future_watch_count(), 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 1);

  // The timer is the required fallback when terminal notifications are
  // temporarily unavailable. Cancelling an unrelated session watch must not
  // delete that timer while the generic watch is still live.
  vm_owner_set_future_terminal_notifier(nullptr);
  ASSERT_EQ(gateway_destroy_session_internal(
                "gw-test-mixed-future-watch", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_future_watch_count(), 1);

  vm_owner_thread_start(1);
  for (int i = 0; i < 200; ++i) {
    if (vm_owner_future_state(static_cast<uint64_t>(generic_future_id)) ==
        VM_OWNER_FUTURE_COMPLETED) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  for (int i = 0; i < 200; ++i) {
    event_base_loop(g_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK);
    if (gateway_test_call_number(
            "query_last_generic_owner_future_context_id", generic_ob) == 706) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  vm_owner_thread_stop();

  const auto dispatched = gateway_test_call_number(
      "query_last_generic_owner_future_context_id", generic_ob);
  EXPECT_EQ(dispatched, 706);
  if (gateway_future_watch_count() != 0) {
    gateway_process_future_watches_at(std::numeric_limits<uint64_t>::max());
  }
  EXPECT_EQ(gateway_future_watch_count(), 0);
  EXPECT_EQ(vm_owner_future_state(static_cast<uint64_t>(generic_future_id)),
            VM_OWNER_FUTURE_UNKNOWN);

  destruct_object(generic_ob);
  destruct_object(session_ob);
  free_object(
      &generic_ob,
      "TestGatewaySessionWatchCancellationKeepsGenericWatchWakeups/generic");
  free_object(
      &session_ob,
      "TestGatewaySessionWatchCancellationKeepsGenericWatchWakeups/session");
}

TEST_F(DriverTest, TestGatewayGenericFutureWatchRejectsAnotherObjectFuture) {
  auto *source = clone_object_for_test("clone/gateway_login_example");
  auto *target = clone_object_for_test("clone/gateway_login_example");
  ASSERT_NE(source, nullptr);
  ASSERT_NE(target, nullptr);
  vm_owner_set_id(source, "owner/test/gateway/generic-future-source");
  vm_owner_set_id(target, "owner/test/gateway/generic-future-target");

  push_number(41);
  auto *submitted = call_lpc_method(source, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_GT(future_id, 0);
  ASSERT_EQ(gateway_watch_future_for_object(target, 702, static_cast<uint64_t>(future_id), 1000), 0);
  ASSERT_EQ(gateway_future_watch_count(), 0);

  auto *cancelled = vm_owner_future_cancel(static_cast<uint64_t>(future_id), "test cleanup");
  free_mapping(cancelled);
  auto *consumed = vm_owner_future_take(static_cast<uint64_t>(future_id));
  free_mapping(consumed);
  destruct_object(source);
  destruct_object(target);
}

TEST_F(DriverTest, TestGatewayGenericFutureWatchTimesOutAndConsumesFuture) {
  auto *ob = clone_object_for_test("clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  vm_owner_set_id(ob, "owner/test/gateway/generic-future-timeout");

  push_number(41);
  auto *submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_GT(future_id, 0);
  ASSERT_EQ(gateway_watch_future_for_object(ob, 703, static_cast<uint64_t>(future_id), 1), 1);
  ASSERT_EQ(gateway_process_future_watches_at(std::numeric_limits<uint64_t>::max()), 1);
  ASSERT_EQ(gateway_future_watch_count(), 0);

  auto *future = call_lpc_method(ob, "query_last_generic_owner_future");
  ASSERT_NE(future, nullptr);
  ASSERT_EQ(future->type, T_MAPPING);
  ASSERT_STREQ(gateway_test_mapping_string(future->u.map, "state"), "failed");
  ASSERT_EQ(gateway_test_mapping_number(future->u.map, "timed_out"), 1);
  auto *consumed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_STREQ(gateway_test_mapping_string(consumed, "state"), "unknown");
  free_mapping(consumed);
  destruct_object(ob);
}

TEST_F(DriverTest, TestGatewayGenericFutureWatchTargetDestructCancelsAndConsumesFuture) {
  auto *ob = clone_object_for_test("clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  vm_owner_set_id(ob, "owner/test/gateway/generic-future-destruct");

  push_number(41);
  auto *submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_GT(future_id, 0);
  ASSERT_EQ(gateway_watch_future_for_object(ob, 704, static_cast<uint64_t>(future_id), 1000), 1);
  destruct_object(ob);

  ASSERT_EQ(gateway_process_future_watches_at(0), 1);
  ASSERT_EQ(gateway_future_watch_count(), 0);
  auto *consumed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_STREQ(gateway_test_mapping_string(consumed, "state"), "unknown");
  free_mapping(consumed);
}

TEST_F(DriverTest, TestGatewayGenericAndSessionWatchCannotConsumeSameFuture) {
  auto *ob = create_gateway_session_for_test("gw-test-generic-future-exclusive",
                                             "/clone/gateway_login_example", 99);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayGenericAndSessionWatchCannotConsumeSameFuture");
  vm_owner_set_id(ob, "owner/test/gateway/generic-future-exclusive");

  push_number(41);
  auto *submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_GT(future_id, 0);
  ASSERT_EQ(gateway_watch_future_for_object(ob, 705, static_cast<uint64_t>(future_id), 1000), 1);

  auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_EQ(gateway_watch_session_future_for_object(
                ob, reservation_id, static_cast<uint64_t>(future_id), 1000),
            0);
  ASSERT_EQ(gateway_release_session_output_for_object(ob, reservation_id), 1);
  ASSERT_EQ(gateway_process_future_watches_at(std::numeric_limits<uint64_t>::max()), 1);
  ASSERT_EQ(gateway_future_watch_count(), 0);

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-generic-future-exclusive",
                                             "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayGenericAndSessionWatchCannotConsumeSameFuture");
}

TEST_F(DriverTest, TestOwnerFutureCancelEfunSupportsSubmitRollback) {
  auto *ob = create_gateway_session_for_test("gw-test-future-cancel-efun",
                                             "/clone/gateway_login_example", 98);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestOwnerFutureCancelEfunSupportsSubmitRollback");
  vm_owner_set_id(ob, "owner/test/gateway/future-cancel-efun");

  push_number(41);
  auto *submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_GT(future_id, 0);

  push_number(future_id);
  copy_and_push_string("watch registration failed");
  auto *cancelled = call_lpc_method(ob, "cancel_gateway_owner_future", 2);
  ASSERT_NE(cancelled, nullptr);
  ASSERT_EQ(cancelled->type, T_MAPPING);
  ASSERT_STREQ(gateway_test_mapping_string(cancelled->u.map, "state"), "failed");
  ASSERT_STREQ(gateway_test_mapping_string(cancelled->u.map, "error"),
               "watch registration failed");
  ASSERT_EQ(gateway_test_mapping_number(cancelled->u.map, "cancelled"), 1);

  push_number(future_id);
  auto *taken = call_lpc_method(ob, "take_gateway_owner_future", 1);
  ASSERT_NE(taken, nullptr);
  ASSERT_EQ(taken->type, T_MAPPING);
  ASSERT_STREQ(gateway_test_mapping_string(taken->u.map, "state"), "failed");
  ASSERT_EQ(gateway_test_mapping_number(taken->u.map, "consumed"), 1);
  auto *unknown = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_STREQ(gateway_test_mapping_string(unknown, "state"), "unknown");
  free_mapping(unknown);

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-future-cancel-efun", "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestOwnerFutureCancelEfunSupportsSubmitRollback");
}

TEST_F(DriverTest, TestGatewayDaemonUsesDefaultOwnerForSystemMessages) {
  if (auto *existing = find_object2("adm/daemons/gateway_d.c")) {
    destruct_object_for_test(existing);
  }
  ScopedCurrentObjectAsMaster master_scope;
  auto *player = clone_object_for_test("single/owner_singleton");
  ASSERT_NE(player, nullptr);
  vm_owner_set_id(player, "owner/test/gateway/daemon-player");

  VMOwnerScope scope(vm_context(), vm_owner_id(player), vm_owner_epoch(player));
  current_object = player;
  auto *daemon = load_object_for_test("adm/daemons/gateway_d.c");
  ASSERT_NE(daemon, nullptr);
  ASSERT_STREQ(vm_owner_default_id(), vm_owner_id(daemon));
  ASSERT_NE(vm_owner_id(daemon), vm_owner_id(player));
  auto daemon_epoch = vm_owner_epoch(daemon);

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1, R"({"type":"sys","action":"owner_probe","source":"cpp_test"})"));

  auto *info = call_lpc_method(daemon, "query_last_system_message");
  ASSERT_NE(info, nullptr);
  ASSERT_EQ(info->type, T_MAPPING);
  auto mapping_number = [](mapping_t *map, const char *key) -> LPC_INT {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t *map, const char *key) -> const char * {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
  ASSERT_STREQ(mapping_string(info->u.map, "owner_id"), vm_owner_default_id());
  ASSERT_EQ(mapping_number(info->u.map, "owner_epoch"), static_cast<long>(daemon_epoch));
  ASSERT_NE(std::string(mapping_string(info->u.map, "this_player")).find("adm/daemons/gateway_d"),
            std::string::npos);
  ASSERT_STREQ(mapping_string(info->u.map, "type"), "sys");
  ASSERT_STREQ(mapping_string(info->u.map, "action"), "owner_probe");
  ASSERT_STREQ(mapping_string(info->u.map, "source"), "cpp_test");

  auto *trace = vm_owner_task_trace(16);
  ASSERT_NE(trace, nullptr);
  ASSERT_EQ(mapping_number(trace, "success"), 1);
  auto *events_value = find_string_in_mapping(trace, "events");
  ASSERT_NE(events_value, nullptr);
  ASSERT_EQ(events_value ? events_value->type : T_INVALID, T_ARRAY);
  bool found_gateway_trace = false;
  if (events_value && events_value->type == T_ARRAY) {
    for (int i = 0; i < events_value->u.arr->size; i++) {
      auto *event = events_value->u.arr->item[i].u.map;
      if (std::string(mapping_string(event, "task_type")) == "gateway" &&
          std::string(mapping_string(event, "task_key")) == "receive_system_message" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_default_id() &&
          mapping_number(event, "owner_epoch") == static_cast<long>(daemon_epoch) &&
          std::string(mapping_string(event, "state")) == "dispatched") {
        found_gateway_trace = true;
        break;
      }
    }
  }
  ASSERT_TRUE(found_gateway_trace);
  free_mapping(trace);

  destruct_object_for_test(daemon);
  destruct_object_for_test(player);
}

TEST_F(DriverTest, TestGatewaySessionDestroyCallsGatewayDisconnected) {
  auto *ob = create_gateway_session_for_test("gw-test-destroy", "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));

  add_ref(ob, "TestGatewaySessionDestroyCallsGatewayDisconnected");

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-destroy", "client_close", "bye"), 1);
  ASSERT_EQ(ob->interactive, nullptr);

  auto *code = call_lpc_method(ob, "query_last_disconnect_code");
  ASSERT_NE(code, nullptr);
  ASSERT_EQ(code->type, T_STRING);
  ASSERT_STREQ(code->u.string, "client_close");

  auto *text = call_lpc_method(ob, "query_last_disconnect_text");
  ASSERT_NE(text, nullptr);
  ASSERT_EQ(text->type, T_STRING);
  ASSERT_STREQ(text->u.string, "bye");

  destruct_object(ob);
  free_object(&ob, "TestGatewaySessionDestroyCallsGatewayDisconnected");
}

TEST_F(DriverTest, TestGatewaySessionDestroyAccountsReadyAndPendingFifoEntries) {
  const char *session_id = "gw-test-destroy-fifo-accounting";
  auto *ob = create_gateway_session_for_test(
      session_id, "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewaySessionDestroyAccountsReadyAndPendingFifoEntries");
  auto *session = gateway_find_session(session_id);
  ASSERT_NE(session, nullptr);
  ASSERT_EQ(gateway_enqueue_session_protocol_output(
                session, "ready-on-destroy", 16),
            1);
  ASSERT_GT(gateway_reserve_session_output(session), 0u);
  ASSERT_EQ(session->output_fifo.size(), 2u);
  ASSERT_TRUE(session->output_fifo.front().ready);
  ASSERT_FALSE(session->output_fifo.back().ready);

  const auto ready_before =
      g_gateway_runtime_counters.output_fifo_destroyed_ready.load(
          std::memory_order_relaxed);
  const auto pending_before =
      g_gateway_runtime_counters.output_fifo_destroyed_pending.load(
          std::memory_order_relaxed);
  ASSERT_EQ(gateway_destroy_session_internal(
                session_id, "client_close", "bye"),
            1);
  ASSERT_EQ(gateway_find_session(session_id), nullptr);
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_destroyed_ready.load(
                std::memory_order_relaxed),
            ready_before + 1);
  ASSERT_EQ(g_gateway_runtime_counters.output_fifo_destroyed_pending.load(
                std::memory_order_relaxed),
            pending_before + 1);

  destruct_object(ob);
  free_object(&ob,
              "TestGatewaySessionDestroyAccountsReadyAndPendingFifoEntries");
}

TEST_F(DriverTest, TestGatewaySessionDestroyAllowsNetDeadSelfDestruct) {
  auto *ob = create_gateway_session_for_test("gw-test-net-dead-destruct",
                                             "/clone/gateway_net_dead_destruct_user");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));

  add_ref(ob, "TestGatewaySessionDestroyAllowsNetDeadSelfDestruct");

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-net-dead-destruct", "client_close", "bye"),
            1);
  ASSERT_TRUE(ob->flags & O_DESTRUCTED);
  ASSERT_EQ(ob->interactive, nullptr);

  free_object(&ob, "TestGatewaySessionDestroyAllowsNetDeadSelfDestruct");
}

TEST_F(DriverTest, TestGatewayOutputEnvelopeFastPathIsByteEquivalent) {
  const std::vector<std::pair<std::string, std::string>> cases = {
      {"plain-session", "plain XK frame"},
      {"quote\"slash\\session", "line1\nline2\t\"quoted\"\\tail"},
      {"control", std::string("zero\0one\b\ftwo\r", 14)},
      {"utf8-session-\xe4\xbc\x9a\xe8\xaf\x9d", "\xe4\xbe\xa0\xe5\xae\xa2\xe8\xa1\x8c\xe4\xb8\xad"},
  };

  for (const auto &[session_id, data] : cases) {
    nlohmann::json expected{
        {"type", "output"},
        {"cid", session_id},
        {"data", data},
    };
    ASSERT_EQ(gateway_encode_output_envelope_for_test(
                  session_id, data.data(), data.size()),
              expected.dump());
  }

  const std::string invalid_utf8{"\xc3\x28", 2};
  EXPECT_THROW(gateway_encode_output_envelope_for_test("invalid", invalid_utf8.data(),
                                                        invalid_utf8.size()),
               nlohmann::json::type_error);
  EXPECT_THROW(gateway_encode_output_envelope_for_test(invalid_utf8, "frame", 5),
               nlohmann::json::type_error);
}

TEST_F(DriverTest, TestGatewayMasterReconnectRebindsExistingSessionWithoutLosingState) {
  static std::vector<std::pair<int, std::string>> writes;
  writes.clear();
  auto writer = [](int fd, const char *data, size_t len) -> int {
    writes.emplace_back(fd, std::string(data, len));
    return 1;
  };
  constexpr int old_master_fd = 1701;
  constexpr int new_master_fd = 1702;
  const char *session_id = "gw-test-master-reconnect";
  auto *ob = create_gateway_session_for_test(session_id, "/clone/gateway_login_example",
                                             old_master_fd);
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  add_ref(ob, "TestGatewayMasterReconnectRebindsExistingSessionWithoutLosingState");

  auto *session = gateway_find_session(session_id);
  ASSERT_NE(session, nullptr);
  ASSERT_EQ(session->master_fd, old_master_fd);
  ASSERT_EQ(ob->interactive->gateway_master_fd, old_master_fd);
  const auto detached_bytes_before =
      gateway_session_fifo_wire_detached_bytes();

  gateway_cleanup_master_sessions(old_master_fd);
  session = gateway_find_session(session_id);
  ASSERT_NE(session, nullptr);
  ASSERT_EQ(session->user_ob, ob);
  ASSERT_EQ(session->master_fd, -1);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));

  ASSERT_EQ(gateway_enqueue_session_protocol_output(
                session, "queued-while-detached",
                sizeof("queued-while-detached") - 1),
            1);
  ASSERT_EQ(session->output_fifo.size(), 1u);
  ASSERT_EQ(gateway_session_fifo_wire_detached_bytes(),
            detached_bytes_before + session->output_fifo_wire_bytes);

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      new_master_fd,
      R"({"type":"login","cid":"gw-test-master-reconnect","data":{"ip":"127.0.0.2","port":6041}})"));
  session = gateway_find_session(session_id);
  ASSERT_NE(session, nullptr);
  ASSERT_EQ(session->master_fd, new_master_fd);
  ASSERT_EQ(ob->interactive->gateway_master_fd, new_master_fd);
  ASSERT_STREQ(ob->interactive->gateway_real_ip, "127.0.0.2");
  ASSERT_EQ(ob->interactive->gateway_real_port, 6041);
  ASSERT_EQ(gateway_session_fifo_wire_detached_bytes(),
            detached_bytes_before);
  ASSERT_EQ(gateway_flush_session_output_fifo_with_writer(session, writer), 1);
  ASSERT_EQ(session->output_fifo_wire_bytes, 0u);
  ASSERT_EQ(writes.size(), 1u);
  ASSERT_EQ(writes[0].first, new_master_fd);
  const auto queued_wire = nlohmann::json::parse(writes[0].second);
  ASSERT_EQ(queued_wire["type"], "output");
  ASSERT_EQ(queued_wire["cid"], session_id);
  ASSERT_EQ(queued_wire["data"], "queued-while-detached");

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      old_master_fd,
      R"({"type":"data","cid":"gw-test-master-reconnect","data":{"cmd":"stale"}})"));
  auto *payload = call_lpc_method(ob, "query_last_gateway_payload");
  ASSERT_TRUE(payload == nullptr || payload->type == T_NUMBER);

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      new_master_fd,
      R"({"type":"data","cid":"gw-test-master-reconnect","data":{"cmd":"current"}})"));
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);
  payload = call_lpc_method(ob, "query_last_gateway_payload");
  ASSERT_NE(payload, nullptr);
  ASSERT_EQ(payload->type, T_MAPPING);
  auto *command_value = find_string_in_mapping(payload->u.map, "cmd");
  ASSERT_NE(command_value, nullptr);
  ASSERT_EQ(command_value->type, T_STRING);
  ASSERT_STREQ(command_value->u.string, "current");

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      old_master_fd,
      R"({"type":"sys","action":"session_notice","cid":"gw-test-master-reconnect","data":{"cmd":"stale-sys"}})"));
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 0);
  payload = call_lpc_method(ob, "query_last_gateway_payload");
  ASSERT_NE(payload, nullptr);
  command_value = find_string_in_mapping(payload->u.map, "cmd");
  ASSERT_NE(command_value, nullptr);
  ASSERT_EQ(command_value->type, T_STRING);
  ASSERT_STREQ(command_value->u.string, "current");

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      old_master_fd,
      R"({"type":"discon","cid":"gw-test-master-reconnect"})"));
  ASSERT_EQ(gateway_find_session(session_id), session);
  ASSERT_NE(ob->interactive, nullptr);

  gateway_cleanup_master_sessions(old_master_fd);
  ASSERT_EQ(gateway_find_session(session_id), session);
  ASSERT_NE(ob->interactive, nullptr);

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  ASSERT_EQ(ob->interactive, nullptr);
  destruct_object(ob);
  free_object(&ob, "TestGatewayMasterReconnectRebindsExistingSessionWithoutLosingState");
}

TEST_F(DriverTest, TestGatewayInboundJsonRejectsExcessiveSvalueDepth) {
  constexpr int master_fd = 1703;
  const char *session_id = "gw-test-json-depth";
  auto *ob = create_gateway_session_for_test(
      session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayInboundJsonRejectsExcessiveSvalueDepth");

  nlohmann::json data = "leaf";
  for (int depth = 0; depth < 21; ++depth) {
    data = nlohmann::json::array({std::move(data)});
  }
  const auto rejected_before =
      g_gateway_runtime_counters.json_frames_rejected.load(
          std::memory_order_relaxed);
  const auto message = nlohmann::json({
      {"type", "data"},
      {"cid", session_id},
      {"data", std::move(data)},
  }).dump();

  EXPECT_TRUE(gateway_dispatch_message_for_test(master_fd, message.c_str()));
  EXPECT_EQ(vm_owner_drain_main_tasks(8), 0);
  EXPECT_EQ(g_gateway_runtime_counters.json_frames_rejected.load(
                std::memory_order_relaxed),
            rejected_before + 1);

  nlohmann::json wire_depth_data = "leaf";
  for (int depth = 0; depth < 30; ++depth) {
    wire_depth_data = nlohmann::json::array({std::move(wire_depth_data)});
  }
  const auto wire_depth_message = nlohmann::json({
      {"type", "data"},
      {"cid", session_id},
      {"data", std::move(wire_depth_data)},
  }).dump();
  EXPECT_FALSE(gateway_dispatch_message_for_test(
      master_fd, wire_depth_message.c_str()));
  EXPECT_EQ(vm_owner_drain_main_tasks(8), 0);
  EXPECT_EQ(g_gateway_runtime_counters.json_frames_rejected.load(
                std::memory_order_relaxed),
            rejected_before + 2);

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayInboundJsonRejectsExcessiveSvalueDepth");
}

TEST_F(DriverTest, TestGatewayInboundJsonRejectsUnsignedIntegerOutsideLpcRange) {
  constexpr int master_fd = 1704;
  const char *session_id = "gw-test-json-unsigned";
  auto *ob = create_gateway_session_for_test(
      session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayInboundJsonRejectsUnsignedIntegerOutsideLpcRange");

  const auto rejected_before =
      g_gateway_runtime_counters.json_frames_rejected.load(
          std::memory_order_relaxed);
  const auto message = nlohmann::json({
      {"type", "data"},
      {"cid", session_id},
      {"data", nlohmann::json({
                   {"value", std::numeric_limits<uint64_t>::max()},
               })},
  }).dump();

  EXPECT_TRUE(gateway_dispatch_message_for_test(master_fd, message.c_str()));
  EXPECT_EQ(vm_owner_drain_main_tasks(8), 0);
  EXPECT_EQ(g_gateway_runtime_counters.json_frames_rejected.load(
                std::memory_order_relaxed),
            rejected_before + 1);

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob,
              "TestGatewayInboundJsonRejectsUnsignedIntegerOutsideLpcRange");
}

TEST_F(DriverTest, TestGatewayInboundJsonRejectsStringsLpcCannotRepresent) {
  constexpr int master_fd = 1705;
  const char *session_id = "gw-test-json-string";
  auto *ob = create_gateway_session_for_test(
      session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayInboundJsonRejectsStringsLpcCannotRepresent");

  const auto max_string_length = CONFIG_INT(__MAX_STRING_LENGTH__);
  ASSERT_GT(max_string_length, 0);
  const std::vector<std::string> invalid_strings = {
      std::string("prefix\0suffix", sizeof("prefix\0suffix") - 1),
      std::string(static_cast<size_t>(max_string_length) + 1, 'x'),
  };
  const auto rejected_before =
      g_gateway_runtime_counters.json_frames_rejected.load(
          std::memory_order_relaxed);

  for (const auto &value : invalid_strings) {
    const auto message = nlohmann::json({
        {"type", "data"},
        {"cid", session_id},
        {"data", nlohmann::json({{"value", value}})},
    }).dump();
    EXPECT_TRUE(gateway_dispatch_message_for_test(master_fd, message.c_str()));
    EXPECT_EQ(vm_owner_drain_main_tasks(8), 0);
  }
  EXPECT_EQ(g_gateway_runtime_counters.json_frames_rejected.load(
                std::memory_order_relaxed),
            rejected_before + invalid_strings.size());

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayInboundJsonRejectsStringsLpcCannotRepresent");
}

TEST_F(DriverTest, TestGatewayInboundJsonShapeFailureDoesNotLeakPartialArrays) {
  constexpr int master_fd = 1706;
  const char *session_id = "gw-test-json-shape";
  auto *ob = create_gateway_session_for_test(
      session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayInboundJsonShapeFailureDoesNotLeakPartialArrays");

  const auto max_array_size = CONFIG_INT(__MAX_ARRAY_SIZE__);
  ASSERT_GT(max_array_size, 0);
  nlohmann::json oversized = nlohmann::json::array();
  for (int index = 0; index <= max_array_size; ++index) {
    oversized.push_back(0);
  }
  const auto message = nlohmann::json({
      {"type", "data"},
      {"cid", session_id},
      {"data", nlohmann::json::array({
                   nlohmann::json({{"safe", 1}}),
                   std::move(oversized),
               })},
  }).dump();
  const auto arrays_before = num_arrays;
  const auto rejected_before =
      g_gateway_runtime_counters.json_frames_rejected.load(
          std::memory_order_relaxed);

  (void)gateway_dispatch_message_for_test(master_fd, message.c_str());
  EXPECT_EQ(vm_owner_drain_main_tasks(8), 0);
  EXPECT_EQ(num_arrays, arrays_before);
  EXPECT_EQ(g_gateway_runtime_counters.json_frames_rejected.load(
                std::memory_order_relaxed),
            rejected_before + 1);

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayInboundJsonShapeFailureDoesNotLeakPartialArrays");
}

TEST_F(DriverTest, TestGatewaySessionIdRejectsOverlongValue) {
  const std::string at_limit(kGatewayMaxSessionIdBytes, 'a');
  auto *accepted = create_gateway_session_for_test(
      at_limit.c_str(), "/clone/gateway_login_example");
  ASSERT_NE(accepted, nullptr);
  add_ref(accepted, "TestGatewaySessionIdRejectsOverlongValueAccepted");
  ASSERT_EQ(gateway_destroy_session_internal(at_limit.c_str(), "test_done", "done"),
            1);
  destruct_object(accepted);
  free_object(&accepted, "TestGatewaySessionIdRejectsOverlongValueAccepted");

  const std::string overlong(kGatewayMaxSessionIdBytes + 1, 'b');
  auto *rejected = create_gateway_session_for_test(
      overlong.c_str(), "/clone/gateway_login_example");
  const bool was_created = rejected != nullptr;
  if (rejected) {
    add_ref(rejected, "TestGatewaySessionIdRejectsOverlongValueRejected");
    EXPECT_EQ(
        gateway_destroy_session_internal(overlong.c_str(), "test_done", "done"),
        1);
    destruct_object(rejected);
    free_object(&rejected, "TestGatewaySessionIdRejectsOverlongValueRejected");
  }
  EXPECT_FALSE(was_created);
}

TEST_F(DriverTest, TestGatewaySessionIdRejectsControlCharacters) {
  const std::string control_id = "gw-test-control\nforged-log-line";
  auto *rejected = create_gateway_session_for_test(
      control_id.c_str(), "/clone/gateway_login_example");
  const bool was_created = rejected != nullptr;
  if (rejected) {
    add_ref(rejected, "TestGatewaySessionIdRejectsControlCharacters");
    EXPECT_EQ(gateway_destroy_session_internal(
                  control_id.c_str(), "test_done", "done"),
              1);
    destruct_object(rejected);
    free_object(&rejected, "TestGatewaySessionIdRejectsControlCharacters");
  }
  EXPECT_FALSE(was_created);
}

TEST_F(DriverTest, TestGatewayWireCidRejectsEmbeddedNullAlias) {
  const char *session_id = "gw-test-cid-alias";
  auto *ob = create_gateway_session_for_test(
      session_id, "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayWireCidRejectsEmbeddedNullAlias");
  gateway_reset_ingress_sequence_for_test();
  const auto rejected_before =
      g_gateway_runtime_counters.session_id_frames_rejected.load(
          std::memory_order_relaxed);

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1,
      R"({"type":"data","cid":"gw-test-cid-alias","data":{"cmd":"baseline"}})"));
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);

  std::string aliased_id(session_id);
  aliased_id.push_back('\0');
  aliased_id.append("forged-suffix");
  const auto message = nlohmann::json({
      {"type", "data"},
      {"cid", aliased_id},
      {"data", nlohmann::json({{"cmd", "aliased"}})},
  }).dump();
  ASSERT_TRUE(gateway_dispatch_message_for_test(-1, message.c_str()));
  EXPECT_EQ(vm_owner_drain_main_tasks(8), 0);

  const auto sys_message = nlohmann::json({
      {"type", "sys"},
      {"action", "custom"},
      {"cid", aliased_id},
      {"data", nlohmann::json({{"cmd", "sys-aliased"}})},
  }).dump();
  ASSERT_TRUE(gateway_dispatch_message_for_test(-1, sys_message.c_str()));
  EXPECT_EQ(vm_owner_drain_main_tasks(8), 0);

  const auto discon_message = nlohmann::json({
      {"type", "discon"},
      {"cid", aliased_id},
  }).dump();
  ASSERT_TRUE(gateway_dispatch_message_for_test(-1, discon_message.c_str()));
  EXPECT_NE(gateway_find_session(session_id), nullptr);

  const std::string overlong_id(kGatewayMaxSessionIdBytes + 1, 'z');
  const auto login_message = nlohmann::json({
      {"type", "login"},
      {"cid", overlong_id},
      {"data", nlohmann::json({{"ip", "127.0.0.1"}, {"port", 6040}})},
  }).dump();
  ASSERT_TRUE(gateway_dispatch_message_for_test(-1, login_message.c_str()));
  EXPECT_EQ(vm_owner_drain_main_tasks(8), 0);
  EXPECT_EQ(gateway_find_session(overlong_id.c_str()), nullptr);

  auto *payload = call_lpc_method(ob, "query_last_gateway_payload");
  ASSERT_NE(payload, nullptr);
  auto *command = find_string_in_mapping(payload->u.map, "cmd");
  ASSERT_NE(command, nullptr);
  EXPECT_STREQ(command->u.string, "baseline");
  EXPECT_EQ(g_gateway_runtime_counters.session_id_frames_rejected.load(
                std::memory_order_relaxed),
            rejected_before + 4);

  auto *status = gateway_status_internal();
  ASSERT_NE(status, nullptr);
  auto *max_bytes =
      find_string_in_mapping(status, "gateway_session_id_max_bytes");
  auto *rejected =
      find_string_in_mapping(status, "gateway_session_id_frames_rejected");
  ASSERT_NE(max_bytes, nullptr);
  ASSERT_NE(rejected, nullptr);
  EXPECT_EQ(max_bytes->u.number,
            static_cast<LPC_INT>(kGatewayMaxSessionIdBytes));
  EXPECT_EQ(rejected->u.number, static_cast<LPC_INT>(rejected_before + 4));
  free_mapping(status);

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayWireCidRejectsEmbeddedNullAlias");
  gateway_reset_ingress_sequence_for_test();
}

TEST_F(DriverTest, TestGatewayReliableIngressExecutesContinuousSequenceExactlyOnce) {
  const char *session_id = "gw-test-reliable-ingress";
  auto *ob = create_gateway_session_for_test(session_id, "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  add_ref(ob, "TestGatewayReliableIngressExecutesContinuousSequenceExactlyOnce");

  gateway_reset_ingress_sequence_for_test();
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1, R"({"type":"hello","data":{"ingress_id":"stream-a","version":2}})"));
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1,
      R"({"type":"data","cid":"gw-test-reliable-ingress","ingress_id":"stream-a","ingress_seq":1,"data":{"cmd":"first"}})"));
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);

  auto *payload = call_lpc_method(ob, "query_last_gateway_payload");
  ASSERT_NE(payload, nullptr);
  auto *command = find_string_in_mapping(payload->u.map, "cmd");
  ASSERT_NE(command, nullptr);
  ASSERT_STREQ(command->u.string, "first");

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1,
      R"({"type":"data","cid":"gw-test-reliable-ingress","ingress_id":"stream-a","ingress_seq":1,"data":{"cmd":"duplicate"}})"));
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1,
      R"({"type":"data","cid":"gw-test-reliable-ingress","ingress_id":"stream-a","ingress_seq":3,"data":{"cmd":"gap"}})"));
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 0);
  payload = call_lpc_method(ob, "query_last_gateway_payload");
  command = find_string_in_mapping(payload->u.map, "cmd");
  ASSERT_STREQ(command->u.string, "first");

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1,
      R"({"type":"data","cid":"gw-test-reliable-ingress","ingress_id":"stream-a","ingress_seq":2,"data":{"cmd":"second"}})"));
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);
  payload = call_lpc_method(ob, "query_last_gateway_payload");
  command = find_string_in_mapping(payload->u.map, "cmd");
  ASSERT_STREQ(command->u.string, "second");

  auto *status = gateway_status_internal();
  ASSERT_NE(status, nullptr);
  auto mapping_number = [](mapping_t *map, const char *key) -> long {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  ASSERT_EQ(mapping_number(status, "gateway_ingress_sequence_last_accepted"), 2);
  ASSERT_EQ(mapping_number(status, "gateway_ingress_sequence_duplicates"), 1);
  ASSERT_EQ(mapping_number(status, "gateway_ingress_sequence_gaps"), 1);
  free_mapping(status);

  gateway_reset_ingress_sequence_for_test();
  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayReliableIngressExecutesContinuousSequenceExactlyOnce");
}

TEST_F(DriverTest, TestGatewayReliableIngressRecoversSequenceAfterProcessRestart) {
  const char *session_id = "gw-test-reliable-restart";
  auto *ob = create_gateway_session_for_test(session_id, "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  add_ref(ob, "TestGatewayReliableIngressRecoversSequenceAfterProcessRestart");

  gateway_reset_ingress_sequence_for_test();
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1, R"({"type":"hello","data":{"ingress_id":"stream-recovered","version":2}})"));
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1,
      R"({"type":"data","cid":"gw-test-reliable-restart","ingress_id":"stream-recovered","ingress_seq":41,"data":{"cmd":"recovered"}})"));
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);

  auto *payload = call_lpc_method(ob, "query_last_gateway_payload");
  ASSERT_NE(payload, nullptr);
  auto *command = find_string_in_mapping(payload->u.map, "cmd");
  ASSERT_NE(command, nullptr);
  ASSERT_STREQ(command->u.string, "recovered");

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1,
      R"({"type":"data","cid":"gw-test-reliable-restart","ingress_id":"stream-recovered","ingress_seq":42,"data":{"cmd":"next"}})"));
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);
  payload = call_lpc_method(ob, "query_last_gateway_payload");
  command = find_string_in_mapping(payload->u.map, "cmd");
  ASSERT_STREQ(command->u.string, "next");

  auto *status = gateway_status_internal();
  ASSERT_NE(status, nullptr);
  auto mapping_number = [](mapping_t *map, const char *key) -> long {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  ASSERT_EQ(mapping_number(status, "gateway_ingress_sequence_last_accepted"), 42);
  ASSERT_EQ(mapping_number(status, "gateway_ingress_sequence_gaps"), 0);
  free_mapping(status);

  gateway_reset_ingress_sequence_for_test();
  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayReliableIngressRecoversSequenceAfterProcessRestart");
}

TEST_F(DriverTest, TestGatewayReliableIngressResetsOnlyForNewStreamIdentity) {
  const char *session_id = "gw-test-reliable-stream";
  auto *ob = create_gateway_session_for_test(session_id, "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  add_ref(ob, "TestGatewayReliableIngressResetsOnlyForNewStreamIdentity");

  gateway_reset_ingress_sequence_for_test();
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1, R"({"type":"hello","data":{"ingress_id":"stream-a","version":2}})"));
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1,
      R"({"type":"data","cid":"gw-test-reliable-stream","ingress_id":"stream-a","ingress_seq":1,"data":{"cmd":"a-one"}})"));
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1, R"({"type":"hello","data":{"ingress_id":"stream-a","version":2}})"));
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1,
      R"({"type":"data","cid":"gw-test-reliable-stream","ingress_id":"stream-a","ingress_seq":1,"data":{"cmd":"a-duplicate"}})"));
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 0);

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1, R"({"type":"hello","data":{"ingress_id":"stream-b","version":2}})"));
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1,
      R"({"type":"data","cid":"gw-test-reliable-stream","ingress_id":"stream-b","ingress_seq":1,"data":{"cmd":"b-one"}})"));
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1,
      R"({"type":"data","cid":"gw-test-reliable-stream","ingress_id":"stream-a","ingress_seq":2,"data":{"cmd":"stale-a"}})"));
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 0);

  auto *payload = call_lpc_method(ob, "query_last_gateway_payload");
  ASSERT_NE(payload, nullptr);
  auto *command = find_string_in_mapping(payload->u.map, "cmd");
  ASSERT_NE(command, nullptr);
  ASSERT_STREQ(command->u.string, "b-one");

  gateway_reset_ingress_sequence_for_test();
  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayReliableIngressResetsOnlyForNewStreamIdentity");
}

TEST_F(DriverTest, TestGatewayReliableIngressActiveOwnerRejectsCompetingStream) {
  constexpr int owner_fd = 993;
  constexpr int contender_fd = 994;
  const char *session_id = "gw-test-reliable-owner";
  bufferevent *owner_pair[2] = {nullptr, nullptr};
  bufferevent *contender_pair[2] = {nullptr, nullptr};

  gateway_reset_ingress_sequence_for_test();
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, owner_pair), 0);
  ASSERT_EQ(
      bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, contender_pair),
      0);
  ASSERT_NE(gateway_register_master_for_test(owner_fd, owner_pair[0]), nullptr);
  auto *contender =
      gateway_register_master_for_test(contender_fd, contender_pair[0]);
  ASSERT_NE(contender, nullptr);
  auto *ob = create_gateway_session_for_test(
      session_id, "/clone/gateway_login_example", owner_fd);
  ASSERT_NE(ob, nullptr);
  add_ref(ob,
          "TestGatewayReliableIngressActiveOwnerRejectsCompetingStream");

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      owner_fd,
      R"({"type":"hello","data":{"ingress_id":"owner-stream","version":2}})"));
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      owner_fd,
      R"({"type":"data","cid":"gw-test-reliable-owner","ingress_id":"owner-stream","ingress_seq":1,"data":{"cmd":"first"}})"));
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      contender_fd,
      R"({"type":"hello","data":{"ingress_id":"contender-stream","version":2}})"));
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      contender_fd,
      R"({"type":"data","cid":"gw-test-reliable-owner","ingress_id":"contender-stream","ingress_seq":1,"data":{"cmd":"contender"}})"));
  EXPECT_FALSE(contender->ingress_ack_pending);
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 0);

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      owner_fd,
      R"({"type":"data","cid":"gw-test-reliable-owner","ingress_id":"owner-stream","ingress_seq":2,"data":{"cmd":"second"}})"));
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);
  auto *payload = call_lpc_method(ob, "query_last_gateway_payload");
  ASSERT_NE(payload, nullptr);
  auto *command = find_string_in_mapping(payload->u.map, "cmd");
  ASSERT_NE(command, nullptr);
  ASSERT_STREQ(command->u.string, "second");

  auto *status = gateway_status_internal();
  ASSERT_NE(status, nullptr);
  auto mapping_number = [](mapping_t *map, const char *key) -> long {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  EXPECT_EQ(mapping_number(status, "gateway_ingress_sequence_last_accepted"), 2);
  EXPECT_EQ(mapping_number(status, "gateway_ingress_sequence_stream_resets"), 1);
  EXPECT_EQ(mapping_number(status, "gateway_ingress_sequence_owner_fd"), owner_fd);
  EXPECT_EQ(mapping_number(status, "gateway_ingress_sequence_owner_active"), 1);
  EXPECT_EQ(mapping_number(status, "gateway_ingress_sequence_owner_mismatches"), 2);
  free_mapping(status);

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  gateway_remove_master_for_test(contender_fd);
  contender_pair[0] = nullptr;
  gateway_remove_master_for_test(owner_fd);
  owner_pair[0] = nullptr;
  bufferevent_free(contender_pair[1]);
  contender_pair[1] = nullptr;
  bufferevent_free(owner_pair[1]);
  owner_pair[1] = nullptr;
  destruct_object(ob);
  free_object(
      &ob, "TestGatewayReliableIngressActiveOwnerRejectsCompetingStream");
  gateway_reset_ingress_sequence_for_test();
}

TEST_F(DriverTest, TestGatewayReliableIngressReconnectPreservesAcceptedSequence) {
  constexpr int owner_fd = 995;
  constexpr int replacement_fd = 996;
  const char *session_id = "gw-test-reliable-reconnect";
  bufferevent *owner_pair[2] = {nullptr, nullptr};
  bufferevent *replacement_pair[2] = {nullptr, nullptr};

  gateway_reset_ingress_sequence_for_test();
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, owner_pair), 0);
  ASSERT_EQ(
      bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, replacement_pair),
      0);
  ASSERT_NE(gateway_register_master_for_test(owner_fd, owner_pair[0]), nullptr);
  auto *ob = create_gateway_session_for_test(
      session_id, "/clone/gateway_login_example", owner_fd);
  ASSERT_NE(ob, nullptr);
  add_ref(ob,
          "TestGatewayReliableIngressReconnectPreservesAcceptedSequence");

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      owner_fd,
      R"({"type":"hello","data":{"ingress_id":"reconnect-stream","version":2}})"));
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      owner_fd,
      R"({"type":"data","cid":"gw-test-reliable-reconnect","ingress_id":"reconnect-stream","ingress_seq":1,"data":{"cmd":"first"}})"));
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "reconnect", "reconnect"),
            1);
  destruct_object(ob);
  free_object(
      &ob, "TestGatewayReliableIngressReconnectPreservesAcceptedSequenceOld");
  gateway_remove_master_for_test(owner_fd);
  owner_pair[0] = nullptr;
  ASSERT_NE(
      gateway_register_master_for_test(replacement_fd, replacement_pair[0]),
      nullptr);
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      replacement_fd,
      R"({"type":"hello","data":{"ingress_id":"reconnect-stream","version":2}})"));

  ob = create_gateway_session_for_test(
      session_id, "/clone/gateway_login_example", replacement_fd);
  ASSERT_NE(ob, nullptr);
  add_ref(ob,
          "TestGatewayReliableIngressReconnectPreservesAcceptedSequenceNew");
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      replacement_fd,
      R"({"type":"data","cid":"gw-test-reliable-reconnect","ingress_id":"reconnect-stream","ingress_seq":1,"data":{"cmd":"duplicate"}})"));
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 0);
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      replacement_fd,
      R"({"type":"data","cid":"gw-test-reliable-reconnect","ingress_id":"reconnect-stream","ingress_seq":2,"data":{"cmd":"second"}})"));
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);

  auto *payload = call_lpc_method(ob, "query_last_gateway_payload");
  ASSERT_NE(payload, nullptr);
  auto *command = find_string_in_mapping(payload->u.map, "cmd");
  ASSERT_NE(command, nullptr);
  ASSERT_STREQ(command->u.string, "second");

  auto *status = gateway_status_internal();
  ASSERT_NE(status, nullptr);
  auto *last_accepted =
      find_string_in_mapping(status, "gateway_ingress_sequence_last_accepted");
  auto *owner =
      find_string_in_mapping(status, "gateway_ingress_sequence_owner_fd");
  ASSERT_NE(last_accepted, nullptr);
  ASSERT_NE(owner, nullptr);
  EXPECT_EQ(last_accepted->u.number, 2);
  EXPECT_EQ(owner->u.number, replacement_fd);
  free_mapping(status);

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  gateway_remove_master_for_test(replacement_fd);
  replacement_pair[0] = nullptr;
  bufferevent_free(replacement_pair[1]);
  replacement_pair[1] = nullptr;
  bufferevent_free(owner_pair[1]);
  owner_pair[1] = nullptr;
  destruct_object(ob);
  free_object(
      &ob, "TestGatewayReliableIngressReconnectPreservesAcceptedSequence");
  gateway_reset_ingress_sequence_for_test();
}

TEST_F(DriverTest, TestGatewayReliableIngressNacksExpectedSequenceUntilSessionExists) {
  const int master_fd = 992;
  const char *session_id = "gw-test-reliable-late-session";
  bufferevent *pair[2] = {nullptr, nullptr};

  gateway_reset_ingress_sequence_for_test();
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  auto *master = gateway_register_master_for_test(master_fd, pair[0]);
  ASSERT_NE(master, nullptr);

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      master_fd,
      R"({"type":"hello","data":{"ingress_id":"late-session-stream","version":2}})"));
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      master_fd,
      R"({"type":"data","cid":"gw-test-reliable-late-session","ingress_id":"late-session-stream","ingress_seq":1,"data":{"cmd":"retry-me"}})"));
  ASSERT_TRUE(master->ingress_ack_pending);
  ASSERT_EQ(master->ingress_ack_sequence, 0);
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 0);

  auto *ob = create_gateway_session_for_test(
      session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(ob, nullptr);
  add_ref(ob,
          "TestGatewayReliableIngressNacksExpectedSequenceUntilSessionExists");
  ASSERT_TRUE(gateway_dispatch_message_for_test(
      master_fd,
      R"({"type":"data","cid":"gw-test-reliable-late-session","ingress_id":"late-session-stream","ingress_seq":1,"data":{"cmd":"retry-me"}})"));
  ASSERT_TRUE(master->ingress_ack_pending);
  ASSERT_EQ(master->ingress_ack_sequence, 1);
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);
  auto *payload = call_lpc_method(ob, "query_last_gateway_payload");
  ASSERT_NE(payload, nullptr);
  auto *command = find_string_in_mapping(payload->u.map, "cmd");
  ASSERT_NE(command, nullptr);
  ASSERT_STREQ(command->u.string, "retry-me");

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  gateway_remove_master_for_test(master_fd);
  pair[0] = nullptr;
  bufferevent_free(pair[1]);
  pair[1] = nullptr;
  destruct_object(ob);
  free_object(
      &ob,
      "TestGatewayReliableIngressNacksExpectedSequenceUntilSessionExists");
  gateway_reset_ingress_sequence_for_test();
}

TEST_F(DriverTest, TestGatewayReliableIngressBatchFlushesOneFramedCumulativeAck) {
  const int master_fd = 991;
  const char *session_id = "gw-test-reliable-wire";
  bufferevent *pair[2] = {nullptr, nullptr};

  gateway_reset_ingress_sequence_for_test();
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  ASSERT_EQ(bufferevent_enable(pair[0], EV_WRITE), 0);
  ASSERT_EQ(bufferevent_enable(pair[1], EV_READ), 0);
  auto *master = gateway_register_master_for_test(master_fd, pair[0]);
  ASSERT_NE(master, nullptr);
  auto *ob = create_gateway_session_for_test(
      session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayReliableIngressBatchFlushesOneFramedCumulativeAck");

  const auto frame = [](const std::string &payload) {
    const auto size = static_cast<uint32_t>(payload.size());
    std::string encoded(sizeof(uint32_t), '\0');
    const auto network_size = htonl(size);
    memcpy(encoded.data(), &network_size, sizeof(network_size));
    encoded += payload;
    return encoded;
  };
  master->read_buffer =
      frame(R"({"type":"hello","data":{"ingress_id":"wire-stream","version":2}})") +
      frame(R"({"type":"data","cid":"gw-test-reliable-wire","ingress_id":"wire-stream","ingress_seq":1,"data":{"cmd":"first"}})") +
      frame(R"({"type":"data","cid":"gw-test-reliable-wire","ingress_id":"wire-stream","ingress_seq":2,"data":{"cmd":"second"}})");
  ASSERT_EQ(gateway_dispatch_buffered_frames_for_test(master, 16), 3);
  ASSERT_FALSE(master->ingress_ack_pending);
  ASSERT_EQ(g_gateway_runtime_counters.ingress_ack_frames_sent.load(), 1);

  auto *input = bufferevent_get_input(pair[1]);
  ASSERT_NE(input, nullptr);
  for (int attempt = 0; attempt < 8 && evbuffer_get_length(input) == 0;
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
  const auto ack = nlohmann::json::parse(
      framed.substr(sizeof(network_length), payload_length));
  ASSERT_EQ(ack.at("type"), "ingress_ack");
  ASSERT_EQ(ack.at("ingress_id"), "wire-stream");
  ASSERT_EQ(ack.at("ingress_seq"), 2);

  ASSERT_EQ(vm_owner_drain_main_tasks(8), 2);
  auto *payload = call_lpc_method(ob, "query_last_gateway_payload");
  ASSERT_NE(payload, nullptr);
  auto *command = find_string_in_mapping(payload->u.map, "cmd");
  ASSERT_NE(command, nullptr);
  ASSERT_STREQ(command->u.string, "second");

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  gateway_remove_master_for_test(master_fd);
  pair[0] = nullptr;
  bufferevent_free(pair[1]);
  pair[1] = nullptr;
  destruct_object(ob);
  free_object(&ob,
              "TestGatewayReliableIngressBatchFlushesOneFramedCumulativeAck");
  gateway_reset_ingress_sequence_for_test();
}

TEST_F(DriverTest, TestGatewayDetachedSessionExpiresAfterReconnectGrace) {
  const char *session_id = "gw-test-reconnect-expired";
  auto *ob = create_gateway_session_for_test(session_id, "/clone/gateway_login_example", 1801);
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  add_ref(ob, "TestGatewayDetachedSessionExpiresAfterReconnectGrace");

  auto old_grace = g_gateway_reconnect_grace;
  g_gateway_reconnect_grace = 1;
  gateway_cleanup_master_sessions(1801);
  auto *session = gateway_find_session(session_id);
  ASSERT_NE(session, nullptr);
  session->detached_at = get_current_time() - 2;

  gateway_check_session_timeouts();
  ASSERT_EQ(gateway_find_session(session_id), nullptr);
  ASSERT_EQ(ob->interactive, nullptr);
  auto *code = call_lpc_method(ob, "query_last_disconnect_code");
  ASSERT_NE(code, nullptr);
  ASSERT_EQ(code->type, T_STRING);
  ASSERT_STREQ(code->u.string, "session_timeout");

  g_gateway_reconnect_grace = old_grace;
  destruct_object(ob);
  free_object(&ob, "TestGatewayDetachedSessionExpiresAfterReconnectGrace");
}

TEST_F(DriverTest, TestGatewayReceiveRunsThroughOwnerMainQueue) {
  auto *ob = create_gateway_session_for_test("gw-test-receive", "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));
  auto owner_epoch = vm_owner_epoch(ob);

  auto mapping_number = [](mapping_t *map, const char *key) -> long {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t *map, const char *key) -> const char * {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1, R"({"type":"data","cid":"gw-test-receive","data":{"cmd":"look","seq":7}})"));
  ASSERT_EQ(vm_owner_main_queue_total_depth(), 1);
  ASSERT_EQ(vm_owner_drain_main_tasks(1), 1);

  auto *payload = call_lpc_method(ob, "query_last_gateway_payload");
  ASSERT_NE(payload, nullptr);
  ASSERT_EQ(payload->type, T_MAPPING);
  ASSERT_STREQ(mapping_string(payload->u.map, "cmd"), "look");
  ASSERT_EQ(mapping_number(payload->u.map, "seq"), 7);

  auto *context = call_lpc_method(ob, "query_last_gateway_receive_context");
  ASSERT_NE(context, nullptr);
  ASSERT_EQ(context->type, T_MAPPING);
  ASSERT_STREQ(mapping_string(context->u.map, "owner_id"), vm_owner_id(ob));
  ASSERT_EQ(mapping_number(context->u.map, "owner_epoch"), static_cast<long>(owner_epoch));
  ASSERT_NE(std::string(mapping_string(context->u.map, "this_player")).find(ob->obname), std::string::npos);

  auto *trace = vm_owner_task_trace(32);
  ASSERT_NE(trace, nullptr);
  ASSERT_EQ(mapping_number(trace, "success"), 1);
  auto *events_value = find_string_in_mapping(trace, "events");
  ASSERT_NE(events_value, nullptr);
  ASSERT_EQ(events_value ? events_value->type : T_INVALID, T_ARRAY);
  bool found_queued = false;
  bool found_main_dispatched = false;
  bool found_dispatched = false;
  if (events_value && events_value->type == T_ARRAY) {
    for (int i = 0; i < events_value->u.arr->size; i++) {
      auto *event = events_value->u.arr->item[i].u.map;
      if (std::string(mapping_string(event, "task_type")) == "gateway" &&
          std::string(mapping_string(event, "task_key")) == "gateway_receive" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        auto state = std::string(mapping_string(event, "state"));
        found_queued = found_queued || state == "main_queued";
        found_main_dispatched = found_main_dispatched || state == "main_dispatched";
        found_dispatched = found_dispatched || state == "dispatched";
      }
    }
  }
  ASSERT_TRUE(found_queued);
  ASSERT_TRUE(found_main_dispatched);
  ASSERT_TRUE(found_dispatched);
  free_mapping(trace);
  ASSERT_EQ(vm_owner_drain_main_tasks(1), 0);

  add_ref(ob, "TestGatewayReceiveRunsThroughOwnerMainQueue");
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-receive", "test_done", "done"), 1);
  ASSERT_EQ(ob->interactive, nullptr);
  destruct_object(ob);
  free_object(&ob, "TestGatewayReceiveRunsThroughOwnerMainQueue");
}

TEST_F(DriverTest, TestGatewayReceiveDefersMainDrainWhenEventBaseIsAvailable) {
  auto *ob = create_gateway_session_for_test("gw-test-receive-threaded", "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));
  auto owner_epoch = vm_owner_epoch(ob);

  auto mapping_number = [](mapping_t *map, const char *key) -> long {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t *map, const char *key) -> const char * {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

  auto *before_status = gateway_status_internal();
  ASSERT_NE(before_status, nullptr);
  auto before_receive_enqueued = mapping_number(before_status, "gateway_receive_tasks_enqueued");
  auto before_receive_dispatched = mapping_number(before_status, "gateway_receive_tasks_dispatched");
  auto before_main_drain_runs = mapping_number(before_status, "gateway_main_drain_runs");
  auto before_inline_drain_calls = mapping_number(before_status, "gateway_receive_inline_drain_calls");
  auto before_receive_deferred_requests = mapping_number(before_status, "gateway_receive_deferred_drain_requests");
  auto before_queue_depth_samples = mapping_number(before_status, "gateway_receive_main_queue_depth_samples");
  auto before_apply_samples = mapping_number(before_status, "gateway_receive_apply_samples");
  auto before_apply_cpu_samples = mapping_number(before_status, "gateway_receive_apply_thread_cpu_samples");
  auto before_apply_cpu_unavailable =
      mapping_number(before_status, "gateway_receive_apply_thread_cpu_unavailable");
  free_mapping(before_status);

  vm_owner_thread_start(1);
  ASSERT_TRUE(vm_owner_executor_available());

  auto *initial_payload = call_lpc_method(ob, "query_last_gateway_payload");
  ASSERT_NE(initial_payload, nullptr);
  ASSERT_EQ(initial_payload->type, T_NUMBER);
  ASSERT_EQ(initial_payload->u.number, 0);

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1, R"({"type":"data","cid":"gw-test-receive-threaded","data":{"cmd":"look","seq":9}})"));

  auto *payload = call_lpc_method(ob, "query_last_gateway_payload");
  ASSERT_NE(payload, nullptr);
  ASSERT_EQ(payload->type, T_NUMBER);
  ASSERT_EQ(payload->u.number, 0);

  auto *queued_status = gateway_status_internal();
  ASSERT_NE(queued_status, nullptr);
  ASSERT_GE(mapping_number(queued_status, "gateway_receive_tasks_enqueued"),
            before_receive_enqueued + 1);
  ASSERT_EQ(mapping_number(queued_status, "gateway_receive_tasks_dispatched"),
            before_receive_dispatched);
  ASSERT_EQ(mapping_number(queued_status, "gateway_main_drain_runs"),
            before_main_drain_runs);
  ASSERT_EQ(mapping_number(queued_status, "gateway_receive_inline_drain_calls"),
            before_inline_drain_calls);
  ASSERT_GE(mapping_number(queued_status, "gateway_receive_deferred_drain_requests"),
            before_receive_deferred_requests + 1);
  ASSERT_GE(mapping_number(queued_status, "gateway_receive_main_queue_depth_samples"),
            before_queue_depth_samples + 1);
  free_mapping(queued_status);

  ASSERT_EQ(vm_owner_main_queue_total_depth(), 1);
  ASSERT_EQ(vm_owner_drain_main_tasks(1), 1);

  payload = call_lpc_method(ob, "query_last_gateway_payload");
  ASSERT_NE(payload, nullptr);
  ASSERT_EQ(payload->type, T_MAPPING);
  ASSERT_STREQ(mapping_string(payload->u.map, "cmd"), "look");
  ASSERT_EQ(mapping_number(payload->u.map, "seq"), 9);

  auto *context = call_lpc_method(ob, "query_last_gateway_receive_context");
  ASSERT_NE(context, nullptr);
  ASSERT_EQ(context->type, T_MAPPING);
  ASSERT_STREQ(mapping_string(context->u.map, "owner_id"), vm_owner_id(ob));
  ASSERT_EQ(mapping_number(context->u.map, "owner_epoch"), static_cast<long>(owner_epoch));
  ASSERT_NE(std::string(mapping_string(context->u.map, "this_player")).find(ob->obname), std::string::npos);

  auto *after_status = gateway_status_internal();
  ASSERT_NE(after_status, nullptr);
  ASSERT_GE(mapping_number(after_status, "gateway_receive_tasks_enqueued"), before_receive_enqueued + 1);
  ASSERT_GE(mapping_number(after_status, "gateway_receive_tasks_dispatched"), before_receive_dispatched + 1);
  ASSERT_EQ(mapping_number(after_status, "gateway_main_drain_runs"), before_main_drain_runs);
  ASSERT_EQ(mapping_number(after_status, "gateway_receive_inline_drain_calls"), before_inline_drain_calls);
  ASSERT_GE(mapping_number(after_status, "gateway_receive_deferred_drain_requests"),
            before_receive_deferred_requests + 1);
  ASSERT_GE(mapping_number(after_status, "gateway_receive_main_queue_depth_samples"),
            before_queue_depth_samples + 1);
  auto apply_samples_delta =
      mapping_number(after_status, "gateway_receive_apply_samples") - before_apply_samples;
  auto apply_cpu_samples_delta = mapping_number(after_status, "gateway_receive_apply_thread_cpu_samples") -
                                 before_apply_cpu_samples;
  auto apply_cpu_unavailable_delta =
      mapping_number(after_status, "gateway_receive_apply_thread_cpu_unavailable") -
      before_apply_cpu_unavailable;
  ASSERT_GE(apply_samples_delta, 1);
  ASSERT_EQ(apply_samples_delta, apply_cpu_samples_delta + apply_cpu_unavailable_delta);
  free_mapping(after_status);

  auto *trace = vm_owner_task_trace(32);
  ASSERT_NE(trace, nullptr);
  ASSERT_EQ(mapping_number(trace, "success"), 1);
  auto *events_value = find_string_in_mapping(trace, "events");
  ASSERT_NE(events_value, nullptr);
  ASSERT_EQ(events_value ? events_value->type : T_INVALID, T_ARRAY);
  bool found_queued = false;
  bool found_main_dispatched = false;
  bool found_dispatched = false;
  if (events_value && events_value->type == T_ARRAY) {
    for (int i = 0; i < events_value->u.arr->size; i++) {
      auto *event = events_value->u.arr->item[i].u.map;
      if (std::string(mapping_string(event, "task_type")) == "gateway" &&
          std::string(mapping_string(event, "task_key")) == "gateway_receive" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        auto state = std::string(mapping_string(event, "state"));
        found_queued = found_queued || state == "main_queued";
        found_main_dispatched = found_main_dispatched || state == "main_dispatched";
        found_dispatched = found_dispatched || state == "dispatched";
      }
    }
  }
  ASSERT_TRUE(found_queued);
  ASSERT_TRUE(found_main_dispatched);
  ASSERT_TRUE(found_dispatched);
  free_mapping(trace);
  ASSERT_EQ(vm_owner_drain_main_tasks(1), 0);

  add_ref(ob, "TestGatewayReceiveDefersMainDrainWhenEventBaseIsAvailable");
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-receive-threaded", "test_done", "done"), 1);
  ASSERT_EQ(ob->interactive, nullptr);
  destruct_object(ob);
  free_object(&ob, "TestGatewayReceiveDefersMainDrainWhenEventBaseIsAvailable");
}

TEST_F(DriverTest, TestGatewayReadBatchDrainServicesAdmittedReceiveTask) {
  auto *ob = create_gateway_session_for_test("gw-test-read-batch-drain",
                                             "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);

  auto mapping_number = [](mapping_t *map, const char *key) -> long {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };

  auto *before = gateway_status_internal();
  ASSERT_NE(before, nullptr);
  auto before_runs = mapping_number(before, "gateway_read_batch_drain_runs");
  auto before_tasks =
      mapping_number(before, "gateway_read_batch_drain_tasks_total");
  auto before_backlog = mapping_number(
      before, "gateway_read_batch_drain_backlog_rescheduled");
  auto before_wall_samples =
      mapping_number(before, "gateway_read_batch_drain_wall_samples");
  auto before_remaining_samples =
      mapping_number(before, "gateway_read_batch_drain_remaining_samples");
  auto before_remaining_total =
      mapping_number(before, "gateway_read_batch_drain_remaining_total");
  free_mapping(before);

  ASSERT_TRUE(gateway_dispatch_message_for_test(
      -1,
      R"({"type":"data","cid":"gw-test-read-batch-drain","data":{"cmd":"look","seq":11}})"));
  ASSERT_EQ(vm_owner_main_queue_total_depth(), 1);

  gateway_service_admitted_receive_tasks_for_test();
  ASSERT_EQ(vm_owner_main_queue_total_depth(), 0);

  auto *payload = call_lpc_method(ob, "query_last_gateway_payload");
  ASSERT_NE(payload, nullptr);
  ASSERT_EQ(payload->type, T_MAPPING);
  ASSERT_EQ(mapping_number(payload->u.map, "seq"), 11);

  auto *after = gateway_status_internal();
  ASSERT_NE(after, nullptr);
  ASSERT_EQ(mapping_number(after, "gateway_read_batch_drain_runs"),
            before_runs + 1);
  ASSERT_EQ(mapping_number(after, "gateway_read_batch_drain_tasks_total"),
            before_tasks + 1);
  ASSERT_GE(mapping_number(after, "gateway_read_batch_drain_tasks_max"), 1);
  ASSERT_EQ(mapping_number(after,
                           "gateway_read_batch_drain_backlog_rescheduled"),
            before_backlog);
  ASSERT_EQ(mapping_number(after, "gateway_read_batch_drain_wall_samples"),
            before_wall_samples + 1);
  ASSERT_EQ(
      mapping_number(after, "gateway_read_batch_drain_remaining_samples"),
      before_remaining_samples + 1);
  ASSERT_EQ(mapping_number(after, "gateway_read_batch_drain_remaining_total"),
            before_remaining_total);
  free_mapping(after);

  add_ref(ob, "TestGatewayReadBatchDrainServicesAdmittedReceiveTask");
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-read-batch-drain",
                                             "test_done", "done"),
            1);
  ASSERT_EQ(ob->interactive, nullptr);
  destruct_object(ob);
  free_object(&ob, "TestGatewayReadBatchDrainServicesAdmittedReceiveTask");
}

TEST_F(DriverTest, TestGatewayCommandTaskCarriesOwnerHandlePayload) {
  auto *ob = create_gateway_session_for_test("gw-test-command", "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));
  auto owner_epoch = vm_owner_epoch(ob);

  auto mapping_number = [](mapping_t *map, const char *key) -> long {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t *map, const char *key) -> const char * {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
  auto mapping_has_string_key = [](mapping_t *map, const char *key) -> bool {
    auto *keys = mapping_indices(map);
    bool found = false;
    for (int i = 0; keys && i < keys->size; i++) {
      if (keys->item[i].type == T_STRING && std::strcmp(keys->item[i].u.string, key) == 0) {
        found = true;
        break;
      }
    }
    if (keys) {
      free_array(keys);
    }
    return found;
  };

  ASSERT_EQ(gateway_inject_input_internal(ob, "look"), 1);
  ASSERT_EQ(gateway_process_pending_command_internal(ob), 1);

  auto *trace = vm_owner_task_trace(48);
  ASSERT_NE(trace, nullptr);
  ASSERT_EQ(mapping_number(trace, "success"), 1);
  auto *events_value = find_string_in_mapping(trace, "events");
  ASSERT_NE(events_value, nullptr);
  ASSERT_EQ(events_value ? events_value->type : T_INVALID, T_ARRAY);
  bool found_command_task = false;
  bool found_process_input_apply_frame_entered = false;
  bool found_parser_frame_entered = false;
  bool found_reply_task_queued = false;
  bool found_reply_task_dispatched = false;
  bool found_write_prompt_apply_frame_entered = false;
  if (events_value && events_value->type == T_ARRAY) {
    for (int i = 0; i < events_value->u.arr->size; i++) {
      auto *event = events_value->u.arr->item[i].u.map;
      if (std::string(mapping_string(event, "task_type")) == "command_reply" &&
          std::string(mapping_string(event, "task_key")) == "prompt_telnet_reschedule_io" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        auto state = std::string(mapping_string(event, "state"));
        found_reply_task_queued = found_reply_task_queued || state == "main_queued";
        found_reply_task_dispatched = found_reply_task_dispatched || state == "main_dispatched";
      }
      if (std::string(mapping_string(event, "task_type")) == "command_reply" &&
          std::string(mapping_string(event, "task_key")) == "write_prompt_apply" &&
          std::string(mapping_string(event, "state")) == "frame_entered" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        found_write_prompt_apply_frame_entered = true;
      }
      if (std::string(mapping_string(event, "task_type")) == "interactive_command_parser" &&
          std::string(mapping_string(event, "task_key")) == "process_input_apply" &&
          std::string(mapping_string(event, "state")) == "frame_entered" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        found_process_input_apply_frame_entered = true;
      }
      if (std::string(mapping_string(event, "task_type")) == "interactive_command_parser" &&
          std::string(mapping_string(event, "task_key")) == "safe_parse_command" &&
          std::string(mapping_string(event, "state")) == "frame_entered" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        found_parser_frame_entered = true;
      }
      if (std::string(mapping_string(event, "task_type")) == "gateway_command_execute" &&
          std::string(mapping_string(event, "task_key")) == "process_user_command" &&
          std::string(mapping_string(event, "state")) == "main_queued" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        found_command_task = true;
        ASSERT_EQ(mapping_number(event, "has_target_handle"), 1);
        ASSERT_EQ(mapping_number(event, "target_handle_current"), 1);
        ASSERT_STREQ(mapping_string(event, "target_handle_status"), "current");
        ASSERT_EQ(mapping_number(event, "target_owner_epoch"), static_cast<long>(owner_epoch));
        ASSERT_STREQ(mapping_string(event, "payload_key"), "gateway_command_input");
        ASSERT_STREQ(mapping_string(event, "command_text_snapshot_policy"), "owner_private_redacted_from_trace");
        ASSERT_EQ(mapping_number(event, "command_text_snapshot_ready"), 1);
        ASSERT_GT(mapping_number(event, "command_text_snapshot_bytes"), 0);
        ASSERT_EQ(mapping_number(event, "command_text_snapshot_redacted"), 1);
        ASSERT_STREQ(mapping_string(event, "command_text_snapshot_blocker"), "");
        ASSERT_STREQ(mapping_string(event, "command_consume_model"), "owner_owned_snapshot_main_thread_consume");
        ASSERT_EQ(mapping_number(event, "command_consume_snapshot_ready"), 1);
        ASSERT_EQ(mapping_number(event, "command_consume_executor_ready"), 0);
        ASSERT_STREQ(mapping_string(event, "command_consume_blocker"),
                     "interactive_command_requires_main_thread_io_adapter");
        ASSERT_STREQ(mapping_string(event, "execution_frame_model"), "gateway_command_execution_frame_v1");
        ASSERT_STREQ(mapping_string(event, "execution_frame_policy"), "owner_scope_current_interactive_command_giver");
        ASSERT_STREQ(mapping_string(event, "execution_frame_restore_policy"), "main_thread_vmcontext_scope");
        ASSERT_EQ(mapping_number(event, "execution_frame_restore_ready"), 1);
        ASSERT_STREQ(mapping_string(event, "execution_frame_restore_blocker"), "");
        ASSERT_EQ(mapping_number(event, "execution_frame_requires_current_interactive"), 1);
        ASSERT_EQ(mapping_number(event, "execution_frame_requires_command_giver"), 1);
        ASSERT_EQ(mapping_number(event, "execution_frame_executor_ready"), 0);
        ASSERT_EQ(mapping_number(event, "payload_frozen"), 1);
        auto *payload_value = find_string_in_mapping(event, "payload");
        ASSERT_NE(payload_value, nullptr);
        ASSERT_EQ(payload_value ? payload_value->type : T_INVALID, T_MAPPING);
        auto *payload = payload_value->u.map;
        ASSERT_STREQ(mapping_string(payload, "payload_model"), "gateway_command_buffer_metadata_v1");
        ASSERT_STREQ(mapping_string(payload, "payload_policy"), "no_raw_command_text_in_trace");
        ASSERT_STREQ(mapping_string(payload, "input_source"), "interactive_text_buffer");
        ASSERT_STREQ(mapping_string(payload, "command_text_snapshot_policy"), "owner_private_redacted_from_trace");
        ASSERT_EQ(mapping_number(payload, "command_text_snapshot_ready"), 1);
        ASSERT_GT(mapping_number(payload, "command_text_snapshot_bytes"), 0);
        ASSERT_EQ(mapping_number(payload, "command_text_snapshot_redacted"), 1);
        ASSERT_STREQ(mapping_string(payload, "input_callback_state_policy"), "redacted_input_to_get_char_state_v1");
        ASSERT_EQ(mapping_number(payload, "input_callback_state_snapshot_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_state_redacted"), 1);
        ASSERT_STREQ(mapping_string(payload, "input_callback_frame_model"),
                     "owner_command_frame_input_callback_detach_v1");
        ASSERT_EQ(mapping_number(payload, "input_callback_frame_detach_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_frame_executor_ready"), 1);
        ASSERT_STREQ(mapping_string(payload, "input_callback_apply_frame_model"),
                     "owner_command_frame_input_callback_apply");
        ASSERT_STREQ(mapping_string(payload, "input_callback_apply_frame_task_type"),
                     "interactive_input_callback");
        ASSERT_EQ(mapping_number(payload, "input_callback_apply_frame_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_apply_frame_executor_ready"), 1);
        ASSERT_STREQ(mapping_string(payload, "input_callback_mode_delta_model"),
                     "owner_command_frame_input_callback_mode_delta");
        ASSERT_EQ(mapping_number(payload, "input_callback_mode_delta_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_mode_delta_executor_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_active"), 0);
        ASSERT_EQ(mapping_number(payload, "input_callback_single_char"), 0);
        ASSERT_EQ(mapping_number(payload, "input_callback_noescape"), 0);
        ASSERT_EQ(mapping_number(payload, "input_callback_noecho"), 0);
        ASSERT_EQ(mapping_number(payload, "input_callback_carryover_count"), 0);
        ASSERT_EQ(mapping_number(payload, "input_callback_function_redacted"), 0);
        ASSERT_EQ(mapping_number(payload, "input_callback_object_redacted"), 0);
        ASSERT_STREQ(mapping_string(payload, "process_input_add_action_parser_state_policy"),
                     "redacted_process_input_add_action_parser_state_v1");
        ASSERT_EQ(mapping_number(payload, "process_input_add_action_parser_state_snapshot_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "process_input_add_action_parser_state_redacted"), 1);
        ASSERT_EQ(mapping_number(payload, "process_input_add_action_parser_has_process_input"), 1);
        ASSERT_EQ(mapping_number(payload, "process_input_add_action_parser_safe_parse_fallback"), 1);
        ASSERT_EQ(mapping_number(payload, "process_input_add_action_parser_requires_command_giver"), 1);
        ASSERT_EQ(mapping_number(payload, "process_input_add_action_parser_command_giver_redacted"), 1);
        ASSERT_EQ(mapping_number(payload, "process_input_add_action_parser_command_text_redacted"), 1);
        ASSERT_STREQ(mapping_string(payload, "process_input_apply_frame_model"),
                     "owner_command_frame_process_input_apply");
        ASSERT_STREQ(mapping_string(payload, "process_input_apply_frame_task_type"),
                     "interactive_command_parser");
        ASSERT_EQ(mapping_number(payload, "process_input_apply_frame_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "process_input_apply_frame_executor_ready"), 1);
        ASSERT_STREQ(mapping_string(payload, "process_input_add_action_parser_frame_model"),
                     "owner_command_parser_context_v1");
        ASSERT_EQ(mapping_number(payload, "process_input_add_action_parser_frame_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "process_input_add_action_parser_frame_executor_ready"), 1);
        ASSERT_STREQ(mapping_string(payload, "process_input_add_action_parser_blocker"), "");
        ASSERT_FALSE(mapping_has_string_key(payload, "process_input_add_action_parser_command_giver"));
        ASSERT_FALSE(mapping_has_string_key(payload, "process_input_add_action_parser_command_text"));
        ASSERT_STREQ(mapping_string(payload, "interactive_mode_flags_state_policy"),
                     "redacted_interactive_mode_flags_v1");
        ASSERT_EQ(mapping_number(payload, "interactive_mode_flags_state_snapshot_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_flags_state_redacted"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_noecho"), 0);
        ASSERT_STREQ(mapping_string(payload, "interactive_mode_localecho_restore_model"),
                     "owner_command_frame_localecho_restore");
        ASSERT_STREQ(mapping_string(payload, "interactive_mode_localecho_restore_task_type"),
                     "interactive_mode_flags");
        ASSERT_STREQ(mapping_string(payload, "interactive_mode_localecho_restore_boundary"),
                     "main_reply_queue_after_command_consume");
        ASSERT_EQ(mapping_number(payload, "interactive_mode_localecho_restore_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_localecho_restore_executor_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_localecho_restore_required"), 0);
        ASSERT_STREQ(mapping_string(payload, "interactive_mode_terminal_mode_delta_boundary"),
                     "main_mode_delta_queue_after_command_consume");
        ASSERT_EQ(mapping_number(payload, "interactive_mode_terminal_mode_delta_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_terminal_linemode_restore_required"), 0);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_terminal_charmode_restore_required"), 0);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_noescape"), 0);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_single_char"), 0);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_was_single_char"), 0);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_using_mxp"), 0);
        ASSERT_STREQ(mapping_string(payload, "interactive_mode_mxp_tag_filter_model"),
                     "owner_command_frame_mxp_tag_filter");
        ASSERT_STREQ(mapping_string(payload, "interactive_mode_mxp_tag_filter_task_type"),
                     "interactive_mode_flags");
        ASSERT_EQ(mapping_number(payload, "interactive_mode_mxp_tag_filter_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_mxp_tag_filter_executor_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_mxp_tag_filter_required"), 0);
        ASSERT_STREQ(mapping_string(payload, "interactive_mode_ed_command_model"),
                     "owner_command_frame_ed_command");
        ASSERT_STREQ(mapping_string(payload, "interactive_mode_ed_command_task_type"),
                     "interactive_mode_flags");
        ASSERT_EQ(mapping_number(payload, "interactive_mode_ed_command_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_ed_command_executor_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_ed_command_required"), 0);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_ed_buffer_active"), 0);
        ASSERT_STREQ(mapping_string(payload, "prompt_telnet_reschedule_state_policy"),
                     "redacted_prompt_telnet_reschedule_io_v1");
        ASSERT_EQ(mapping_number(payload, "prompt_telnet_reschedule_state_snapshot_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "prompt_telnet_reschedule_state_redacted"), 1);
        ASSERT_STREQ(mapping_string(payload, "prompt_telnet_reschedule_boundary"),
                     "main_reply_queue_after_owner_command");
        ASSERT_EQ(mapping_number(payload, "prompt_telnet_reschedule_reply_queue_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "prompt_telnet_reschedule_blocks_activation"), 0);
        ASSERT_EQ(mapping_number(payload, "prompt_has_write_prompt"), 1);
        ASSERT_EQ(mapping_number(payload, "prompt_text_redacted"), 1);
        ASSERT_EQ(mapping_number(payload, "prompt_write_prompt_apply_required"), 1);
        ASSERT_STREQ(mapping_string(payload, "prompt_write_prompt_apply_frame_model"),
                     "owner_command_frame_write_prompt_apply");
        ASSERT_STREQ(mapping_string(payload, "prompt_write_prompt_apply_frame_task_type"), "command_reply");
        ASSERT_EQ(mapping_number(payload, "prompt_write_prompt_apply_frame_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "prompt_write_prompt_apply_frame_executor_ready"), 0);
        ASSERT_EQ(mapping_number(payload, "telnet_handle_active"), 0);
        ASSERT_EQ(mapping_number(payload, "telnet_using_telnet"), 0);
        ASSERT_EQ(mapping_number(payload, "telnet_suppress_ga"), 0);
        ASSERT_EQ(mapping_number(payload, "telnet_ga_required"), 0);
        ASSERT_EQ(mapping_number(payload, "reschedule_cmd_in_buf"), 1);
        ASSERT_FALSE(mapping_has_string_key(payload, "prompt_text"));
        ASSERT_STREQ(mapping_string(payload, "command_executor_blocker"),
                     "interactive_command_requires_main_thread_io_adapter");
        ASSERT_STREQ(mapping_string(payload, "command_consume_model"), "owner_owned_snapshot_main_thread_consume");
        ASSERT_EQ(mapping_number(payload, "command_consume_snapshot_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "command_consume_executor_ready"), 0);
        ASSERT_STREQ(mapping_string(payload, "command_consume_blocker"),
                     "interactive_command_requires_main_thread_io_adapter");
        ASSERT_STREQ(mapping_string(payload, "execution_frame_restore_policy"), "main_thread_vmcontext_scope");
        ASSERT_EQ(mapping_number(payload, "execution_frame_restore_ready"), 1);
        ASSERT_STREQ(mapping_string(payload, "execution_frame_restore_blocker"), "");
        ASSERT_FALSE(mapping_has_string_key(payload, "command_text"));
        ASSERT_STREQ(mapping_string(payload, "session_id"), "gw-test-command");
        ASSERT_EQ(mapping_number(payload, "cmd_in_buf"), 1);
        ASSERT_EQ(mapping_number(payload, "gateway_session"), 1);
        ASSERT_GT(mapping_number(payload, "pending_bytes"), 0);
      }
    }
  }
  ASSERT_TRUE(found_command_task);
  ASSERT_TRUE(found_process_input_apply_frame_entered);
  ASSERT_TRUE(found_parser_frame_entered);
  ASSERT_TRUE(found_reply_task_queued);
  ASSERT_TRUE(found_reply_task_dispatched);
  ASSERT_TRUE(found_write_prompt_apply_frame_entered);
  free_mapping(trace);

  add_ref(ob, "TestGatewayCommandTaskCarriesOwnerHandlePayload");
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-command", "test_done", "done"), 1);
  ASSERT_EQ(ob->interactive, nullptr);
  destruct_object(ob);
  free_object(&ob, "TestGatewayCommandTaskCarriesOwnerHandlePayload");
}

TEST_F(DriverTest, TestGatewayCommandMxpTagFilterFrame) {
  auto *ob = create_gateway_session_for_test("gw-test-command-mxp", "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));
  auto owner_epoch = vm_owner_epoch(ob);
  ob->interactive->iflags |= USING_MXP;

  auto mapping_number = [](mapping_t *map, const char *key) -> long {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t *map, const char *key) -> const char * {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

  ASSERT_EQ(gateway_inject_input_internal(ob, " [xz"), 1);
  ASSERT_EQ(gateway_process_pending_command_internal(ob), 1);

  auto *trace = vm_owner_task_trace(64);
  ASSERT_NE(trace, nullptr);
  ASSERT_EQ(mapping_number(trace, "success"), 1);
  auto *events_value = find_string_in_mapping(trace, "events");
  ASSERT_NE(events_value, nullptr);
  ASSERT_EQ(events_value ? events_value->type : T_INVALID, T_ARRAY);
  bool found_mxp_filter_frame = false;
  bool found_command_payload = false;
  if (events_value && events_value->type == T_ARRAY) {
    for (int i = 0; i < events_value->u.arr->size; i++) {
      auto *event = events_value->u.arr->item[i].u.map;
      if (std::string(mapping_string(event, "task_type")) == "interactive_mode_flags" &&
          std::string(mapping_string(event, "task_key")) == "mxp_tag_filter" &&
          std::string(mapping_string(event, "state")) == "frame_entered" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        found_mxp_filter_frame = true;
      }
      if (std::string(mapping_string(event, "task_type")) == "gateway_command_execute" &&
          std::string(mapping_string(event, "task_key")) == "process_user_command" &&
          std::string(mapping_string(event, "state")) == "main_queued" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        auto *payload_value = find_string_in_mapping(event, "payload");
        ASSERT_NE(payload_value, nullptr);
        ASSERT_EQ(payload_value ? payload_value->type : T_INVALID, T_MAPPING);
        auto *payload = payload_value->u.map;
        if (std::string(mapping_string(payload, "session_id")) != "gw-test-command-mxp") {
          continue;
        }
        found_command_payload = true;
        ASSERT_EQ(mapping_number(payload, "interactive_mode_using_mxp"), 1);
        ASSERT_STREQ(mapping_string(payload, "interactive_mode_mxp_tag_filter_model"),
                     "owner_command_frame_mxp_tag_filter");
        ASSERT_EQ(mapping_number(payload, "interactive_mode_mxp_tag_filter_required"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_mxp_tag_filter_executor_ready"), 1);
      }
    }
  }
  ASSERT_TRUE(found_mxp_filter_frame);
  ASSERT_TRUE(found_command_payload);
  free_mapping(trace);

  add_ref(ob, "TestGatewayCommandMxpTagFilterFrame");
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-command-mxp", "test_done", "done"), 1);
  ASSERT_EQ(ob->interactive, nullptr);
  destruct_object(ob);
  free_object(&ob, "TestGatewayCommandMxpTagFilterFrame");
}

TEST_F(DriverTest, TestGatewayCommandEdCommandFrame) {
  auto *ob = create_gateway_session_for_test("gw-test-command-ed", "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));
  auto owner_epoch = vm_owner_epoch(ob);

  auto *enabled = call_lpc_method(ob, "enable_gateway_ed");
  ASSERT_NE(enabled, nullptr);
  ASSERT_EQ(enabled->type, T_NUMBER);
  ASSERT_EQ(enabled->u.number, 1);
  ASSERT_NE(ob->interactive->ed_buffer, nullptr);

  auto mapping_number = [](mapping_t *map, const char *key) -> long {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t *map, const char *key) -> const char * {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

  ASSERT_EQ(gateway_inject_input_internal(ob, "Q"), 1);
  ASSERT_EQ(gateway_process_pending_command_internal(ob), 1);
  ASSERT_EQ(ob->interactive->ed_buffer, nullptr);

  auto *trace = vm_owner_task_trace(64);
  ASSERT_NE(trace, nullptr);
  ASSERT_EQ(mapping_number(trace, "success"), 1);
  auto *events_value = find_string_in_mapping(trace, "events");
  ASSERT_NE(events_value, nullptr);
  ASSERT_EQ(events_value ? events_value->type : T_INVALID, T_ARRAY);
  bool found_ed_frame = false;
  bool found_command_payload = false;
  if (events_value && events_value->type == T_ARRAY) {
    for (int i = 0; i < events_value->u.arr->size; i++) {
      auto *event = events_value->u.arr->item[i].u.map;
      if (std::string(mapping_string(event, "task_type")) == "interactive_mode_flags" &&
          std::string(mapping_string(event, "task_key")) == "ed_command" &&
          std::string(mapping_string(event, "state")) == "frame_entered" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        found_ed_frame = true;
      }
      if (std::string(mapping_string(event, "task_type")) == "gateway_command_execute" &&
          std::string(mapping_string(event, "task_key")) == "process_user_command" &&
          std::string(mapping_string(event, "state")) == "main_queued" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        auto *payload_value = find_string_in_mapping(event, "payload");
        ASSERT_NE(payload_value, nullptr);
        ASSERT_EQ(payload_value ? payload_value->type : T_INVALID, T_MAPPING);
        auto *payload = payload_value->u.map;
        if (std::string(mapping_string(payload, "session_id")) != "gw-test-command-ed") {
          continue;
        }
        found_command_payload = true;
        ASSERT_STREQ(mapping_string(payload, "interactive_mode_ed_command_model"),
                     "owner_command_frame_ed_command");
        ASSERT_EQ(mapping_number(payload, "interactive_mode_ed_command_required"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_ed_command_executor_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_ed_buffer_active"), 1);
      }
    }
  }
  ASSERT_TRUE(found_ed_frame);
  ASSERT_TRUE(found_command_payload);
  free_mapping(trace);

  add_ref(ob, "TestGatewayCommandEdCommandFrame");
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-command-ed", "test_done", "done"), 1);
  ASSERT_EQ(ob->interactive, nullptr);
  destruct_object(ob);
  free_object(&ob, "TestGatewayCommandEdCommandFrame");
}

TEST_F(DriverTest, TestGatewayCommandPayloadSnapshotsActiveInputToState) {
  auto *ob = create_gateway_session_for_test("gw-test-command-input-to", "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));
  auto owner_epoch = vm_owner_epoch(ob);

  auto mapping_number = [](mapping_t *map, const char *key) -> long {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t *map, const char *key) -> const char * {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
  auto mapping_has_string_key = [](mapping_t *map, const char *key) -> bool {
    auto *keys = mapping_indices(map);
    bool found = false;
    for (int i = 0; keys && i < keys->size; i++) {
      if (keys->item[i].type == T_STRING && std::strcmp(keys->item[i].u.string, key) == 0) {
        found = true;
        break;
      }
    }
    if (keys) {
      free_array(keys);
    }
    return found;
  };

  auto *enabled = call_lpc_method(ob, "enable_gateway_input_to");
  ASSERT_NE(enabled, nullptr);
  ASSERT_EQ(enabled->type, T_NUMBER);
  ASSERT_EQ(enabled->u.number, 1);
  ASSERT_NE(ob->interactive->input_to, nullptr);
  ASSERT_EQ(ob->interactive->num_carry, 1);
  ASSERT_TRUE((ob->interactive->iflags & NOECHO) != 0);
  ASSERT_TRUE((ob->interactive->iflags & NOESC) != 0);

  ASSERT_EQ(gateway_inject_input_internal(ob, "answer"), 1);
  ASSERT_EQ(gateway_process_pending_command_internal(ob), 1);

  auto *line = call_lpc_method(ob, "query_last_input_to_line");
  ASSERT_NE(line, nullptr);
  ASSERT_EQ(line->type, T_STRING);
  ASSERT_STREQ(line->u.string, "answer");
  auto *token = call_lpc_method(ob, "query_last_input_to_token");
  ASSERT_NE(token, nullptr);
  ASSERT_EQ(token->type, T_STRING);
  ASSERT_STREQ(token->u.string, "carry-token");

  auto *trace = vm_owner_task_trace(64);
  ASSERT_NE(trace, nullptr);
  ASSERT_EQ(mapping_number(trace, "success"), 1);
  auto *events_value = find_string_in_mapping(trace, "events");
  ASSERT_NE(events_value, nullptr);
  ASSERT_EQ(events_value ? events_value->type : T_INVALID, T_ARRAY);
  bool found_command_task = false;
  bool found_input_callback_apply_frame_entered = false;
  bool found_noecho_localecho_frame_detached = false;
  bool found_localecho_restore_queued = false;
  bool found_localecho_restore_dispatched = false;
  if (events_value && events_value->type == T_ARRAY) {
    for (int i = 0; i < events_value->u.arr->size; i++) {
      auto *event = events_value->u.arr->item[i].u.map;
      if (std::string(mapping_string(event, "task_type")) == "interactive_input_callback" &&
          std::string(mapping_string(event, "task_key")) == "gateway_input_to_callback" &&
          std::string(mapping_string(event, "state")) == "frame_entered" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        found_input_callback_apply_frame_entered = true;
      }
      if (std::string(mapping_string(event, "task_type")) == "interactive_mode_flags" &&
          std::string(mapping_string(event, "task_key")) == "noecho_localecho_restore" &&
          std::string(mapping_string(event, "state")) == "frame_detached" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        found_noecho_localecho_frame_detached = true;
      }
      if (std::string(mapping_string(event, "task_type")) == "command_reply" &&
          std::string(mapping_string(event, "task_key")) == "localecho_restore" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        auto state = std::string(mapping_string(event, "state"));
        found_localecho_restore_queued = found_localecho_restore_queued || state == "main_queued";
        found_localecho_restore_dispatched = found_localecho_restore_dispatched || state == "main_dispatched";
      }
      if (std::string(mapping_string(event, "task_type")) == "gateway_command_execute" &&
          std::string(mapping_string(event, "task_key")) == "process_user_command" &&
          std::string(mapping_string(event, "state")) == "main_queued" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        auto *payload_value = find_string_in_mapping(event, "payload");
        ASSERT_NE(payload_value, nullptr);
        ASSERT_EQ(payload_value ? payload_value->type : T_INVALID, T_MAPPING);
        auto *payload = payload_value->u.map;
        if (std::string(mapping_string(payload, "session_id")) != "gw-test-command-input-to") {
          continue;
        }
        found_command_task = true;
        ASSERT_STREQ(mapping_string(payload, "input_callback_state_policy"), "redacted_input_to_get_char_state_v1");
        ASSERT_EQ(mapping_number(payload, "input_callback_state_snapshot_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_state_redacted"), 1);
        ASSERT_STREQ(mapping_string(payload, "input_callback_frame_model"),
                     "owner_command_frame_input_callback_detach_v1");
        ASSERT_EQ(mapping_number(payload, "input_callback_frame_detach_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_frame_executor_ready"), 1);
        ASSERT_STREQ(mapping_string(payload, "input_callback_apply_frame_model"),
                     "owner_command_frame_input_callback_apply");
        ASSERT_STREQ(mapping_string(payload, "input_callback_apply_frame_task_type"),
                     "interactive_input_callback");
        ASSERT_EQ(mapping_number(payload, "input_callback_apply_frame_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_apply_frame_executor_ready"), 1);
        ASSERT_STREQ(mapping_string(payload, "input_callback_mode_delta_model"),
                     "owner_command_frame_input_callback_mode_delta");
        ASSERT_EQ(mapping_number(payload, "input_callback_mode_delta_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_mode_delta_executor_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_active"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_single_char"), 0);
        ASSERT_EQ(mapping_number(payload, "input_callback_noescape"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_noecho"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_noecho"), 1);
        ASSERT_STREQ(mapping_string(payload, "interactive_mode_localecho_restore_model"),
                     "owner_command_frame_localecho_restore");
        ASSERT_STREQ(mapping_string(payload, "interactive_mode_localecho_restore_task_type"),
                     "interactive_mode_flags");
        ASSERT_STREQ(mapping_string(payload, "interactive_mode_localecho_restore_boundary"),
                     "main_reply_queue_after_command_consume");
        ASSERT_EQ(mapping_number(payload, "interactive_mode_localecho_restore_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_localecho_restore_executor_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_localecho_restore_required"), 1);
        ASSERT_STREQ(mapping_string(payload, "interactive_mode_terminal_mode_delta_boundary"),
                     "main_mode_delta_queue_after_command_consume");
        ASSERT_EQ(mapping_number(payload, "interactive_mode_terminal_mode_delta_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_terminal_linemode_restore_required"), 0);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_terminal_charmode_restore_required"), 0);
        ASSERT_EQ(mapping_number(payload, "input_callback_carryover_count"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_function_redacted"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_object_redacted"), 1);
        ASSERT_FALSE(mapping_has_string_key(payload, "input_callback_function"));
        ASSERT_FALSE(mapping_has_string_key(payload, "input_callback_object"));
        ASSERT_FALSE(mapping_has_string_key(payload, "command_text"));
      }
    }
  }
  ASSERT_TRUE(found_command_task);
  ASSERT_TRUE(found_input_callback_apply_frame_entered);
  ASSERT_TRUE(found_noecho_localecho_frame_detached);
  ASSERT_TRUE(found_localecho_restore_queued);
  ASSERT_TRUE(found_localecho_restore_dispatched);
  free_mapping(trace);

  add_ref(ob, "TestGatewayCommandPayloadSnapshotsActiveInputToState");
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-command-input-to", "test_done", "done"), 1);
  ASSERT_EQ(ob->interactive, nullptr);
  destruct_object(ob);
  free_object(&ob, "TestGatewayCommandPayloadSnapshotsActiveInputToState");
}

TEST_F(DriverTest, TestGatewayCommandPayloadSnapshotsActiveGetCharState) {
  auto *ob = create_gateway_session_for_test("gw-test-command-get-char", "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));
  auto owner_epoch = vm_owner_epoch(ob);

  auto mapping_number = [](mapping_t *map, const char *key) -> long {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t *map, const char *key) -> const char * {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
  auto mapping_has_string_key = [](mapping_t *map, const char *key) -> bool {
    auto *keys = mapping_indices(map);
    bool found = false;
    for (int i = 0; keys && i < keys->size; i++) {
      if (keys->item[i].type == T_STRING && std::strcmp(keys->item[i].u.string, key) == 0) {
        found = true;
        break;
      }
    }
    if (keys) {
      free_array(keys);
    }
    return found;
  };

  auto *enabled = call_lpc_method(ob, "enable_gateway_get_char");
  ASSERT_NE(enabled, nullptr);
  ASSERT_EQ(enabled->type, T_NUMBER);
  ASSERT_EQ(enabled->u.number, 1);
  ASSERT_NE(ob->interactive->input_to, nullptr);
  ASSERT_EQ(ob->interactive->num_carry, 1);
  ASSERT_TRUE((ob->interactive->iflags & SINGLE_CHAR) != 0);
  ASSERT_TRUE((ob->interactive->iflags & NOECHO) != 0);
  ASSERT_TRUE((ob->interactive->iflags & NOESC) != 0);

  ASSERT_EQ(gateway_inject_input_internal(ob, "z"), 1);
  ASSERT_EQ(gateway_process_pending_command_internal(ob), 1);

  auto *value = call_lpc_method(ob, "query_last_get_char_value");
  ASSERT_NE(value, nullptr);
  ASSERT_EQ(value->type, T_STRING);
  ASSERT_STREQ(value->u.string, "z");
  auto *token = call_lpc_method(ob, "query_last_get_char_token");
  ASSERT_NE(token, nullptr);
  ASSERT_EQ(token->type, T_STRING);
  ASSERT_STREQ(token->u.string, "char-token");

  auto *trace = vm_owner_task_trace(96);
  ASSERT_NE(trace, nullptr);
  ASSERT_EQ(mapping_number(trace, "success"), 1);
  auto *events_value = find_string_in_mapping(trace, "events");
  ASSERT_NE(events_value, nullptr);
  ASSERT_EQ(events_value ? events_value->type : T_INVALID, T_ARRAY);
  bool found_command_task = false;
  bool found_input_callback_frame_detached = false;
  bool found_input_callback_apply_frame_entered = false;
  bool found_input_callback_mode_frame_detached = false;
  bool found_linemode_restore_queued = false;
  bool found_linemode_restore_dispatched = false;
  if (events_value && events_value->type == T_ARRAY) {
    for (int i = 0; i < events_value->u.arr->size; i++) {
      auto *event = events_value->u.arr->item[i].u.map;
      if (std::string(mapping_string(event, "task_type")) == "interactive_input_callback" &&
          std::string(mapping_string(event, "task_key")) == "gateway_get_char_callback" &&
          std::string(mapping_string(event, "state")) == "frame_detached" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        found_input_callback_frame_detached = true;
      }
      if (std::string(mapping_string(event, "task_type")) == "interactive_input_callback" &&
          std::string(mapping_string(event, "task_key")) == "gateway_get_char_callback" &&
          std::string(mapping_string(event, "state")) == "frame_entered" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        found_input_callback_apply_frame_entered = true;
      }
      if (std::string(mapping_string(event, "task_type")) == "interactive_input_callback_mode" &&
          std::string(mapping_string(event, "task_key")) == "input_to_get_char_mode_flags" &&
          std::string(mapping_string(event, "state")) == "frame_detached" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        found_input_callback_mode_frame_detached = true;
      }
      if (std::string(mapping_string(event, "task_type")) == "command_mode_delta" &&
          std::string(mapping_string(event, "task_key")) == "get_char_linemode_restore" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        auto state = std::string(mapping_string(event, "state"));
        found_linemode_restore_queued = found_linemode_restore_queued || state == "main_queued";
        found_linemode_restore_dispatched = found_linemode_restore_dispatched || state == "main_dispatched";
      }
      if (std::string(mapping_string(event, "task_type")) == "gateway_command_execute" &&
          std::string(mapping_string(event, "task_key")) == "process_user_command" &&
          std::string(mapping_string(event, "state")) == "main_queued" &&
          std::string(mapping_string(event, "owner_id")) == vm_owner_id(ob) &&
          mapping_number(event, "owner_epoch") == static_cast<long>(owner_epoch)) {
        auto *payload_value = find_string_in_mapping(event, "payload");
        ASSERT_NE(payload_value, nullptr);
        ASSERT_EQ(payload_value ? payload_value->type : T_INVALID, T_MAPPING);
        auto *payload = payload_value->u.map;
        if (std::string(mapping_string(payload, "session_id")) != "gw-test-command-get-char") {
          continue;
        }
        found_command_task = true;
        ASSERT_STREQ(mapping_string(payload, "input_callback_state_policy"), "redacted_input_to_get_char_state_v1");
        ASSERT_EQ(mapping_number(payload, "input_callback_state_snapshot_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_state_redacted"), 1);
        ASSERT_STREQ(mapping_string(payload, "input_callback_frame_model"),
                     "owner_command_frame_input_callback_detach_v1");
        ASSERT_EQ(mapping_number(payload, "input_callback_frame_detach_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_frame_executor_ready"), 1);
        ASSERT_STREQ(mapping_string(payload, "input_callback_apply_frame_model"),
                     "owner_command_frame_input_callback_apply");
        ASSERT_STREQ(mapping_string(payload, "input_callback_apply_frame_task_type"),
                     "interactive_input_callback");
        ASSERT_EQ(mapping_number(payload, "input_callback_apply_frame_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_apply_frame_executor_ready"), 1);
        ASSERT_STREQ(mapping_string(payload, "input_callback_mode_delta_model"),
                     "owner_command_frame_input_callback_mode_delta");
        ASSERT_EQ(mapping_number(payload, "input_callback_mode_delta_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_mode_delta_executor_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_active"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_single_char"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_noescape"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_noecho"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_carryover_count"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_function_redacted"), 1);
        ASSERT_EQ(mapping_number(payload, "input_callback_object_redacted"), 1);
        ASSERT_STREQ(mapping_string(payload, "interactive_mode_flags_state_policy"),
                     "redacted_interactive_mode_flags_v1");
        ASSERT_EQ(mapping_number(payload, "interactive_mode_noecho"), 1);
        ASSERT_STREQ(mapping_string(payload, "interactive_mode_terminal_mode_delta_boundary"),
                     "main_mode_delta_queue_after_command_consume");
        ASSERT_EQ(mapping_number(payload, "interactive_mode_terminal_mode_delta_ready"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_terminal_linemode_restore_required"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_terminal_charmode_restore_required"), 0);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_noescape"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_single_char"), 1);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_was_single_char"), 0);
        ASSERT_EQ(mapping_number(payload, "interactive_mode_ed_buffer_active"), 0);
        ASSERT_FALSE(mapping_has_string_key(payload, "input_callback_function"));
        ASSERT_FALSE(mapping_has_string_key(payload, "input_callback_object"));
        ASSERT_FALSE(mapping_has_string_key(payload, "command_text"));
      }
    }
  }
  ASSERT_TRUE(found_command_task);
  ASSERT_TRUE(found_input_callback_frame_detached);
  ASSERT_TRUE(found_input_callback_apply_frame_entered);
  ASSERT_TRUE(found_input_callback_mode_frame_detached);
  ASSERT_TRUE(found_linemode_restore_queued);
  ASSERT_TRUE(found_linemode_restore_dispatched);
  free_mapping(trace);

  add_ref(ob, "TestGatewayCommandPayloadSnapshotsActiveGetCharState");
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-command-get-char", "test_done", "done"), 1);
  ASSERT_EQ(ob->interactive, nullptr);
  destruct_object(ob);
  free_object(&ob, "TestGatewayCommandPayloadSnapshotsActiveGetCharState");
}

TEST_F(DriverTest, TestGatewayCommandExecutesThroughOwnerMainQueue) {
  struct RuntimeGuard {
    int saved_mode;
    object_t* ob{nullptr};
    const char* session_id{nullptr};

    ~RuntimeGuard() {
      vm_owner_thread_stop();
      vm_owner_drain_main_tasks(64);
      if (ob) {
        add_ref(ob, "TestGatewayCommandExecutesThroughOwnerMainQueue");
        if (ob->interactive && session_id) {
          gateway_destroy_session_internal(session_id, "test_done", "done");
        }
        vm_owner_clear_id(ob);
        destruct_object(ob);
        free_object(&ob, "TestGatewayCommandExecutesThroughOwnerMainQueue");
      }
      CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
    }
  } runtime_guard{CONFIG_INT(__RC_MULTICORE_MODE__)};
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;
  vm_owner_thread_stop();

  auto *ob = create_gateway_session_for_test("gw-test-command-executor", "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));
  runtime_guard.ob = ob;
  runtime_guard.session_id = "gw-test-command-executor";
  vm_owner_set_id(ob, "owner/test/gateway-command-main-queue");
  const std::string owner_id = vm_owner_id(ob);
  auto owner_epoch = vm_owner_epoch(ob);

  auto call_number = [](const char* method, object_t* target) -> long {
    auto* ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
    EXPECT_EQ(ret ? ret->type : T_INVALID, T_NUMBER);
    return ret && ret->type == T_NUMBER ? ret->u.number : -1;
  };
  auto call_string = [](const char* method, object_t* target) -> std::string {
    auto* ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
    if (ret && ret->type == T_NUMBER && ret->u.number == 0) {
      return "";
    }
    EXPECT_EQ(ret ? ret->type : T_INVALID, T_STRING);
    return ret && ret->type == T_STRING ? ret->u.string : "";
  };
  auto mapping_number = [](mapping_t *map, const char *key) -> long {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t *map, const char *key) -> const char * {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
  auto trace_has_state = [&](const char* state, uint64_t task_id = 0) {
    auto* trace = vm_owner_task_trace(256);
    auto* events = find_string_in_mapping(trace, "events");
    bool found = false;
    EXPECT_NE(events, nullptr);
    EXPECT_EQ(events ? events->type : T_INVALID, T_ARRAY);
    if (events && events->type == T_ARRAY) {
      for (int i = 0; i < events->u.arr->size; i++) {
        auto* event = events->u.arr->item[i].u.map;
        if (std::string(mapping_string(event, "task_type")) == "gateway_command_execute" &&
            std::string(mapping_string(event, "task_key")) == "process_user_command" &&
            std::string(mapping_string(event, "state")) == state &&
            (task_id == 0 || mapping_number(event, "task_id") == static_cast<long>(task_id))) {
          found = true;
          break;
        }
      }
    }
    free_mapping(trace);
    return found;
  };

  safe_apply("reset_gateway_command_probe", ob, 0, ORIGIN_DRIVER);
  ASSERT_EQ(gateway_session_command_pending_count(), 0);
  auto* before = vm_owner_thread_status();
  auto before_callback_queued = mapping_number(before, "executor_callback_queued");
  auto before_callback_dispatched = mapping_number(before, "executor_callback_dispatched");
  free_mapping(before);

  ASSERT_EQ(gateway_inject_input_internal(ob, "look"), 1);
  ASSERT_EQ(gateway_session_command_input_pending_count(), 1);
  ASSERT_EQ(gateway_session_command_task_pending_count(), 0);
  auto task_id = gateway_enqueue_pending_command_internal(ob);
  ASSERT_GT(task_id, 0u);
  ASSERT_EQ(gateway_session_command_pending_count(), 2);
  ASSERT_EQ(gateway_session_command_task_pending_count(), 1);

  ASSERT_GE(vm_owner_drain_main_tasks(64), 1);
  ASSERT_EQ(gateway_session_command_pending_count(), 0);
  ASSERT_TRUE(trace_has_state("main_dispatched", task_id));
  ASSERT_EQ(call_number("query_last_process_input_off_main", ob), 0);
  ASSERT_EQ(call_string("query_last_process_input_command", ob), "look");
  ASSERT_FALSE(ob->interactive->iflags & CMD_IN_BUF);

  auto* after = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(after, "executor_callback_queued"), before_callback_queued);
  ASSERT_EQ(mapping_number(after, "executor_callback_dispatched"), before_callback_dispatched);
  ASSERT_EQ(mapping_number(after, "gateway_command_execute_ready"), 1);
  ASSERT_STREQ(mapping_string(after, "gateway_command_execute_task_type"), "gateway_command_execute");
  ASSERT_STREQ(mapping_string(after, "gateway_command_execute_route"), "owner_main_queue_io_adapter");
  ASSERT_STREQ(mapping_string(after, "gateway_command_execute_fallback_route"),
               "");
  ASSERT_STREQ(mapping_string(after, "gateway_command_execute_policy"),
               "main_thread_io_adapter_until_interactive_detached");
  ASSERT_EQ(mapping_number(after, "gateway_command_execute_reply_queue_main_ready"), 1);
  ASSERT_EQ(mapping_number(after, "gateway_command_execute_stale_drop_ready"), 1);
  ASSERT_EQ(mapping_number(after, "gateway_command_execute_context_cleanup_ready"), 1);
  ASSERT_EQ(mapping_number(after, "gateway_command_execute_session_revalidate_ready"), 1);
  free_mapping(after);
}

TEST_F(DriverTest, TestGatewayCommandMainQueueDropsStaleOwnerEpoch) {
  const char* owner = "owner/test/gateway-command-main-stale";
  const char* moved_owner = "owner/test/gateway-command-main-stale/moved";
  struct RuntimeGuard {
    int saved_mode;
    object_t* ob{nullptr};
    const char* session_id{nullptr};

    ~RuntimeGuard() {
      vm_owner_thread_stop();
      vm_owner_drain_main_tasks(64);
      if (ob) {
        add_ref(ob, "TestGatewayCommandMainQueueDropsStaleOwnerEpoch");
        if (ob->interactive && session_id) {
          gateway_destroy_session_internal(session_id, "test_done", "done");
        }
        vm_owner_clear_id(ob);
        destruct_object(ob);
        free_object(&ob, "TestGatewayCommandMainQueueDropsStaleOwnerEpoch");
      }
      CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
    }
  } runtime_guard{CONFIG_INT(__RC_MULTICORE_MODE__)};
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;
  vm_owner_thread_stop();

  auto *ob = create_gateway_session_for_test("gw-test-command-executor-stale", "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));
  runtime_guard.ob = ob;
  runtime_guard.session_id = "gw-test-command-executor-stale";
  vm_owner_set_id(ob, owner);

  auto call_number = [](const char* method, object_t* target) -> long {
    auto* ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
    EXPECT_EQ(ret ? ret->type : T_INVALID, T_NUMBER);
    return ret && ret->type == T_NUMBER ? ret->u.number : -1;
  };
  auto mapping_number = [](mapping_t *map, const char *key) -> long {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t *map, const char *key) -> const char * {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
  auto trace_has_gateway_stale = [&]() {
    auto* trace = vm_owner_task_trace(256);
    auto* events = find_string_in_mapping(trace, "events");
    bool found = false;
    EXPECT_NE(events, nullptr);
    EXPECT_EQ(events ? events->type : T_INVALID, T_ARRAY);
    if (events && events->type == T_ARRAY) {
      for (int i = 0; i < events->u.arr->size; i++) {
        auto* event = events->u.arr->item[i].u.map;
        if (std::string(mapping_string(event, "task_type")) == "gateway_command_execute" &&
            std::string(mapping_string(event, "task_key")) == "process_user_command" &&
            std::string(mapping_string(event, "owner_id")) == owner &&
            std::string(mapping_string(event, "state")) == "main_stale") {
          found = true;
          break;
        }
      }
    }
    free_mapping(trace);
    return found;
  };

  safe_apply("reset_gateway_command_probe", ob, 0, ORIGIN_DRIVER);
  ASSERT_EQ(gateway_session_command_pending_count(), 0);
  ASSERT_EQ(gateway_inject_input_internal(ob, "look"), 1);
  ASSERT_GT(gateway_enqueue_pending_command_internal(ob), 0u);
  ASSERT_EQ(gateway_session_command_pending_count(), 2);
  vm_owner_set_id(ob, moved_owner);
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);
  ASSERT_EQ(gateway_session_command_input_pending_count(), 1);
  ASSERT_EQ(gateway_session_command_task_pending_count(), 0);
  ASSERT_EQ(gateway_session_command_pending_count(), 1);

  for (int i = 0; i < 100 && !trace_has_gateway_stale(); i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_TRUE(trace_has_gateway_stale());
  ASSERT_TRUE(ob->interactive->iflags & CMD_IN_BUF);
  ASSERT_EQ(call_number("query_last_process_input_off_main", ob), 0);
}

TEST_F(DriverTest, TestGatewayCommandMainQueueDropsDisconnectedSession) {
  const char* owner = "owner/test/gateway-command-main-disconnect";
  const char* session_id = "gw-test-command-main-disconnect";
  struct RuntimeGuard {
    int saved_mode;
    object_t* ob{nullptr};

    ~RuntimeGuard() {
      vm_owner_thread_stop();
      vm_owner_drain_main_tasks(64);
      if (ob) {
        vm_owner_clear_id(ob);
        destruct_object(ob);
        free_object(&ob, "TestGatewayCommandMainQueueDropsDisconnectedSession");
      }
      CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
    }
  } runtime_guard{CONFIG_INT(__RC_MULTICORE_MODE__)};
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;
  vm_owner_thread_stop();

  auto *ob = create_gateway_session_for_test(session_id, "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));
  runtime_guard.ob = ob;
  add_ref(ob, "TestGatewayCommandMainQueueDropsDisconnectedSession");
  vm_owner_set_id(ob, owner);

  auto mapping_number = [](mapping_t *map, const char *key) -> long {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t *map, const char *key) -> const char * {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
  auto trace_has_disconnected_stale = [&]() {
    auto* trace = vm_owner_task_trace(256);
    auto* events = find_string_in_mapping(trace, "events");
    bool found = false;
    EXPECT_NE(events, nullptr);
    EXPECT_EQ(events ? events->type : T_INVALID, T_ARRAY);
    if (events && events->type == T_ARRAY) {
      for (int i = 0; i < events->u.arr->size; i++) {
        auto* event = events->u.arr->item[i].u.map;
        if (std::string(mapping_string(event, "task_type")) == "gateway_command_execute" &&
            std::string(mapping_string(event, "task_key")) == "process_user_command" &&
            std::string(mapping_string(event, "owner_id")) == owner &&
            std::string(mapping_string(event, "state")) == "session_stale") {
          found = true;
          break;
        }
      }
    }
    free_mapping(trace);
    return found;
  };

  ASSERT_EQ(gateway_inject_input_internal(ob, "look"), 1);
  ASSERT_GT(gateway_enqueue_pending_command_internal(ob), 0u);
  ASSERT_EQ(gateway_session_command_pending_count(), 2);
  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_disconnect", "disconnect"), 1);
  ASSERT_EQ(ob->interactive, nullptr);
  ASSERT_EQ(gateway_session_command_pending_count(), 0);
  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);
  ASSERT_EQ(gateway_session_command_pending_count(), 0);

  for (int i = 0; i < 100 && !trace_has_disconnected_stale(); i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_TRUE(trace_has_disconnected_stale());

  auto* status = vm_owner_thread_status();
  ASSERT_EQ(mapping_number(status, "gateway_command_execute_stale_drop_ready"), 1);
  free_mapping(status);
}

TEST_F(DriverTest, TestGatewayCommandMainQueueReschedulesBufferedCommands) {
  struct RuntimeGuard {
    int saved_mode;
    object_t* ob{nullptr};
    const char* session_id{nullptr};

    ~RuntimeGuard() {
      vm_owner_thread_stop();
      vm_owner_drain_main_tasks(64);
      if (ob) {
        add_ref(ob, "TestGatewayCommandMainQueueReschedulesBufferedCommands");
        if (ob->interactive && session_id) {
          gateway_destroy_session_internal(session_id, "test_done", "done");
        }
        vm_owner_clear_id(ob);
        destruct_object(ob);
        free_object(&ob, "TestGatewayCommandMainQueueReschedulesBufferedCommands");
      }
      CONFIG_INT(__RC_MULTICORE_MODE__) = saved_mode;
    }
  } runtime_guard{CONFIG_INT(__RC_MULTICORE_MODE__)};
  CONFIG_INT(__RC_MULTICORE_MODE__) = VM_MULTICORE_MODE_AUDIT;
  vm_owner_thread_stop();

  auto *ob = create_gateway_session_for_test("gw-test-command-buffered", "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));
  runtime_guard.ob = ob;
  runtime_guard.session_id = "gw-test-command-buffered";
  vm_owner_set_id(ob, "owner/test/gateway-command-buffered");

  auto call_string = [](const char* method, object_t* target) -> std::string {
    auto* ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
    if (ret && ret->type == T_NUMBER) {
      return "";
    }
    EXPECT_EQ(ret ? ret->type : T_INVALID, T_STRING);
    return ret && ret->type == T_STRING ? ret->u.string : "";
  };

  safe_apply("reset_gateway_command_probe", ob, 0, ORIGIN_DRIVER);
  ASSERT_EQ(gateway_inject_input_internal(ob, "look\nscore"), 1);
  ASSERT_EQ(gateway_session_command_input_pending_count(), 1);
  ASSERT_GT(gateway_enqueue_pending_command_internal(ob), 0u);
  ASSERT_EQ(gateway_session_command_pending_count(), 2);
  ASSERT_EQ(gateway_enqueue_pending_command_internal(ob), 0u);

  ASSERT_GE(vm_owner_drain_main_tasks(8), 1);
  ASSERT_EQ(call_string("query_last_process_input_command", ob), "look");
  ASSERT_TRUE(ob->interactive->iflags & CMD_IN_BUF);
  ASSERT_EQ(gateway_session_command_input_pending_count(), 1);
  ASSERT_EQ(gateway_session_command_task_pending_count(), 0);
  ASSERT_EQ(gateway_process_pending_command_internal(ob), 1);
  ASSERT_EQ(call_string("query_last_process_input_command", ob), "score");
  ASSERT_FALSE(ob->interactive->iflags & CMD_IN_BUF);
  ASSERT_EQ(gateway_session_command_pending_count(), 0);
}

TEST_F(DriverTest, TestGatewayCommandMainQueueStaleTraceIncludesFrameMetadata) {
  auto *ob = create_gateway_session_for_test("gw-test-command-stale", "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));
  vm_owner_set_id(ob, "owner/test/gateway-command-main-stale");
  const std::string owner_id = vm_owner_id(ob);
  auto stale_epoch = vm_owner_epoch(ob);

  auto mapping_number = [](mapping_t *map, const char *key) -> long {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t *map, const char *key) -> const char * {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };

  ASSERT_EQ(gateway_inject_input_internal(ob, "look"), 1);
  auto task_id = gateway_enqueue_pending_command_internal(ob);
  ASSERT_GT(task_id, 0u);
  vm_owner_set_id(ob, "owner/test/gateway-command-main-stale/moved");
  vm_owner_set_id(ob, owner_id.c_str());
  ASSERT_STREQ(vm_owner_id(ob), owner_id.c_str());
  auto current_epoch = vm_owner_epoch(ob);
  ASSERT_GT(current_epoch, stale_epoch);

  ASSERT_EQ(vm_owner_drain_main_tasks(8), 1);
  ASSERT_TRUE(ob->interactive->iflags & CMD_IN_BUF);

  auto *trace = vm_owner_task_trace(64);
  ASSERT_NE(trace, nullptr);
  ASSERT_EQ(mapping_number(trace, "success"), 1);
  auto *events_value = find_string_in_mapping(trace, "events");
  ASSERT_NE(events_value, nullptr);
  ASSERT_EQ(events_value ? events_value->type : T_INVALID, T_ARRAY);
  bool found_stale_command_task = false;
  bool found_interactive_dispatch = false;
  if (events_value && events_value->type == T_ARRAY) {
    for (int i = 0; i < events_value->u.arr->size; i++) {
      auto *event = events_value->u.arr->item[i].u.map;
      if (std::string(mapping_string(event, "task_type")) == "gateway_command_execute" &&
          std::string(mapping_string(event, "task_key")) == "process_user_command" &&
          mapping_number(event, "task_id") == static_cast<long>(task_id)) {
        if (std::string(mapping_string(event, "state")) == "main_stale") {
          found_stale_command_task = true;
          ASSERT_STREQ(mapping_string(event, "owner_id"), owner_id.c_str());
          ASSERT_EQ(mapping_number(event, "owner_epoch"), static_cast<long>(stale_epoch));
          ASSERT_EQ(mapping_number(event, "has_target_handle"), 1);
          ASSERT_EQ(mapping_number(event, "target_handle_current"), 0);
          ASSERT_STREQ(mapping_string(event, "target_handle_status"), "owner_epoch_mismatch");
          ASSERT_EQ(mapping_number(event, "target_owner_epoch"), static_cast<long>(stale_epoch));
          ASSERT_STREQ(mapping_string(event, "command_text_snapshot_policy"), "owner_private_redacted_from_trace");
          ASSERT_EQ(mapping_number(event, "command_text_snapshot_ready"), 1);
          ASSERT_GT(mapping_number(event, "command_text_snapshot_bytes"), 0);
          ASSERT_EQ(mapping_number(event, "command_text_snapshot_redacted"), 1);
          ASSERT_STREQ(mapping_string(event, "command_text_snapshot_blocker"), "");
          ASSERT_STREQ(mapping_string(event, "command_consume_model"), "owner_owned_snapshot_main_thread_consume");
          ASSERT_EQ(mapping_number(event, "command_consume_snapshot_ready"), 1);
          ASSERT_EQ(mapping_number(event, "command_consume_executor_ready"), 0);
          ASSERT_STREQ(mapping_string(event, "command_consume_blocker"),
                       "interactive_command_requires_main_thread_io_adapter");
          ASSERT_STREQ(mapping_string(event, "execution_frame_model"), "gateway_command_execution_frame_v1");
          ASSERT_STREQ(mapping_string(event, "execution_frame_policy"),
                       "owner_scope_current_interactive_command_giver");
          ASSERT_STREQ(mapping_string(event, "execution_frame_restore_policy"), "main_thread_vmcontext_scope");
          ASSERT_EQ(mapping_number(event, "execution_frame_restore_ready"), 1);
          ASSERT_STREQ(mapping_string(event, "execution_frame_restore_blocker"), "");
          ASSERT_EQ(mapping_number(event, "execution_frame_executor_ready"), 0);
          ASSERT_EQ(mapping_number(event, "payload_frozen"), 1);
        }
      }
      if (std::string(mapping_string(event, "task_type")) == "interactive" &&
          std::string(mapping_string(event, "task_key")) == "process_user_command" &&
          std::string(mapping_string(event, "owner_id")) == owner_id &&
          mapping_number(event, "owner_epoch") == static_cast<long>(current_epoch)) {
        found_interactive_dispatch = true;
      }
    }
  }
  ASSERT_TRUE(found_stale_command_task);
  ASSERT_FALSE(found_interactive_dispatch);
  free_mapping(trace);

  add_ref(ob, "TestGatewayCommandMainQueueStaleTraceIncludesFrameMetadata");
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-command-stale", "test_done", "done"), 1);
  ASSERT_EQ(ob->interactive, nullptr);
  destruct_object(ob);
  free_object(&ob, "TestGatewayCommandMainQueueStaleTraceIncludesFrameMetadata");
}

TEST_F(DriverTest, TestGatewaySessionExecLogonKeepsSessionLookupWorking) {
  auto *ob = create_gateway_session_for_test("gw-test-exec", "/clone/gateway_login_exec_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));
  ASSERT_NE(std::string(ob->obname).find("clone/gateway_exec_user"), std::string::npos);
  ASSERT_STREQ("owner/test/gateway/exec-user", vm_owner_id(ob));
  auto owner_epoch = vm_owner_epoch(ob);

  auto *info = call_lpc_method(ob, "query_gateway_session_snapshot");
  ASSERT_NE(info, nullptr);
  ASSERT_EQ(info->type, T_MAPPING);
  auto mapping_number = [](mapping_t *map, const char *key) -> long {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER);
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  auto mapping_string = [](mapping_t *map, const char *key) -> const char * {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr);
    EXPECT_EQ(value ? value->type : T_INVALID, T_STRING);
    return value && value->type == T_STRING ? value->u.string : "";
  };
  ASSERT_STREQ(mapping_string(info->u.map, "owner_id"), "owner/test/gateway/exec-user");
  ASSERT_EQ(mapping_number(info->u.map, "owner_epoch"), static_cast<long>(owner_epoch));
  ASSERT_STREQ(mapping_string(info->u.map, "object_name"), ob->obname);

  add_ref(ob, "TestGatewaySessionExecLogonKeepsSessionLookupWorking");
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-exec", "test_done", "done"), 1);
  ASSERT_EQ(ob->interactive, nullptr);
  ASSERT_STREQ("owner/test/gateway/exec-user", vm_owner_id(ob));
  ASSERT_EQ(vm_owner_epoch(ob), owner_epoch);
  destruct_object(ob);
  free_object(&ob, "TestGatewaySessionExecLogonKeepsSessionLookupWorking");
}

namespace {
// Fault-injection runtime: claims one owner, then throws from task
// execution to prove the executor releases the claim and classifies
// the exception instead of dying.
class ThrowingOwnerExecutorRuntime : public OwnerExecutorRuntime {
 public:
  std::string claimed;
  int releases = 0;
  int exceptions = 0;
  bool throw_std = true;

  void bind_context() override {}
  std::string claim_next_owner() override {
    if (!claimed.empty()) {
      return "";
    }
    claimed = "owner/test/executor-exception";
    return claimed;
  }
  void run_claimed_owner(const std::string &owner_id) override {
    (void)owner_id;
    if (throw_std) {
      throw std::runtime_error("injected task failure");
    }
    throw 42;  // non-std exception path
  }
  void release_owner_after_task(const std::string &owner_id) noexcept override {
    EXPECT_EQ(owner_id, claimed);
    releases++;
  }
  void record_owner_exception(const std::string &owner_id, const char *what) override {
    EXPECT_EQ(owner_id, claimed);
    EXPECT_NE(what, nullptr);
    exceptions++;
  }
};

TEST(OwnerExecutorTest, ReleasesOwnerClaimWhenTaskExecutionThrowsStdException) {
  ThrowingOwnerExecutorRuntime runtime;
  runtime.throw_std = true;
  OwnerExecutor executor(runtime);
  executor.run();
  EXPECT_EQ(runtime.exceptions, 1);
  EXPECT_EQ(runtime.releases, 1);
}

TEST(OwnerExecutorTest, ReleasesOwnerClaimWhenTaskExecutionThrowsUnknownException) {
  ThrowingOwnerExecutorRuntime runtime;
  runtime.throw_std = false;
  OwnerExecutor executor(runtime);
  executor.run();
  EXPECT_EQ(runtime.exceptions, 1);
  EXPECT_EQ(runtime.releases, 1);
}
}  // namespace

TEST_F(DriverTest, TestVmObjectHandleAcquireKeepsReferenceAcrossDestruct) {
  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);

  vm_owner_set_id(obj, "owner/test/handle/acquire");
  auto handle = vm_object_handle(obj);
  ASSERT_TRUE(handle.valid);

  // Acquire takes an owning reference; after destruct the pointer must still
  // be valid (referenced) so the guard can free it safely.
  object_t* acquired = vm_object_handle_acquire(handle);
  ASSERT_EQ(acquired, obj);

  destruct_object(obj);
  // The object is destructed but the guard's reference keeps memory alive.
  ASSERT_TRUE((acquired->flags & O_DESTRUCTED) != 0);

  VMObjectRefGuard guard(acquired);
  ASSERT_EQ(guard.get(), acquired);
  // Moving transfers ownership without double-free.
  VMObjectRefGuard moved(std::move(guard));
  ASSERT_EQ(guard.get(), nullptr);
  ASSERT_EQ(moved.get(), acquired);
  // detach() transfers the pointer without releasing (caller frees it).
  auto* detached = moved.detach();
  ASSERT_EQ(detached, acquired);
  ASSERT_EQ(moved.get(), nullptr);
  free_object(&detached, "VMObjectRefGuard detach test");
  // release_ref() drops the reference and never returns the old pointer.
  object_t* obj2 = load_object_for_test("single/void");
  ASSERT_NE(obj2, nullptr);
  VMObjectRefGuard guard2(vm_object_handle_acquire(vm_object_handle(obj2)));
  ASSERT_NE(guard2.get(), nullptr);
  guard2.release_ref();
  ASSERT_EQ(guard2.get(), nullptr);
  destruct_object(obj2);
}

TEST_F(DriverTest, TestVmObjectHandleAcquireRejectsStaleHandle) {
  object_t* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);

  vm_owner_set_id(obj, "owner/test/handle/acquire-stale");
  auto handle = vm_object_handle(obj);

  vm_owner_clear_id(obj);
  vm_owner_set_id(obj, "owner/test/handle/acquire-stale");
  // Epoch changed: acquire must return nullptr and must not add a reference.
  ASSERT_EQ(vm_object_handle_acquire(handle), nullptr);

  vm_owner_clear_id(obj);
  destruct_object(obj);
}

// T15: table-driven extreme-value coverage for gateway session ids.
TEST_F(DriverTest, TestGatewaySessionIdBoundaryTableDriven) {
  struct Case {
    const char *input;
    size_t len;
    bool expected;
  };
  const Case cases[] = {
      {nullptr, 0, false},
      {"", 0, false},
      {"a", 1, true},
      {"0123456789abcdef0123456789abcdef", 32, true},
      {"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", 64, true},
      // 128 bytes is the documented maximum.
      {"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
       "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", 128, true},
      // 129 bytes: exceeds the max session id length.
      {"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
       "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdefx", 129, false},
      // Control characters and space are rejected.
      {"a b", 3, false},
      {"a\x01b", 3, false},
      // Adjacent literals: \x7f must not swallow the following 'b' byte.
      {"a\x7f" "b", 3, false},
      // DEL and high bytes are rejected (0x21..0x7e only).
      {"a\x80" "b", 3, false},
  };
  for (const auto &c : cases) {
    EXPECT_EQ(gateway_session_id_is_valid(c.input, c.len), c.expected)
        << "input=" << (c.input ? c.input : "(null)") << " len=" << c.len;
  }
}
