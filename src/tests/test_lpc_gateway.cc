#include "test_gateway_support.h"

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

#ifndef _WIN32
TEST_F(DriverTest, TestAuditAsciiLifetimeClose) {
  for (int reconnect = 0; reconnect < 16; ++reconnect) {
    SCOPED_TRACE(reconnect);
    AsciiInputFixture fixture;
    ASSERT_NO_FATAL_FAILURE(fixture.open());
    ASSERT_NO_FATAL_FAILURE(fixture.receive("close\nignored\n"));
    fixture.expect_closed(false);
    fixture.expect_lines({"close"});
    auto* result = call_lpc_method(fixture.object(), "query_disconnects");
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->type, T_NUMBER);
    EXPECT_EQ(result->u.number, 1);
  }
}
#endif

#ifndef _WIN32
TEST_F(DriverTest, TestAuditAsciiLifetimeDestruct) {
  AsciiInputFixture fixture;
  ASSERT_NO_FATAL_FAILURE(fixture.open());
  ASSERT_NO_FATAL_FAILURE(fixture.receive("destruct\nignored\n"));
  fixture.expect_closed(false);
  EXPECT_NE(fixture.object()->flags & O_DESTRUCTED, 0);
}
#endif

#ifndef _WIN32
TEST_F(DriverTest, TestAuditAsciiLifetimeReplacementSurvives) {
  AsciiInputFixture fixture;
  ASSERT_NO_FATAL_FAILURE(fixture.open());
  ASSERT_NO_FATAL_FAILURE(fixture.receive("replace\nignored\n"));
  fixture.expect_closed(true);
  fixture.expect_lines({"replace"});
  auto* session = gateway_find_session("audit-ascii-replacement");
  ASSERT_NE(session, nullptr);
  ASSERT_NE(session->user_ob, nullptr);
  EXPECT_NE(session->user_ob->interactive, nullptr);
  EXPECT_NE(session->user_ob, fixture.object());
  auto* result = call_lpc_method(session->user_ob, "query_received");
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_ARRAY);
  EXPECT_EQ(result->u.arr->size, 0);
}
#endif

#ifndef _WIN32
TEST_F(DriverTest, TestAuditAsciiLifetimeCallbackErrorRecovers) {
  AsciiInputFixture fixture;
  ASSERT_NO_FATAL_FAILURE(fixture.open());
  ASSERT_NO_FATAL_FAILURE(fixture.receive("error\nok\n"));
  fixture.expect_lines({"error", "ok"});
  ASSERT_NE(fixture.object()->interactive, nullptr);
  EXPECT_EQ(fixture.object()->interactive->telnet_receive_depth, 0);
}
#endif

#ifndef _WIN32
TEST_F(DriverTest, TestAuditAsciiEmptyLineDelivery) {
  check_ascii_chunks("", {}, {0});
  for (const std::string input : {"\n", "\r\n"}) {
    SCOPED_TRACE(input.size());
    check_ascii_chunks(input, {""}, {input.size()});
  }
  check_ascii_chunks("\n\n", {"", ""}, {1, 1});
  check_ascii_chunks("\r\n\n", {"", ""}, {1, 1, 1});
}
#endif

#ifndef _WIN32
TEST_F(DriverTest, TestAuditAsciiCrLfEveryPartition) {
  const std::vector<std::pair<std::string, std::vector<std::string>>> samples{
      {"a\r\nb\n", {"a", "b"}}, {"\r\n\nx\r\n", {"", "", "x"}},
      {"a\nrest", {"a"}},       {"x\b\x7f\x1b[D\n", {"x\b\x7f\x1b[D"}},
      {"中\n", {"中"}},         {"unfinished", {}}};
  for (const auto& [input, expected] : samples) {
    SCOPED_TRACE(input);
    for (size_t mask = 0; mask < (size_t{1} << (input.size() - 1)); ++mask) {
      SCOPED_TRACE(mask);
      std::vector<size_t> chunks;
      size_t count = 1;
      for (size_t i = 0; i + 1 < input.size(); ++i) {
        if (mask & (size_t{1} << i)) {
          chunks.push_back(count);
          count = 0;
        }
        ++count;
      }
      chunks.push_back(count);
      ASSERT_NO_FATAL_FAILURE(check_ascii_chunks(input, expected, chunks));
    }
  }
}
#endif

#ifndef _WIN32
TEST_F(DriverTest, TestAuditTelnetStreamTextUtf8AndEncodingChunkBoundaries) {
  const std::string utf8 = std::string("A") + "\xf0\x9f\x98\x80" + "B";
  for (size_t split = 1; split < utf8.size(); ++split) {
    SCOPED_TRACE(split);
    AsciiInputFixture fixture;
    ASSERT_NO_FATAL_FAILURE(fixture.open_telnet());
    ASSERT_NO_FATAL_FAILURE(fixture.receive(utf8.substr(0, split)));
    ASSERT_NO_FATAL_FAILURE(fixture.receive(utf8.substr(split)));
    EXPECT_EQ(fixture.buffered_text(), utf8);
    EXPECT_EQ(fixture.telnet_carry_length(), 0);
  }

  {
    AsciiInputFixture fixture;
    ASSERT_NO_FATAL_FAILURE(fixture.open_telnet());
    const std::string input = "\xf0\x9f\x98\x80";
    for (const char byte : input) {
      ASSERT_NO_FATAL_FAILURE(fixture.receive(std::string(1, byte)));
    }
    EXPECT_EQ(fixture.buffered_text(), input);
  }

  {
    AsciiInputFixture fixture;
    ASSERT_NO_FATAL_FAILURE(fixture.open_telnet());
    ASSERT_NO_FATAL_FAILURE(fixture.set_input_encoding("shift_jis"));
    const std::string input = "\x82\xa0";
    ASSERT_NO_FATAL_FAILURE(fixture.receive(input.substr(0, 1)));
    EXPECT_EQ(fixture.buffered_text(), "");
    ASSERT_NO_FATAL_FAILURE(fixture.receive(input.substr(1)));
    EXPECT_EQ(fixture.buffered_text(), "\xe3\x81\x82");
  }
}
#endif

#ifndef _WIN32
TEST_F(DriverTest, TestAuditTelnetStreamTextMalformedUtf8StillSanitizes) {
  AsciiInputFixture fixture;
  ASSERT_NO_FATAL_FAILURE(fixture.open_telnet());
  ASSERT_NO_FATAL_FAILURE(fixture.receive(std::string("\xf0", 1)));
  EXPECT_EQ(fixture.buffered_text(), "");
  EXPECT_EQ(fixture.telnet_carry_length(), 1);
  ASSERT_NO_FATAL_FAILURE(fixture.receive(std::string("\x28", 1)));
  EXPECT_EQ(fixture.buffered_text(), "\xef\xbf\xbd(");
  EXPECT_EQ(fixture.telnet_carry_length(), 0);
}
#endif

#ifndef _WIN32
TEST_F(DriverTest, TestAuditTelnetStreamTextEofAndCloseClearsCarry) {
  AsciiInputFixture fixture;
  ASSERT_NO_FATAL_FAILURE(fixture.open_telnet());
  ASSERT_NO_FATAL_FAILURE(fixture.receive(std::string("\xf0", 1)));
  EXPECT_EQ(fixture.telnet_carry_length(), 1);
  ASSERT_NO_FATAL_FAILURE(fixture.close_transport());
  EXPECT_EQ(fixture.object()->interactive, nullptr);
}
#endif

#ifndef _WIN32
TEST_F(DriverTest, TestAuditAsciiCrLfBoundedRandomFragments) {
  const std::string input = "a\r\n\nbeta\n\r\n最后\r\nunfinished";
  const std::vector<std::string> expected{"a", "", "beta", "", "最后"};
  uint32_t seed = 0xa5010910;
  for (size_t trial = 0; trial < 128; ++trial) {
    SCOPED_TRACE(trial);
    std::vector<size_t> chunks;
    size_t offset = 0;
    while (offset < input.size()) {
      seed = seed * 1664525 + 1013904223;
      const auto count = std::min<size_t>(1 + seed % 7, input.size() - offset);
      chunks.push_back(count);
      offset += count;
    }
    ASSERT_NO_FATAL_FAILURE(check_ascii_chunks(input, expected, chunks));
  }
}
#endif

