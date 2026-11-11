#include "test_lpc_support.h"

TEST_F(DriverTest, TestGatewayMainQueueReadAdmissionUsesComposedHighLowWaterBackpressure) {
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway.cc");
  const auto header = read_source_file_for_test("../src/packages/gateway/gateway.h");
  const auto spec = read_source_file_for_test("../src/packages/gateway/gateway.spec");

  ASSERT_NE(source.find("constexpr long kGatewayMainQueueReadHighWatermark = 32"),
            std::string::npos);
  ASSERT_NE(source.find("constexpr long kGatewayMainQueueReadLowWatermark = 16"),
            std::string::npos);
  const auto admission_assert_pos = source.find("static_assert(kGatewayReadFrameBudget <=");
  ASSERT_NE(admission_assert_pos, std::string::npos);
  ASSERT_NE(source.find("kGatewayMainQueueReadHighWatermark -", admission_assert_pos),
            std::string::npos);
  ASSERT_NE(source.find("kGatewayMainQueueReadLowWatermark);", admission_assert_pos),
            std::string::npos);
  ASSERT_NE(header.find("GATEWAY_READ_PAUSE_BUFFERED_BACKLOG"), std::string::npos);
  ASSERT_NE(header.find("GATEWAY_READ_PAUSE_MAIN_QUEUE"), std::string::npos);
  ASSERT_NE(header.find("uint8_t read_dispatch_pause_reasons{0}"), std::string::npos);

  const auto dispatch_pos = source.find("int gateway_dispatch_buffered_frames");
  const auto dispatch_end = source.find("\nvoid gateway_record_read_dispatch", dispatch_pos);
  ASSERT_NE(dispatch_pos, std::string::npos);
  ASSERT_NE(dispatch_end, std::string::npos);
  const auto dispatch_source = source.substr(dispatch_pos, dispatch_end - dispatch_pos);
  ASSERT_NE(dispatch_source.find("gateway_main_queue_admission_budget(budget)"), std::string::npos);
  ASSERT_NE(dispatch_source.find("gateway_pause_all_reads_for_main_queue_pressure()"),
            std::string::npos);

  const auto deferred_pos = source.find("void gateway_drain_owner_main_tasks_later");
  const auto deferred_end =
      source.find("\nvoid gateway_service_admitted_receive_tasks", deferred_pos);
  ASSERT_NE(deferred_pos, std::string::npos);
  ASSERT_NE(deferred_end, std::string::npos);
  const auto deferred_source = source.substr(deferred_pos, deferred_end - deferred_pos);
  ASSERT_NE(deferred_source.find("gateway_resume_main_queue_reads_if_below_low_watermark();"),
            std::string::npos);

  const auto service_pos = source.find("void gateway_service_admitted_receive_tasks");
  const auto service_end = source.find("\nbool gateway_apply_receive", service_pos);
  ASSERT_NE(service_pos, std::string::npos);
  ASSERT_NE(service_end, std::string::npos);
  const auto service_source = source.substr(service_pos, service_end - service_pos);
  ASSERT_NE(service_source.find("gateway_resume_main_queue_reads_if_below_low_watermark();"),
            std::string::npos);

  ASSERT_NE(source.find("int64_t gateway_main_queue_pending_count()"), std::string::npos);
  ASSERT_EQ(source.find("long gateway_main_queue_pending_count()"), std::string::npos);
  ASSERT_NE(header.find("int64_t gateway_main_queue_pending_count();"), std::string::npos);
  ASSERT_EQ(header.find("long gateway_main_queue_pending_count();"), std::string::npos);
  ASSERT_NE(
      source.find(
          "std::chrono::nanoseconds gateway_deferred_main_drain_wall_budget(int64_t backlog)"),
      std::string::npos);
  ASSERT_EQ(source.find("gateway_deferred_main_drain_wall_budget(long backlog)"),
            std::string::npos);
  ASSERT_NE(source.find("std::max<int64_t>(0, vm_owner_main_queue_total_depth())"),
            std::string::npos);
  ASSERT_NE(source.find("std::max<int64_t>(0, drain_result.remaining_main_tasks)"),
            std::string::npos);
  ASSERT_EQ(source.find("std::max<long>(0, vm_owner_main_queue_total_depth())"), std::string::npos);
  ASSERT_EQ(source.find("std::max<long>(0, drain_result.remaining_main_tasks)"), std::string::npos);
  ASSERT_NE(spec.find("int gateway_main_queue_pending();"), std::string::npos);
  ASSERT_NE(source.find("LPC_INT gateway_buffered_input_pending_count()"), std::string::npos);
  ASSERT_NE(spec.find("int gateway_buffered_input_pending();"), std::string::npos);
  ASSERT_NE(source.find("bufferevent_get_input(master->bev)"), std::string::npos);
  ASSERT_NE(source.find("evbuffer_copyout("), std::string::npos);
  ASSERT_NE(source.find("static_cast<ev_ssize_t>"), std::string::npos);
  const auto efun_pos = source.find("void f_gateway_main_queue_pending()");
  const auto efun_end = source.find("\nvoid f_gateway_status()", efun_pos);
  ASSERT_NE(efun_pos, std::string::npos);
  ASSERT_NE(efun_end, std::string::npos);
  ASSERT_NE(source.substr(efun_pos, efun_end - efun_pos).find("gateway_main_queue_pending_count()"),
            std::string::npos);

  const auto buffered_efun_pos = source.find("void f_gateway_buffered_input_pending()");
  ASSERT_NE(buffered_efun_pos, std::string::npos);
  ASSERT_NE(source.substr(buffered_efun_pos).find("gateway_buffered_input_pending_count()"),
            std::string::npos);

  const auto pressure_pos = source.find("LPC_INT gateway_command_pressure_count()");
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
  ASSERT_NE(
      source.find("constexpr auto kGatewayReadContinuationDelay = std::chrono::milliseconds(1)"),
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
  ASSERT_NE(schedule_source.find("kGatewayReadContinuationDelay"), std::string::npos);
  ASSERT_EQ(schedule_source.find("std::chrono::milliseconds(0)"), std::string::npos);
  ASSERT_NE(schedule_source.find("BackendEventPriority::kGateway"), std::string::npos);
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
  const auto append_frame = [&master](const std::string& payload) {
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
  const auto frame_for = [](const std::string& payload) {
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
  master.read_buffer =
      frame_for(R"({"type":"hello"})") + frame_for("{") + complete_tail.substr(0, 7);

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
    ~PacketSizeGuard() {
      g_gateway_max_packet_size = original;
    }
  } packet_size_guard;

  g_gateway_max_packet_size = 1024;
  GatewayMaster master;
  master.fd = -1;
  const auto rejected_before = g_gateway_runtime_counters.read_dispatch_frame_length_rejected.load(
      std::memory_order_relaxed);
  const uint32_t network_length = htonl(1025);
  master.read_buffer.assign(reinterpret_cast<const char*>(&network_length), sizeof(network_length));

  ASSERT_EQ(gateway_dispatch_buffered_frames_for_test(&master, 2), -1);
  ASSERT_EQ(g_gateway_runtime_counters.read_dispatch_frame_length_rejected.load(
                std::memory_order_relaxed),
            rejected_before + 1);
}

TEST_F(DriverTest, TestGatewayReadBufferRejectsAggregateOverflowBeforeCopy) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() {
      g_gateway_max_packet_size = original;
    }
  } packet_size_guard;

  g_gateway_max_packet_size = 1024;
  const auto limit = gateway_read_buffer_limit_for_test();
  ASSERT_EQ(limit, 1024u + sizeof(uint32_t));
  GatewayMaster master;
  const std::string bounded(limit, 'x');
  const auto rejected_before = g_gateway_runtime_counters.read_dispatch_buffer_limit_rejected.load(
      std::memory_order_relaxed);

  ASSERT_TRUE(gateway_append_read_bytes_for_test(&master, bounded.data(), bounded.size()));
  ASSERT_EQ(master.read_buffer.size(), limit);
  ASSERT_FALSE(gateway_append_read_bytes_for_test(&master, "x", 1));
  ASSERT_EQ(master.read_buffer.size(), limit);
  ASSERT_EQ(g_gateway_runtime_counters.read_dispatch_buffer_limit_rejected.load(
                std::memory_order_relaxed),
            rejected_before + 1);
}

