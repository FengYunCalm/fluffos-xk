#ifdef __PACKAGE_SOCKETS__

#include <globals.h>

#define EESUCCESS 1
#define STREAM_BINARY 3

nosave int server_fd = -1;
nosave int client_fd = -1;
nosave int accepted_fd = -1;
nosave int test_completed = 0;
nosave int sent = 0;
private nosave int completion_token;

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
    ASSERT2(0, "binary stream fragment test timed out");
  }
}

void finish_test() {
  test_completed = 1;
  remove_call_out("timeout_fail");
  cleanup();
  "/command/tests"->complete_async(completion_token);
}

void server_read(int fd, buffer message) {
  ASSERT_EQ(5, sizeof(message));
  ASSERT_EQ(0xe4, message[0]);
  ASSERT_EQ(0xb8, message[1]);
  ASSERT_EQ(0xad, message[2]);
  ASSERT_EQ(0, message[3]);
  ASSERT_EQ(0xff, message[4]);
  call_out("finish_test", 0);
}

void server_write(int fd) {}

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
  fragment = allocate_buffer(5);
  fragment[0] = 0xe4;
  fragment[1] = 0xb8;
  fragment[2] = 0xad;
  fragment[3] = 0;
  fragment[4] = 0xff;
  ASSERT2(EESUCCESS == socket_write(fd, fragment), "binary fragment write failed");
}

void client_read(int fd, mixed message) {
  ASSERT2(0, "binary client received unexpected data");
}

void begin_test() {
  string local;

  local = socket_address(server_fd, 1);
  client_fd = socket_create(STREAM_BINARY, "client_read");
  ASSERT2(client_fd >= 0, "failed to create binary client socket");
  ASSERT_EQ(EESUCCESS, socket_connect(client_fd, local, "client_read", "client_write"));
}

void do_tests() {
  completion_token = "/command/tests"->begin_async();
  server_fd = socket_create(STREAM_BINARY, "server_accept_ready");
  ASSERT2(server_fd >= 0, "failed to create binary server socket");
  ASSERT_EQ(EESUCCESS, socket_bind(server_fd, 0, "127.0.0.1 0"));
  ASSERT_EQ(EESUCCESS, socket_listen(server_fd, "server_accept_ready"));
  call_out("begin_test", 0);
  call_out("timeout_fail", 5);
}

#else
void do_tests() {
  write("PACKAGE_SOCKETS not available, skipping binary stream fragment test.\n");
}
#endif