TEST_F(DriverTest, TestAuditNoechoLogRedactsInput) {
  constexpr const char* kSessionId = "gw-test-noecho-log";
  auto* ob = create_gateway_session_for_test(kSessionId, "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));
  add_ref(ob, "TestAuditNoechoLogRedactsInput");

  auto* enabled = call_lpc_method(ob, "enable_gateway_input_to");
  ASSERT_NE(enabled, nullptr);
  ASSERT_EQ(enabled->type, T_NUMBER);
  ASSERT_EQ(enabled->u.number, 1);

  auto* ip = ob->interactive;
  const std::string secret = "audit-noecho-secret";
  const std::string secret_line = secret + "\n";
  ASSERT_LT(secret_line.size(), sizeof(ip->text));
  memcpy(ip->text, secret_line.data(), secret_line.size());
  ip->text_start = 0;
  ip->text_end = static_cast<int>(secret_line.size());
  ip->iflags |= CMD_IN_BUF;

  const auto previous_debug_level = debug_level;
  debug_level_set("connections");
  testing::internal::CaptureStdout();
  save_command_giver(ob);
  const auto processed_secret = process_user_command(ip);
  restore_command_giver();
  const auto secret_output = testing::internal::GetCapturedStdout();
  debug_level = previous_debug_level;

  ASSERT_EQ(processed_secret, 1);
  EXPECT_EQ(secret_output.find(secret), std::string::npos);
  EXPECT_NE(secret_output.find("<redacted>"), std::string::npos);
  auto* received = call_lpc_method(ob, "query_last_input_to_line");
  ASSERT_NE(received, nullptr);
  ASSERT_EQ(received->type, T_STRING);
  EXPECT_STREQ(received->u.string, secret.c_str());

  ASSERT_EQ(gateway_destroy_session_internal(kSessionId, "secret_done", "done"), 1);
  ASSERT_EQ(ob->interactive, nullptr);
  destruct_object(ob);
  free_object(&ob, "TestAuditNoechoLogRedactsInput");

  // Use a fresh session for the normal-input assertion. The noecho callback
  // intentionally exercises terminal-mode restoration; reusing that
  // interactive in the same test would couple the logging assertion to
  // session teardown timing on Windows.
  constexpr const char* kNormalSessionId = "gw-test-normal-log";
  ob = create_gateway_session_for_test(kNormalSessionId, "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  add_ref(ob, "TestAuditNoechoLogRedactsInputNormal");
  ip = ob->interactive;
  const std::string normal = "audit-normal-input";
  const std::string normal_line = normal + "\n";
  ASSERT_LT(normal_line.size(), sizeof(ip->text));
  memcpy(ip->text, normal_line.data(), normal_line.size());
  ip->text_start = 0;
  ip->text_end = static_cast<int>(normal_line.size());
  ip->iflags |= CMD_IN_BUF;

  const auto normal_debug_level = debug_level;
  debug_level_set("connections");
  testing::internal::CaptureStdout();
  save_command_giver(ob);
  const auto processed_normal = process_user_command(ip);
  restore_command_giver();
  const auto normal_output = testing::internal::GetCapturedStdout();
  debug_level = normal_debug_level;

  ASSERT_EQ(processed_normal, 1);
  EXPECT_NE(normal_output.find(normal), std::string::npos);

  ASSERT_EQ(gateway_destroy_session_internal(kNormalSessionId, "test_done", "done"), 1);
  ASSERT_EQ(ob->interactive, nullptr);
  destruct_object(ob);
  free_object(&ob, "TestAuditNoechoLogRedactsInputNormal");
}

TEST_F(DriverTest, TestAuditNoechoLogRedactsGetCharInput) {
  constexpr const char* kSessionId = "gw-test-noecho-get-char-log";
  auto* ob = create_gateway_session_for_test(kSessionId, "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));
  add_ref(ob, "TestAuditNoechoLogRedactsGetCharInput");

  auto* enabled = call_lpc_method(ob, "enable_gateway_get_char");
  ASSERT_NE(enabled, nullptr);
  ASSERT_EQ(enabled->type, T_NUMBER);
  ASSERT_EQ(enabled->u.number, 1);

  auto* ip = ob->interactive;
  constexpr char secret = 'q';
  ip->text[0] = secret;
  ip->text_start = 0;
  ip->text_end = 1;
  ip->iflags |= CMD_IN_BUF;

  const auto previous_debug_level = debug_level;
  debug_level_set("connections");
  testing::internal::CaptureStdout();
  save_command_giver(ob);
  const auto processed = process_user_command(ip);
  restore_command_giver();
  const auto output = testing::internal::GetCapturedStdout();
  debug_level = previous_debug_level;

  ASSERT_EQ(processed, 1);
  EXPECT_EQ(output.find("user_command = (q)"), std::string::npos);
  EXPECT_NE(output.find("<redacted>"), std::string::npos);
  auto* received = call_lpc_method(ob, "query_last_get_char_value");
  ASSERT_NE(received, nullptr);
  ASSERT_EQ(received->type, T_STRING);
  EXPECT_STREQ(received->u.string, "q");

  ASSERT_EQ(gateway_destroy_session_internal(kSessionId, "test_done", "done"), 1);
  ASSERT_EQ(ob->interactive, nullptr);
  destruct_object(ob);
  free_object(&ob, "TestAuditNoechoLogRedactsGetCharInput");
}

TEST_F(DriverTest, TestAuditCharMode) {
  constexpr const char* kSessionId = "gw-test-char-mode";
  auto* ob = create_gateway_session_for_test(kSessionId, "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  ASSERT_TRUE(gateway_is_session(ob));
  add_ref(ob, "TestAuditCharMode");

  auto arm_get_char = [&]() {
    auto* enabled = call_lpc_method(ob, "enable_gateway_get_char");
    ASSERT_NE(enabled, nullptr);
    ASSERT_EQ(enabled->type, T_NUMBER);
    ASSERT_EQ(enabled->u.number, 1);
  };
  auto expect_value = [&](const std::string& input, const std::string& expected) {
    arm_get_char();
    auto* ip = ob->interactive;
    ASSERT_NE(ip, nullptr);
    ASSERT_TRUE((ip->iflags & SINGLE_CHAR) != 0);
    on_user_input(ip, input.data(), input.size());
    ASSERT_EQ(ip->text_start, 0);
    ASSERT_EQ(ip->text_end, static_cast<int>(input.size()));
    for (size_t index = 0; index < input.size(); index++) {
      ASSERT_EQ(static_cast<unsigned char>(ip->text[index]),
                static_cast<unsigned char>(input[index]));
    }
    ASSERT_TRUE(cmd_in_buf(ip));
    ip->iflags |= CMD_IN_BUF;
    ASSERT_EQ(gateway_process_pending_command_internal(ob), 1);
    auto* value = call_lpc_method(ob, "query_last_get_char_value");
    ASSERT_NE(value, nullptr);
    ASSERT_EQ(value->type, T_STRING);
    EXPECT_EQ(std::string(value->u.string), expected);
  };

  expect_value(std::string(1, '\x08'), std::string(1, '\x08'));
  expect_value(std::string(1, '\x7f'), std::string(1, '\x7f'));
  expect_value(std::string(1, '\x1b'), std::string(1, '\x1b'));
  expect_value("A", "A");
  expect_value("\xe4\xb8\xad", "\xe4\xb8\xad");
  expect_value("\xf0\x9f\x98\x80", "\xf0\x9f\x98\x80");

  // A valid UTF-8 prefix does not become a callback until its continuation arrives.
  arm_get_char();
  auto* ip = ob->interactive;
  ASSERT_NE(ip, nullptr);
  on_user_input(ip, "\xe4", 1);
  EXPECT_FALSE(cmd_in_buf(ip));
  EXPECT_EQ(ip->iflags & CMD_IN_BUF, 0);
  on_user_input(ip, "\xb8\xad", 2);
  ASSERT_TRUE(cmd_in_buf(ip));
  ip->iflags |= CMD_IN_BUF;
  ASSERT_EQ(gateway_process_pending_command_internal(ob), 1);
  auto* split_value = call_lpc_method(ob, "query_last_get_char_value");
  ASSERT_NE(split_value, nullptr);
  ASSERT_EQ(split_value->type, T_STRING);
  EXPECT_STREQ(split_value->u.string, "\xe4\xb8\xad");

  // A malformed lead byte must make progress instead of waiting forever.
  expect_value(std::string("\xe2", 1) + "A", std::string(1, '\xe2'));
  expect_value(std::string("\xe0\x80", 2) + "A", std::string(1, '\xe0'));
  expect_value(std::string("\xf4\x90", 2) + "A", std::string(1, '\xf4'));

  ASSERT_EQ(gateway_destroy_session_internal(kSessionId, "test_done", "done"), 1);
  ASSERT_EQ(ob->interactive, nullptr);
  destruct_object(ob);
  free_object(&ob, "TestAuditCharMode");
}

TEST_F(DriverTest, TestAuditOutputBudgetArithmetic) {
  EXPECT_TRUE(comm_output_budget_allows_for_test(0, 0, 4, 4, 8));
  EXPECT_FALSE(comm_output_budget_allows_for_test(4, 0, 1, 4, 8));
  EXPECT_FALSE(comm_output_budget_allows_for_test(0, 8, 1, 4, 8));
  EXPECT_FALSE(comm_output_budget_allows_for_test(0, 0, 5, 4, 8));
  EXPECT_FALSE(comm_output_budget_allows_for_test(0, 0, 9, 4, 8));
}

#ifndef _WIN32
TEST_F(DriverTest, TestAuditOutputBudgetLoopbackDrain) {
  OutputBudgetConfigGuard config_guard;
  CONFIG_INT(__RC_MAX_PENDING_OUTPUT_BYTES__) = 16;
  CONFIG_INT(__RC_MAX_TOTAL_PENDING_OUTPUT_BYTES__) = 32;
  const auto baseline = comm_pending_output_bytes_for_test();

  AsciiInputFixture fixture;
  ASSERT_NO_FATAL_FAILURE(fixture.open());
  ASSERT_NO_FATAL_FAILURE(fixture.receive("emit\n"));
  ASSERT_EQ(fixture.pump_transport(), "123456");
  ASSERT_NE(fixture.object()->interactive, nullptr);
  EXPECT_EQ(fixture.object()->interactive->pending_output_bytes, 0u);
  EXPECT_EQ(comm_pending_output_bytes_for_test(), baseline);
}
#endif

#ifndef _WIN32
TEST_F(DriverTest, TestAuditOutputBudgetInteractiveAccounting) {
  OutputBudgetConfigGuard config_guard;
  CONFIG_INT(__RC_MAX_PENDING_OUTPUT_BYTES__) = 16;
  CONFIG_INT(__RC_MAX_TOTAL_PENDING_OUTPUT_BYTES__) = 32;
  const auto baseline = comm_pending_output_bytes_for_test();

  AsciiInputFixture fixture;
  ASSERT_NO_FATAL_FAILURE(fixture.open());
  auto* ip = fixture.object()->interactive;
  ASSERT_NE(ip, nullptr);
  ASSERT_NO_FATAL_FAILURE(fixture.receive("emit\n"));
  auto* output = bufferevent_get_output(ip->ev_buffer);
  ASSERT_NE(output, nullptr);
  EXPECT_EQ(evbuffer_get_length(output), 6u);
  EXPECT_EQ(ip->pending_output_bytes, 6u);
  EXPECT_EQ(comm_pending_output_bytes_for_test(), baseline + 6);

  ASSERT_EQ(evbuffer_unfreeze(output, 1), 0);
  ASSERT_EQ(evbuffer_drain(output, 6), 0);
  ASSERT_EQ(evbuffer_freeze(output, 1), 0);
  comm_update_output_accounting(ip, output);
  EXPECT_EQ(ip->pending_output_bytes, 0u);
  EXPECT_EQ(comm_pending_output_bytes_for_test(), baseline);
}
#endif

#ifndef _WIN32
TEST_F(DriverTest, TestAuditOutputBudgetRejectsSlowInteractiveConnection) {
  OutputBudgetConfigGuard config_guard;
  CONFIG_INT(__RC_MAX_PENDING_OUTPUT_BYTES__) = 4;
  CONFIG_INT(__RC_MAX_TOTAL_PENDING_OUTPUT_BYTES__) = 8;
  const auto baseline = comm_pending_output_bytes_for_test();
  const auto rejected_before = comm_output_budget_rejections_for_test();

  AsciiInputFixture fixture;
  ASSERT_NO_FATAL_FAILURE(fixture.open());
  ASSERT_NO_FATAL_FAILURE(fixture.receive("emit\n"));
  EXPECT_EQ(comm_output_budget_rejections_for_test(), rejected_before + 1);
  fixture.expect_closed(false);
  EXPECT_EQ(comm_pending_output_bytes_for_test(), baseline);
}
#endif

#ifndef _WIN32
TEST_F(DriverTest, TestAuditOutputBudgetRejectsGlobalSlowInteractiveConnection) {
  OutputBudgetConfigGuard config_guard;
  CONFIG_INT(__RC_MAX_PENDING_OUTPUT_BYTES__) = 8;
  CONFIG_INT(__RC_MAX_TOTAL_PENDING_OUTPUT_BYTES__) = 10;
  const auto baseline = comm_pending_output_bytes_for_test();
  const auto rejected_before = comm_output_budget_rejections_for_test();

  AsciiInputFixture first;
  ASSERT_NO_FATAL_FAILURE(first.open());
  ASSERT_NO_FATAL_FAILURE(first.receive("emit\n"));
  ASSERT_NE(first.object()->interactive, nullptr);

  AsciiInputFixture second;
  ASSERT_NO_FATAL_FAILURE(second.open());
  ASSERT_NO_FATAL_FAILURE(second.receive("emit\n"));
  EXPECT_EQ(comm_output_budget_rejections_for_test(), rejected_before + 1);
  second.expect_closed(false);
  EXPECT_EQ(comm_pending_output_bytes_for_test(), baseline + 6);
}
#endif

TEST_F(DriverTest, TestAuditAsciiEmptyLineRedzoneBounds) {
  auto buffer = std::make_unique<char[]>(1);
  buffer[0] = '\n';
  size_t line_end = 99;
  size_t next = 99;
  ASSERT_TRUE(comm_ascii_line_bounds_for_test(buffer.get(), 0, 1, &line_end, &next));
  EXPECT_EQ(line_end, 0);
  EXPECT_EQ(next, 1);
  EXPECT_FALSE(comm_ascii_line_bounds_for_test(buffer.get(), 0, 0, &line_end, &next));
  EXPECT_FALSE(comm_ascii_line_bounds_for_test(buffer.get(), 1, 1, &line_end, &next));

  const char preceding_cr[] = {'\r', '\n'};
  ASSERT_TRUE(comm_ascii_line_bounds_for_test(preceding_cr, 1, 2, &line_end, &next));
  EXPECT_EQ(line_end, 1);
  EXPECT_EQ(next, 2);
}

TEST_F(DriverTest, TestGatewayCreateSessionEventFailureCleansPendingUser) {
  ASSERT_NE(master_ob, nullptr);
  ASSERT_EQ(master_ob->interactive, nullptr);

  const auto master_refs = master_ob->ref;
  const std::vector<interactive_t*> users_before(users().begin(), users().end());

  for (bool another_session : {false, true}) {
    SCOPED_TRACE(another_session);
    interactive_t* attached = nullptr;
    if (another_session) {
      attached = user_add();
      ASSERT_NE(attached, nullptr);
      attached->ob = master_ob;
      attached->connection_type = PORT_TYPE_GATEWAY;
      attached->iflags = GATEWAY_SESSION;
      master_ob->interactive = attached;
      add_ref(master_ob, "gateway create failure test attached user");
    }

    const std::string session_id = another_session ? "gw-test-create-event-failure-attached"
                                                   : "gw-test-create-event-failure-unbound";
    gateway_fail_next_command_event_for_test();
    EXPECT_EQ(gateway_create_session_internal(session_id.c_str(), nullptr, "127.0.0.1", 6040, -1),
              nullptr);
    EXPECT_EQ(users().size(), users_before.size() + (another_session ? 1 : 0));
    EXPECT_EQ(master_ob->interactive, attached);
    EXPECT_EQ(master_ob->flags & O_ONCE_INTERACTIVE, 0);

    std::vector<interactive_t*> unexpected_users;
    for (auto* candidate : users()) {
      if (std::find(users_before.begin(), users_before.end(), candidate) == users_before.end() &&
          candidate != attached) {
        unexpected_users.push_back(candidate);
      }
    }
    for (auto* candidate : unexpected_users) {
      cancel_user_logon(candidate);
      user_del(candidate);
      FREE(candidate);
    }

    if (master_ob->interactive == attached && attached) {
      remove_interactive(master_ob, 1);
    }
    EXPECT_EQ(users().size(), users_before.size());
    EXPECT_EQ(master_ob->interactive, nullptr);
    EXPECT_EQ(master_ob->ref, master_refs);
  }
}

TEST_F(DriverTest, TestGatewayQueryIpNumberUsesForwardedClientAddress) {
  const char* session_id = "gw-test-query-ip-number";
  auto* ob = create_gateway_session_for_test(session_id, "/clone/gateway_login_example");
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayQueryIpNumberUsesForwardedClientAddress");

  const char* address = query_ip_number(ob);
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
    ~PacketSizeGuard() {
      g_gateway_max_packet_size = original;
    }
  } packet_size_guard;

  g_gateway_max_packet_size = 1024;
  constexpr int master_fd = 1709;
  const char* first_session_id = "gw-test-master-fifo-budget-first";
  const char* second_session_id = "gw-test-master-fifo-budget-second";
  bufferevent* pair[2] = {nullptr, nullptr};
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  ASSERT_NE(gateway_register_master_for_test(master_fd, pair[0]), nullptr);

  auto* first_ob =
      create_gateway_session_for_test(first_session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(first_ob, nullptr);
  add_ref(first_ob, "TestGatewayMasterSessionFifoBytesAreAggregatelyBoundedFirst");
  auto* second_ob =
      create_gateway_session_for_test(second_session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(second_ob, nullptr);
  add_ref(second_ob, "TestGatewayMasterSessionFifoBytesAreAggregatelyBoundedSecond");
  auto* first = gateway_find_session(first_session_id);
  auto* second = gateway_find_session(second_session_id);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);

  const auto aggregate_limit = gateway_write_buffer_limit_for_test();
  const auto aggregate_rejected_before =
      g_gateway_runtime_counters.output_fifo_aggregate_wire_bytes_rejected.load(
          std::memory_order_relaxed);
  const std::string payload(512, 'x');
  bool rejected = false;
  for (int attempt = 0; attempt < 4096; ++attempt) {
    auto* session = attempt % 2 == 0 ? first : second;
    if (!gateway_enqueue_session_protocol_output(session, payload.data(), payload.size())) {
      rejected = true;
      break;
    }
  }

  ASSERT_TRUE(rejected);
  EXPECT_LE(first->output_fifo_wire_bytes + second->output_fifo_wire_bytes, aggregate_limit);
  EXPECT_EQ(g_gateway_runtime_counters.output_fifo_aggregate_wire_bytes_rejected.load(
                std::memory_order_relaxed),
            aggregate_rejected_before + 1);

  ASSERT_EQ(gateway_destroy_session_internal(first_session_id, "test_done", "done"), 1);
  destruct_object(first_ob);
  free_object(&first_ob, "TestGatewayMasterSessionFifoBytesAreAggregatelyBoundedFirst");
  ASSERT_EQ(gateway_destroy_session_internal(second_session_id, "test_done", "done"), 1);
  destruct_object(second_ob);
  free_object(&second_ob, "TestGatewayMasterSessionFifoBytesAreAggregatelyBoundedSecond");
  gateway_remove_master_for_test(master_fd);
  pair[0] = nullptr;
  bufferevent_free(pair[1]);
}

TEST_F(DriverTest, TestGatewayDetachedSessionFifoBytesRemainAggregatelyBounded) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() {
      g_gateway_max_packet_size = original;
    }
  } packet_size_guard;

  g_gateway_max_packet_size = 1024;
  constexpr int first_master_fd = 1711;
  constexpr int second_master_fd = 1712;
  const char* first_session_id = "gw-test-detached-fifo-budget-first";
  const char* second_session_id = "gw-test-detached-fifo-budget-second";
  auto* first_ob = create_gateway_session_for_test(first_session_id, "/clone/gateway_login_example",
                                                   first_master_fd);
  ASSERT_NE(first_ob, nullptr);
  add_ref(first_ob, "TestGatewayDetachedSessionFifoBytesRemainAggregatelyBoundedFirst");
  auto* second_ob = create_gateway_session_for_test(
      second_session_id, "/clone/gateway_login_example", second_master_fd);
  ASSERT_NE(second_ob, nullptr);
  add_ref(second_ob, "TestGatewayDetachedSessionFifoBytesRemainAggregatelyBoundedSecond");
  auto* first = gateway_find_session(first_session_id);
  auto* second = gateway_find_session(second_session_id);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);

  const auto aggregate_limit = gateway_write_buffer_limit_for_test();
  const std::string payload(512, 'x');
  while (gateway_enqueue_session_protocol_output(first, payload.data(), payload.size())) {
    EXPECT_LE(first->output_fifo_wire_bytes, aggregate_limit);
  }
  while (gateway_enqueue_session_protocol_output(second, payload.data(), payload.size())) {
    EXPECT_LE(second->output_fifo_wire_bytes, aggregate_limit);
  }
  ASSERT_GT(first->output_fifo_wire_bytes, aggregate_limit / 2);
  ASSERT_GT(second->output_fifo_wire_bytes, aggregate_limit / 2);
  EXPECT_EQ(gateway_session_fifo_wire_detached_bytes(), 0u);
  const auto rebucket_dropped_before =
      g_gateway_runtime_counters.output_fifo_rebucket_wire_bytes_dropped.load(
          std::memory_order_relaxed);

  gateway_cleanup_master_sessions(first_master_fd);
  EXPECT_EQ(gateway_session_fifo_wire_detached_bytes(), first->output_fifo_wire_bytes);
  gateway_cleanup_master_sessions(second_master_fd);
  EXPECT_LE(gateway_session_fifo_wire_detached_bytes(), aggregate_limit);
  EXPECT_GT(g_gateway_runtime_counters.output_fifo_rebucket_wire_bytes_dropped.load(
                std::memory_order_relaxed),
            rebucket_dropped_before);

  ASSERT_EQ(gateway_destroy_session_internal(first_session_id, "test_done", "done"), 1);
  destruct_object(first_ob);
  free_object(&first_ob, "TestGatewayDetachedSessionFifoBytesRemainAggregatelyBoundedFirst");
  ASSERT_EQ(gateway_destroy_session_internal(second_session_id, "test_done", "done"), 1);
  destruct_object(second_ob);
  free_object(&second_ob, "TestGatewayDetachedSessionFifoBytesRemainAggregatelyBoundedSecond");
  EXPECT_EQ(gateway_session_fifo_wire_detached_bytes(), 0u);
}

TEST_F(DriverTest, TestGatewayProjectedWireBatchRejectsAggregateBudgetAtomically) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() {
      g_gateway_max_packet_size = original;
    }
  } packet_size_guard;
  static std::vector<std::string> writes;
  writes.clear();
  auto writer = [](int, const char* data, size_t len) -> int {
    writes.emplace_back(data, len);
    return 1;
  };

  g_gateway_max_packet_size = 1024;
  constexpr int master_fd = 1713;
  const char* prefill_session_id = "gw-test-aggregate-batch-prefill";
  const char* first_session_id = "gw-test-aggregate-batch-first";
  const char* second_session_id = "gw-test-aggregate-batch-second";
  auto* prefill_ob = create_gateway_session_for_test(prefill_session_id,
                                                     "/clone/gateway_login_example", master_fd);
  ASSERT_NE(prefill_ob, nullptr);
  add_ref(prefill_ob, "TestGatewayProjectedWireBatchRejectsAggregateBudgetPrefill");
  auto* first_ob =
      create_gateway_session_for_test(first_session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(first_ob, nullptr);
  add_ref(first_ob, "TestGatewayProjectedWireBatchRejectsAggregateBudgetFirst");
  auto* second_ob =
      create_gateway_session_for_test(second_session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(second_ob, nullptr);
  add_ref(second_ob, "TestGatewayProjectedWireBatchRejectsAggregateBudgetSecond");
  auto* prefill = gateway_find_session(prefill_session_id);
  auto* first = gateway_find_session(first_session_id);
  auto* second = gateway_find_session(second_session_id);
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
  while (prefill->output_fifo_wire_bytes + filler_wire.size() <= target_prefill) {
    ASSERT_EQ(gateway_enqueue_session_protocol_output(prefill, filler_payload.data(),
                                                      filler_payload.size()),
              1);
  }
  const auto minimum_wire = gateway_encode_output_envelope_for_test(prefill_session_id, "", 0);
  const auto padding_wire_bytes = target_prefill - prefill->output_fifo_wire_bytes;
  if (padding_wire_bytes >= minimum_wire.size()) {
    const std::string padding_payload(padding_wire_bytes - minimum_wire.size(), 'p');
    ASSERT_EQ(gateway_enqueue_session_protocol_output(prefill, padding_payload.data(),
                                                      padding_payload.size()),
              1);
  }
  const auto remaining = aggregate_limit - prefill->output_fifo_wire_bytes;
  ASSERT_GT(remaining, largest_batch_wire);
  ASSERT_LT(remaining, batch_wire_bytes);

  const auto aggregate_rejected_before =
      g_gateway_runtime_counters.output_fifo_aggregate_wire_bytes_rejected.load(
          std::memory_order_relaxed);
  ASSERT_FALSE(gateway_fill_projected_wires_for_test(
      {first, second}, {first_reservation, second_reservation}, {first_wire, second_wire}, writer));
  EXPECT_TRUE(writes.empty());
  EXPECT_FALSE(first->output_fifo.front().ready);
  EXPECT_TRUE(first->output_fifo.front().wire_bytes.empty());
  EXPECT_FALSE(second->output_fifo.front().ready);
  EXPECT_TRUE(second->output_fifo.front().wire_bytes.empty());
  EXPECT_EQ(first->output_fifo_wire_bytes, 0u);
  EXPECT_EQ(second->output_fifo_wire_bytes, 0u);
  EXPECT_EQ(g_gateway_runtime_counters.output_fifo_aggregate_wire_bytes_rejected.load(
                std::memory_order_relaxed),
            aggregate_rejected_before + 1);

  ASSERT_EQ(gateway_destroy_session_internal(prefill_session_id, "test_done", "done"), 1);
  destruct_object(prefill_ob);
  free_object(&prefill_ob, "TestGatewayProjectedWireBatchRejectsAggregateBudgetPrefill");
  ASSERT_EQ(gateway_destroy_session_internal(first_session_id, "test_done", "done"), 1);
  destruct_object(first_ob);
  free_object(&first_ob, "TestGatewayProjectedWireBatchRejectsAggregateBudgetFirst");
  ASSERT_EQ(gateway_destroy_session_internal(second_session_id, "test_done", "done"), 1);
  destruct_object(second_ob);
  free_object(&second_ob, "TestGatewayProjectedWireBatchRejectsAggregateBudgetSecond");
}

TEST_F(DriverTest, TestGatewayMasterWriteCallbackRetriesBackpressuredSessionFifo) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() {
      g_gateway_max_packet_size = original;
    }
  } packet_size_guard;

  g_gateway_max_packet_size = 1024;
  constexpr int master_fd = 1709;
  const char* session_id = "gw-test-write-backpressure-retry";
  bufferevent* pair[2] = {nullptr, nullptr};
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  ASSERT_NE(gateway_register_master_for_test(master_fd, pair[0]), nullptr);
  auto* ob = create_gateway_session_for_test(session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayMasterWriteCallbackRetriesBackpressuredSessionFifo");
  auto* sess = gateway_find_session(session_id);
  ASSERT_NE(sess, nullptr);
  const std::string payload(512, 'x');

  for (int attempt = 0; attempt < 1024 && sess->output_fifo.empty(); ++attempt) {
    ASSERT_EQ(gateway_enqueue_session_protocol_output(sess, payload.data(), payload.size()), 1);
  }
  ASSERT_FALSE(sess->output_fifo.empty());
  ASSERT_TRUE(sess->output_fifo.front().ready);
  ASSERT_LE(evbuffer_get_length(bufferevent_get_output(pair[0])),
            gateway_write_buffer_limit_for_test());

  ASSERT_EQ(bufferevent_enable(pair[0], EV_WRITE), 0);
  ASSERT_EQ(bufferevent_enable(pair[1], EV_READ), 0);
  auto* peer_input = bufferevent_get_input(pair[1]);
  ASSERT_NE(peer_input, nullptr);
  for (int attempt = 0; attempt < 256 && !sess->output_fifo.empty(); ++attempt) {
    event_base_loop(g_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK);
    const auto received = evbuffer_get_length(peer_input);
    if (received > 0) {
      ASSERT_EQ(evbuffer_drain(peer_input, received), 0);
    }
  }
  ASSERT_TRUE(sess->output_fifo.empty());

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayMasterWriteCallbackRetriesBackpressuredSessionFifo");
  gateway_remove_master_for_test(master_fd);
  pair[0] = nullptr;
  bufferevent_free(pair[1]);
}