TEST_F(DriverTest, TestGatewayMasterReadWatermarkMatchesBoundedAggregateBuffer) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() {
      g_gateway_max_packet_size = original;
    }
  } packet_size_guard;

  g_gateway_max_packet_size = 2048;
  constexpr int master_fd = 1707;
  bufferevent* pair[2] = {nullptr, nullptr};
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  ASSERT_NE(gateway_register_master_for_test(master_fd, pair[0]), nullptr);
  size_t lowmark = 0;
  size_t highmark = 0;

  ASSERT_EQ(bufferevent_getwatermark(pair[0], EV_READ, &lowmark, &highmark), 0);
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
  ASSERT_NE(listener_source.find("BackendEventPriority::kGateway"), std::string::npos);
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
      g_gateway_runtime_counters.external_bind_rejected.load(std::memory_order_relaxed);
  ASSERT_EQ(gateway_listen_internal(65535, 1), 0);
  ASSERT_EQ(g_gateway_runtime_counters.external_bind_rejected.load(std::memory_order_relaxed),
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
    ~TickQueueGuard() {
      clear_tick_events();
    }
  } tick_queue_guard;

  ASSERT_GT(CONFIG_INT(__TIME_TO_CLEAN_UP__), 0) << "sweep requires a clean_up interval";

  current_object = master_ob;
  auto* ob = load_object_for_test("clone/clean_up_deadline");
  ASSERT_NE(ob, nullptr);
  ASSERT_TRUE(ob->flags & O_WILL_CLEAN_UP)
      << "an object that defines clean_up() is queried by the sweep";
  // Control: an object whose deadline has not passed yet must survive the
  // same sweep (a future deadline is not "due" and the idle-time rule does
  // not apply to a freshly loaded object).
  auto* future = load_object_for_test("clone/clean_up_deadline");
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
  object_t* dest = nullptr;
  object_t* item = nullptr;

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
  ASSERT_EQ(getsockname(listener->fd, reinterpret_cast<sockaddr*>(&address), &address_length), 0);

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
      if (tick_event_queue_size_for_test() == 0 && walltime_event_queue_size_for_test() == 0 &&
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
      -1,
      R"({"type":"login","cid":"gw-test-login-main-queue","data":{"ip":"127.0.0.1","port":6040}})"));

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
      4040,
      R"({"type":"login","cid":"gw-test-login-stale-master","data":{"ip":"127.0.0.1","port":6040}})"));

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
  auto* object = load_object_for_test("single/void");
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
  auto* object = load_object_for_test("std/json");
  ASSERT_NE(object, nullptr);

  std::vector<char> destination(256, 'S');
  ASSERT_EQ(
      save_object_str(object, 0, destination.data(), static_cast<int>(destination.size() - 1)), 1);
  const std::string serialized(destination.data());
  ASSERT_EQ(serialized.compare(0, std::strlen("#/std/json.c\n"), "#/std/json.c\n"), 0);

  const auto header_size = serialized.find('\n') + 1;
  ASSERT_GT(header_size, 0U);
  std::vector<char> exact_header_destination(header_size + 1, 'S');
  ASSERT_EQ(
      save_object_str(object, 0, exact_header_destination.data(), static_cast<int>(header_size)),
      1);
  ASSERT_EQ(std::string(exact_header_destination.data(), header_size),
            serialized.substr(0, header_size));
  ASSERT_EQ(exact_header_destination[header_size], '\0');
  std::vector<char> short_destination(header_size + 1, 'S');
  ASSERT_EQ(save_object_str(object, 0, short_destination.data(), static_cast<int>(header_size - 1)),
            0);
  ASSERT_EQ(short_destination[0], 'S');

  destruct_object_for_test(object);
}

TEST_F(DriverTest, TestSaveObjectPathAvoidsFixedBuffers) {
  const auto source = read_source_file_for_test("../src/vm/internal/base/object.cc");
  const auto save_object_pos = source.find("int save_object(");
  const auto save_object_str_pos = source.find("int save_object_str(");

  ASSERT_NE(save_object_pos, std::string::npos);
  ASSERT_NE(save_object_str_pos, std::string::npos);
  const auto save_object_source =
      source.substr(save_object_pos, save_object_str_pos - save_object_pos);

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

  ASSERT_NE(port_header.find("int64_t get_current_performance_wall_time_ns();"), std::string::npos);
  ASSERT_NE(helper_pos, std::string::npos);
  ASSERT_NE(port_source.find("CLOCK_MONOTONIC_RAW", helper_pos), std::string::npos);
  ASSERT_NE(port_source.find("clock_gettime(CLOCK_MONOTONIC_RAW", helper_pos), std::string::npos);
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
  ASSERT_GE(wall_after - wall_before + kCpuClockQuantizationAllowanceNs, cpu_after - cpu_before);
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
  ASSERT_NE(metrics_source.find("owner_async_lpc_execute_thread_cpu_ns_total"), std::string::npos);
  ASSERT_NE(metrics_source.find("owner_async_lpc_execute_thread_cpu_unavailable"),
            std::string::npos);
  ASSERT_NE(metrics_source.find("owner_async_result_completion_thread_cpu_ns_total"),
            std::string::npos);
  ASSERT_NE(metrics_source.find("owner_async_result_completion_thread_cpu_unavailable"),
            std::string::npos);
  ASSERT_NE(owner_source.find("owner_async_lpc_execute_thread_cpu_total_us"), std::string::npos);
  ASSERT_NE(owner_source.find("owner_async_result_completion_thread_cpu_total_us"),
            std::string::npos);
}

TEST_F(DriverTest, TestOwnerRuntimeMetricMappingsUseLpcIntegerWidth) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto status_start = owner_source.find("void add_owner_runtime_v2_status_fields(");
  const auto status_end = owner_source.find(
      "std::shared_ptr<VMFrozenValue> frozen_compute_result_mapping", status_start);

  ASSERT_NE(status_start, std::string::npos);
  ASSERT_NE(status_end, std::string::npos);
  ASSERT_GT(status_end, status_start);
  const auto status_body = owner_source.substr(status_start, status_end - status_start);
  ASSERT_EQ(status_body.find("static_cast<long>"), std::string::npos);
}

TEST_F(DriverTest, TestOwnerStatusMappingsUseLpcIntegerWidth) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  auto assert_status_uses_lpc_width = [&](const char* start_marker, const char* end_marker) {
    const auto start = owner_source.find(start_marker);
    const auto end = owner_source.find(end_marker, start);

    ASSERT_NE(start, std::string::npos) << start_marker;
    ASSERT_NE(end, std::string::npos) << end_marker;
    ASSERT_GT(end, start) << start_marker;
    const auto body = owner_source.substr(start, end - start);
    EXPECT_EQ(body.find("static_cast<long>"), std::string::npos) << start_marker;
    EXPECT_NE(body.find("static_cast<LPC_INT>"), std::string::npos) << start_marker;
  };

  assert_status_uses_lpc_width("mapping_t *vm_owner_thread_status()",
                               "mapping_t *vm_owner_runtime_status()");
  assert_status_uses_lpc_width("mapping_t *vm_owner_runtime_status()",
                               "#ifdef DEBUGMALLOC_EXTENSIONS");
}

TEST_F(DriverTest, TestOwnerStatusSerializationUsesSharedFieldBuilder) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto helper_start = owner_source.find("void add_owner_runtime_status_fields(");
  const auto thread_start = owner_source.find("mapping_t *vm_owner_thread_status()");
  const auto runtime_start = owner_source.find("mapping_t *vm_owner_runtime_status()");
  const auto debugmalloc_start = owner_source.find("#ifdef DEBUGMALLOC_EXTENSIONS", runtime_start);

  ASSERT_NE(helper_start, std::string::npos);
  ASSERT_NE(thread_start, std::string::npos);
  ASSERT_NE(runtime_start, std::string::npos);
  ASSERT_NE(debugmalloc_start, std::string::npos);
  ASSERT_LT(helper_start, thread_start);
  ASSERT_LT(thread_start, runtime_start);
  ASSERT_LT(runtime_start, debugmalloc_start);

  const auto helper_body = owner_source.substr(helper_start, thread_start - helper_start);
  const auto thread_body = owner_source.substr(thread_start, runtime_start - thread_start);
  const auto runtime_body = owner_source.substr(runtime_start, debugmalloc_start - runtime_start);

  EXPECT_NE(helper_body.find("add_mapping_pair(map, \"queue_depth\""), std::string::npos);
  EXPECT_EQ(thread_body.find("add_mapping_pair(map, \"queue_depth\""), std::string::npos);
  EXPECT_EQ(runtime_body.find("add_mapping_pair(map, \"queue_depth\""), std::string::npos);
  EXPECT_NE(thread_body.find("add_owner_runtime_status_fields(map, status);"), std::string::npos);
  EXPECT_NE(runtime_body.find("add_owner_runtime_status_fields(map, status);"), std::string::npos);
}

TEST_F(DriverTest, TestOwnerTaskConstructionUsesSharedIdentityInitializer) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const std::string initializer = "initialize_owner_task_identity(";
  size_t initializer_count = 0;
  for (size_t offset = owner_source.find(initializer); offset != std::string::npos;
       offset = owner_source.find(initializer, offset + initializer.size())) {
    initializer_count++;
  }
  EXPECT_EQ(initializer_count, 13u);

  const std::string direct_task_id_init = "task.task_id = next_mailbox_task_id.fetch_add";
  const auto direct_init = owner_source.find(direct_task_id_init);
  ASSERT_NE(direct_init, std::string::npos);
  EXPECT_EQ(owner_source.find(direct_task_id_init, direct_init + direct_task_id_init.size()),
            std::string::npos);
}

TEST_F(DriverTest, TestGatewayStatusMappingsUseLpcIntegerWidth) {
  const auto gateway_source = read_source_file_for_test("../src/packages/gateway/gateway.cc");
  const auto helper_name = gateway_source.find("gateway_avg_us(");
  const auto helper_start = gateway_source.rfind("LPC_INT", helper_name);
  const auto helper_end = gateway_source.find("void gateway_record_main_drain(", helper_name);
  const auto status_start = gateway_source.find("mapping_t *gateway_status_internal()");
  const auto status_end = gateway_source.find("void f_is_gateway_user()", status_start);

  ASSERT_NE(helper_name, std::string::npos);
  ASSERT_NE(helper_start, std::string::npos);
  ASSERT_NE(helper_end, std::string::npos);
  ASSERT_GT(helper_end, helper_start);
  ASSERT_NE(status_start, std::string::npos);
  ASSERT_NE(status_end, std::string::npos);
  ASSERT_GT(status_end, status_start);
  const auto helper_body = gateway_source.substr(helper_start, helper_end - helper_start);
  const auto status_body = gateway_source.substr(status_start, status_end - status_start);
  EXPECT_EQ(helper_body.find("static_cast<long>"), std::string::npos);
  EXPECT_EQ(helper_body.find("long gateway_"), std::string::npos);
  EXPECT_NE(helper_body.find("LPC_INT gateway_avg_us"), std::string::npos);
  EXPECT_EQ(status_body.find("static_cast<long>"), std::string::npos);
  EXPECT_NE(status_body.find("static_cast<LPC_INT>"), std::string::npos);
}

