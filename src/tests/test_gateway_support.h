#pragma once

#include "test_lpc_support.h"
#include "base/internal/log.h"
#include "net/telnet.h"

#include <unicode/ucnv.h>

namespace {
#include "test_lpc_support.h"
#include "base/internal/log.h"
#include "net/telnet.h"

#include <unicode/ucnv.h>


svalue_t* call_lpc_method(object_t* ob, const char* method, int num_args = 0) {
  save_command_giver(ob);
  set_eval(max_eval_cost);
  auto* ret = safe_apply(method, ob, num_args, ORIGIN_DRIVER);
  restore_command_giver();
  return ret;
}

object_t* create_gateway_session_for_test(const char* session_id, const char* login_file,
                                          int master_fd = -1) {
  svalue_t data{};
  data.type = T_MAPPING;
  data.u.map = allocate_mapping(1);
  add_mapping_string(data.u.map, "ip", "127.0.0.1");
  copy_and_push_string(login_file);
  safe_apply("set_test_login_ob", master_ob, 1, ORIGIN_DRIVER);
  auto* ob = gateway_create_session_internal(session_id, &data, "127.0.0.1", 6040, master_fd);
  safe_apply("reset_test_login_ob", master_ob, 0, ORIGIN_DRIVER);
  free_svalue(&data, "create_gateway_session_for_test");
  return ob;
}

struct OutputBudgetConfigGuard {
  int per_connection{CONFIG_INT(__RC_MAX_PENDING_OUTPUT_BYTES__)};
  int total{CONFIG_INT(__RC_MAX_TOTAL_PENDING_OUTPUT_BYTES__)};

  ~OutputBudgetConfigGuard() {
    CONFIG_INT(__RC_MAX_PENDING_OUTPUT_BYTES__) = per_connection;
    CONFIG_INT(__RC_MAX_TOTAL_PENDING_OUTPUT_BYTES__) = total;
  }
};

#ifndef _WIN32
class AsciiInputFixture {
 public:
  AsciiInputFixture() : base_(event_base_new(), event_base_free), user_count_(users_num(true)) {}

  ~AsciiInputFixture() {
    if (auto* session = gateway_find_session("audit-ascii-replacement")) {
      auto* replacement = session->user_ob;
      add_ref(replacement, "AsciiInputFixture replacement");
      remove_interactive(replacement, 1);
      destruct_object_for_test(replacement);
      free_object(&replacement, "AsciiInputFixture replacement");
    }
    if (object_) {
      if (object_->interactive) {
        remove_interactive(object_, 1);
      }
      destruct_object_for_test(object_);
      free_object(&object_, "AsciiInputFixture");
    }
    if (unowned_fd_ >= 0) {
      evutil_closesocket(unowned_fd_);
    }
    if (peer_fd_ >= 0) {
      evutil_closesocket(peer_fd_);
    }
    if (base_) {
      EXPECT_NE(event_base_loop(base_.get(), EVLOOP_NONBLOCK), -1);
      EXPECT_EQ(event_base_get_num_events(base_.get(), EVENT_BASE_COUNT_ADDED), 0);
    }
    EXPECT_EQ(users_num(true), user_count_);
  }

  void open() {
    ASSERT_NE(base_, nullptr);
    ASSERT_EQ(gateway_find_session("audit-ascii-replacement"), nullptr);
    object_ = clone_object_for_test("/clone/audit_ascii_input");
    ASSERT_NE(object_, nullptr);
    add_ref(object_, "AsciiInputFixture");

    int descriptors[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors), 0);
    old_fd_ = unowned_fd_ = descriptors[0];
    peer_fd_ = descriptors[1];
    ASSERT_EQ(evutil_make_socket_nonblocking(old_fd_), 0);
    ASSERT_EQ(evutil_make_socket_nonblocking(peer_fd_), 0);

    auto* ip = user_add();
    ip->ob = object_;
    ip->connection_type = PORT_TYPE_ASCII;
    ip->fd = old_fd_;
    auto* address = reinterpret_cast<sockaddr_in*>(&ip->addr);
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ip->addrlen = sizeof(sockaddr_in);
    object_->interactive = ip;
    object_->flags |= O_ONCE_INTERACTIVE;
    add_ref(object_, "AsciiInputFixture interactive");
    ip->ev_buffer = bufferevent_socket_new(base_.get(), old_fd_,
                                           BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
    ASSERT_NE(ip->ev_buffer, nullptr);
    bufferevent_setcb(
        ip->ev_buffer, nullptr,
        [](bufferevent* bev, void* arg) {
          auto* user = static_cast<interactive_t*>(arg);
          comm_update_output_accounting(user, bufferevent_get_output(bev));
        },
        nullptr, ip);
    unowned_fd_ = -1;
    ip->ev_command = evtimer_new(base_.get(), [](evutil_socket_t, short, void*) {}, nullptr);
    ASSERT_NE(ip->ev_command, nullptr);
  }