TEST_F(DriverTest, TestGatewayMasterWriteCallbackDoesNotRescheduleWithoutProgress) {
  struct PacketSizeGuard {
    size_t original{g_gateway_max_packet_size};
    ~PacketSizeGuard() {
      g_gateway_max_packet_size = original;
    }
  } packet_size_guard;

  g_gateway_max_packet_size = 1024;
  constexpr int master_fd = 1710;
  const char* session_id = "gw-test-write-no-progress";
  bufferevent* pair[2] = {nullptr, nullptr};
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  auto* master = gateway_register_master_for_test(master_fd, pair[0]);
  ASSERT_NE(master, nullptr);
  auto* output = bufferevent_get_output(pair[0]);
  ASSERT_NE(output, nullptr);
  const auto output_limit = gateway_write_buffer_limit_for_test();
  ASSERT_GT(output_limit, 32u);
  const std::string filler(output_limit - 32, 'f');
  ASSERT_EQ(evbuffer_add(output, filler.data(), filler.size()), 0);

  auto* ob = create_gateway_session_for_test(session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayMasterWriteCallbackDoesNotRescheduleWithoutProgress");
  auto* sess = gateway_find_session(session_id);
  ASSERT_NE(sess, nullptr);
  const std::string payload(512, 'x');
  ASSERT_EQ(gateway_enqueue_session_protocol_output(sess, payload.data(), payload.size()), 1);
  ASSERT_EQ(sess->output_fifo.size(), 1u);
  ASSERT_TRUE(sess->output_fifo.front().ready);
  ASSERT_FALSE(master->write_flush_scheduled);
  const auto scheduled_before = walltime_event_queue_size_for_test();

  gateway_invoke_master_write_callback_for_test(master);

  ASSERT_EQ(sess->output_fifo.size(), 1u);
  ASSERT_FALSE(master->write_flush_scheduled);
  ASSERT_EQ(walltime_event_queue_size_for_test(), scheduled_before);

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayMasterWriteCallbackDoesNotRescheduleWithoutProgress");
  gateway_remove_master_for_test(master_fd);
  pair[0] = nullptr;
  bufferevent_free(pair[1]);
}

TEST_F(DriverTest, TestGatewayMasterOutputContinuationCountsOverBudget) {
  constexpr int master_fd = 1711;
  bufferevent* pair[2] = {nullptr, nullptr};
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  auto* master = gateway_register_master_for_test(master_fd, pair[0]);
  ASSERT_NE(master, nullptr);

  auto before_remaining = g_gateway_runtime_counters.master_output_ready_remaining_total.load(
      std::memory_order_relaxed);
  auto before_continuations =
      g_gateway_runtime_counters.master_output_continuation_needed_total.load(
          std::memory_order_relaxed);

  // Create budget + N ready sessions on the same master. FIFO entries are
  // filled through a failing writer so they stay queued until the explicit
  // master flush below (no automatic enqueue-time flush).
  const size_t budget = 8;
  const size_t extra = 4;
  auto failing_writer = [](int /*fd*/, const char* /*data*/, size_t /*len*/) -> int {
    return 0;
  };
  std::vector<std::string> session_ids;
  std::vector<object_t*> objects;
  std::vector<GatewaySession*> sessions;
  for (size_t i = 0; i < budget + extra; i++) {
    auto id = "gw-test-continuation-" + std::to_string(i);
    auto* ob =
        create_gateway_session_for_test(id.c_str(), "/clone/gateway_login_example", master_fd);
    ASSERT_NE(ob, nullptr);
    add_ref(ob, "TestGatewayMasterOutputContinuationCountsOverBudget");
    auto* sess = gateway_find_session(id.c_str());
    ASSERT_NE(sess, nullptr);
    auto reservation = gateway_reserve_session_output(sess);
    ASSERT_GT(reservation, 0u);
    ASSERT_EQ(gateway_fill_session_protocol_output_with_writer(sess, reservation, "ping", 4,
                                                               failing_writer),
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
  ASSERT_EQ(g_gateway_runtime_counters.master_output_ready_remaining_total.load(
                std::memory_order_relaxed),
            before_remaining + extra);
  ASSERT_EQ(g_gateway_runtime_counters.master_output_continuation_needed_total.load(
                std::memory_order_relaxed),
            before_continuations + 1);

  // Exactly `extra` sessions still carry their FIFO entries (registry
  // traversal order is not insertion order).
  size_t remaining_fifos = 0;
  for (auto* sess : sessions) {
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
  for (auto* sess : sessions) {
    ASSERT_TRUE(sess->output_fifo.empty());
  }
  ASSERT_EQ(g_gateway_runtime_counters.master_output_continuation_needed_total.load(
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

TEST_F(DriverTest, TestGatewayMasterOutputContinuationScheduleSemantics) {
  constexpr int master_fd = 1713;
  bufferevent* pair[2] = {nullptr, nullptr};
  ASSERT_EQ(bufferevent_pair_new(g_event_base, BEV_OPT_CLOSE_ON_FREE, pair), 0);
  ASSERT_NE(pair[0], nullptr);
  ASSERT_NE(pair[1], nullptr);
  auto* master = gateway_register_master_for_test(master_fd, pair[0]);
  ASSERT_NE(master, nullptr);

  auto load = [](const std::atomic<uint64_t>& counter) -> uint64_t {
    return counter.load(std::memory_order_relaxed);
  };
  const auto b_attempts =
      load(g_gateway_runtime_counters.master_output_continuation_schedule_attempts_total);
  const auto b_scheduled =
      load(g_gateway_runtime_counters.master_output_continuation_scheduled_total);
  const auto b_coalesced =
      load(g_gateway_runtime_counters.master_output_continuation_coalesced_requests_total);
  const auto b_callbacks =
      load(g_gateway_runtime_counters.master_output_continuation_callbacks_executed_total);

  // One ready session with a pending FIFO entry (writer fails so the entry
  // stays queued).
  auto failing_writer = [](int /*fd*/, const char* /*data*/, size_t /*len*/) -> int {
    return 0;
  };
  const char* session_id = "gw-test-schedule-semantics";
  auto* ob = create_gateway_session_for_test(session_id, "/clone/gateway_login_example", master_fd);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayMasterOutputContinuationScheduleSemantics");
  auto* sess = gateway_find_session(session_id);
  ASSERT_NE(sess, nullptr);
  auto reservation = gateway_reserve_session_output(sess);
  ASSERT_GT(reservation, 0u);
  ASSERT_EQ(gateway_fill_session_protocol_output_with_writer(sess, reservation, "ping", 4,
                                                             failing_writer),
            1);
  ASSERT_FALSE(sess->output_fifo.empty());
  ASSERT_TRUE(sess->output_fifo.front().ready);

  // First request: one attempt, one successful schedule, no coalesce.
  gateway_schedule_write_flush_for_test(master_fd);
  ASSERT_EQ(load(g_gateway_runtime_counters.master_output_continuation_schedule_attempts_total),
            b_attempts + 1);
  ASSERT_EQ(load(g_gateway_runtime_counters.master_output_continuation_scheduled_total),
            b_scheduled + 1);
  ASSERT_EQ(load(g_gateway_runtime_counters.master_output_continuation_coalesced_requests_total),
            b_coalesced);

  // Second request while a continuation is scheduled: coalesced, no new
  // attempt, no new schedule.
  gateway_schedule_write_flush_for_test(master_fd);
  ASSERT_EQ(load(g_gateway_runtime_counters.master_output_continuation_schedule_attempts_total),
            b_attempts + 1);
  ASSERT_EQ(load(g_gateway_runtime_counters.master_output_continuation_scheduled_total),
            b_scheduled + 1);
  ASSERT_EQ(load(g_gateway_runtime_counters.master_output_continuation_coalesced_requests_total),
            b_coalesced + 1);

  // Let the 1ms continuation timer fire: the callback counter moves exactly
  // once (and the scheduled flush runs its scan). Drive the event loop until
  // the callback runs (bounded), then assert the deltas.
  const auto b_runs = load(g_gateway_runtime_counters.master_output_scan_runs_total);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
  while (load(g_gateway_runtime_counters.master_output_continuation_callbacks_executed_total) <
             b_callbacks + 1 &&
         std::chrono::steady_clock::now() < deadline) {
    event_base_loop(g_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_GE(load(g_gateway_runtime_counters.master_output_continuation_callbacks_executed_total),
            b_callbacks + 1);
  ASSERT_GE(load(g_gateway_runtime_counters.master_output_scan_runs_total), b_runs + 1);

  ASSERT_EQ(gateway_destroy_session_internal(session_id, "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayMasterOutputContinuationScheduleSemantics");
  gateway_remove_master_for_test(master_fd);
  pair[0] = nullptr;
  bufferevent_free(pair[1]);
}

TEST_F(DriverTest, TestGatewayProbeSuppressionIsSessionScopedAndOneShot) {
  auto* ob =
      create_gateway_session_for_test("gw-test-probe-suppress", "/clone/gateway_login_example", 89);
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
  constexpr const char* kSessionId = "gw-test-inject-input-efun";
  auto* ob = create_gateway_session_for_test(kSessionId, "/clone/gateway_login_example", 90);
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  add_ref(ob, "TestGatewayInjectInputEfunPreservesArgumentLifetimeAndStack");

  const std::string input(128, 'x');
  copy_and_push_string(input.c_str());
  auto* result = call_lpc_method(ob, "inject_gateway_input_via_efun", 1);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_NUMBER);
  ASSERT_EQ(result->u.number, 1);

  const auto expected = input + "-efun\n";
  ASSERT_EQ(ob->interactive->text_end, static_cast<int>(expected.size()));
  ASSERT_EQ(std::string(ob->interactive->text, static_cast<size_t>(ob->interactive->text_end)),
            expected);

  ASSERT_EQ(gateway_destroy_session_internal(kSessionId, "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayInjectInputEfunPreservesArgumentLifetimeAndStack");
}

TEST_F(DriverTest, TestGatewayCoreEfunsPreserveReturnStackContracts) {
  constexpr const char* kSessionId = "gw-test-core-efun-stack-contract";
  auto* ob = create_gateway_session_for_test(kSessionId, "/clone/gateway_login_example", 91);
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(ob->interactive, nullptr);
  add_ref(ob, "TestGatewayCoreEfunsPreserveReturnStackContracts");

  auto* pending = call_lpc_method(ob, "query_gateway_pending_counters_via_efun");
  ASSERT_NE(pending, nullptr);
  ASSERT_EQ(pending->type, T_NUMBER);
  ASSERT_GE(pending->u.number, 0);

  auto* listen = call_lpc_method(ob, "listen_gateway_via_efun");
  ASSERT_NE(listen, nullptr);
  ASSERT_EQ(listen->type, T_NUMBER);
  ASSERT_EQ(listen->u.number, 0);

  auto* empty_session = call_lpc_method(ob, "create_empty_gateway_session_via_efun");
  ASSERT_NE(empty_session, nullptr);
  ASSERT_EQ(empty_session->type, T_NUMBER);
  ASSERT_EQ(empty_session->u.number, 0);

  auto* set_heartbeat = call_lpc_method(ob, "set_gateway_heartbeat_via_efun");
  ASSERT_NE(set_heartbeat, nullptr);
  ASSERT_EQ(set_heartbeat->type, T_NUMBER);
  ASSERT_EQ(set_heartbeat->u.number, 1);

  auto* check_timeout = call_lpc_method(ob, "check_gateway_timeout_via_efun");
  ASSERT_NE(check_timeout, nullptr);
  ASSERT_EQ(check_timeout->type, T_NUMBER);
  ASSERT_EQ(check_timeout->u.number, 1);

  auto* non_session = clone_object_for_test("clone/gateway_login_example");
  ASSERT_NE(non_session, nullptr);
  auto* info = call_lpc_method(non_session, "gateway_session_info_non_session_via_efun");
  ASSERT_NE(info, nullptr);
  ASSERT_EQ(info->type, T_NUMBER);
  ASSERT_EQ(info->u.number, 0);
  destruct_object_for_test(non_session);

  ASSERT_EQ(gateway_destroy_session_internal(kSessionId, "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayCoreEfunsPreserveReturnStackContracts");
}

TEST_F(DriverTest, TestGatewayOutputReservationPublicWrappersAreMainOnly) {
  auto* ob = create_gateway_session_for_test("gw-test-reservation-main-only",
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
    off_main_fill.store(gateway_fill_session_output_for_object(ob, reservation_id, "stale", 5),
                        std::memory_order_release);
    off_main_release.store(gateway_release_session_output_for_object(ob, reservation_id),
                           std::memory_order_release);
  });
  worker.join();

  ASSERT_EQ(off_main_reservation.load(std::memory_order_acquire), 0u);
  ASSERT_EQ(off_main_fill.load(std::memory_order_acquire), 0);
  ASSERT_EQ(off_main_release.load(std::memory_order_acquire), 0);
  ASSERT_EQ(gateway_release_session_output_for_object(ob, reservation_id), 1);
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-reservation-main-only", "test_done", "done"),
            1);

  destruct_object(ob);
  free_object(&ob, "TestGatewayOutputReservationPublicWrappersAreMainOnly");
}

TEST_F(DriverTest, TestGatewayOutputReservationRejectsStaleTokenAfterSessionDestroy) {
  auto* ob = create_gateway_session_for_test("gw-test-reservation-stale",
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
  auto* ob = create_gateway_session_for_test("gw-test-future-watch-pending",
                                             "/clone/gateway_login_example", 93);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayFutureWatchKeepsPendingFutureAndReservation");
  vm_owner_set_id(ob, "owner/test/gateway/future-pending");

  auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  push_number(41);
  auto* submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_GT(future_id, 0);

  push_number(static_cast<LPC_INT>(reservation_id));
  push_number(future_id);
  push_number(1000);
  auto* watched = call_lpc_method(ob, "watch_gateway_owner_future", 3);
  ASSERT_NE(watched, nullptr);
  ASSERT_EQ(watched->type, T_NUMBER);
  ASSERT_EQ(watched->u.number, 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 1);
  ASSERT_EQ(gateway_process_session_future_watches_at(0), 0);
  ASSERT_EQ(gateway_session_future_watch_count(), 1);

  auto* pending = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_STREQ(gateway_test_mapping_string(pending, "state"), "pending");
  free_mapping(pending);
  auto* session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  ASSERT_EQ(session->output_fifo.size(), 1u);
  ASSERT_FALSE(session->output_fifo.front().ready);

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-future-watch-pending", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_cancelled_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_cancelled_future_id", ob), future_id);
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_future_cancelled_reason", ob),
               "gateway session destroyed");
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_cancelled_callback_off_main", ob), 0);
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_cancelled_release_result", ob), 1);
  auto* consumed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
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
  auto* before_owner = vm_owner_thread_status();
  auto before_owner_queue_samples =
      gateway_test_mapping_number(before_owner, "owner_async_queue_wait_samples");
  auto before_owner_execute_samples =
      gateway_test_mapping_number(before_owner, "owner_async_lpc_execute_samples");
  auto before_owner_completion_samples =
      gateway_test_mapping_number(before_owner, "owner_async_result_completion_samples");
  free_mapping(before_owner);
  auto* before_gateway = gateway_status_internal();
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
  auto before_completion_notifications =
      gateway_test_mapping_number(before_gateway, "gateway_future_watch_completion_notifications");
  auto before_completion_wakeups =
      gateway_test_mapping_number(before_gateway, "gateway_future_watch_completion_wakeups");
  free_mapping(before_gateway);

  auto* ob = create_gateway_session_for_test("gw-test-future-watch-completed",
                                             "/clone/gateway_login_example", 94);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayFutureWatchDispatchesCompletedFutureOnMain");
  vm_owner_set_id(ob, "owner/test/gateway/future-completed");

  auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  push_number(41);
  auto* submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_GT(future_id, 0);
  ASSERT_EQ(gateway_watch_session_future_for_object(ob, reservation_id,
                                                    static_cast<uint64_t>(future_id), 1000),
            1);

  vm_owner_thread_start(1);
  for (int i = 0; i < 200; i++) {
    auto* future = vm_owner_future_poll(static_cast<uint64_t>(future_id));
    auto completed = std::string(gateway_test_mapping_string(future, "state")) == "completed";
    free_mapping(future);
    if (completed)
      break;
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
  auto* future = call_lpc_method(ob, "query_last_owner_future");
  ASSERT_NE(future, nullptr);
  ASSERT_EQ(future->type, T_MAPPING);
  ASSERT_STREQ(gateway_test_mapping_string(future->u.map, "state"), "completed");
  ASSERT_GT(gateway_test_mapping_number(future->u.map, "terminal_at_ns"), 0);
  auto* result = find_string_in_mapping(future->u.map, "result");
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_MAPPING);
  ASSERT_EQ(gateway_test_mapping_number(result->u.map, "value"), 42);
  auto* consumed = vm_owner_future_poll(static_cast<uint64_t>(future_id));
  ASSERT_STREQ(gateway_test_mapping_string(consumed, "state"), "unknown");
  free_mapping(consumed);

  auto* after_owner = vm_owner_thread_status();
  ASSERT_GE(gateway_test_mapping_number(after_owner, "owner_async_queue_wait_samples"),
            before_owner_queue_samples + 1);
  ASSERT_GE(gateway_test_mapping_number(after_owner, "owner_async_lpc_execute_samples"),
            before_owner_execute_samples + 1);
  ASSERT_GE(gateway_test_mapping_number(after_owner, "owner_async_result_completion_samples"),
            before_owner_completion_samples + 1);
  auto after_owner_metrics = owner_runtime_metrics_instance().snapshot();
  ASSERT_TRUE(after_owner_metrics.owner_async_lpc_execute_thread_cpu_ns_total >
                  before_owner_metrics.owner_async_lpc_execute_thread_cpu_ns_total ||
              after_owner_metrics.owner_async_lpc_execute_thread_cpu_unavailable >
                  before_owner_metrics.owner_async_lpc_execute_thread_cpu_unavailable);
  ASSERT_TRUE(after_owner_metrics.owner_async_result_completion_thread_cpu_ns_total >
                  before_owner_metrics.owner_async_result_completion_thread_cpu_ns_total ||
              after_owner_metrics.owner_async_result_completion_thread_cpu_unavailable >
                  before_owner_metrics.owner_async_result_completion_thread_cpu_unavailable);
  free_mapping(after_owner);
  auto* after_gateway = gateway_status_internal();
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
  auto after_completion_notifications =
      gateway_test_mapping_number(after_gateway, "gateway_future_watch_completion_notifications");
  ASSERT_GE(after_completion_notifications, before_completion_notifications + 1);
  ASSERT_GE(gateway_test_mapping_number(after_gateway, "gateway_future_watch_completion_wakeups"),
            before_completion_wakeups + 1);
  ASSERT_TRUE(g_gateway_runtime_counters.future_watch_main_completion_thread_cpu_ns_total.load(
                  std::memory_order_relaxed) > before_gateway_completion_cpu_total ||
              g_gateway_runtime_counters.future_watch_main_completion_thread_cpu_unavailable.load(
                  std::memory_order_relaxed) > before_gateway_completion_cpu_unavailable);
  free_mapping(after_gateway);

  push_number(99);
  auto* unwatched = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(unwatched, nullptr);
  ASSERT_EQ(unwatched->type, T_MAPPING);
  auto unwatched_future_id = gateway_test_mapping_number(unwatched->u.map, "future_id");
  ASSERT_GT(unwatched_future_id, 0);
  vm_owner_thread_start(1);
  for (int i = 0; i < 200; i++) {
    auto* polled = vm_owner_future_poll(static_cast<uint64_t>(unwatched_future_id));
    auto completed = std::string(gateway_test_mapping_string(polled, "state")) == "completed";
    free_mapping(polled);
    if (completed)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  vm_owner_thread_stop();
  auto* after_unwatched = gateway_status_internal();
  ASSERT_EQ(
      gateway_test_mapping_number(after_unwatched, "gateway_future_watch_completion_notifications"),
      after_completion_notifications);
  free_mapping(after_unwatched);
  auto* unwatched_consumed = vm_owner_future_take(static_cast<uint64_t>(unwatched_future_id));
  ASSERT_STREQ(gateway_test_mapping_string(unwatched_consumed, "state"), "completed");
  free_mapping(unwatched_consumed);

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-future-watch-completed", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayFutureWatchDispatchesCompletedFutureOnMain");
}

TEST_F(DriverTest, TestGatewayEncodedOutputFutureFillsReservationWithoutLpcCallback) {
  auto* ob = create_gateway_session_for_test("gw-test-future-output-completed",
                                             "/clone/gateway_login_example", 96);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayEncodedOutputFutureFillsReservationWithoutLpcCallback");
  vm_owner_set_id(ob, "owner/test/gateway/future-output-completed");

  auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  copy_and_push_string("native-frame-payload");
  auto* submitted = call_lpc_method(ob, "submit_gateway_owner_frame_string", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_GT(future_id, 0);
  push_number(static_cast<LPC_INT>(reservation_id));
  push_number(future_id);
  push_number(1000);
  auto* watched = call_lpc_method(ob, "watch_gateway_owner_future_output", 3);
  ASSERT_NE(watched, nullptr);
  ASSERT_EQ(watched->type, T_NUMBER);
  ASSERT_EQ(watched->u.number, 1);

  vm_owner_thread_start(1);
  for (int i = 0; i < 200; i++) {
    auto state = vm_owner_future_state(static_cast<uint64_t>(future_id));
    if (state == VM_OWNER_FUTURE_COMPLETED || state == VM_OWNER_FUTURE_FAILED)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  vm_owner_thread_stop();
  ASSERT_EQ(vm_owner_future_state(static_cast<uint64_t>(future_id)), VM_OWNER_FUTURE_COMPLETED);
  std::atomic<int> off_main_take_found{1};
  std::thread off_main_take([&] {
    VMContext worker_context;
    VMContextThreadScope scope(worker_context);
    auto taken = vm_owner_future_take_string(static_cast<uint64_t>(future_id));
    off_main_take_found.store(taken.found ? 1 : 0, std::memory_order_release);
  });
  off_main_take.join();
  ASSERT_EQ(off_main_take_found.load(std::memory_order_acquire), 0);
  ASSERT_EQ(vm_owner_future_state(static_cast<uint64_t>(future_id)), VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(gateway_process_session_future_watches_at(std::numeric_limits<uint64_t>::max()), 1);

  auto* session = gateway_find_session_by_object(ob);
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
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_future_output_state", ob), "completed");
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_output_callback_off_main", ob), 0);
  ASSERT_EQ(vm_owner_future_state(static_cast<uint64_t>(future_id)), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);

  ASSERT_EQ(
      gateway_destroy_session_internal("gw-test-future-output-completed", "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayEncodedOutputFutureFillsReservationWithoutLpcCallback");
}

TEST_F(DriverTest, TestGatewayEncodedOutputFutureReleasesNonStringResult) {
  auto* ob = create_gateway_session_for_test("gw-test-future-output-non-string",
                                             "/clone/gateway_login_example", 97);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayEncodedOutputFutureReleasesNonStringResult");
  vm_owner_set_id(ob, "owner/test/gateway/future-output-non-string");

  auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  push_number(41);
  auto* submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_EQ(gateway_watch_session_future_output_for_object(ob, reservation_id,
                                                           static_cast<uint64_t>(future_id), 1000),
            1);
  vm_owner_thread_start(1);
  for (int i = 0; i < 200; i++) {
    auto state = vm_owner_future_state(static_cast<uint64_t>(future_id));
    if (state == VM_OWNER_FUTURE_COMPLETED || state == VM_OWNER_FUTURE_FAILED)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  vm_owner_thread_stop();
  ASSERT_EQ(gateway_process_session_future_watches_at(std::numeric_limits<uint64_t>::max()), 1);
  auto* session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->output_fifo.empty());
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_output_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_future_output_state", ob), "released");
  ASSERT_EQ(vm_owner_future_state(static_cast<uint64_t>(future_id)), VM_OWNER_FUTURE_UNKNOWN);

  ASSERT_EQ(
      gateway_destroy_session_internal("gw-test-future-output-non-string", "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayEncodedOutputFutureReleasesNonStringResult");
}

TEST_F(DriverTest, TestGatewayEncodedOutputFutureTimeoutReleasesReservation) {
  auto* ob = create_gateway_session_for_test("gw-test-future-output-timeout",
                                             "/clone/gateway_login_example", 98);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayEncodedOutputFutureTimeoutReleasesReservation");
  vm_owner_set_id(ob, "owner/test/gateway/future-output-timeout");

  auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  copy_and_push_string("never-completed-frame");
  auto* submitted = call_lpc_method(ob, "submit_gateway_owner_frame_string", 1);
  auto future_id = gateway_test_mapping_number(submitted->u.map, "future_id");
  ASSERT_EQ(gateway_watch_session_future_output_for_object(ob, reservation_id,
                                                           static_cast<uint64_t>(future_id), 1),
            1);
  ASSERT_EQ(gateway_process_session_future_watches_at(std::numeric_limits<uint64_t>::max()), 1);
  auto* session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->output_fifo.empty());
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_output_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_future_output_state", ob), "released");
  ASSERT_EQ(vm_owner_future_state(static_cast<uint64_t>(future_id)), VM_OWNER_FUTURE_UNKNOWN);

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-future-output-timeout", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayEncodedOutputFutureTimeoutReleasesReservation");
}

TEST_F(DriverTest, TestVmOwnerNativeStringFuturePollReportsFrozenResult) {
  auto* ob = create_gateway_session_for_test("gw-test-native-string-frozen-result",
                                             "/clone/gateway_login_example", 110);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestVmOwnerNativeStringFuturePollReportsFrozenResult");
  vm_owner_set_id(ob, "owner/test/gateway/native-string-frozen-result");

  std::atomic<int> projector_runs{0};
  vm_owner_thread_start(1);
  const auto submission = vm_owner_submit_frozen_string_task(
      ob, "room_output_projection", "unit-native-string-frozen-result",
      [&projector_runs](std::string* output) {
        projector_runs.fetch_add(1, std::memory_order_relaxed);
        *output = "native-string-result";
        return true;
      });
  ASSERT_TRUE(submission.queued);
  ASSERT_GT(submission.future_id, 0u);
  for (int index = 0;
       index < 200 && vm_owner_future_state(submission.future_id) == VM_OWNER_FUTURE_PENDING;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(vm_owner_future_state(submission.future_id), VM_OWNER_FUTURE_COMPLETED);

  auto* polled = vm_owner_future_poll(submission.future_id);
  ASSERT_EQ(gateway_test_mapping_number(polled, "frozen_result"), 1);
  free_mapping(polled);
  const auto taken = vm_owner_future_take_string(submission.future_id);
  ASSERT_TRUE(taken.found);
  ASSERT_TRUE(taken.consumed);
  ASSERT_TRUE(taken.string_result);
  ASSERT_EQ(taken.value, "native-string-result");
  ASSERT_EQ(projector_runs.load(std::memory_order_relaxed), 1);

  vm_owner_thread_stop();
  ASSERT_EQ(
      gateway_destroy_session_internal("gw-test-native-string-frozen-result", "test_done", "done"),
      1);
  destruct_object(ob);
  free_object(&ob, "TestVmOwnerNativeStringFuturePollReportsFrozenResult");
}

TEST_F(DriverTest, TestVmOwnerRepeatedTimeoutReportsTerminalChangeOnce) {
  const auto future_id = vm_owner_register_compute_future(
      "owner/test/future-terminal-change", 900001, "unit-terminal-change", "unit-terminal-change");
  ASSERT_GT(future_id, 0u);

  auto* first = vm_owner_future_timeout(future_id, "unit timeout");
  ASSERT_STREQ(gateway_test_mapping_string(first, "state"), "failed");
  ASSERT_EQ(gateway_test_mapping_number(first, "terminal_changed"), 1);
  free_mapping(first);

  auto* repeated = vm_owner_future_timeout(future_id, "repeated unit timeout");
  ASSERT_STREQ(gateway_test_mapping_string(repeated, "state"), "failed");
  ASSERT_EQ(gateway_test_mapping_number(repeated, "terminal_changed"), 0);
  free_mapping(repeated);

  auto* taken = vm_owner_future_take(future_id);
  ASSERT_EQ(gateway_test_mapping_number(taken, "consumed"), 1);
  free_mapping(taken);
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_UNKNOWN);
}

TEST_F(DriverTest, TestVmOwnerQueuedFrozenStringCancellationRemovesTaskBeforeProjection) {
  auto* ob = create_gateway_session_for_test("gw-test-queued-string-cancel",
                                             "/clone/gateway_login_example", 109);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestVmOwnerQueuedFrozenStringCancellationRemovesTaskBeforeProjection");
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
  for (int index = 0; index < 200 && blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  auto* before_cancel = vm_owner_thread_status();
  const auto callback_cancelled_before =
      gateway_test_mapping_number(before_cancel, "executor_callback_cancelled_before_dispatch");
  const auto owner_message_cancelled_before =
      gateway_test_mapping_number(before_cancel, "owner_message_cancelled_before_dispatch");
  const auto callback_dropped_before =
      gateway_test_mapping_number(before_cancel, "executor_callback_dropped");
  free_mapping(before_cancel);

  const auto submission = vm_owner_submit_frozen_string_task(
      ob, "room_output_projection", "unit-queued-cancel-projector",
      [&projector_runs](std::string* output) {
        projector_runs.fetch_add(1, std::memory_order_relaxed);
        *output = "must-not-run";
        return true;
      });
  ASSERT_TRUE(submission.queued);
  ASSERT_EQ(vm_owner_future_state(submission.future_id), VM_OWNER_FUTURE_PENDING);

  auto* cancelled =
      vm_owner_future_cancel_queued_task(submission.future_id, "unit queued cancellation");
  ASSERT_STREQ(gateway_test_mapping_string(cancelled, "state"), "failed");
  ASSERT_EQ(gateway_test_mapping_number(cancelled, "terminal_changed"), 1);
  ASSERT_EQ(gateway_test_mapping_number(cancelled, "queued_task_removed"), 1);
  ASSERT_STREQ(gateway_test_mapping_string(cancelled, "queued_task_kind"), "executor_callback");
  free_mapping(cancelled);
  auto* taken = vm_owner_future_take(submission.future_id);
  ASSERT_EQ(gateway_test_mapping_number(taken, "consumed"), 1);
  free_mapping(taken);

  release_blocker.store(1, std::memory_order_release);
  vm_owner_thread_stop();
  ASSERT_EQ(projector_runs.load(std::memory_order_relaxed), 0);
  ASSERT_EQ(vm_owner_future_state(submission.future_id), VM_OWNER_FUTURE_UNKNOWN);
  auto* status = vm_owner_thread_status();
  ASSERT_EQ(gateway_test_mapping_number(status, "queue_depth"), 0);
  ASSERT_EQ(gateway_test_mapping_number(status, "executor_callback_cancelled_before_dispatch"),
            callback_cancelled_before + 1);
  ASSERT_EQ(gateway_test_mapping_number(status, "owner_message_cancelled_before_dispatch"),
            owner_message_cancelled_before);
  ASSERT_EQ(gateway_test_mapping_number(status, "executor_callback_dropped"),
            callback_dropped_before + 1);
  free_mapping(status);

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-queued-string-cancel", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(&ob, "TestVmOwnerQueuedFrozenStringCancellationRemovesTaskBeforeProjection");
}

TEST_F(DriverTest, TestVmOwnerQueuedMessageCancellationDoesNotCountAsCallbackDrop) {
  auto* blocker = create_gateway_session_for_test("gw-test-queued-message-blocker",
                                                  "/clone/gateway_login_example", 130);
  auto* target = create_gateway_session_for_test("gw-test-queued-message-target",
                                                 "/clone/gateway_login_example", 131);
  ASSERT_NE(blocker, nullptr);
  ASSERT_NE(target, nullptr);
  add_ref(blocker, "TestVmOwnerQueuedMessageCancellationDoesNotCountAsCallbackDrop");
  add_ref(target, "TestVmOwnerQueuedMessageCancellationDoesNotCountAsCallbackDrop");
  vm_owner_set_id(blocker, "owner/test/gateway/queued-message-blocker");
  vm_owner_set_id(target, "owner/test/gateway/queued-message-target");

  std::atomic<int> blocker_started{0};
  std::atomic<int> release_blocker{0};
  vm_owner_thread_start(1);
  struct OwnerThreadGuard {
    std::atomic<int>& release;
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
                blocker, "room_output_projection", "unit-queued-message-blocker",
                [&] {
                  blocker_started.store(1, std::memory_order_release);
                  while (release_blocker.load(std::memory_order_acquire) == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
                }),
            0u);
  for (int index = 0; index < 200 && blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  auto* before_cancel = vm_owner_thread_status();
  const auto callback_cancelled_before =
      gateway_test_mapping_number(before_cancel, "executor_callback_cancelled_before_dispatch");
  const auto owner_message_cancelled_before =
      gateway_test_mapping_number(before_cancel, "owner_message_cancelled_before_dispatch");
  const auto callback_dropped_before =
      gateway_test_mapping_number(before_cancel, "executor_callback_dropped");
  free_mapping(before_cancel);

  push_number(41);
  auto* submitted = call_lpc_method(target, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  const auto future_id =
      static_cast<uint64_t>(gateway_test_mapping_number(submitted->u.map, "future_id"));
  ASSERT_GT(future_id, 0u);
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_PENDING);

  auto* cancelled =
      vm_owner_future_cancel_queued_task(future_id, "unit queued owner message cancellation");
  ASSERT_STREQ(gateway_test_mapping_string(cancelled, "state"), "failed");
  ASSERT_EQ(gateway_test_mapping_number(cancelled, "terminal_changed"), 1);
  ASSERT_EQ(gateway_test_mapping_number(cancelled, "queued_task_removed"), 1);
  ASSERT_STREQ(gateway_test_mapping_string(cancelled, "queued_task_kind"), "owner_message");
  free_mapping(cancelled);
  auto* taken = vm_owner_future_take(future_id);
  ASSERT_EQ(gateway_test_mapping_number(taken, "consumed"), 1);
  free_mapping(taken);

  owner_thread_guard.stop();
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_UNKNOWN);
  auto* status = vm_owner_thread_status();
  ASSERT_EQ(gateway_test_mapping_number(status, "executor_callback_cancelled_before_dispatch"),
            callback_cancelled_before);
  ASSERT_EQ(gateway_test_mapping_number(status, "owner_message_cancelled_before_dispatch"),
            owner_message_cancelled_before + 1);
  ASSERT_EQ(gateway_test_mapping_number(status, "executor_callback_dropped"),
            callback_dropped_before);
  free_mapping(status);

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-queued-message-blocker", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-queued-message-target", "test_done", "done"),
            1);
  destruct_object(blocker);
  destruct_object(target);
  free_object(&blocker, "TestVmOwnerQueuedMessageCancellationDoesNotCountAsCallbackDrop");
  free_object(&target, "TestVmOwnerQueuedMessageCancellationDoesNotCountAsCallbackDrop");
}

TEST_F(DriverTest, TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric) {
  auto* blocker = create_gateway_session_for_test("gw-test-quiesce-all-blocker",
                                                  "/clone/gateway_login_example", 132);
  auto* mapping_target = create_gateway_session_for_test("gw-test-quiesce-all-mapping",
                                                         "/clone/gateway_login_example", 133);
  auto* protocol_target = create_gateway_session_for_test("gw-test-quiesce-all-protocol",
                                                          "/clone/gateway_login_example", 134);
  auto* room_target = create_gateway_session_for_test("gw-test-quiesce-all-room",
                                                      "/clone/gateway_login_example", 135);
  auto* generic_target = clone_object_for_test("clone/gateway_login_example");
  ASSERT_NE(blocker, nullptr);
  ASSERT_NE(mapping_target, nullptr);
  ASSERT_NE(protocol_target, nullptr);
  ASSERT_NE(room_target, nullptr);
  ASSERT_NE(generic_target, nullptr);
  add_ref(blocker, "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  add_ref(mapping_target, "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  add_ref(protocol_target, "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  add_ref(room_target, "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  add_ref(generic_target, "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  vm_owner_set_id(blocker, "owner/test/gateway/quiesce-all-blocker");
  vm_owner_set_id(mapping_target, "owner/test/gateway/quiesce-all-mapping");
  vm_owner_set_id(protocol_target, "owner/test/gateway/quiesce-all-protocol");
  vm_owner_set_id(room_target, "owner/test/gateway/quiesce-all-room");
  vm_owner_set_id(generic_target, "owner/test/gateway/quiesce-all-generic");

  auto* mapping_session = gateway_find_session_by_object(mapping_target);
  auto* protocol_session = gateway_find_session_by_object(protocol_target);
  auto* room_session = gateway_find_session_by_object(room_target);
  ASSERT_NE(mapping_session, nullptr);
  ASSERT_NE(protocol_session, nullptr);
  ASSERT_NE(room_session, nullptr);
  const auto mapping_reservation = gateway_reserve_session_output_for_object(mapping_target);
  const auto protocol_reservation = gateway_reserve_session_output_for_object(protocol_target);
  const auto room_reservation = gateway_reserve_session_output_for_object(room_target);
  ASSERT_GT(mapping_reservation, 0u);
  ASSERT_GT(protocol_reservation, 0u);
  ASSERT_GT(room_reservation, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(room_session, room_reservation));

  std::atomic<int> blocker_started{0};
  std::atomic<int> release_blocker{0};
  vm_owner_thread_start(1);
  struct OwnerThreadGuard {
    std::atomic<int>& release;
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
  for (int index = 0; index < 200 && blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  push_number(41);
  auto* mapping_submitted = call_lpc_method(mapping_target, "submit_gateway_owner_future", 1);
  ASSERT_NE(mapping_submitted, nullptr);
  ASSERT_EQ(mapping_submitted->type, T_MAPPING);
  const auto mapping_future =
      static_cast<uint64_t>(gateway_test_mapping_number(mapping_submitted->u.map, "future_id"));
  ASSERT_GT(mapping_future, 0u);
  ASSERT_EQ(gateway_watch_session_future_for_object(mapping_target, mapping_reservation,
                                                    mapping_future, 1000),
            1);

  copy_and_push_string("quiesce-protocol-frame");
  auto* protocol_submitted =
      call_lpc_method(protocol_target, "submit_gateway_owner_frame_string", 1);
  ASSERT_NE(protocol_submitted, nullptr);
  ASSERT_EQ(protocol_submitted->type, T_MAPPING);
  const auto protocol_future =
      static_cast<uint64_t>(gateway_test_mapping_number(protocol_submitted->u.map, "future_id"));
  ASSERT_GT(protocol_future, 0u);
  ASSERT_EQ(gateway_watch_session_future_output_for_object(protocol_target, protocol_reservation,
                                                           protocol_future, 1000),
            1);

  GatewayPendingMessageEventBatchOwnerSubmitResult room_submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {room_target}, {room_reservation}, {"quiesce-room"}, {23}, 1000, &room_submitted));
  ASSERT_EQ(room_submitted.submitted, (std::vector<bool>{true}));
  ASSERT_EQ(room_submitted.future_ids.size(), 1u);
  const auto room_future = room_submitted.future_ids[0];

  push_number(51);
  auto* generic_submitted = call_lpc_method(generic_target, "submit_gateway_owner_future", 1);
  ASSERT_NE(generic_submitted, nullptr);
  ASSERT_EQ(generic_submitted->type, T_MAPPING);
  const auto generic_future =
      static_cast<uint64_t>(gateway_test_mapping_number(generic_submitted->u.map, "future_id"));
  ASSERT_GT(generic_future, 0u);
  ASSERT_EQ(gateway_watch_future_for_object(generic_target, 901, generic_future, 1000), 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 3);
  ASSERT_EQ(gateway_future_watch_count(), 1);

  auto* before = vm_owner_thread_status();
  const auto callback_cancelled_before =
      gateway_test_mapping_number(before, "executor_callback_cancelled_before_dispatch");
  const auto owner_message_cancelled_before =
      gateway_test_mapping_number(before, "owner_message_cancelled_before_dispatch");
  const auto callback_dropped_before =
      gateway_test_mapping_number(before, "executor_callback_dropped");
  free_mapping(before);

  auto* quiesced = gateway_owner_output_quiesce("unit quiesce all watches");
  ASSERT_NE(quiesced, nullptr);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "success"), 1);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "session_watches_before"), 3);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "session_watches_after"), 0);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "session_reservations_before"), 3);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "session_reservations_after"), 0);
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
  ASSERT_EQ(
      gateway_test_call_number("query_last_owner_future_cancelled_reservation_id", mapping_target),
      static_cast<long>(mapping_reservation));
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_cancelled_future_id", mapping_target),
            static_cast<long>(mapping_future));
  ASSERT_EQ(
      gateway_test_call_number("query_last_owner_future_cancelled_release_result", mapping_target),
      1);
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_future_output_state", protocol_target),
               "released");
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_room_output_state", room_target),
               "released");

  auto* after = vm_owner_thread_status();
  ASSERT_EQ(gateway_test_mapping_number(after, "executor_callback_cancelled_before_dispatch"),
            callback_cancelled_before + 1);
  ASSERT_EQ(gateway_test_mapping_number(after, "owner_message_cancelled_before_dispatch"),
            owner_message_cancelled_before + 2);
  ASSERT_EQ(gateway_test_mapping_number(after, "executor_callback_dropped"),
            callback_dropped_before + 1);
  free_mapping(after);

  owner_thread_guard.stop();
  ASSERT_EQ(vm_owner_future_state(generic_future), VM_OWNER_FUTURE_PENDING);
  vm_owner_thread_start(1);
  for (int index = 0;
       index < 200 && vm_owner_future_state(generic_future) == VM_OWNER_FUTURE_PENDING; ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  vm_owner_thread_stop();
  ASSERT_EQ(vm_owner_future_state(generic_future), VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(gateway_process_future_watches_at(std::numeric_limits<uint64_t>::max()), 1);
  ASSERT_EQ(gateway_future_watch_count(), 0);
  ASSERT_EQ(gateway_test_call_number("query_last_generic_owner_future_context_id", generic_target),
            901);
  ASSERT_EQ(vm_owner_future_state(generic_future), VM_OWNER_FUTURE_UNKNOWN);

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-quiesce-all-blocker", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-quiesce-all-mapping", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-quiesce-all-protocol", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-quiesce-all-room", "test_done", "done"), 1);
  destruct_object(blocker);
  destruct_object(mapping_target);
  destruct_object(protocol_target);
  destruct_object(room_target);
  destruct_object(generic_target);
  free_object(&blocker, "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  free_object(&mapping_target,
              "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  free_object(&protocol_target,
              "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  free_object(&room_target, "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
  free_object(&generic_target,
              "TestGatewayOwnerOutputQuiesceClosesAllSessionWatchesButKeepsGeneric");
}

