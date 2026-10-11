#ifdef __PACKAGE_SOCKETS__

#include <globals.h>

#define EESUCCESS 1
#define STREAM 1

nosave int server_fd = -1;
nosave int client_fd = -1;
nosave int accepted_fd = -1;
nosave int sent = 0;
nosave int test_completed = 0;
private nosave int completion_token;
private nosave string expected = "\xef\xbf\xbd(";

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
    ASSERT2(0, "stream text invalid-sequence test timed out");
  }
}

void finish_test() {
  test_completed = 1;
  remove_call_out("timeout_fail");
  cleanup();
  "/command/tests"->complete_async(completion_token);
}

void server_read(int fd, string message) {
  ASSERT2(message == expected,
          sprintf("invalid sequence was not sanitized: expected %O, received %O", expected, message));
  call_out("finish_test", 0);
}

void server_write(int fd) {
}

void server_accept_ready(int fd) {
  accepted_fd = socket_accept(fd, "server_read", "server_write");
  ASSERT2(accepted_fd >= 0, "socket_accept failed");
}

void client_write(int fd) {
  buffer fragment;

  if (sent) {
    return;
  }
  sent = 1;
  fragment = allocate_buffer(2);
  fragment[0] = 0xe4;
  fragment[1] = '(';
  ASSERT2(EESUCCESS == socket_write(fd, fragment), "invalid fragment write failed");
}

void client_read(int fd, mixed message) {
  ASSERT2(0, "client received unexpected data");
}

void begin_test() {
  string local;

  local = socket_address(server_fd, 1);
  client_fd = socket_create(STREAM, "client_read");
  ASSERT2(client_fd >= 0, "failed to create client socket");
  ASSERT_EQ(EESUCCESS, socket_connect(client_fd, local, "client_read", "client_write"));
}

void do_tests() {
  completion_token = "/command/tests"->begin_async();
  server_fd = socket_create(STREAM, "server_accept_ready");
  ASSERT2(server_fd >= 0, "failed to create server socket");
  ASSERT_EQ(EESUCCESS, socket_bind(server_fd, 0, "127.0.0.1 0"));
  ASSERT_EQ(EESUCCESS, socket_listen(server_fd, "server_accept_ready"));
  call_out("begin_test", 0);
  call_out("timeout_fail", 5);
}

#else
void do_tests() {
  write("PACKAGE_SOCKETS not available, skipping stream text invalid-sequence test.\n");
}
#endif