TEST_F(DriverTest, TestVmWorkerMappingCountersUseLpcIntegerWidth) {
  const auto worker_source = read_source_file_for_test("../src/packages/core/vm_worker.cc");
  auto assert_mapping_uses_lpc_width = [&](const char* start_marker, const char* end_marker) {
    const auto start = worker_source.find(start_marker);
    const auto end = worker_source.find(end_marker, start);

    ASSERT_NE(start, std::string::npos) << start_marker;
    ASSERT_NE(end, std::string::npos) << end_marker;
    ASSERT_GT(end, start) << start_marker;
    const auto body = worker_source.substr(start, end - start);
    EXPECT_EQ(body.find("static_cast<long>"), std::string::npos) << start_marker;
    EXPECT_NE(body.find("static_cast<LPC_INT>"), std::string::npos) << start_marker;
  };

  assert_mapping_uses_lpc_width("mapping_t *worker_bench_response(mapping_t *options)",
                                "mapping_t *worker_actor_bench_response(mapping_t *options)");
  assert_mapping_uses_lpc_width("mapping_t *worker_status_response()",
                                "mapping_t *worker_submit_response(");
}

TEST_F(DriverTest, TestGatewaySessionCountMappingsUseLpcIntegerWidth) {
  const auto gateway_header = read_source_file_for_test("../src/packages/gateway/gateway.h");
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
    const auto long_signature = std::string("long ") + function_name + "(";
    const auto lpc_signature = std::string("LPC_INT ") + function_name + "(";
    EXPECT_EQ(gateway_header.find(long_signature), std::string::npos) << function_name;
    EXPECT_NE(gateway_header.find(lpc_signature), std::string::npos) << function_name;
    EXPECT_EQ(session_source.find(long_signature), std::string::npos) << function_name;
    EXPECT_NE(session_source.find(lpc_signature), std::string::npos) << function_name;
  }

  const auto projection_start =
      session_source.find("gateway_room_output_projection_pending_count()");
  const auto projection_end =
      session_source.find("GatewaySession *gateway_find_session(", projection_start);
  const auto watch_start = session_source.find("gateway_session_future_watch_count()");
  const auto watch_end =
      session_source.find("uint64_t gateway_session_fifo_enqueued_total()", watch_start);
  const auto quiesce_start =
      session_source.find("mapping_t *gateway_owner_output_quiesce(const char *reason)");
  const auto quiesce_end =
      session_source.find("int gateway_fill_session_protocol_output_with_writer(", quiesce_start);

  ASSERT_NE(projection_start, std::string::npos);
  ASSERT_NE(projection_end, std::string::npos);
  ASSERT_GT(projection_end, projection_start);
  ASSERT_NE(watch_start, std::string::npos);
  ASSERT_NE(watch_end, std::string::npos);
  ASSERT_GT(watch_end, watch_start);
  ASSERT_NE(quiesce_start, std::string::npos);
  ASSERT_NE(quiesce_end, std::string::npos);
  ASSERT_GT(quiesce_end, quiesce_start);

  const auto projection_body =
      session_source.substr(projection_start, projection_end - projection_start);
  const auto watch_body = session_source.substr(watch_start, watch_end - watch_start);
  const auto quiesce_body = session_source.substr(quiesce_start, quiesce_end - quiesce_start);
  EXPECT_EQ(projection_body.find("static_cast<long>"), std::string::npos);
  EXPECT_EQ(projection_body.find("numeric_limits<long>"), std::string::npos);
  EXPECT_EQ(watch_body.find("static_cast<long>"), std::string::npos);
  EXPECT_EQ(quiesce_body.find("static_cast<long>"), std::string::npos);
  EXPECT_EQ(quiesce_body.find("numeric_limits<long>"), std::string::npos);
  EXPECT_EQ(quiesce_body.find("std::max<long>"), std::string::npos);
}

TEST_F(DriverTest, TestGatewaySessionInfoMappingsUseLpcIntegerWidth) {
  const auto session_source =
      read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto payload_start = session_source.find("svalue_t gateway_command_task_payload(");
  const auto payload_end =
      session_source.find("void cleanup_temp_gateway_interactive(", payload_start);
  const auto info_start = session_source.find("void f_gateway_session_info()");
  const auto info_end = session_source.find("void f_gateway_inject_input()", info_start);

  ASSERT_NE(payload_start, std::string::npos);
  ASSERT_NE(payload_end, std::string::npos);
  ASSERT_GT(payload_end, payload_start);
  ASSERT_NE(info_start, std::string::npos);
  ASSERT_NE(info_end, std::string::npos);
  ASSERT_GT(info_end, info_start);
  const auto payload_body = session_source.substr(payload_start, payload_end - payload_start);
  const auto info_body = session_source.substr(info_start, info_end - info_start);
  EXPECT_EQ(payload_body.find("static_cast<long>"), std::string::npos);
  EXPECT_NE(payload_body.find("static_cast<LPC_INT>"), std::string::npos);
  EXPECT_EQ(info_body.find("static_cast<long>"), std::string::npos);
  EXPECT_NE(info_body.find("static_cast<LPC_INT>"), std::string::npos);
  EXPECT_EQ(info_body.find("long pending_reservations"), std::string::npos);
  EXPECT_NE(info_body.find("LPC_INT pending_reservations"), std::string::npos);
}

TEST_F(DriverTest, TestObjectStoreShardMappingsUseLpcIntegerWidth) {
  const auto object_store_source = read_source_file_for_test("../src/vm/internal/object_store.cc");
  auto assert_mapping_uses_lpc_width = [&](const char* start_marker, const char* end_marker) {
    const auto start = object_store_source.find(start_marker);
    const auto end = object_store_source.find(end_marker, start);

    ASSERT_NE(start, std::string::npos) << start_marker;
    ASSERT_NE(end, std::string::npos) << end_marker;
    ASSERT_GT(end, start) << start_marker;
    const auto body = object_store_source.substr(start, end - start);
    EXPECT_EQ(body.find("static_cast<long>"), std::string::npos) << start_marker;
    EXPECT_NE(body.find("static_cast<LPC_INT>"), std::string::npos) << start_marker;
  };

  assert_mapping_uses_lpc_width("execution_runnable_tasks(", "mapping_t *shard_mapping(");
  assert_mapping_uses_lpc_width("mapping_t *shard_mapping(", "}  // namespace");
  EXPECT_EQ(object_store_source.find("long execution_runnable_tasks("), std::string::npos);
  EXPECT_NE(object_store_source.find("LPC_INT execution_runnable_tasks("), std::string::npos);
}

TEST_F(DriverTest, TestObjectStoreGlobalStatusMappingUsesLpcIntegerWidth) {
  const auto object_store_source = read_source_file_for_test("../src/vm/internal/object_store.cc");
  const auto status_start = object_store_source.find("mapping_t *vm_object_store_status()");
  const auto status_end =
      object_store_source.find("mapping_t *vm_object_store_owner_status(", status_start);

  ASSERT_NE(status_start, std::string::npos);
  ASSERT_NE(status_end, std::string::npos);
  ASSERT_GT(status_end, status_start);
  const auto status_body = object_store_source.substr(status_start, status_end - status_start);
  EXPECT_EQ(status_body.find("static_cast<long>"), std::string::npos);
  EXPECT_NE(status_body.find("static_cast<LPC_INT>"), std::string::npos);
}

TEST_F(DriverTest, TestOwnerSchedulerCountsUseFixedIntegerWidth) {
  const auto scheduler_header =
      read_source_file_for_test("../src/vm/internal/owner_scheduler_state.h");
  const auto scheduler_source =
      read_source_file_for_test("../src/vm/internal/owner_scheduler_state.cc");
  const auto trace_header = read_source_file_for_test("../src/vm/internal/owner_trace_store.h");
  const auto owner_header = read_source_file_for_test("../src/vm/owner.h");
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");

  EXPECT_EQ(scheduler_header.find("long "), std::string::npos);
  EXPECT_NE(scheduler_header.find("int64_t mailbox_depth("), std::string::npos);
  EXPECT_EQ(scheduler_source.find("static_cast<long>"), std::string::npos);
  EXPECT_EQ(scheduler_source.find("long OwnerSchedulerState::"), std::string::npos);
  EXPECT_NE(scheduler_source.find("int64_t OwnerSchedulerState::"), std::string::npos);

  const auto trace_start = trace_header.find("struct OwnerExecutorTrace {");
  const auto trace_end = trace_header.find("template <typename Trace>", trace_start);
  ASSERT_NE(trace_start, std::string::npos);
  ASSERT_NE(trace_end, std::string::npos);
  ASSERT_GT(trace_end, trace_start);
  const auto trace_body = trace_header.substr(trace_start, trace_end - trace_start);
  EXPECT_EQ(trace_body.find("long "), std::string::npos);
  EXPECT_NE(trace_body.find("int64_t backlog"), std::string::npos);

  const auto drain_result_start = owner_header.find("struct VMOwnerMainDrainResult {");
  const auto drain_result_end =
      owner_header.find("using VMOwnerFutureTerminalNotifier", drain_result_start);
  ASSERT_NE(drain_result_start, std::string::npos);
  ASSERT_NE(drain_result_end, std::string::npos);
  ASSERT_GT(drain_result_end, drain_result_start);
  const auto drain_result_body =
      owner_header.substr(drain_result_start, drain_result_end - drain_result_start);
  EXPECT_EQ(drain_result_body.find("long remaining_"), std::string::npos);
  EXPECT_NE(drain_result_body.find("int64_t remaining_main_tasks"), std::string::npos);
  EXPECT_EQ(owner_header.find("long vm_owner_main_queue_total_depth()"), std::string::npos);
  EXPECT_NE(owner_header.find("int64_t vm_owner_main_queue_total_depth()"), std::string::npos);

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
    EXPECT_EQ(owner_source.find(std::string("long ") + function_name + "("), std::string::npos)
        << function_name;
    EXPECT_NE(owner_source.find(std::string("int64_t ") + function_name + "("), std::string::npos)
        << function_name;
  }
  EXPECT_EQ(owner_source.find("long vm_owner_main_queue_total_depth()"), std::string::npos);
  EXPECT_NE(owner_source.find("int64_t vm_owner_main_queue_total_depth()"), std::string::npos);

  const auto drain_start =
      owner_source.find("VMOwnerMainDrainResult vm_owner_drain_main_tasks_with_budget(");
  const auto drain_end = owner_source.find("int vm_owner_drain_main_tasks(int limit)", drain_start);
  ASSERT_NE(drain_start, std::string::npos);
  ASSERT_NE(drain_end, std::string::npos);
  ASSERT_GT(drain_end, drain_start);
  const auto drain_body = owner_source.substr(drain_start, drain_end - drain_start);
  EXPECT_EQ(drain_body.find("static_cast<long>"), std::string::npos);
  EXPECT_NE(drain_body.find("static_cast<int64_t>"), std::string::npos);
}

