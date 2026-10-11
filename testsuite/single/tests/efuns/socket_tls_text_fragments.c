#ifdef __PACKAGE_SOCKETS__

#include <globals.h>

#define EESUCCESS 1
#define STREAM_TLS 5
#define SO_TLS_VERIFY_PEER 1
#define SO_TLS_SNI_HOSTNAME 2
#define SO_TLS_CERT 3
#define SO_TLS_KEY 4

nosave int server_fd = -1;
nosave int client_fd = -1;
nosave int accepted_fd = -1;
nosave int read_callbacks = 0;
nosave int client_stage = 0;
nosave int server_handshake_done = 0;
nosave int client_handshake_done = 0;
nosave int test_completed = 0;
nosave string server_received = "";
private nosave int completion_token;
private nosave string payload = "\xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80!";

void cleanup() {
  if (accepted_fd != -1) {
    socket_close(accepted_fd);
    accepted_fd = -1;
  }
  if (client_fd != -1) {
    socket_close(client_fd);
    client_fd = -1;
  }
  if (server_fd != -1) {
    socket_close(server_fd);
    server_fd = -1;
  }
}

void timeout_fail() {
  if (!test_completed) {
    cleanup();
    ASSERT2(0, "TLS text fragment test timed out");
  }
}

void finish_test() {
  ASSERT2(server_handshake_done, "TLS server handshake callback missing");
  ASSERT2(client_handshake_done, "TLS client handshake callback missing");
  ASSERT2(read_callbacks >= 3,
          sprintf("expected observed TLS split reads, got %d callbacks", read_callbacks));
  ASSERT2(payload == server_received,
          sprintf("TLS payload mismatch: expected %O, received %O", payload, server_received));
  test_completed = 1;
  remove_call_out("timeout_fail");
  cleanup();
  "/command/tests"->complete_async(completion_token);
}

void client_close(int fd) {
  if (!test_completed) {
    ASSERT2(0, sprintf("client socket %d closed unexpectedly", fd));
  }
}

buffer make_fragment(int first, int second) {
  buffer fragment;

  fragment = allocate_buffer(second < 0 ? 1 : 2);
  fragment[0] = first;
  if (second >= 0) {
    fragment[1] = second;
  }
  return fragment;
}

void send_fragment() {
  buffer fragment;
  int next_stage;

  switch (client_stage) {
    case 0:
      fragment = make_fragment(0xc3, -1);
      break;
    case 1:
      fragment = make_fragment(0xa9, -1);
      break;
    case 2:
      fragment = make_fragment(0xe4, 0xb8);
      break;
    case 3:
      fragment = make_fragment(0xad, 0xf0);
      break;
    case 4:
      fragment = make_fragment(0x9f, 0x98);
      break;
    case 5:
      fragment = make_fragment(0x80, '!');
      break;
    default:
      return;
  }

  next_stage = client_stage + 1;
  ASSERT2(EESUCCESS == socket_write(client_fd, fragment),
          sprintf("TLS fragment %d write failed", client_stage));
  client_stage = next_stage;
  if (client_stage < 6) {
    call_out("send_fragment", 0.05);
  }
}

void client_write(int fd) {
  client_handshake_done = 1;
  send_fragment();
}

void client_read(int fd, mixed message) {
  ASSERT2(0, "TLS client received unexpected data");
}

void server_read(int fd, string message) {
  read_callbacks++;
  ASSERT2(strsrch(message, "\xef\xbf\xbd") == -1,
          sprintf("replacement character in TLS fragment %O", message));
  server_received += message;
  ASSERT2(strlen(server_received) <= strlen(payload),
          sprintf("TLS fragment payload grew unexpectedly: %O", server_received));
  if (server_received == payload) {
    call_out("finish_test", 0);
  }
}

void server_write(int fd) {
  server_handshake_done = 1;
}

void server_accept_ready(int fd) {
  accepted_fd = socket_accept(fd, "server_read", "server_write");
  ASSERT2(accepted_fd >= 0, "TLS socket_accept failed");
}

void start_client(int port) {
  client_fd = socket_create(STREAM_TLS, "client_read", "client_close");
  ASSERT2(client_fd >= 0, "failed to create TLS client socket");
  socket_set_option(client_fd, SO_TLS_VERIFY_PEER, 0);
  socket_set_option(client_fd, SO_TLS_SNI_HOSTNAME, "localhost");
  ASSERT_EQ(EESUCCESS,
            socket_connect(client_fd, sprintf("127.0.0.1 %d", port),
                           "client_read", "client_write"));
}

void begin_test() {
  string local;
  string *parts;
  int port;

  local = socket_address(server_fd, 1);
  ASSERT(stringp(local));
  parts = explode(local, " ");
  ASSERT_EQ(2, sizeof(parts));
  port = to_int(parts[1]);
  start_client(port);
}

void do_tests() {
  completion_token = "/command/tests"->begin_async();
  server_fd = socket_create(STREAM_TLS, "server_accept_ready");
  ASSERT2(server_fd >= 0, "failed to create TLS server socket");
  socket_set_option(server_fd, SO_TLS_CERT, "etc/cert.pem");
  socket_set_option(server_fd, SO_TLS_KEY, "etc/key.pem");
  ASSERT_EQ(EESUCCESS, socket_bind(server_fd, 0, "127.0.0.1 0"));
  ASSERT_EQ(EESUCCESS, socket_listen(server_fd, "server_accept_ready"));
  call_out("begin_test", 0);
  call_out("timeout_fail", 5);
}

#else
void do_tests() {
  write("PACKAGE_SOCKETS not available, skipping TLS text fragment test.\n");
}
#endif