TEST_F(DriverTest, TestGatewayMappingFutureStaleOwnerNotifiesCancellationAndClosesReservation) {
  auto* ob = create_gateway_session_for_test("gw-test-mapping-stale-owner",
                                             "/clone/gateway_login_example", 136);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayMappingFutureStaleOwnerNotifiesCancellationAndClosesReservation");
  vm_owner_set_id(ob, "owner/test/gateway/mapping-stale-a");

  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  push_number(41);
  auto* submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  const auto future_id =
      static_cast<uint64_t>(gateway_test_mapping_number(submitted->u.map, "future_id"));
  ASSERT_GT(future_id, 0u);
  ASSERT_EQ(gateway_watch_session_future_for_object(ob, reservation_id, future_id, 1000), 1);

  vm_owner_set_id(ob, "owner/test/gateway/mapping-stale-b");
  ASSERT_EQ(gateway_process_session_future_watches_at(0), 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_cancelled_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_cancelled_future_id", ob),
            static_cast<long>(future_id));
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_future_cancelled_reason", ob),
               "gateway session stale");
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_cancelled_release_result", ob), 1);
  auto* session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->output_fifo.empty());

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-mapping-stale-owner", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayMappingFutureStaleOwnerNotifiesCancellationAndClosesReservation");
}