TEST_F(DriverTest, TestOwnerDiagnosticMappingsUseLpcIntegerWidth) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  auto assert_mapping_uses_lpc_width = [&](const char* start_marker, const char* end_marker) {
    const auto start = owner_source.find(start_marker);
    const auto end = owner_source.find(end_marker, start);

    ASSERT_NE(start, std::string::npos) << start_marker;
    ASSERT_NE(end, std::string::npos) << end_marker;
    ASSERT_GT(end, start) << start_marker;
    const auto body = owner_source.substr(start, end - start);
    EXPECT_EQ(body.find("static_cast<long>"), std::string::npos) << start_marker;
    EXPECT_NE(body.find("static_cast<LPC_INT>"), std::string::npos) << start_marker;
  };

  assert_mapping_uses_lpc_width("mapping_t *vm_owner_status(object_t *object)",
                                "mapping_t *vm_owner_guard(");
  assert_mapping_uses_lpc_width("mapping_t *vm_owner_drain_mailbox(",
                                "mapping_t *vm_owner_purge_mailbox(");
  assert_mapping_uses_lpc_width("mapping_t *vm_owner_purge_mailbox(",
                                "mapping_t *vm_owner_schedule(");
  assert_mapping_uses_lpc_width("mapping_t *vm_owner_schedule(",
                                "mapping_t *vm_owner_mailbox_status(");
  assert_mapping_uses_lpc_width("mapping_t *vm_owner_mailbox_status(",
                                "mapping_t *vm_owner_task_trace(");
  assert_mapping_uses_lpc_width("mapping_t *vm_owner_task_trace(",
                                "mapping_t *vm_owner_executor_trace(");
  assert_mapping_uses_lpc_width("mapping_t *vm_owner_executor_trace(",
                                "mapping_t *vm_owner_access_trace(");
  assert_mapping_uses_lpc_width("mapping_t *vm_owner_access_trace(",
                                "mapping_t *submit_owner_message(");
  assert_mapping_uses_lpc_width("mapping_t *vm_owner_message_trace(",
                                "mapping_t *vm_owner_future_poll(");
  assert_mapping_uses_lpc_width("mapping_t *vm_owner_commit_trace(", "void vm_owner_thread_start(");

  const auto context_start = owner_source.find("mapping_t *vm_context_contract_mapping()");
  const auto context_end =
      owner_source.find("mapping_t *owner_executor_boundary_contract_mapping()", context_start);
  ASSERT_NE(context_start, std::string::npos);
  ASSERT_NE(context_end, std::string::npos);
  ASSERT_GT(context_end, context_start);
  const auto context_body = owner_source.substr(context_start, context_end - context_start);
  EXPECT_EQ(context_body.find("static_cast<long>(vm_context_object_store_sync_rejections())"),
            std::string::npos);
  EXPECT_NE(context_body.find("static_cast<LPC_INT>(vm_context_object_store_sync_rejections())"),
            std::string::npos);
}

TEST_F(DriverTest, TestOwnerTraceAppendMetadataUsesStoreLockBoundary) {
  const auto trace_source = read_source_file_for_test("../src/vm/internal/owner_trace_store.cc");
  auto assert_ordered_markers = [&](const char* start_marker, const char* end_marker,
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
      "uint64_t OwnerTraceStore::append_task(", "uint64_t OwnerTraceStore::append_executor(",
      {"std::lock_guard<std::mutex> lock(mutex_);", "next_task_trace_id_.fetch_add(",
       "if (trace.sequence == 0)", "task_traces_.push_back(", "total_task_traced_.fetch_add("});
  assert_ordered_markers("uint64_t OwnerTraceStore::append_executor(",
                         "uint64_t OwnerTraceStore::append_access(",
                         {"std::lock_guard<std::mutex> lock(mutex_);",
                          "next_executor_trace_id_.fetch_add(", "trace.sequence = trace.trace_id;",
                          "executor_traces_.push_back(", "total_executor_traced_.fetch_add("});
  assert_ordered_markers("uint64_t OwnerTraceStore::append_access(",
                         "void OwnerTraceStore::append_message(",
                         {"std::lock_guard<std::mutex> lock(mutex_);",
                          "next_access_trace_id_.fetch_add(", "total_access_traced_.load(",
                          "access_traces_.push_back(", "total_access_traced_.fetch_add("});
  assert_ordered_markers(
      "void OwnerTraceStore::append_message(", "OwnerCommitTrace OwnerTraceStore::append_commit(",
      {"std::lock_guard<std::mutex> lock(mutex_);", "total_message_traced_.load(",
       "message_traces_.push_back(", "total_message_traced_.fetch_add("});
  assert_ordered_markers("OwnerCommitTrace OwnerTraceStore::append_commit(",
                         "uint64_t OwnerTraceStore::append_commit_observed(",
                         {"std::lock_guard<std::mutex> lock(mutex_);",
                          "next_commit_trace_id_.fetch_add(", "total_commit_traced_.load(",
                          "commit_traces_.push_back(", "total_commit_traced_.fetch_add("});
  assert_ordered_markers("uint64_t OwnerTraceStore::append_commit_observed(",
                         "uint64_t OwnerTraceStore::next_message_id(",
                         {"std::lock_guard<std::mutex> lock(mutex_);",
                          "next_commit_trace_id_.fetch_add(", "total_commit_traced_.load(",
                          "commit_traces_.push_back(", "total_commit_traced_.fetch_add("});

  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto record_start = owner_source.find("uint64_t vm_owner_record_task_trace(");
  const auto record_end = owner_source.find("void record_owner_main_queue_fallback(", record_start);
  ASSERT_NE(record_start, std::string::npos);
  ASSERT_NE(record_end, std::string::npos);
  ASSERT_GT(record_end, record_start);
  const auto record_body = owner_source.substr(record_start, record_end - record_start);
  EXPECT_EQ(record_body.find("total_task_traced()"), std::string::npos);
  EXPECT_NE(record_body.find("append_owner_task_trace(0, 0,"), std::string::npos);
}

TEST_F(DriverTest, TestOwnerMessageSubmissionLimitsCoordinatorLockToRegistrationAndEnqueue) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto start = owner_source.find("mapping_t *submit_owner_message(");
  const auto end = owner_source.find("mapping_t *vm_owner_submit_message(", start);

  ASSERT_NE(start, std::string::npos);
  ASSERT_NE(end, std::string::npos);
  ASSERT_GT(end, start);
  const auto body = owner_source.substr(start, end - start);
  const auto lock_pos = body.find("std::lock_guard<std::mutex> lock(owner_runtime_mutex);");
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
  const auto locked_body = body.substr(lock_scope_start, lock_scope_end - lock_scope_start);
  const auto after_lock = body.substr(lock_scope_end);

  EXPECT_NE(before_lock.find("vm_object_store_record_message("), std::string::npos);
  EXPECT_NE(before_lock.find("vm_object_handle_resolve_status("), std::string::npos);
  EXPECT_NE(locked_body.find("owner_future_store.admit_pending("), std::string::npos);
  EXPECT_NE(locked_body.find("owner_trace_store.append_message("), std::string::npos);
  EXPECT_NE(locked_body.find("enqueue_owner_task_locked("), std::string::npos);
  for (const char* external_store_work : {
           "vm_object_store_record_message(",
           "vm_object_store_remove_message(",
           "complete_owner_future_for_task_locked(",
           "vm_object_handle_resolve_status(",
       }) {
    EXPECT_EQ(locked_body.find(external_store_work), std::string::npos) << external_store_work;
  }
  EXPECT_NE(after_lock.find("vm_object_store_remove_message("), std::string::npos);
  EXPECT_NE(after_lock.find("complete_owner_future_for_task_locked("), std::string::npos);
}