  void open_telnet() {
    open();
    auto* ip = object_->interactive;
    ASSERT_NE(ip, nullptr);
    ip->connection_type = PORT_TYPE_TELNET;
    ip->telnet = net_telnet_init(ip);
    ASSERT_NE(ip->telnet, nullptr);
  }

  void set_input_encoding(const char* name) {
    ASSERT_NE(object_, nullptr);
    auto* ip = object_->interactive;
    ASSERT_NE(ip, nullptr);
    UErrorCode error_code = U_ZERO_ERROR;
    ip->trans = ucnv_open(name, &error_code);
    ASSERT_FALSE(U_FAILURE(error_code));
    error_code = U_ZERO_ERROR;
    ip->telnet_input_trans = ucnv_open(name, &error_code);
    ASSERT_FALSE(U_FAILURE(error_code));
  }

  void receive(const std::string& bytes) {
    ASSERT_NE(object_, nullptr);
    auto* ip = object_->interactive;
    ASSERT_NE(ip, nullptr);
    auto* input = bufferevent_get_input(ip->ev_buffer);
    ASSERT_EQ(evbuffer_unfreeze(input, 0), 0);
    ASSERT_EQ(evbuffer_add(input, bytes.data(), bytes.size()), 0);
    ASSERT_EQ(evbuffer_freeze(input, 0), 0);
    save_command_giver(object_);
    get_user_data(ip);
    restore_command_giver();
  }

  void close_transport() {
    ASSERT_NE(object_, nullptr);
    ASSERT_NE(object_->interactive, nullptr);
    remove_interactive(object_, 1);
  }

  unsigned char telnet_carry_length() const {
    if (!object_ || !object_->interactive) {
      return 0;
    }
    return object_->interactive->telnet_u8_carry_len;
  }

  std::string buffered_text() const {
    if (!object_ || !object_->interactive) {
      return {};
    }
    return {object_->interactive->text, static_cast<std::size_t>(object_->interactive->text_end)};
  }

  void expect_lines(const std::vector<std::string>& expected) {
    auto* result = call_lpc_method(object_, "query_received");
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->type, T_ARRAY);
    ASSERT_EQ(result->u.arr->size, expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
      ASSERT_EQ(result->u.arr->item[i].type, T_STRING);
      EXPECT_EQ(result->u.arr->item[i].u.string, expected[i]);
    }
  }

  void expect_closed(bool replacement) {
    ASSERT_NE(object_, nullptr);
    EXPECT_EQ(object_->interactive, nullptr);
    EXPECT_NE(event_base_loop(base_.get(), EVLOOP_NONBLOCK), -1);
    EXPECT_EQ(event_base_get_num_events(base_.get(), EVENT_BASE_COUNT_ADDED), 0);
    EXPECT_EQ(users_num(true), user_count_ + (replacement ? 1 : 0));
    errno = 0;
    EXPECT_EQ(fcntl(old_fd_, F_GETFD), -1);
    EXPECT_EQ(errno, EBADF);
  }

  std::string pump_transport() {
    std::string received;
    bool observed_empty_output = false;
    for (int attempt = 0; attempt < 16; ++attempt) {
      if (event_base_loop(base_.get(), EVLOOP_NONBLOCK) == -1) {
        ADD_FAILURE() << "event_base_loop failed";
        return received;
      }
      char buffer[128];
      for (;;) {
        const auto count = recv(peer_fd_, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (count > 0) {
          received.append(buffer, static_cast<size_t>(count));
          continue;
        }
        if (count == 0) {
          return received;
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
          ADD_FAILURE() << "unexpected recv errno: " << errno;
          return received;
        }
        break;
      }
      const bool output_empty =
          !object_->interactive ||
          evbuffer_get_length(bufferevent_get_output(object_->interactive->ev_buffer)) == 0;
      if (output_empty) {
        if (observed_empty_output) {
          break;
        }
        observed_empty_output = true;
      } else {
        observed_empty_output = false;
      }
    }
    return received;
  }

  object_t* object() const {
    return object_;
  }

 private:
  std::unique_ptr<event_base, decltype(&event_base_free)> base_;
  size_t user_count_;
  object_t* object_{nullptr};
  int old_fd_{-1};
  int unowned_fd_{-1};
  int peer_fd_{-1};
};

void check_ascii_chunks(const std::string& input, const std::vector<std::string>& expected,
                        const std::vector<size_t>& chunks) {
  AsciiInputFixture fixture;
  ASSERT_NO_FATAL_FAILURE(fixture.open());
  size_t offset = 0;
  for (const auto count : chunks) {
    ASSERT_LE(count, input.size() - offset);
    ASSERT_NO_FATAL_FAILURE(fixture.receive(input.substr(offset, count)));
    offset += count;
  }
  ASSERT_EQ(offset, input.size());
  fixture.expect_lines(expected);
}
#endif


// F10: with budget + N ready sessions, exactly one continuation is recorded
// per flush and the FIFO drains fully in order across scheduled flushes.


// R2-F10: schedule/coalesce/callback counters each map to one deterministic
// event: a request while a continuation is already scheduled is a coalesced
// request (never a new schedule), and the callback itself is counted
// separately when it actually runs.


LPC_INT gateway_test_mapping_number(mapping_t* map, const char* key) {
  auto* value = find_string_in_mapping(map, key);
  EXPECT_NE(value, nullptr) << key;
  EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER) << key;
  return value && value->type == T_NUMBER ? value->u.number : 0;
}