TEST_F(DriverTest, TestGatewayMappingFutureCancellationCallbackMayReenterSessionDestroy) {
  constexpr const char* kSessionId = "gw-test-mapping-cancel-reentry";
  auto* ob = create_gateway_session_for_test(kSessionId, "/clone/gateway_login_example", 137);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayMappingFutureCancellationCallbackMayReenterSessionDestroy");
  vm_owner_set_id(ob, "owner/test/gateway/mapping-cancel-reentry");

  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  push_number(41);
  auto* submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  const auto future_id =
      static_cast<uint64_t>(gateway_test_mapping_number(submitted->u.map, "future_id"));
  ASSERT_GT(future_id, 0u);
  ASSERT_EQ(gateway_watch_session_future_for_object(ob, reservation_id, future_id, 1000), 1);

  copy_and_push_string(kSessionId);
  auto* configured = call_lpc_method(ob, "configure_owner_future_cancelled_reentry", 1);
  ASSERT_NE(configured, nullptr);
  auto* quiesced = gateway_owner_output_quiesce("unit mapping cancellation reentry");
  ASSERT_NE(quiesced, nullptr);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "success"), 1);
  free_mapping(quiesced);

  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_cancelled_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_cancelled_future_id", ob),
            static_cast<long>(future_id));
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_cancelled_reentry_result", ob), 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_find_session(kSessionId), nullptr);

  destruct_object(ob);
  free_object(&ob, "TestGatewayMappingFutureCancellationCallbackMayReenterSessionDestroy");
}

TEST_F(DriverTest, TestGatewayMappingFutureReservationStaleStillNotifiesCancellation) {
  auto* ob = create_gateway_session_for_test("gw-test-mapping-reservation-stale",
                                             "/clone/gateway_login_example", 138);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayMappingFutureReservationStaleStillNotifiesCancellation");
  vm_owner_set_id(ob, "owner/test/gateway/mapping-reservation-stale");

  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  push_number(41);
  auto* submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  const auto future_id =
      static_cast<uint64_t>(gateway_test_mapping_number(submitted->u.map, "future_id"));
  ASSERT_GT(future_id, 0u);
  ASSERT_EQ(gateway_watch_session_future_for_object(ob, reservation_id, future_id, 1000), 1);
  ASSERT_EQ(gateway_release_session_output_for_object(ob, reservation_id), 1);

  ASSERT_EQ(gateway_process_session_future_watches_at(0), 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_cancelled_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_future_cancelled_reason", ob),
               "gateway reservation stale");
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_cancelled_release_result", ob), 0);

  ASSERT_EQ(
      gateway_destroy_session_internal("gw-test-mapping-reservation-stale", "test_done", "done"),
      1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayMappingFutureReservationStaleStillNotifiesCancellation");
}

TEST_F(DriverTest, TestGatewayMappingFutureCompletionCallbackFailureRunsCancellationCleanup) {
  auto* ob = create_gateway_session_for_test("gw-test-mapping-callback-failure",
                                             "/clone/gateway_login_example", 139);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayMappingFutureCompletionCallbackFailureRunsCancellationCleanup");
  vm_owner_set_id(ob, "owner/test/gateway/mapping-callback-failure");

  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  push_number(1);
  auto* configured = call_lpc_method(ob, "configure_owner_future_completed_failure", 1);
  ASSERT_NE(configured, nullptr);
  push_number(41);
  auto* submitted = call_lpc_method(ob, "submit_gateway_owner_future", 1);
  ASSERT_NE(submitted, nullptr);
  ASSERT_EQ(submitted->type, T_MAPPING);
  const auto future_id =
      static_cast<uint64_t>(gateway_test_mapping_number(submitted->u.map, "future_id"));
  ASSERT_GT(future_id, 0u);
  ASSERT_EQ(gateway_watch_session_future_for_object(ob, reservation_id, future_id, 1000), 1);

  vm_owner_thread_start(1);
  for (int index = 0; index < 200 && vm_owner_future_state(future_id) == VM_OWNER_FUTURE_PENDING;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  vm_owner_thread_stop();
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(gateway_process_session_future_watches_at(std::numeric_limits<uint64_t>::max()), 1);

  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(vm_owner_future_state(future_id), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_cancelled_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_future_cancelled_reason", ob),
               "gateway owner future callback failed");
  ASSERT_EQ(gateway_test_call_number("query_last_owner_future_cancelled_release_result", ob), 1);
  auto* session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->output_fifo.empty());

  ASSERT_EQ(
      gateway_destroy_session_internal("gw-test-mapping-callback-failure", "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayMappingFutureCompletionCallbackFailureRunsCancellationCleanup");
}

TEST_F(DriverTest, TestGatewayOwnerOutputQuiesceClosesStoppedWorkerWave) {
  auto* blocker = create_gateway_session_for_test("gw-test-owner-quiesce-blocker",
                                                  "/clone/gateway_login_example", 131);
  auto* first = create_gateway_session_for_test("gw-test-owner-quiesce-a",
                                                "/clone/gateway_login_example", 132);
  auto* second = create_gateway_session_for_test("gw-test-owner-quiesce-b",
                                                 "/clone/gateway_login_example", 133);
  ASSERT_NE(blocker, nullptr);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  add_ref(blocker, "TestGatewayOwnerOutputQuiesceClosesStoppedWorkerWave");
  add_ref(first, "TestGatewayOwnerOutputQuiesceClosesStoppedWorkerWave");
  add_ref(second, "TestGatewayOwnerOutputQuiesceClosesStoppedWorkerWave");
  vm_owner_set_id(blocker, "owner/test/gateway/quiesce-blocker");
  vm_owner_set_id(first, "owner/test/gateway/quiesce-a");
  vm_owner_set_id(second, "owner/test/gateway/quiesce-b");
  auto* first_session = gateway_find_session_by_object(first);
  auto* second_session = gateway_find_session_by_object(second);
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
  for (int index = 0; index < 200 && blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {first, second}, {first_reservation, second_reservation},
      {"owner-quiesce-a", "owner-quiesce-b"}, {21, 22}, 1000, &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true, true}));
  ASSERT_EQ(gateway_session_future_watch_count(), 2);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 2);

  std::thread stopper([] {
    vm_owner_thread_stop();
  });
  for (int index = 0; index < 200; ++index) {
    auto* status = vm_owner_thread_status();
    const auto stopping = gateway_test_mapping_number(status, "stopping");
    free_mapping(status);
    if (stopping == 1) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  release_blocker.store(1, std::memory_order_release);
  stopper.join();
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]), VM_OWNER_FUTURE_PENDING);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]), VM_OWNER_FUTURE_PENDING);

  auto* quiesced = call_lpc_method(first, "quiesce_gateway_owner_output");
  ASSERT_NE(quiesced, nullptr);
  ASSERT_EQ(quiesced->type, T_MAPPING);
  ASSERT_EQ(gateway_test_mapping_number(quiesced->u.map, "success"), 1);
  ASSERT_EQ(gateway_test_mapping_number(quiesced->u.map, "room_waves_before"), 1);
  ASSERT_EQ(gateway_test_mapping_number(quiesced->u.map, "room_pending_before"), 2);
  ASSERT_EQ(gateway_test_mapping_number(quiesced->u.map, "room_watches_after"), 0);
  ASSERT_EQ(gateway_test_mapping_number(quiesced->u.map, "room_reservations_after"), 0);
  ASSERT_EQ(gateway_test_mapping_number(quiesced->u.map, "room_pending_after"), 0);
  ASSERT_EQ(gateway_test_mapping_number(quiesced->u.map, "requires_immediate_owner_stop"), 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_TRUE(first_session->output_fifo.empty());
  ASSERT_TRUE(second_session->output_fifo.empty());
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]), VM_OWNER_FUTURE_UNKNOWN);
  auto* status = vm_owner_thread_status();
  ASSERT_EQ(gateway_test_mapping_number(status, "queue_depth"), 0);
  free_mapping(status);

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-owner-quiesce-blocker", "test_done", "done"),
            1);
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-owner-quiesce-a", "test_done", "done"), 1);
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-owner-quiesce-b", "test_done", "done"), 1);
  destruct_object(blocker);
  destruct_object(first);
  destruct_object(second);
  free_object(&blocker, "TestGatewayOwnerOutputQuiesceClosesStoppedWorkerWave");
  free_object(&first, "TestGatewayOwnerOutputQuiesceClosesStoppedWorkerWave");
  free_object(&second, "TestGatewayOwnerOutputQuiesceClosesStoppedWorkerWave");
}