TEST_F(DriverTest, TestOwnerQueueTraceRecordsQueuedAfterSuccessfulAdmissionOnly) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto helper_start = owner_source.find("bool enqueue_owner_task_locked(");
  const auto helper_end = owner_source.find("void finish_active_main_owner_task(", helper_start);

  ASSERT_NE(helper_start, std::string::npos);
  ASSERT_NE(helper_end, std::string::npos);
  ASSERT_GT(helper_end, helper_start);
  const auto helper_body = owner_source.substr(helper_start, helper_end - helper_start);
  EXPECT_NE(helper_body.find("append_owner_task_trace(task, \"queued\");"), std::string::npos);

  auto expect_helper_is_single_queued_trace_writer = [&](const char* start_marker,
                                                         const char* end_marker) {
    const auto start = owner_source.find(start_marker);
    const auto end = owner_source.find(end_marker, start);

    ASSERT_NE(start, std::string::npos) << start_marker;
    ASSERT_NE(end, std::string::npos) << end_marker;
    ASSERT_GT(end, start) << start_marker;
    const auto body = owner_source.substr(start, end - start);
    EXPECT_NE(body.find("enqueue_owner_task_locked("), std::string::npos) << start_marker;
    EXPECT_EQ(body.find("append_owner_task_trace(task, \"queued\");"), std::string::npos)
        << start_marker;
  };

  expect_helper_is_single_queued_trace_writer("uint64_t vm_owner_enqueue_task_epoch(",
                                              "uint64_t vm_owner_enqueue_command_frame_restore(");
  expect_helper_is_single_queued_trace_writer("uint64_t vm_owner_enqueue_command_frame_restore(",
                                              "bool vm_owner_executor_available()");
  expect_helper_is_single_queued_trace_writer(
      "uint64_t vm_owner_enqueue_test_main_required_message(", "mapping_t *vm_owner_lpc_probe(");
  expect_helper_is_single_queued_trace_writer("mapping_t *vm_owner_lpc_probe(",
                                              "mapping_t *vm_owner_lpc_canary(");
  expect_helper_is_single_queued_trace_writer("mapping_t *vm_owner_lpc_canary(",
                                              "mapping_t *vm_owner_lpc_task(");
  expect_helper_is_single_queued_trace_writer("mapping_t *vm_owner_lpc_task(",
                                              "mapping_t *vm_owner_ordinary_lpc_task(");
  expect_helper_is_single_queued_trace_writer("mapping_t *vm_owner_ordinary_lpc_task(",
                                              "uint64_t vm_owner_record_task_trace(");
}

TEST_F(DriverTest, TestOwnerRejectedSubmissionsCompleteAfterCoordinatorUnlock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  auto assert_completion_after_lock = [&](const char* start_marker, const char* end_marker,
                                          const char* completion_guard) {
    const auto start = owner_source.find(start_marker);
    const auto end = owner_source.find(end_marker, start);

    ASSERT_NE(start, std::string::npos) << start_marker;
    ASSERT_NE(end, std::string::npos) << end_marker;
    ASSERT_GT(end, start) << start_marker;
    const auto body = owner_source.substr(start, end - start);
    const auto lock_pos = body.find("std::lock_guard<std::mutex> lock(owner_runtime_mutex);");
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

    const auto locked_body = body.substr(lock_scope_start, lock_scope_end - lock_scope_start);
    const auto after_lock = body.substr(lock_scope_end);
    EXPECT_NE(locked_body.find("owner_future_store.admit_pending("), std::string::npos)
        << start_marker;
    EXPECT_NE(locked_body.find("enqueue_owner_task_locked("), std::string::npos) << start_marker;
    EXPECT_EQ(locked_body.find("complete_owner_future_for_task_locked("), std::string::npos)
        << start_marker;
    const auto guard_pos = after_lock.find(completion_guard);
    const auto completion_pos = after_lock.find("complete_owner_future_for_task_locked(");
    ASSERT_NE(guard_pos, std::string::npos) << start_marker;
    ASSERT_NE(completion_pos, std::string::npos) << start_marker;
    EXPECT_LT(guard_pos, completion_pos) << start_marker;
  };

  assert_completion_after_lock("VMOwnerStringTaskSubmission vm_owner_submit_frozen_string_task(",
                               "uint64_t vm_owner_enqueue_executor_callback_cleanup(",
                               "if (future_registered && !queued)");
  assert_completion_after_lock("mapping_t *vm_owner_lpc_task(",
                               "mapping_t *vm_owner_ordinary_lpc_task(", "if (!queued)");
  assert_completion_after_lock("mapping_t *vm_owner_ordinary_lpc_task(",
                               "uint64_t vm_owner_record_task_trace(", "if (!queued)");
}

TEST_F(DriverTest, TestOwnerComputeResultBackpressureCompletesAfterCoordinatorUnlock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto start = owner_source.find("uint64_t vm_owner_enqueue_compute_result_fields(");
  const auto end = owner_source.find("mapping_t *vm_owner_message_trace(", start);

  ASSERT_NE(start, std::string::npos);
  ASSERT_NE(end, std::string::npos);
  ASSERT_GT(end, start);
  const auto body = owner_source.substr(start, end - start);
  const auto lock_pos = body.find("std::lock_guard<std::mutex> lock(owner_runtime_mutex);");
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

  const auto locked_body = body.substr(lock_scope_start, lock_scope_end - lock_scope_start);
  const auto after_lock = body.substr(lock_scope_end);
  EXPECT_NE(locked_body.find("enqueue_owner_task_locked("), std::string::npos);
  EXPECT_EQ(locked_body.find("complete_owner_future_for_task_locked("), std::string::npos);
  const auto guard_pos = after_lock.find("if (!queued)");
  const auto completion_pos = after_lock.find("complete_owner_future_for_task_locked(");
  ASSERT_NE(guard_pos, std::string::npos);
  ASSERT_NE(completion_pos, std::string::npos);
  EXPECT_LT(guard_pos, completion_pos);
}