const char* gateway_test_mapping_string(mapping_t* map, const char* key) {
  auto* value = find_string_in_mapping(map, key);
  EXPECT_NE(value, nullptr) << key;
  EXPECT_EQ(value ? value->type : T_INVALID, T_STRING) << key;
  return value && value->type == T_STRING ? value->u.string : "";
}

LPC_INT gateway_test_call_number(const char* method, object_t* target) {
  auto* ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
  EXPECT_NE(ret, nullptr) << method;
  EXPECT_EQ(ret ? ret->type : T_INVALID, T_NUMBER) << method;
  return ret && ret->type == T_NUMBER ? ret->u.number : -1;
}

const char* gateway_test_call_string(const char* method, object_t* target) {
  auto* ret = safe_apply(method, target, 0, ORIGIN_DRIVER);
  EXPECT_NE(ret, nullptr) << method;
  EXPECT_EQ(ret ? ret->type : T_INVALID, T_STRING) << method;
  return ret && ret->type == T_STRING ? ret->u.string : "";
}

const std::string& gateway_test_owner_room_stable_payload() {
  static const std::string payload =
      "{\"schema_version\":1,\"channel\":\"main\",\"intent\":\"append\","
      "\"priority\":\"low\",\"reliability\":\"important\","
      "\"display_mode\":\"instant\",\"ttl_ms\":30000,"
      "\"collapse_key\":\"\",\"text\":\"owner-room-output\",\"payload\":{}}";
  return payload;
}

bool gateway_test_seed_owner_room_event(GatewaySession* session, uint64_t reservation_id) {
  return gateway_append_preencoded_message_event_wave(
      {session}, {reservation_id}, {1201}, {1202}, {17}, {8101},
      gateway_test_owner_room_stable_payload(), "player", 222333, 128);
}

bool gateway_test_encode_owner_room_wire(GatewaySession* session, uint64_t reservation_id,
                                         std::string* wire_bytes,
                                         const std::string& scope_id = "owner-room-player",
                                         LPC_INT slot_epoch = 19) {
  GatewayPendingMessageEventProjectionSnapshot snapshot;
  GatewayPendingMessageEventProjectionColumns columns;
  return wire_bytes &&
         gateway_snapshot_pending_message_event_batch(session, reservation_id, scope_id, slot_epoch,
                                                      &snapshot, &columns) &&
         gateway_encode_pending_message_event_projection(snapshot, columns, wire_bytes);
}

bool gateway_test_drain_owner_room_mailbox(object_t* target, const char* task_key) {
  auto drained = std::make_shared<std::atomic<int>>(0);
  if (vm_owner_enqueue_executor_task(target, "room_output_projection", task_key, [drained] {
        drained->store(1, std::memory_order_release);
      }) == 0) {
    return false;
  }
  for (int i = 0; i < 2000 && drained->load(std::memory_order_acquire) == 0; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return drained->load(std::memory_order_acquire) == 1;
}

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
  void run_claimed_owner(const std::string& owner_id) override {
    (void)owner_id;
    if (throw_std) {
      throw std::runtime_error("injected task failure");
    }
    throw 42;  // non-std exception path
  }
  void release_owner_after_task(const std::string& owner_id) noexcept override {
    EXPECT_EQ(owner_id, claimed);
    releases++;
  }
  void record_owner_exception(const std::string& owner_id, const char* what) override {
    EXPECT_EQ(owner_id, claimed);
    EXPECT_NE(what, nullptr);
    exceptions++;
  }
};

// T15: table-driven extreme-value coverage for gateway session ids.
}  // namespace