TEST_F(DriverTest, TestGatewayOwnerOutputQuiesceCountsDetachedWaveReservation) {
  auto* first = create_gateway_session_for_test("gw-test-quiesce-detached-a",
                                                "/clone/gateway_login_example", 136);
  auto* second = create_gateway_session_for_test("gw-test-quiesce-detached-b",
                                                 "/clone/gateway_login_example", 137);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  add_ref(first, "TestGatewayOwnerOutputQuiesceCountsDetachedWaveReservation");
  add_ref(second, "TestGatewayOwnerOutputQuiesceCountsDetachedWaveReservation");
  vm_owner_set_id(first, "owner/test/gateway/quiesce-detached-a");
  vm_owner_set_id(second, "owner/test/gateway/quiesce-detached-b");
  auto* first_session = gateway_find_session_by_object(first);
  auto* second_session = gateway_find_session_by_object(second);
  ASSERT_NE(first_session, nullptr);
  ASSERT_NE(second_session, nullptr);
  const auto first_reservation = gateway_reserve_session_output_for_object(first);
  const auto second_reservation = gateway_reserve_session_output_for_object(second);
  ASSERT_GT(first_reservation, 0u);
  ASSERT_GT(second_reservation, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(first_session, first_reservation));
  ASSERT_TRUE(gateway_test_seed_owner_room_event(second_session, second_reservation));

  std::atomic<int> blocker_started{0};
  std::atomic<int> release_blocker{0};
  vm_owner_thread_start(2);
  struct OwnerThreadGuard {
    std::atomic<int>& release;
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
                second, "room_output_projection", "unit-quiesce-detached-blocker",
                [&] {
                  blocker_started.store(1, std::memory_order_release);
                  while (release_blocker.load(std::memory_order_acquire) == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
                }),
            0u);
  for (int index = 0; index < 200 && blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {first, second}, {first_reservation, second_reservation},
      {"quiesce-detached-a", "quiesce-detached-b"}, {24, 25}, 1000, &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true, true}));
  ASSERT_EQ(submitted.future_ids.size(), 2u);
  for (int index = 0; index < 200; ++index) {
    if (vm_owner_future_state(submitted.future_ids[0]) == VM_OWNER_FUTURE_COMPLETED) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]), VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]), VM_OWNER_FUTURE_PENDING);
  ASSERT_EQ(gateway_process_session_future_watches_at(0), 1);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_session_future_watch_count(), 1);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 1);
  ASSERT_EQ(first_session->output_fifo.size(), 1u);
  ASSERT_EQ(second_session->output_fifo.size(), 1u);
  ASSERT_EQ(first_session->output_fifo.front().reservation_id, first_reservation);
  ASSERT_EQ(second_session->output_fifo.front().reservation_id, second_reservation);
  ASSERT_FALSE(first_session->output_fifo.front().ready);
  ASSERT_FALSE(second_session->output_fifo.front().ready);

  auto* quiesced = gateway_owner_output_quiesce("unit detached reservation quiesce");
  ASSERT_NE(quiesced, nullptr);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "success"), 1);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "room_watches_before"), 1);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "room_reservations_before"), 2);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "released_reservations"), 2);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "cancelled_futures"), 1);
  ASSERT_EQ(gateway_test_mapping_number(quiesced, "room_reservations_after"), 0);
  free_mapping(quiesced);

  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_TRUE(first_session->output_fifo.empty());
  ASSERT_TRUE(second_session->output_fifo.empty());

  owner_thread_guard.stop();
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-quiesce-detached-a", "test_done", "done"), 1);
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-quiesce-detached-b", "test_done", "done"), 1);
  destruct_object(first);
  destruct_object(second);
  free_object(&first, "TestGatewayOwnerOutputQuiesceCountsDetachedWaveReservation");
  free_object(&second, "TestGatewayOwnerOutputQuiesceCountsDetachedWaveReservation");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputProjectionUsesSameWireAndFifoSlot) {
  auto* ob = create_gateway_session_for_test("gw-test-owner-room-output",
                                             "/clone/gateway_login_example", 111);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayOwnerRoomOutputProjectionUsesSameWireAndFifoSlot");
  vm_owner_set_id(ob, "owner/test/gateway/room-output");
  auto* session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  const auto worker_wall_samples_before =
      g_gateway_runtime_counters.room_output_projection_worker_samples.load(
          std::memory_order_relaxed);
  const auto worker_cpu_samples_before =
      g_gateway_runtime_counters.room_output_projection_worker_thread_cpu_samples.load(
          std::memory_order_relaxed);
  const auto worker_cpu_unavailable_before =
      g_gateway_runtime_counters.room_output_projection_worker_thread_cpu_unavailable.load(
          std::memory_order_relaxed);

  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(session, reservation_id));
  std::string expected_wire;
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(session, reservation_id, &expected_wire));

  vm_owner_thread_start(2);
  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {ob}, {reservation_id}, {"owner-room-player"}, {19}, 1000, &submitted));
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
    if (state == VM_OWNER_FUTURE_COMPLETED || state == VM_OWNER_FUTURE_FAILED) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]), VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(gateway_process_session_future_watches_at(std::numeric_limits<uint64_t>::max()), 1);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_test_call_number("query_last_owner_room_output_reservation_id", ob),
            static_cast<long>(reservation_id));
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_room_output_state", ob), "completed");
  ASSERT_EQ(gateway_test_call_number("query_last_owner_room_output_event_count", ob), 1);
  ASSERT_EQ(gateway_test_call_number("query_last_owner_room_output_slot_server_seq", ob), 1201);
  ASSERT_EQ(gateway_test_call_number("query_last_owner_room_output_callback_off_main", ob), 0);
  ASSERT_EQ(session->output_fifo.size(), 1u);
  ASSERT_TRUE(session->output_fifo.front().ready);
  ASSERT_EQ(session->output_fifo.front().reservation_id, reservation_id);
  ASSERT_EQ(session->output_fifo.front().wire_bytes, expected_wire);
  const auto wire = nlohmann::json::parse(expected_wire);
  ASSERT_EQ(wire["type"], "output");
  ASSERT_EQ(wire["cid"], "gw-test-owner-room-output");
  ASSERT_TRUE(wire["data"].is_string());
  ASSERT_GT(
      g_gateway_runtime_counters.room_output_projection_submitted.load(std::memory_order_relaxed),
      0u);
  ASSERT_GT(g_gateway_runtime_counters.room_output_projection_worker_samples.load(
                std::memory_order_relaxed),
            0u);
  const auto worker_wall_samples_after =
      g_gateway_runtime_counters.room_output_projection_worker_samples.load(
          std::memory_order_relaxed);
  const auto worker_cpu_samples_after =
      g_gateway_runtime_counters.room_output_projection_worker_thread_cpu_samples.load(
          std::memory_order_relaxed);
  const auto worker_cpu_unavailable_after =
      g_gateway_runtime_counters.room_output_projection_worker_thread_cpu_unavailable.load(
          std::memory_order_relaxed);
  ASSERT_EQ(worker_wall_samples_after - worker_wall_samples_before,
            (worker_cpu_samples_after - worker_cpu_samples_before) +
                (worker_cpu_unavailable_after - worker_cpu_unavailable_before));
  ASSERT_GT(g_gateway_runtime_counters.room_output_projection_publish_samples.load(
                std::memory_order_relaxed),
            0u);

  vm_owner_thread_stop();

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-owner-room-output", "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayOwnerRoomOutputProjectionUsesSameWireAndFifoSlot");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputMissingWaveReleasesExactlyOnce) {
  const auto cancelled_before = g_gateway_runtime_counters.future_watches_cancelled.load();
  const auto released_before = g_gateway_runtime_counters.room_output_projection_released.load();
  auto* ob = create_gateway_session_for_test("gw-test-owner-room-missing-wave",
                                             "/clone/gateway_login_example", 120);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayOwnerRoomOutputMissingWaveReleasesExactlyOnce");
  vm_owner_set_id(ob, "owner/test/gateway/room-missing-wave");
  auto* session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(session, reservation_id));

  vm_owner_thread_start(1);
  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {ob}, {reservation_id}, {"owner-room-player"}, {19}, 1000, &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true}));
  ASSERT_EQ(submitted.future_ids.size(), 1u);
  ASSERT_TRUE(gateway_drop_room_output_wave_for_test(reservation_id));
  ASSERT_EQ(gateway_process_session_future_watches_at(std::numeric_limits<uint64_t>::max()), 1);

  EXPECT_TRUE(session->output_fifo.empty());
  EXPECT_EQ(vm_owner_future_state(submitted.future_ids[0]), VM_OWNER_FUTURE_UNKNOWN);
  EXPECT_EQ(gateway_session_future_watch_count(), 0);
  EXPECT_EQ(gateway_test_call_number("query_last_owner_room_output_reservation_id", ob),
            static_cast<long>(reservation_id));
  EXPECT_STREQ(gateway_test_call_string("query_last_owner_room_output_state", ob), "released");
  EXPECT_EQ(g_gateway_runtime_counters.future_watches_cancelled.load() - cancelled_before, 1u);
  EXPECT_EQ(g_gateway_runtime_counters.room_output_projection_released.load() - released_before,
            1u);

  vm_owner_thread_stop();
  ASSERT_EQ(
      gateway_destroy_session_internal("gw-test-owner-room-missing-wave", "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayOwnerRoomOutputMissingWaveReleasesExactlyOnce");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputRejectsInvalidUtf8BeforeOwnerSubmit) {
  auto* ob = create_gateway_session_for_test("gw-test-owner-room-invalid-utf8",
                                             "/clone/gateway_login_example", 141);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayOwnerRoomOutputRejectsInvalidUtf8BeforeOwnerSubmit");
  vm_owner_set_id(ob, "owner/test/gateway/room-invalid-utf8");
  auto* session = gateway_find_session_by_object(ob);
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
    auto* quiesced = gateway_owner_output_quiesce("unit invalid utf8 pre-submit cleanup");
    ASSERT_NE(quiesced, nullptr);
    free_mapping(quiesced);
  }
  vm_owner_thread_stop();

  EXPECT_FALSE(accepted);
  EXPECT_EQ(gateway_session_future_watch_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_EQ(
      gateway_destroy_session_internal("gw-test-owner-room-invalid-utf8", "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayOwnerRoomOutputRejectsInvalidUtf8BeforeOwnerSubmit");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputRejectsOversizedWireBeforeOwnerSubmit) {
  auto* ob = create_gateway_session_for_test("gw-test-owner-room-oversized",
                                             "/clone/gateway_login_example", 142);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayOwnerRoomOutputRejectsOversizedWireBeforeOwnerSubmit");
  vm_owner_set_id(ob, "owner/test/gateway/room-oversized");
  auto* session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  const std::string stable_prefix =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"normal\",\"reliability\":\"important\","
      "\"display_mode\":\"paced\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"";
  const std::string stable_suffix = "\",\"payload\":{}}";
  const std::string oversized_event = stable_prefix + std::string(70 * 1024, 'x') + stable_suffix;
  for (LPC_INT index = 0; index < 16; ++index) {
    ASSERT_TRUE(gateway_append_preencoded_message_event_wave(
        {session}, {reservation_id}, {1201 + index * 2}, {1202 + index * 2}, {17}, {8101 + index},
        oversized_event, "player", 222333 + index, 128));
  }

  vm_owner_thread_start(1);
  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  const auto accepted = gateway_submit_pending_message_event_batches_for_objects(
      {ob}, {reservation_id}, {"owner-room-player"}, {19}, 1000, &submitted);
  if (accepted) {
    auto* quiesced = gateway_owner_output_quiesce("unit oversized wire pre-submit cleanup");
    ASSERT_NE(quiesced, nullptr);
    free_mapping(quiesced);
  }
  vm_owner_thread_stop();

  EXPECT_FALSE(accepted);
  EXPECT_EQ(gateway_session_future_watch_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-owner-room-oversized", "test_done", "done"),
            1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayOwnerRoomOutputRejectsOversizedWireBeforeOwnerSubmit");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputWaitsForWholeWaveBeforePublish) {
  const auto submitted_before = g_gateway_runtime_counters.room_output_projection_submitted.load();
  const auto completed_before = g_gateway_runtime_counters.room_output_projection_completed.load();
  const auto released_before = g_gateway_runtime_counters.room_output_projection_released.load();
  auto* first = create_gateway_session_for_test("gw-test-owner-room-wave-a",
                                                "/clone/gateway_login_example", 121);
  auto* second = create_gateway_session_for_test("gw-test-owner-room-wave-b",
                                                 "/clone/gateway_login_example", 122);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  add_ref(first, "TestGatewayOwnerRoomOutputWaitsForWholeWaveBeforePublish");
  add_ref(second, "TestGatewayOwnerRoomOutputWaitsForWholeWaveBeforePublish");
  vm_owner_set_id(first, "owner/test/gateway/room-wave-a");
  vm_owner_set_id(second, "owner/test/gateway/room-wave-b");
  auto* first_session = gateway_find_session_by_object(first);
  auto* second_session = gateway_find_session_by_object(second);
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
    std::atomic<int>& release;
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
                second, "room_output_projection", "unit-wave-blocker",
                [&] {
                  blocker_started.store(1, std::memory_order_release);
                  while (release_blocker.load(std::memory_order_acquire) == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
                  blocker_finished.store(1, std::memory_order_release);
                }),
            0u);
  for (int index = 0; index < 200 && blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {first, second}, {first_reservation, second_reservation},
      {"owner-room-player-a", "owner-room-player-b"}, {19, 20}, 1000, &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true, true}));
  ASSERT_EQ(submitted.future_ids.size(), 2u);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 2);
  ASSERT_EQ(g_gateway_runtime_counters.room_output_projection_submitted.load(),
            submitted_before + 2);
  for (int index = 0; index < 200; ++index) {
    if (vm_owner_future_state(submitted.future_ids[0]) == VM_OWNER_FUTURE_COMPLETED) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]), VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]), VM_OWNER_FUTURE_PENDING);

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
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]), VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(gateway_process_session_future_watches_at(std::numeric_limits<uint64_t>::max()), 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_TRUE(first_session->output_fifo.front().ready);
  ASSERT_TRUE(second_session->output_fifo.front().ready);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_EQ(g_gateway_runtime_counters.room_output_projection_completed.load(),
            completed_before + 2);
  ASSERT_EQ(g_gateway_runtime_counters.room_output_projection_released.load(), released_before);
  owner_thread_guard.stop();

  ASSERT_EQ(gateway_destroy_session_internal("gw-test-owner-room-wave-a", "test_done", "done"), 1);
  ASSERT_EQ(gateway_destroy_session_internal("gw-test-owner-room-wave-b", "test_done", "done"), 1);
  destruct_object(first);
  destruct_object(second);
  free_object(&first, "TestGatewayOwnerRoomOutputWaitsForWholeWaveBeforePublish");
  free_object(&second, "TestGatewayOwnerRoomOutputWaitsForWholeWaveBeforePublish");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputSupportsSameSessionMultipleReservations) {
  const auto inline_fallbacks_before =
      g_gateway_runtime_counters.room_output_projection_inline_fallbacks.load();
  auto* ob = create_gateway_session_for_test("gw-test-owner-room-same-session",
                                             "/clone/gateway_login_example", 133);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayOwnerRoomOutputSupportsSameSessionMultipleReservations");
  vm_owner_set_id(ob, "owner/test/gateway/room-same-session");
  auto* session = gateway_find_session_by_object(ob);
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
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(session, first_reservation, &first_expected,
                                                  "owner-room-player", 19));
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(session, second_reservation, &second_expected,
                                                  "owner-room-player", 19));

  vm_owner_thread_start(2);
  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {ob, ob}, {first_reservation, second_reservation}, {"owner-room-player", "owner-room-player"},
      {19, 19}, 1000, &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true, true}));
  ASSERT_EQ(submitted.filled_inline, (std::vector<bool>{false, false}));
  ASSERT_EQ(submitted.future_ids.size(), 2u);
  ASSERT_NE(submitted.future_ids[0], submitted.future_ids[1]);
  ASSERT_TRUE(gateway_test_drain_owner_room_mailbox(ob, "unit-same-session-drain"));
  ASSERT_EQ(gateway_process_session_future_watches_at(std::numeric_limits<uint64_t>::max()), 2);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(session->output_fifo.size(), 2u);
  ASSERT_TRUE(session->output_fifo[0].ready);
  ASSERT_TRUE(session->output_fifo[1].ready);
  ASSERT_EQ(session->output_fifo[0].reservation_id, first_reservation);
  ASSERT_EQ(session->output_fifo[1].reservation_id, second_reservation);
  ASSERT_EQ(session->output_fifo[0].wire_bytes, first_expected);
  ASSERT_EQ(session->output_fifo[1].wire_bytes, second_expected);
  ASSERT_EQ(g_gateway_runtime_counters.room_output_projection_inline_fallbacks.load(),
            inline_fallbacks_before);

  vm_owner_thread_stop();
  ASSERT_EQ(
      gateway_destroy_session_internal("gw-test-owner-room-same-session", "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayOwnerRoomOutputSupportsSameSessionMultipleReservations");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputPublishRetryIsBoundedAndPreservesFifo) {
  const auto retry_enqueued_before =
      g_gateway_runtime_counters.room_output_projection_retry_enqueued.load();
  const auto retry_attempted_before =
      g_gateway_runtime_counters.room_output_projection_retry_attempted.load();
  auto* ob = create_gateway_session_for_test("gw-test-owner-room-publish-retry",
                                             "/clone/gateway_login_example", 145);
  ASSERT_NE(ob, nullptr);
  add_ref(ob, "TestGatewayOwnerRoomOutputPublishRetryIsBoundedAndPreservesFifo");
  vm_owner_set_id(ob, "owner/test/gateway/room-publish-retry");
  auto* session = gateway_find_session_by_object(ob);
  ASSERT_NE(session, nullptr);
  const auto reservation_id = gateway_reserve_session_output_for_object(ob);
  ASSERT_GT(reservation_id, 0u);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(session, reservation_id));
  std::string expected_wire;
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(session, reservation_id, &expected_wire));

  vm_owner_thread_start(1);
  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {ob}, {reservation_id}, {"owner-room-player"}, {19}, 1000, &submitted));
  for (int index = 0; index < 200; ++index) {
    if (vm_owner_future_state(submitted.future_ids[0]) == VM_OWNER_FUTURE_COMPLETED) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]), VM_OWNER_FUTURE_COMPLETED);

  struct GatewayPacketLimitGuard {
    size_t original;
    ~GatewayPacketLimitGuard() {
      g_gateway_max_packet_size = original;
    }
  } packet_limit_guard{g_gateway_max_packet_size};
  g_gateway_max_packet_size = 1;
  ASSERT_EQ(gateway_process_session_future_watches_at(std::numeric_limits<uint64_t>::max()), 1);
  EXPECT_EQ(gateway_session_future_watch_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_pending_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_wave_count(), 1);
  EXPECT_EQ(gateway_room_output_projection_reservation_count(), 1);
  EXPECT_EQ(gateway_room_output_projection_retry_count(), 1);
  ASSERT_EQ(session->output_fifo.size(), 1u);
  EXPECT_FALSE(session->output_fifo.front().ready);

  g_gateway_max_packet_size = packet_limit_guard.original;
  std::this_thread::sleep_for(std::chrono::milliseconds(3));
  EXPECT_EQ(gateway_process_session_future_watches_at(std::numeric_limits<uint64_t>::max()), 0);
  EXPECT_EQ(gateway_room_output_projection_wave_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_reservation_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_retry_count(), 0);
  ASSERT_EQ(session->output_fifo.size(), 1u);
  EXPECT_TRUE(session->output_fifo.front().ready);
  EXPECT_EQ(session->output_fifo.front().wire_bytes, expected_wire);
  EXPECT_EQ(g_gateway_runtime_counters.room_output_projection_retry_enqueued.load(),
            retry_enqueued_before + 1);
  EXPECT_EQ(g_gateway_runtime_counters.room_output_projection_retry_attempted.load(),
            retry_attempted_before + 1);

  vm_owner_thread_stop();
  ASSERT_EQ(
      gateway_destroy_session_internal("gw-test-owner-room-publish-retry", "test_done", "done"), 1);
  destruct_object(ob);
  free_object(&ob, "TestGatewayOwnerRoomOutputPublishRetryIsBoundedAndPreservesFifo");
}