TEST_F(DriverTest, TestOwnerTerminalCompletionPathsAvoidCoordinatorLock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  auto assert_function_has_no_coordinator_lock =
      [&](const char* start_marker, const char* end_marker, const char* completion_call) {
        const auto start = owner_source.find(start_marker);
        const auto end = owner_source.find(end_marker, start);

        ASSERT_NE(start, std::string::npos) << start_marker;
        ASSERT_NE(end, std::string::npos) << end_marker;
        ASSERT_GT(end, start) << start_marker;
        const auto body = owner_source.substr(start, end - start);
        EXPECT_NE(body.find(completion_call), std::string::npos) << start_marker;
        EXPECT_EQ(body.find("owner_runtime_mutex"), std::string::npos) << start_marker;
      };

  assert_function_has_no_coordinator_lock("void dispatch_owner_message_in_current_context(",
                                          "void complete_owner_compute_result_task_locked(",
                                          "complete_owner_message_task_locked(task);");
  assert_function_has_no_coordinator_lock(
      "  void complete_owner_message_task(const OwnerMailboxTask &task) {",
      "  void complete_owner_compute_result_task(", "complete_owner_message_task_locked(task);");
  assert_function_has_no_coordinator_lock(
      "  void complete_owner_compute_result_task(const OwnerMailboxTask &task) {",
      "  void run_task(OwnerMailboxTask &task) {",
      "complete_owner_compute_result_task_locked(task);");

  const auto drain_start = owner_source.find("mapping_t *vm_owner_drain_mailbox(");
  const auto drain_end = owner_source.find("mapping_t *vm_owner_purge_mailbox(", drain_start);
  ASSERT_NE(drain_start, std::string::npos);
  ASSERT_NE(drain_end, std::string::npos);
  ASSERT_GT(drain_end, drain_start);
  const auto drain_body = owner_source.substr(drain_start, drain_end - drain_start);
  const auto compute_start = drain_body.find("} else if (task.task_type == \"compute_result\") {");
  const auto compute_end =
      drain_body.find("} else if (task.task_type == \"lpc_task\") {", compute_start);
  ASSERT_NE(compute_start, std::string::npos);
  ASSERT_NE(compute_end, std::string::npos);
  ASSERT_GT(compute_end, compute_start);
  const auto compute_branch = drain_body.substr(compute_start, compute_end - compute_start);
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
    const auto lock_pos = body.find("std::lock_guard<std::mutex> lock(owner_runtime_mutex);");
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

  assert_mapping_builds_after_snapshot("mapping_t *vm_owner_thread_status()",
                                       "mapping_t *vm_owner_runtime_status()");
  assert_mapping_builds_after_snapshot("mapping_t *vm_owner_runtime_status()",
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
  const auto lock_pos = body.find("std::lock_guard<std::mutex> lock(owner_runtime_mutex);");
  const auto scheduler_pos = body.find("owner_scheduler_state.mark_debug_refs(seen);");
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
  const auto lock_pos = body.find("std::lock_guard<std::mutex> lock(owner_runtime_mutex);");
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
  const auto snapshot_pos = body.find("remaining = owner_mailbox_depth(normalized_owner_id);");
  const auto lock_pos =
      body.rfind("std::lock_guard<std::mutex> lock(owner_runtime_mutex);", snapshot_pos);
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
  EXPECT_EQ(mapping_body.find("owner_mailbox_depth(normalized_owner_id)"), std::string::npos);
}

TEST_F(DriverTest, TestOwnerDrainProcessesDetachedTasksAfterRuntimeUnlock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto start = owner_source.find("mapping_t *vm_owner_drain_mailbox(");
  const auto end = owner_source.find("mapping_t *vm_owner_purge_mailbox(", start);

  ASSERT_NE(start, std::string::npos);
  ASSERT_NE(end, std::string::npos);
  ASSERT_GT(end, start);
  const auto body = owner_source.substr(start, end - start);
  const auto tasks_pos = body.find("std::vector<OwnerMailboxTask> drained_tasks;");
  const auto lock_pos =
      body.find("std::lock_guard<std::mutex> lock(owner_runtime_mutex);", tasks_pos);
  const auto detach_pos =
      body.find("drained_tasks = owner_scheduler_state.drain_owner_mailbox(", lock_pos);
  const auto lock_scope_end = body.find("\n  }\n", detach_pos);
  const auto trace_pos = body.find("append_owner_task_trace(task, \"drained\");", detach_pos);
  const auto record_pos = body.find("record_owner_mailbox_task_drained(task);", detach_pos);
  const auto total_pos = body.find(
      "total_drained.fetch_add(drained_tasks.size(), std::memory_order_relaxed);", detach_pos);
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

TEST_F(DriverTest, TestOwnerQueuedFutureCancellationProcessesRemovedTaskAfterRuntimeUnlock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto start = owner_source.find("mapping_t *vm_owner_future_cancel_queued_task(");
  const auto end = owner_source.find("mapping_t *vm_owner_future_timeout(", start);

  ASSERT_NE(start, std::string::npos);
  ASSERT_NE(end, std::string::npos);
  ASSERT_GT(end, start);
  const auto body = owner_source.substr(start, end - start);
  const auto terminal_pos = body.find("mark_owner_future_failed_terminal(");
  const auto lock_pos = body.find("std::lock_guard<std::mutex> lock(owner_runtime_mutex);");
  const auto detach_pos = body.find("owner_scheduler_state.remove_owner_task(", lock_pos);
  const auto lock_scope_end = body.find("\n    }\n", detach_pos);
  const auto kind_pos = body.find("queued_task_kind = removed.task_kind;", detach_pos);
  const auto trace_pos = body.find("append_owner_task_trace(", detach_pos);
  const auto record_pos = body.find("record_owner_mailbox_task_drained(removed);", detach_pos);
  const auto total_pos =
      body.find("total_drained.fetch_add(1, std::memory_order_relaxed);", detach_pos);
  const auto cleanup_pos =
      body.find("schedule_owner_executor_callback_cleanup_on_main(removed);", detach_pos);
  const auto release_pos = body.find("release_owner_task_target(&removed);", detach_pos);

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
  const auto tasks_pos = body.find("std::vector<OwnerMailboxTask> purged_tasks;");
  const auto lock_pos =
      body.find("std::lock_guard<std::mutex> lock(owner_runtime_mutex);", tasks_pos);
  const auto detach_pos =
      body.find("purged_tasks = owner_scheduler_state.remove_owner_mailbox(", lock_pos);
  const auto lock_scope_end = body.find("\n  }\n", detach_pos);
  const auto terminal_pos = body.find("complete_owner_future_for_task_locked(", lock_scope_end);
  const auto release_pos = body.find("release_owner_task_target(&task);", lock_scope_end);
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
  EXPECT_EQ(body.find("enqueue_owner_executor_callback_cleanup_locked(task);"), std::string::npos);
}

TEST_F(DriverTest, TestOwnerScheduleProcessesDetachedTasksAfterRuntimeUnlock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  const auto start = owner_source.find("mapping_t *vm_owner_schedule(");
  const auto end = owner_source.find("mapping_t *vm_owner_mailbox_status(", start);

  ASSERT_NE(start, std::string::npos);
  ASSERT_NE(end, std::string::npos);
  ASSERT_GT(end, start);
  const auto body = owner_source.substr(start, end - start);
  const auto tasks_pos = body.find("std::vector<OwnerMailboxTask> scheduled_tasks;");
  const auto lock_pos =
      body.find("std::lock_guard<std::mutex> lock(owner_runtime_mutex);", tasks_pos);
  const auto detach_pos = body.find("pop_next_schedulable_task(&task, false)", lock_pos);
  const auto lock_scope_end = body.find("\n  }\n", detach_pos);
  const auto array_pos = body.find("auto *tasks = allocate_array", lock_scope_end);
  const auto process_pos = body.find("for (auto &task : scheduled_tasks)", lock_scope_end);
  const auto release_pos = body.find("release_owner_task_target(&task);", lock_scope_end);

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
  EXPECT_EQ(body.find("enqueue_owner_executor_callback_cleanup_locked(task);"), std::string::npos);
}

TEST_F(DriverTest, TestOwnerMainDrainAppendsDetachedTaskTracesAfterRuntimeUnlock) {
  const auto owner_source = read_source_file_for_test("../src/vm/internal/owner.cc");
  auto expect_no_trace_under_runtime_lock = [&](const char* start_marker, const char* end_marker) {
    const auto start = owner_source.find(start_marker);
    const auto end = owner_source.find(end_marker, start);

    ASSERT_NE(start, std::string::npos) << start_marker;
    ASSERT_NE(end, std::string::npos) << end_marker;
    ASSERT_GT(end, start) << start_marker;
    const auto body = owner_source.substr(start, end - start);
    ASSERT_NE(body.find("append_owner_task_trace("), std::string::npos) << start_marker;

    constexpr const char* lock_marker = "std::lock_guard<std::mutex> lock(owner_runtime_mutex);";
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
      const auto locked_body = body.substr(lock_pos, lock_scope_end - lock_pos);
      EXPECT_EQ(locked_body.find("append_owner_task_trace("), std::string::npos) << start_marker;
      search_pos = lock_pos + 1;
      ++lock_count;
    }
    EXPECT_GT(lock_count, 0) << start_marker;
  };

  expect_no_trace_under_runtime_lock("int drain_owner_executor_callback_cleanups(",
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
  const auto lock_pos =
      body.find("std::lock_guard<std::mutex> lock(owner_runtime_mutex);", snapshot_pos);
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
    EXPECT_EQ(mapping_body.find(protected_read), std::string::npos) << protected_read;
  }
}

TEST_F(DriverTest, TestGatewayFutureCompletionExposesMainThreadCpuCounter) {
  const auto gateway_header = read_source_file_for_test("../src/packages/gateway/gateway.h");
  const auto gateway_source =
      read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto gateway_status = read_source_file_for_test("../src/packages/gateway/gateway.cc");

  ASSERT_NE(gateway_source.find("get_current_thread_cpu_time_ns()"), std::string::npos);
  ASSERT_NE(gateway_header.find("future_watch_main_completion_thread_cpu_ns_total"),
            std::string::npos);
  ASSERT_NE(gateway_header.find("future_watch_main_completion_thread_cpu_samples"),
            std::string::npos);
  ASSERT_NE(gateway_header.find("future_watch_main_completion_thread_cpu_unavailable"),
            std::string::npos);
  ASSERT_NE(gateway_status.find("GATEWAY_TOTAL_US(map, future_watch_main_completion_thread_cpu)"),
            std::string::npos);
  ASSERT_NE(gateway_status.find(
                "GATEWAY_COUNTER(map, future_watch_main_completion_thread_cpu_unavailable)"),
            std::string::npos);
}

TEST_F(DriverTest, TestGatewayRoomProjectionExposesDedicatedThreadCpuCounters) {
  const auto gateway_header = read_source_file_for_test("../src/packages/gateway/gateway.h");
  const auto gateway_source =
      read_source_file_for_test("../src/packages/gateway/gateway_session.cc");

  ASSERT_NE(gateway_header.find("room_output_projection_worker_thread_cpu_ns_total"),
            std::string::npos);
  ASSERT_NE(gateway_header.find("room_output_projection_worker_thread_cpu_ns_max"),
            std::string::npos);
  ASSERT_NE(gateway_header.find("room_output_projection_worker_thread_cpu_samples"),
            std::string::npos);
  ASSERT_NE(gateway_header.find("room_output_projection_worker_thread_cpu_unavailable"),
            std::string::npos);
  ASSERT_NE(gateway_header.find("room_output_projection_inline_thread_cpu_ns_total"),
            std::string::npos);
  ASSERT_NE(gateway_header.find("room_output_projection_inline_thread_cpu_ns_max"),
            std::string::npos);
  ASSERT_NE(gateway_header.find("room_output_projection_inline_thread_cpu_samples"),
            std::string::npos);
  ASSERT_NE(gateway_header.find("room_output_projection_inline_thread_cpu_unavailable"),
            std::string::npos);
  ASSERT_NE(gateway_source.find("worker_cpu_started_ns = get_current_thread_cpu_time_ns()"),
            std::string::npos);
  ASSERT_NE(gateway_source.find("cpu_started_ns = get_current_thread_cpu_time_ns()"),
            std::string::npos);

  // Assert the emitted keys, not how the source spells them: the counters are
  // written through GATEWAY_COUNTER(), so a source grep would pin formatting
  // instead of the contract.
  auto* status = gateway_status_internal();
  for (const char* key : {"gateway_room_output_projection_worker_thread_cpu_samples",
                          "gateway_room_output_projection_worker_thread_cpu_avg_us",
                          "gateway_room_output_projection_worker_thread_cpu_unavailable",
                          "gateway_room_output_projection_inline_thread_cpu_samples",
                          "gateway_room_output_projection_inline_thread_cpu_avg_us",
                          "gateway_room_output_projection_inline_thread_cpu_unavailable"}) {
    ASSERT_NE(find_string_in_mapping(status, key), nullptr) << key;
  }
  free_mapping(status);
}

TEST_F(DriverTest, TestGatewayReceiveApplyExposesMainThreadCpuCounter) {
  const auto gateway_header = read_source_file_for_test("../src/packages/gateway/gateway.h");
  const auto gateway_source = read_source_file_for_test("../src/packages/gateway/gateway.cc");

  ASSERT_NE(gateway_header.find("receive_apply_thread_cpu_ns_total"), std::string::npos);
  ASSERT_NE(gateway_header.find("receive_apply_thread_cpu_ns_max"), std::string::npos);
  ASSERT_NE(gateway_header.find("receive_apply_thread_cpu_samples"), std::string::npos);
  ASSERT_NE(gateway_header.find("receive_apply_thread_cpu_unavailable"), std::string::npos);
  ASSERT_NE(gateway_source.find("auto apply_cpu_started_at = get_current_thread_cpu_time_ns()"),
            std::string::npos);
  ASSERT_NE(gateway_source.find("gateway_record_thread_cpu("), std::string::npos);

  // Assert the emitted keys, not how the source spells them: the counters are
  // written through add_gateway_counter() with the prefix and name as separate
  // arguments, so a source grep would pin formatting instead of the contract.
  auto* status = gateway_status_internal();
  for (const char* key : {"gateway_receive_apply_total_us",
                          "gateway_receive_apply_thread_cpu_total_us",
                          "gateway_receive_apply_thread_cpu_unavailable"}) {
    ASSERT_NE(find_string_in_mapping(status, key), nullptr) << key;
  }
  free_mapping(status);
}

TEST_F(DriverTest, TestGatewayReceiveApplyUsesPerformanceWallClock) {
  const auto gateway_source = read_source_file_for_test("../src/packages/gateway/gateway.cc");
  const auto apply_pos =
      gateway_source.find("auto apply_started_at = get_current_performance_wall_time_ns()");

  ASSERT_NE(apply_pos, std::string::npos);
  ASSERT_NE(gateway_source.find("gateway_record_performance_wall(", apply_pos), std::string::npos);
  ASSERT_NE(gateway_source.find("std::chrono::steady_clock::now()"), std::string::npos);
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
        if (u8_egc_index_as_single_codepoint(ascii, 6, 2) != 'c')
          failures++;
        if (u8_egc_index_as_single_codepoint(utf8, 4, 0) != 0x4E2D)
          failures++;
        if (u8_egc_index_as_single_codepoint(utf8, 4, 9) != -2)
          failures++;
      }
    });
  }

  for (auto& worker : workers)
    worker.join();
  ASSERT_EQ(failures.load(), 0);
}