TEST_F(DriverTest, TestGatewayRoomOutputRetrySchedulerHasHardBudgets) {
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto watch_start = source.find("int gateway_process_session_future_watches_at");
  const auto watch_end = source.find("gateway_session_future_watch_count()", watch_start);
  ASSERT_NE(watch_start, std::string::npos);
  ASSERT_NE(watch_end, std::string::npos);
  const auto watch_body = source.substr(watch_start, watch_end - watch_start);
  EXPECT_NE(watch_body.find("gateway_process_room_output_publish_retries("), std::string::npos);
  EXPECT_EQ(watch_body.find("for (const auto &[wave_id, wave] : "
                            "g_gateway_room_output_waves)"),
            std::string::npos);

  const auto retry_start = source.find("void gateway_process_room_output_publish_retries");
  const auto retry_end =
      source.find("void gateway_cancel_room_output_wave_session_items", retry_start);
  ASSERT_NE(retry_start, std::string::npos);
  ASSERT_NE(retry_end, std::string::npos);
  const auto retry_body = source.substr(retry_start, retry_end - retry_start);
  EXPECT_NE(retry_body.find("kGatewayRoomOutputRetryBudget"), std::string::npos);
  EXPECT_NE(retry_body.find("kGatewayRoomOutputRetryWallBudgetNs"), std::string::npos);
  EXPECT_NE(retry_body.find("kGatewayRoomOutputRetryMaxHoldMs"), std::string::npos);
  EXPECT_NE(source.find("kGatewayRoomOutputRetryMaxAttempts = 12"), std::string::npos);
  EXPECT_NE(source.find("GatewayRoomOutputRetrySchedule"), std::string::npos);
}

TEST_F(DriverTest, TestGatewayRoomOutputPreparesFlushSessionsBeforeFifoStage) {
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto publish_start =
      source.find("bool gateway_publish_room_output_wave(uint64_t wave_id) {");
  const auto publish_end =
      source.find("void gateway_process_room_output_publish_retries", publish_start);
  ASSERT_NE(publish_start, std::string::npos);
  ASSERT_NE(publish_end, std::string::npos);
  const auto publish_body = source.substr(publish_start, publish_end - publish_start);
  const auto flush_insert = publish_body.find("flush_sessions.insert");
  const auto fifo_stage = publish_body.find("gateway_stage_session_wire_outputs");
  ASSERT_NE(flush_insert, std::string::npos);
  ASSERT_NE(fifo_stage, std::string::npos);
  EXPECT_LT(flush_insert, fifo_stage);
  EXPECT_EQ(publish_body.find("flush_sessions.insert", fifo_stage), std::string::npos);
}

TEST_F(DriverTest, TestGatewayRoomOutputPartialWatchRollbackCountsCancellation) {
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto submit_start =
      source.find("bool gateway_submit_pending_message_event_batches_for_objects");
  const auto submit_end =
      source.find("int gateway_fill_pending_message_event_batch_for_object", submit_start);
  ASSERT_NE(submit_start, std::string::npos);
  ASSERT_NE(submit_end, std::string::npos);
  const auto submit_body = source.substr(submit_start, submit_end - submit_start);
  const auto rollback_start =
      submit_body.find("for (size_t index = 0; index < registered_count; ++index)");
  const auto rollback_end = submit_body.find(
      "for (size_t index = registered_count; index < submissions.size(); "
      "++index)",
      rollback_start);
  ASSERT_NE(rollback_start, std::string::npos);
  ASSERT_NE(rollback_end, std::string::npos);
  const auto rollback_body = submit_body.substr(rollback_start, rollback_end - rollback_start);
  EXPECT_NE(rollback_body.find("future_watches_cancelled.fetch_add"), std::string::npos);
}

TEST_F(DriverTest, TestGatewayFutureWatchQueueMutationIsTransactional) {
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto session_register = source.find("bool gateway_register_session_future_watch_state");
  const auto session_requeue = source.find("bool gateway_requeue_session_future_watch");
  const auto generic_register = source.find("bool gateway_register_generic_future_watch_state");
  const auto generic_requeue = source.find("bool gateway_requeue_generic_future_watch");
  ASSERT_NE(session_register, std::string::npos);
  ASSERT_NE(session_requeue, std::string::npos);
  ASSERT_NE(generic_register, std::string::npos);
  ASSERT_NE(generic_requeue, std::string::npos);

  const auto watch_register_start =
      source.find("int gateway_watch_session_future_for_object_internal");
  const auto watch_register_end =
      source.find("int gateway_watch_session_future_for_object(", watch_register_start);
  ASSERT_NE(watch_register_start, std::string::npos);
  ASSERT_NE(watch_register_end, std::string::npos);
  const auto watch_register_body =
      source.substr(watch_register_start, watch_register_end - watch_register_start);
  EXPECT_NE(watch_register_body.find("gateway_register_session_future_watch_state"),
            std::string::npos);
  EXPECT_EQ(watch_register_body.find("g_gateway_session_future_watches.emplace"),
            std::string::npos);
  EXPECT_EQ(watch_register_body.find("g_gateway_future_watch_queue.push_back"), std::string::npos);

  const auto generic_watch_start = source.find("int gateway_watch_future_for_object");
  const auto generic_watch_end =
      source.find("int gateway_process_future_watches_at", generic_watch_start);
  ASSERT_NE(generic_watch_start, std::string::npos);
  ASSERT_NE(generic_watch_end, std::string::npos);
  const auto generic_watch_body =
      source.substr(generic_watch_start, generic_watch_end - generic_watch_start);
  EXPECT_NE(generic_watch_body.find("gateway_register_generic_future_watch_state"),
            std::string::npos);
  EXPECT_EQ(generic_watch_body.find("g_gateway_future_watches.emplace"), std::string::npos);
  EXPECT_EQ(generic_watch_body.find("g_gateway_generic_future_watch_queue.push_back"),
            std::string::npos);

  const auto process_session_start = source.find("int gateway_process_session_future_watches_at");
  const auto process_session_end =
      source.find("gateway_session_future_watch_count()", process_session_start);
  ASSERT_NE(process_session_start, std::string::npos);
  ASSERT_NE(process_session_end, std::string::npos);
  const auto process_session_body =
      source.substr(process_session_start, process_session_end - process_session_start);
  EXPECT_NE(process_session_body.find("gateway_requeue_session_future_watch"), std::string::npos);

  const auto process_generic_start = source.find("int gateway_process_future_watches_at");
  const auto process_generic_end =
      source.find("gateway_future_watch_count()", process_generic_start);
  ASSERT_NE(process_generic_start, std::string::npos);
  ASSERT_NE(process_generic_end, std::string::npos);
  const auto process_generic_body =
      source.substr(process_generic_start, process_generic_end - process_generic_start);
  EXPECT_NE(process_generic_body.find("gateway_requeue_generic_future_watch"), std::string::npos);
}

TEST_F(DriverTest, TestGatewayFutureWatchLimitIsSharedAcrossWatchKinds) {
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway_session.cc");

  const auto limit_start = source.find("bool gateway_future_watch_limit_reached()");
  const auto limit_end = source.find("void gateway_future_watch_timer_cb", limit_start);
  ASSERT_NE(limit_start, std::string::npos);
  ASSERT_NE(limit_end, std::string::npos);
  const auto limit_body = source.substr(limit_start, limit_end - limit_start);
  EXPECT_NE(limit_body.find("g_gateway_session_future_watches.size()"), std::string::npos);
  EXPECT_NE(limit_body.find("g_gateway_future_watches.size()"), std::string::npos);
  EXPECT_NE(limit_body.find("kGatewayMaxFutureWatches"), std::string::npos);

  const auto session_start = source.find("int gateway_watch_session_future_for_object_internal");
  const auto session_end =
      source.find("int gateway_watch_session_future_for_object(", session_start);
  ASSERT_NE(session_start, std::string::npos);
  ASSERT_NE(session_end, std::string::npos);
  const auto session_body = source.substr(session_start, session_end - session_start);
  EXPECT_NE(session_body.find("gateway_future_watch_limit_reached()"), std::string::npos);

  const auto generic_start = source.find("int gateway_watch_future_for_object");
  const auto generic_end = source.find("int gateway_process_future_watches_at", generic_start);
  ASSERT_NE(generic_start, std::string::npos);
  ASSERT_NE(generic_end, std::string::npos);
  const auto generic_body = source.substr(generic_start, generic_end - generic_start);
  EXPECT_NE(generic_body.find("gateway_future_watch_limit_reached()"), std::string::npos);
}

TEST_F(DriverTest, TestGatewayOwnerStaleNotificationDoesNotReuseWaveAfterCallback) {
  const auto source = read_source_file_for_test("../src/packages/gateway/gateway_session.cc");
  const auto process_start = source.find(
      "int gateway_process_room_output_wave_watch(\n"
      "    const GatewaySessionFutureWatch &watch, uint64_t now_ms) {");
  const auto process_end = source.find("}  // namespace", process_start);
  ASSERT_NE(process_start, std::string::npos);
  ASSERT_NE(process_end, std::string::npos);
  const auto process_body = source.substr(process_start, process_end - process_start);
  const auto stale_start = process_body.find("if (!owner_current)");
  const auto stale_end = process_body.find("auto future_state", stale_start);
  ASSERT_NE(stale_start, std::string::npos);
  ASSERT_NE(stale_end, std::string::npos);
  const auto stale_body = process_body.substr(stale_start, stale_end - stale_start);
  const auto terminal_snapshot = stale_body.find("gateway_room_output_wave_all_terminal");
  const auto notification = stale_body.find("gateway_notify_room_output_wave_item");
  ASSERT_NE(terminal_snapshot, std::string::npos);
  ASSERT_NE(notification, std::string::npos);
  EXPECT_LT(terminal_snapshot, notification);
  EXPECT_EQ(stale_body.find("wave_it->", notification), std::string::npos);
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputReservationMismatchIsolatesRecipient) {
  auto* first = create_gateway_session_for_test("gw-test-owner-room-mismatch-wave-a",
                                                "/clone/gateway_login_example", 143);
  auto* second = create_gateway_session_for_test("gw-test-owner-room-mismatch-wave-b",
                                                 "/clone/gateway_login_example", 144);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  add_ref(first, "TestGatewayOwnerRoomOutputReservationMismatchIsolatesRecipient");
  add_ref(second, "TestGatewayOwnerRoomOutputReservationMismatchIsolatesRecipient");
  vm_owner_set_id(first, "owner/test/gateway/room-mismatch-wave-a");
  vm_owner_set_id(second, "owner/test/gateway/room-mismatch-wave-b");
  auto* first_session = gateway_find_session_by_object(first);
  auto* second_session = gateway_find_session_by_object(second);
  ASSERT_NE(first_session, nullptr);
  ASSERT_NE(second_session, nullptr);
  const auto first_reservation = gateway_reserve_session_output_for_object(first);
  const auto second_reservation = gateway_reserve_session_output_for_object(second);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(first_session, first_reservation));
  ASSERT_TRUE(gateway_test_seed_owner_room_event(second_session, second_reservation));
  std::string second_expected_wire;
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(
      second_session, second_reservation, &second_expected_wire, "owner-room-player-b", 20));

  std::atomic<int> blocker_started{0};
  std::atomic<int> release_blocker{0};
  vm_owner_thread_start(2);
  ASSERT_GT(vm_owner_enqueue_executor_task(
                second, "room_output_projection", "unit-reservation-mismatch-wave-blocker",
                [&] {
                  blocker_started.store(1, std::memory_order_release);
                  while (release_blocker.load(std::memory_order_acquire) == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
                }),
            0u);
  for (int index = 0; index < 200 && blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {first, second}, {first_reservation, second_reservation},
      {"owner-room-player-a", "owner-room-player-b"}, {19, 20}, 1000, &submitted));
  for (int index = 0; index < 200; ++index) {
    if (vm_owner_future_state(submitted.future_ids[0]) == VM_OWNER_FUTURE_COMPLETED) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]), VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]), VM_OWNER_FUTURE_PENDING);
  ASSERT_EQ(gateway_release_session_output_for_object(first, first_reservation), 1);

  ASSERT_EQ(gateway_process_session_future_watches_at(0), 1);
  const auto watches_after_mismatch = gateway_session_future_watch_count();
  const auto pending_after_mismatch = gateway_room_output_projection_pending_count();
  EXPECT_EQ(watches_after_mismatch, 1);
  EXPECT_EQ(pending_after_mismatch, 1);
  EXPECT_EQ(gateway_room_output_projection_wave_count(), 1);
  EXPECT_EQ(gateway_room_output_projection_reservation_count(), 1);
  EXPECT_TRUE(first_session->output_fifo.empty());
  ASSERT_EQ(second_session->output_fifo.size(), 1u);
  EXPECT_FALSE(second_session->output_fifo.front().ready);

  release_blocker.store(1, std::memory_order_release);
  for (int index = 0; index < 200; ++index) {
    if (vm_owner_future_state(submitted.future_ids[1]) == VM_OWNER_FUTURE_COMPLETED) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]), VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(gateway_process_session_future_watches_at(std::numeric_limits<uint64_t>::max()), 1);
  vm_owner_thread_stop();

  EXPECT_EQ(gateway_session_future_watch_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_pending_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_wave_count(), 0);
  EXPECT_EQ(gateway_room_output_projection_reservation_count(), 0);
  EXPECT_TRUE(first_session->output_fifo.empty());
  ASSERT_EQ(second_session->output_fifo.size(), 1u);
  EXPECT_TRUE(second_session->output_fifo.front().ready);
  EXPECT_EQ(second_session->output_fifo.front().wire_bytes, second_expected_wire);
  ASSERT_EQ(
      gateway_destroy_session_internal("gw-test-owner-room-mismatch-wave-a", "test_done", "done"),
      1);
  ASSERT_EQ(
      gateway_destroy_session_internal("gw-test-owner-room-mismatch-wave-b", "test_done", "done"),
      1);
  destruct_object(first);
  destruct_object(second);
  free_object(&first, "TestGatewayOwnerRoomOutputReservationMismatchIsolatesRecipient");
  free_object(&second, "TestGatewayOwnerRoomOutputReservationMismatchIsolatesRecipient");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputStaleRecipientFallsBackWithSlowPeer) {
  auto* stale = create_gateway_session_for_test("gw-test-owner-room-stale-wave-a",
                                                "/clone/gateway_login_example", 127);
  auto* slow = create_gateway_session_for_test("gw-test-owner-room-stale-wave-b",
                                               "/clone/gateway_login_example", 128);
  ASSERT_NE(stale, nullptr);
  ASSERT_NE(slow, nullptr);
  add_ref(stale, "TestGatewayOwnerRoomOutputStaleRecipientFallsBackWithSlowPeer");
  add_ref(slow, "TestGatewayOwnerRoomOutputStaleRecipientFallsBackWithSlowPeer");
  vm_owner_set_id(stale, "owner/test/gateway/room-stale-wave-a");
  vm_owner_set_id(slow, "owner/test/gateway/room-stale-wave-b");
  auto* stale_session = gateway_find_session_by_object(stale);
  auto* slow_session = gateway_find_session_by_object(slow);
  ASSERT_NE(stale_session, nullptr);
  ASSERT_NE(slow_session, nullptr);
  const auto stale_reservation = gateway_reserve_session_output_for_object(stale);
  const auto slow_reservation = gateway_reserve_session_output_for_object(slow);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(stale_session, stale_reservation));
  ASSERT_TRUE(gateway_test_seed_owner_room_event(slow_session, slow_reservation));
  std::string stale_expected;
  std::string slow_expected;
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(stale_session, stale_reservation, &stale_expected,
                                                  "owner-room-player-a", 19));
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(slow_session, slow_reservation, &slow_expected,
                                                  "owner-room-player-b", 20));

  std::atomic<int> blocker_started{0};
  std::atomic<int> release_blocker{0};
  vm_owner_thread_start(2);
  struct OwnerThreadStaleGuard {
    std::atomic<int>& release;
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
  for (int index = 0; index < 200 && blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {stale, slow}, {stale_reservation, slow_reservation},
      {"owner-room-player-a", "owner-room-player-b"}, {19, 20}, 1000, &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true, true}));
  ASSERT_EQ(submitted.future_ids.size(), 2u);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]), VM_OWNER_FUTURE_PENDING);

  vm_owner_set_id(stale, "owner/test/gateway/room-stale-wave-rebound");
  ASSERT_EQ(gateway_process_session_future_watches_at(0), 1);

  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]), VM_OWNER_FUTURE_PENDING);
  ASSERT_EQ(gateway_session_future_watch_count(), 1);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 1);
  ASSERT_EQ(stale_session->output_fifo.size(), 1u);
  ASSERT_FALSE(stale_session->output_fifo.front().ready);
  ASSERT_EQ(slow_session->output_fifo.size(), 1u);
  ASSERT_FALSE(slow_session->output_fifo.front().ready);
  ASSERT_STRNE(gateway_test_call_string("query_last_owner_room_output_state", stale), "released");

  release_blocker.store(1, std::memory_order_release);
  ASSERT_TRUE(gateway_test_drain_owner_room_mailbox(slow, "unit-stale-wave-drain"));
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]), VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(gateway_process_session_future_watches_at(std::numeric_limits<uint64_t>::max()), 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_EQ(stale_session->output_fifo.size(), 1u);
  ASSERT_TRUE(stale_session->output_fifo.front().ready);
  ASSERT_EQ(stale_session->output_fifo.front().wire_bytes, stale_expected);
  ASSERT_EQ(slow_session->output_fifo.size(), 1u);
  ASSERT_TRUE(slow_session->output_fifo.front().ready);
  ASSERT_EQ(slow_session->output_fifo.front().wire_bytes, slow_expected);
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_room_output_state", stale), "completed");
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_room_output_state", slow), "completed");

  owner_thread_guard.stop();
  ASSERT_EQ(
      gateway_destroy_session_internal("gw-test-owner-room-stale-wave-a", "test_done", "done"), 1);
  ASSERT_EQ(
      gateway_destroy_session_internal("gw-test-owner-room-stale-wave-b", "test_done", "done"), 1);
  destruct_object(stale);
  destruct_object(slow);
  free_object(&stale, "TestGatewayOwnerRoomOutputStaleRecipientFallsBackWithSlowPeer");
  free_object(&slow, "TestGatewayOwnerRoomOutputStaleRecipientFallsBackWithSlowPeer");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputFailedRecipientFallsBackWithSlowPeer) {
  auto* failed = create_gateway_session_for_test("gw-test-owner-room-failed-wave-a",
                                                 "/clone/gateway_login_example", 129);
  auto* slow = create_gateway_session_for_test("gw-test-owner-room-failed-wave-b",
                                               "/clone/gateway_login_example", 130);
  ASSERT_NE(failed, nullptr);
  ASSERT_NE(slow, nullptr);
  add_ref(failed, "TestGatewayOwnerRoomOutputFailedRecipientFallsBackWithSlowPeer");
  add_ref(slow, "TestGatewayOwnerRoomOutputFailedRecipientFallsBackWithSlowPeer");
  vm_owner_set_id(failed, "owner/test/gateway/room-failed-wave-a");
  vm_owner_set_id(slow, "owner/test/gateway/room-failed-wave-b");
  auto* failed_session = gateway_find_session_by_object(failed);
  auto* slow_session = gateway_find_session_by_object(slow);
  ASSERT_NE(failed_session, nullptr);
  ASSERT_NE(slow_session, nullptr);
  const auto failed_reservation = gateway_reserve_session_output_for_object(failed);
  const auto slow_reservation = gateway_reserve_session_output_for_object(slow);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(failed_session, failed_reservation));
  ASSERT_TRUE(gateway_test_seed_owner_room_event(slow_session, slow_reservation));
  std::string failed_expected;
  std::string slow_expected;
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(failed_session, failed_reservation,
                                                  &failed_expected, "owner-room-player-a", 19));
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(slow_session, slow_reservation, &slow_expected,
                                                  "owner-room-player-b", 20));

  std::atomic<int> blockers_started{0};
  std::atomic<int> release_blockers{0};
  vm_owner_thread_start(2);
  struct OwnerThreadFailedGuard {
    std::atomic<int>& release;
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
  auto enqueue_blocker = [&](object_t* target, const char* task_key) {
    return vm_owner_enqueue_executor_task(target, "room_output_projection", task_key, [&] {
      blockers_started.fetch_add(1, std::memory_order_release);
      while (release_blockers.load(std::memory_order_acquire) == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    });
  };
  ASSERT_GT(enqueue_blocker(failed, "unit-failed-wave-blocker-a"), 0u);
  ASSERT_GT(enqueue_blocker(slow, "unit-failed-wave-blocker-b"), 0u);
  for (int index = 0; index < 200 && blockers_started.load(std::memory_order_acquire) != 2;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blockers_started.load(std::memory_order_acquire), 2);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {failed, slow}, {failed_reservation, slow_reservation},
      {"owner-room-player-a", "owner-room-player-b"}, {19, 20}, 1000, &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true, true}));
  ASSERT_EQ(submitted.future_ids.size(), 2u);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]), VM_OWNER_FUTURE_PENDING);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]), VM_OWNER_FUTURE_PENDING);

  auto* cancelled =
      vm_owner_future_cancel(submitted.future_ids[0], "unit room output projection failure");
  ASSERT_STREQ(gateway_test_mapping_string(cancelled, "state"), "failed");
  ASSERT_EQ(gateway_test_mapping_number(cancelled, "cancelled"), 1);
  free_mapping(cancelled);
  ASSERT_EQ(gateway_process_session_future_watches_at(0), 1);

  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]), VM_OWNER_FUTURE_PENDING);
  ASSERT_EQ(gateway_session_future_watch_count(), 1);
  ASSERT_EQ(failed_session->output_fifo.size(), 1u);
  ASSERT_FALSE(failed_session->output_fifo.front().ready);
  ASSERT_EQ(slow_session->output_fifo.size(), 1u);
  ASSERT_FALSE(slow_session->output_fifo.front().ready);
  ASSERT_STRNE(gateway_test_call_string("query_last_owner_room_output_state", failed), "released");

  release_blockers.store(1, std::memory_order_release);
  ASSERT_TRUE(gateway_test_drain_owner_room_mailbox(failed, "unit-failed-wave-drain-a"));
  ASSERT_TRUE(gateway_test_drain_owner_room_mailbox(slow, "unit-failed-wave-drain-b"));
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]), VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(gateway_process_session_future_watches_at(std::numeric_limits<uint64_t>::max()), 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_EQ(failed_session->output_fifo.size(), 1u);
  ASSERT_TRUE(failed_session->output_fifo.front().ready);
  ASSERT_EQ(failed_session->output_fifo.front().wire_bytes, failed_expected);
  ASSERT_EQ(slow_session->output_fifo.size(), 1u);
  ASSERT_TRUE(slow_session->output_fifo.front().ready);
  ASSERT_EQ(slow_session->output_fifo.front().wire_bytes, slow_expected);
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_room_output_state", failed), "completed");
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_room_output_state", slow), "completed");

  owner_thread_guard.stop();
  ASSERT_EQ(
      gateway_destroy_session_internal("gw-test-owner-room-failed-wave-a", "test_done", "done"), 1);
  ASSERT_EQ(
      gateway_destroy_session_internal("gw-test-owner-room-failed-wave-b", "test_done", "done"), 1);
  destruct_object(failed);
  destruct_object(slow);
  free_object(&failed, "TestGatewayOwnerRoomOutputFailedRecipientFallsBackWithSlowPeer");
  free_object(&slow, "TestGatewayOwnerRoomOutputFailedRecipientFallsBackWithSlowPeer");
}

TEST_F(DriverTest, TestGatewayOwnerRoomOutputWaveTimeoutPreservesCompletedRecipient) {
  const auto completed_before = g_gateway_runtime_counters.room_output_projection_completed.load();
  const auto released_before = g_gateway_runtime_counters.room_output_projection_released.load();
  auto* first = create_gateway_session_for_test("gw-test-owner-room-timeout-wave-a",
                                                "/clone/gateway_login_example", 123);
  auto* second = create_gateway_session_for_test("gw-test-owner-room-timeout-wave-b",
                                                 "/clone/gateway_login_example", 124);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  add_ref(first, "TestGatewayOwnerRoomOutputWaveTimeoutPreservesCompletedRecipient");
  add_ref(second, "TestGatewayOwnerRoomOutputWaveTimeoutPreservesCompletedRecipient");
  vm_owner_set_id(first, "owner/test/gateway/room-timeout-wave-a");
  vm_owner_set_id(second, "owner/test/gateway/room-timeout-wave-b");
  auto* first_session = gateway_find_session_by_object(first);
  auto* second_session = gateway_find_session_by_object(second);
  ASSERT_NE(first_session, nullptr);
  ASSERT_NE(second_session, nullptr);
  const auto first_reservation = gateway_reserve_session_output_for_object(first);
  const auto second_reservation = gateway_reserve_session_output_for_object(second);
  ASSERT_TRUE(gateway_test_seed_owner_room_event(first_session, first_reservation));
  ASSERT_TRUE(gateway_test_seed_owner_room_event(second_session, second_reservation));
  std::string first_expected;
  std::string second_expected;
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(first_session, first_reservation, &first_expected,
                                                  "owner-room-player-a", 19));
  ASSERT_TRUE(gateway_test_encode_owner_room_wire(second_session, second_reservation,
                                                  &second_expected, "owner-room-player-b", 20));

  std::atomic<int> blocker_started{0};
  std::atomic<int> release_blocker{0};
  vm_owner_thread_start(2);
  struct OwnerThreadTimeoutGuard {
    std::atomic<int>& release;
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
  for (int index = 0; index < 200 && blocker_started.load(std::memory_order_acquire) == 0;
       ++index) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(blocker_started.load(std::memory_order_acquire), 1);

  GatewayPendingMessageEventBatchOwnerSubmitResult submitted;
  ASSERT_TRUE(gateway_submit_pending_message_event_batches_for_objects(
      {first, second}, {first_reservation, second_reservation},
      {"owner-room-player-a", "owner-room-player-b"}, {19, 20}, 1, &submitted));
  ASSERT_EQ(submitted.submitted, (std::vector<bool>{true, true}));
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 2);
  for (int index = 0; index < 200; ++index) {
    if (vm_owner_future_state(submitted.future_ids[0]) == VM_OWNER_FUTURE_COMPLETED) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]), VM_OWNER_FUTURE_COMPLETED);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]), VM_OWNER_FUTURE_PENDING);

  ASSERT_GE(gateway_process_session_future_watches_at(std::numeric_limits<uint64_t>::max()), 1);
  ASSERT_EQ(gateway_session_future_watch_count(), 0);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[0]), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(vm_owner_future_state(submitted.future_ids[1]), VM_OWNER_FUTURE_UNKNOWN);
  ASSERT_EQ(first_session->output_fifo.size(), 1u);
  ASSERT_TRUE(first_session->output_fifo.front().ready);
  ASSERT_EQ(first_session->output_fifo.front().wire_bytes, first_expected);
  ASSERT_EQ(second_session->output_fifo.size(), 1u);
  ASSERT_TRUE(second_session->output_fifo.front().ready);
  ASSERT_EQ(second_session->output_fifo.front().wire_bytes, second_expected);
  ASSERT_EQ(gateway_room_output_projection_pending_count(), 0);
  ASSERT_EQ(g_gateway_runtime_counters.room_output_projection_completed.load(),
            completed_before + 2);
  ASSERT_EQ(g_gateway_runtime_counters.room_output_projection_released.load(), released_before);
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_room_output_state", first), "completed");
  ASSERT_STREQ(gateway_test_call_string("query_last_owner_room_output_state", second), "completed");
  ASSERT_EQ(gateway_test_call_number("query_last_owner_room_output_event_count", first), 1);
  ASSERT_EQ(gateway_test_call_number("query_last_owner_room_output_slot_server_seq", first), 1201);

  release_blocker.store(1, std::memory_order_release);
  ASSERT_TRUE(gateway_test_drain_owner_room_mailbox(second, "unit-timeout-wave-drain"));
  owner_thread_guard.stop();
  ASSERT_EQ(
      gateway_destroy_session_internal("gw-test-owner-room-timeout-wave-a", "test_done", "done"),
      1);
  ASSERT_EQ(
      gateway_destroy_session_internal("gw-test-owner-room-timeout-wave-b", "test_done", "done"),
      1);
  destruct_object(first);
  destruct_object(second);
  free_object(&first, "TestGatewayOwnerRoomOutputWaveTimeoutPreservesCompletedRecipient");
  free_object(&second, "TestGatewayOwnerRoomOutputWaveTimeoutPreservesCompletedRecipient");
}