TEST_F(DriverTest, TestEgcAsciiSubrangesAndBoundedFind) {
  std::string ascii(2048, 'x');
  for (size_t i = 10; i < ascii.size(); i += 11)
    ascii[i] = ' ';

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

  for (auto& worker : workers)
    worker.join();
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
  auto program = compile_file(std::make_unique<IStreamLexStream>(stream), "long_dump_prog_test");

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

  const auto websocket_source =
      read_source_file_for_test((source_root + "net/websocket.cc").c_str());
  ASSERT_NE(websocket_source.find("bool websocket_establish_session("), std::string::npos);
  ASSERT_NE(websocket_source.find("schedule_user_logon("), std::string::npos);
  ASSERT_NE(websocket_source.find("void websocket_session_teardown"), std::string::npos);
  for (const auto* path : {"net/ws_ascii.cc", "net/ws_telnet.cc"}) {
    const auto source = read_source_file_for_test((source_root + path).c_str());
    ASSERT_NE(source.find("websocket_establish_session("), std::string::npos) << path;
    ASSERT_NE(source.find("websocket_session_teardown"), std::string::npos) << path;
  }
  const auto gateway_source =
      read_source_file_for_test((source_root + "packages/gateway/gateway_session.cc").c_str());
  ASSERT_NE(gateway_source.find("cancel_user_logon"), std::string::npos);

  auto* ip = user_add();
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
  const char* method = event_base_get_method(g_event_base);
  if (!method || std::strcmp(method, "epoll") != 0) {
    GTEST_SKIP() << "The deterministic epoll add-failure injection is unavailable";
  }
  const int invalid_fd = open("/dev/null", O_RDONLY | O_NONBLOCK);
  ASSERT_GE(invalid_fd, 0);
  ASSERT_EQ(event_base_once(
                g_event_base, invalid_fd, EV_READ, [](evutil_socket_t, short, void*) {}, nullptr,
                nullptr),
            -1);
  evutil_closesocket(invalid_fd);
#else
  GTEST_SKIP() << "The epoll-specific failure injection is not available on Windows";
#endif

  int callbacks = 0;
  ASSERT_EQ(event_base_once(
                g_event_base, -1, EV_TIMEOUT,
                [](evutil_socket_t, short, void* arg) {
                  ++*static_cast<int*>(arg);
                },
                &callbacks, nullptr),
            0);
  ASSERT_EQ(event_base_loop(g_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK), 0);
  ASSERT_EQ(callbacks, 1);
}

TEST_F(DriverTest, TestCompressFilePathsUseOwnedDynamicStorage) {
  const auto source = read_source_file_for_test("../src/packages/compress/compress.cc");

  // Paths must be owned std::strings, never fixed buffers or hand-managed copies.
  ASSERT_EQ(source.find("char outname[1024]"), std::string::npos);
  ASSERT_EQ(source.find("strcpy(outname"), std::string::npos);
  ASSERT_EQ(source.find("FREE_MSTR(output_file)"), std::string::npos);
  ASSERT_EQ(source.find("input_file + len - strlen(GZ_EXTENSION)"), std::string::npos);

  ASSERT_NE(source.find("std::string input_path("), std::string::npos);
  ASSERT_NE(source.find("std::string output_path(real_output_file)"), std::string::npos);
  ASSERT_NE(source.find("input_path.size() >= kGzExtensionLength"), std::string::npos);

  // compress_file()/uncompress_file() share one implementation: a single
  // definition and exactly two delegating efuns. Duplicating the body made the
  // permission order (output before input) drift between the two.
  const std::string definition = "static void compress_file_efun(CompressDirection direction)";
  const auto at = source.find(definition);
  ASSERT_NE(at, std::string::npos);
  ASSERT_EQ(source.find(definition, at + 1), std::string::npos);
  ASSERT_NE(source.find("compress_file_efun(CompressDirection::kCompress)"), std::string::npos);
  ASSERT_NE(source.find("compress_file_efun(CompressDirection::kUncompress)"), std::string::npos);
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
    ~TickQueueGuard() {
      clear_tick_events();
    }
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
        add_gametick_event(0, [&] {
          callbacks.fetch_add(1, std::memory_order_relaxed);
        });
      }
    });
  }

  start.store(true, std::memory_order_release);
  for (auto& producer : producers) {
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
    ~TickQueueGuard() {
      clear_tick_events();
    }
  } tick_queue_guard;
  std::atomic<int> callbacks{0};

  add_gametick_event(0, [&] {
    callbacks.fetch_add(1, std::memory_order_relaxed);
    add_gametick_event(0, [&] {
      callbacks.fetch_add(1, std::memory_order_relaxed);
    });
  });
  auto* cancelled = add_gametick_event(0, [&] {
    callbacks.fetch_add(100, std::memory_order_relaxed);
  });
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
  ASSERT_EQ(kBackendOwnerMainDrainContinuationDelay, std::chrono::milliseconds(1));
  ASSERT_NE(source.find("vm_owner_drain_main_tasks_with_budget("), std::string::npos);
  ASSERT_EQ(source.find("vm_owner_drain_main_tasks(1024)"), std::string::npos);
  ASSERT_NE(source.find("kBackendTickContinuationDelay"), std::string::npos);
  ASSERT_NE(source.find("kBackendOwnerMainDrainContinuationDelay"), std::string::npos);
  const auto tick_slice_start = source.find("void drain_game_tick_slice(");
  const auto tick_slice_end = source.find("// Call one bounded event slice", tick_slice_start);
  const auto tick_slice_source = source.substr(tick_slice_start, tick_slice_end - tick_slice_start);
  const auto tick_drain = tick_slice_source.find("call_tick_events_slice()");
  const auto owner_drain = tick_slice_source.find("drain_backend_owner_main_tasks_slice(false)");
  const auto continuation_check = tick_slice_source.find("if (result.due_events_remaining)");
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
    ~HandlerGuard() {
      backend_set_wakeup_handler(check_reqs);
    }
  } handler_guard;
  backend_set_wakeup_handler([&drain_count, &drained_on_main] {
    drained_on_main.store(vm_context_is_main_thread(), std::memory_order_relaxed);
    drain_count.fetch_add(1, std::memory_order_relaxed);
  });

  run_tick_events_for_test();  // drop any wakeup queued by an earlier test
  drain_count.store(0);

  bool wrote = false;
  std::thread worker([&wrote] {
    wrote = backend_wakeup_event_loop();
  });
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
    ~TickQueueGuard() {
      clear_tick_events();
    }
  } tick_queue_guard;

  ASSERT_EQ(event_base_get_npriorities(g_event_base), kBackendEventPriorityLevels);
  auto* normal = add_walltime_event(std::chrono::hours(1), [] {});
  auto* gateway = add_walltime_event(std::chrono::hours(1), [] {}, BackendEventPriority::kGateway);
  auto* background =
      add_walltime_event(std::chrono::hours(1), [] {}, BackendEventPriority::kBackground);

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
  ASSERT_EQ(walltime_event_priority_for_test(normal), walltime_event_priority_for_test(gateway));
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
    event* pressure_event{nullptr};
    int pressure_callbacks{0};
    int observer_after{-1};
  };
  constexpr int kPressureCallbacks = 64;

  auto run_probe = [&](BackendEventPriority pressure_priority,
                       BackendEventPriority observer_priority) {
    FairnessProbe probe;
    probe.pressure_event = evtimer_new(
        g_event_base,
        [](evutil_socket_t, short, void* arg) {
          auto* state = static_cast<FairnessProbe*>(arg);
          state->pressure_callbacks++;
          if (state->pressure_callbacks < kPressureCallbacks) {
            event_active(state->pressure_event, EV_TIMEOUT, 1);
          }
        },
        &probe);
    auto* observer_event = evtimer_new(
        g_event_base,
        [](evutil_socket_t, short, void* arg) {
          auto* state = static_cast<FairnessProbe*>(arg);
          state->observer_after = state->pressure_callbacks;
        },
        &probe);
    ASSERT_NE(probe.pressure_event, nullptr);
    ASSERT_NE(observer_event, nullptr);
    ASSERT_EQ(event_priority_set(probe.pressure_event, static_cast<int>(pressure_priority)), 0);
    ASSERT_EQ(event_priority_set(observer_event, static_cast<int>(observer_priority)), 0);

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

  auto* obj = load_object_for_test("single/void");
  ASSERT_NE(obj, nullptr);
  auto call_number = [](const char* method, object_t* target) -> long {
    auto* ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
    EXPECT_NE(ret, nullptr);
    EXPECT_EQ(ret ? ret->type : T_INVALID, T_NUMBER);
    return ret && ret->type == T_NUMBER ? ret->u.number : -1;
  };

  const auto normal_handle = static_cast<LPC_INT>(call_number("start_walltime_callout_probe", obj));
  const auto gateway_handle =
      static_cast<LPC_INT>(call_number("start_gateway_walltime_callout_probe", obj));
  const auto background_handle =
      static_cast<LPC_INT>(call_number("start_background_walltime_callout_probe", obj));
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
    event* gateway_event{nullptr};
    int background_callbacks{0};
    int gateway_observed_after{-1};
  } probe;
  std::vector<event*> background_events;
  constexpr int kBackgroundEvents = 4;

  probe.gateway_event = evtimer_new(
      g_event_base,
      [](evutil_socket_t, short, void* arg) {
        auto* state = static_cast<PriorityProbe*>(arg);
        state->gateway_observed_after = state->background_callbacks;
      },
      &probe);
  ASSERT_NE(probe.gateway_event, nullptr);
  ASSERT_EQ(
      event_priority_set(probe.gateway_event, static_cast<int>(BackendEventPriority::kGateway)), 0);

  for (int i = 0; i < kBackgroundEvents; ++i) {
    auto* background_event = evtimer_new(
        g_event_base,
        [](evutil_socket_t, short, void* arg) {
          auto* state = static_cast<PriorityProbe*>(arg);
          state->background_callbacks++;
          if (state->background_callbacks == 1) {
            event_active(state->gateway_event, EV_TIMEOUT, 1);
          }
        },
        &probe);
    ASSERT_NE(background_event, nullptr);
    ASSERT_EQ(
        event_priority_set(background_event, static_cast<int>(BackendEventPriority::kBackground)),
        0);
    background_events.push_back(background_event);
    event_active(background_event, EV_TIMEOUT, 1);
  }

  ASSERT_EQ(event_base_loop(g_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK), 0);
  ASSERT_EQ(probe.gateway_observed_after, 1);
  ASSERT_EQ(probe.background_callbacks, kBackgroundEvents);

  for (auto* background_event : background_events) {
    event_free(background_event);
  }
  event_free(probe.gateway_event);
}

TEST_F(DriverTest, TestBackgroundDispatchRepollsBeforeDrainingWholeActiveQueue) {
  struct DispatchProbe {
    event* normal_event{nullptr};
    int background_callbacks{0};
    int normal_observed_after{-1};
  } probe;
  std::vector<event*> background_events;
  constexpr int kBackgroundEvents = kBackendBackgroundDispatchMaxCallbacks * 2;

  probe.normal_event = evtimer_new(
      g_event_base,
      [](evutil_socket_t, short, void* arg) {
        auto* state = static_cast<DispatchProbe*>(arg);
        state->normal_observed_after = state->background_callbacks;
      },
      &probe);
  ASSERT_NE(probe.normal_event, nullptr);
  ASSERT_EQ(event_priority_set(probe.normal_event, static_cast<int>(BackendEventPriority::kNormal)),
            0);

  for (int i = 0; i < kBackgroundEvents; ++i) {
    auto* background_event = evtimer_new(
        g_event_base,
        [](evutil_socket_t, short, void* arg) {
          auto* state = static_cast<DispatchProbe*>(arg);
          state->background_callbacks++;
          if (state->background_callbacks == 1) {
            timeval due_now{};
            ASSERT_EQ(event_add(state->normal_event, &due_now), 0);
          }
        },
        &probe);
    ASSERT_NE(background_event, nullptr);
    ASSERT_EQ(
        event_priority_set(background_event, static_cast<int>(BackendEventPriority::kBackground)),
        0);
    background_events.push_back(background_event);
    event_active(background_event, EV_TIMEOUT, 1);
  }

  ASSERT_EQ(event_base_loop(g_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK), 0);
  ASSERT_EQ(probe.background_callbacks, kBackgroundEvents);
  ASSERT_GT(probe.normal_observed_after, 0);
  ASSERT_LE(probe.normal_observed_after, kBackendBackgroundDispatchMaxCallbacks);

  for (auto* background_event : background_events) {
    event_free(background_event);
  }
  event_free(probe.normal_event);
}

TEST_F(DriverTest, TestGatewayOwnerOutputQuiescesBeforeOwnerStopAndCleanup) {
  const auto source = read_source_file_for_test("../src/vm/internal/simulate.cc");
  const auto shutdown_pos = source.find("void shutdownMudOS(int exit_code)");
  const auto owner_quiesce_pos =
      source.find("gateway_owner_output_quiesce(\"driver shutdown\")", shutdown_pos);
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
  const auto simulate_source = read_source_file_for_test("../src/vm/internal/simulate.cc");

  ASSERT_NE(main_source.find("signal(SIGTERM, startshutdownMudOS);"), std::string::npos);
  ASSERT_NE(
      main_source.find("// Install the async-signal-safe shutdown request before mudlib startup."),
      std::string::npos);
  ASSERT_EQ(main_source.find("signal(SIGTERM, attempt_shutdown);"), std::string::npos);
  ASSERT_NE(simulate_source.find("sig == SIGTERM ? 0 : -1"), std::string::npos);
  ASSERT_NE(backend_source.find("shutdownMudOS(MudOS_shutdown_exit_code);"), std::string::npos);
  ASSERT_NE(backend_source.find("event_base_loopbreak(g_event_base);"), std::string::npos);
}

TEST_F(DriverTest, TestAuditSignalDispatchHandlersOnlyRecordRequestsForMainThread) {
  const auto source = read_source_file_for_test("../src/mainlib.cc");
  const char* const handlers[] = {"request_sigint", "sig_usr1", "sig_usr2", "sig_cld"};
  for (const char* handler : handlers) {
    const std::string marker = std::string("void ") + handler + "(int";
    const auto start = source.find(marker);
    ASSERT_NE(start, std::string::npos) << handler;
    const auto end = source.find("\n}", start);
    ASSERT_NE(end, std::string::npos) << handler;
    const auto body = source.substr(start, end - start);
    EXPECT_EQ(body.find("apply_master_ob"), std::string::npos) << handler;
    EXPECT_EQ(body.find("debug_message"), std::string::npos) << handler;
    EXPECT_EQ(body.find("exit("), std::string::npos) << handler;
    EXPECT_EQ(body.find("fatal("), std::string::npos) << handler;
    EXPECT_EQ(body.find("StackTrace"), std::string::npos) << handler;
    EXPECT_EQ(body.find("vm_owner_thread_stop"), std::string::npos) << handler;
    EXPECT_EQ(body.find("signal("), std::string::npos) << handler;
  }

  EXPECT_NE(source.find("std::atomic<unsigned int> g_pending_signal_mask"), std::string::npos);
  EXPECT_NE(source.find("is_always_lock_free"), std::string::npos);
  EXPECT_NE(source.find("process_pending_signal_requests()"), std::string::npos);
  EXPECT_NE(source.find("signal(SIGINT, request_sigint);"), std::string::npos);
  EXPECT_EQ(source.find("signal(SIGINT, attempt_shutdown);"), std::string::npos);

  const auto interpreter_source = read_source_file_for_test("../src/vm/internal/base/interpret.cc");
  EXPECT_NE(interpreter_source.find("has_pending_signal_requests()"), std::string::npos);
  EXPECT_NE(interpreter_source.find("process_pending_signal_requests()"), std::string::npos);
}

TEST_F(DriverTest, TestControlledShutdownPreparesMudlibBeforeRuntimeTeardown) {
  const auto applies_source = read_source_file_for_test("../src/vm/internal/applies");
  const auto shutdown_source = read_source_file_for_test("../src/vm/internal/simulate.cc");
  const auto shutdown_pos = shutdown_source.find("void shutdownMudOS(int exit_code)");
  const auto prepare_pos =
      shutdown_source.find("safe_apply_master_ob(APPLY_PREPARE_SHUTDOWN, 1)", shutdown_pos);
  const auto async_drain_pos = shutdown_source.find("complete_all_asyncio();", shutdown_pos);
  const auto owner_stop_pos = shutdown_source.find("vm_owner_thread_stop();", shutdown_pos);
  const auto async_stop_pos = shutdown_source.find("stop_async_workers();", shutdown_pos);
  const auto external_cleanup_pos = shutdown_source.find("external_cleanup();", shutdown_pos);

  ASSERT_NE(applies_source.find("PREPARE_SHUTDOWN"), std::string::npos);
  ASSERT_NE(shutdown_pos, std::string::npos);
  ASSERT_NE(prepare_pos, std::string::npos);
  ASSERT_NE(async_drain_pos, std::string::npos);
  ASSERT_NE(owner_stop_pos, std::string::npos);
  ASSERT_NE(async_stop_pos, std::string::npos);
  ASSERT_NE(external_cleanup_pos, std::string::npos);
  ASSERT_LT(prepare_pos, async_drain_pos);
  ASSERT_LT(prepare_pos, owner_stop_pos);
  ASSERT_LT(owner_stop_pos, async_stop_pos);
  ASSERT_LT(async_stop_pos, external_cleanup_pos);
}

TEST_F(DriverTest, TestGatewayOwnerOutputQuiesceRejectsOffMainBeforeContainerAccess) {
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto quiesce_pos =
      source.find("mapping_t *gateway_owner_output_quiesce(const char *reason)");
  const auto next_function_pos =
      source.find("int gateway_fill_session_protocol_output_with_writer", quiesce_pos);
  const auto main_guard_pos = source.find("if (!vm_context_is_main_thread())", quiesce_pos);
  const auto session_watch_read_pos = source.find("g_gateway_session_future_watches", quiesce_pos);
  const auto room_wave_read_pos = source.find("g_gateway_room_output_waves", quiesce_pos);
  const auto room_pending_read_pos =
      source.find("gateway_room_output_projection_pending_count()", quiesce_pos);

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

