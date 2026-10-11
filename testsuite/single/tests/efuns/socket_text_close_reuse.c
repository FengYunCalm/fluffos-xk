#ifdef __PACKAGE_SOCKETS__

#include <globals.h>

#define EESUCCESS 1
#define STREAM 1

nosave int server_fd = -1;
nosave int client_fd = -1;
nosave int accepted_fd = -1;
nosave int first_accepted_fd = -1;
nosave int phase = 0;
nosave int test_completed = 0;
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
    ASSERT2(0, "stream text close/reuse test timed out");
  }
}

void finish_test() {
  test_completed = 1;
  remove_call_out("timeout_fail");
  cleanup();
  "/command/tests"->complete_async(completion_token);
}

void client_read(int fd, mixed message) {
  ASSERT2(0, "client received unexpected data");
}

void client_write(int fd) {
  buffer fragment;

  if (phase == 0) {
    fragment = allocate_buffer(2);
    fragment[0] = 0xe4;
    fragment[1] = 0xb8;
    ASSERT2(EESUCCESS == socket_write(fd, fragment), "pending fragment write failed");
    phase = 1;
    call_out("close_pending_server_socket", 0.1);
    return;
  }
  if (phase == 2) {
    fragment = allocate_buffer(1);
    fragment[0] = 'A';
    ASSERT2(EESUCCESS == socket_write(fd, fragment), "reuse payload write failed");
    phase = 3;
  }
}

void close_pending_server_socket() {
  ASSERT2(accepted_fd >= 0, "pending accepted socket disappeared");
  first_accepted_fd = accepted_fd;
  ASSERT_EQ(EESUCCESS, socket_close(accepted_fd));
  accepted_fd = -1;
  if (client_fd != -1) {
    ASSERT_EQ(EESUCCESS, socket_close(client_fd));
    client_fd = -1;
  }
  call_out("start_reused_client", 0.1);
}

void start_reused_client() {
  string local;

  phase = 2;
  local = socket_address(server_fd, 1);
  client_fd = socket_create(STREAM, "client_read");
  ASSERT2(client_fd >= 0, "failed to create reused client socket");
  ASSERT_EQ(EESUCCESS, socket_connect(client_fd, local, "client_read", "client_write"));
}

void server_read(int fd, string message) {
  ASSERT2(phase == 3, sprintf("unexpected server read in phase %d", phase));
  ASSERT2(message == "A",
          sprintf("stale UTF-8 tail survived local close/reuse: %O", message));
  ASSERT2(fd == first_accepted_fd,
          sprintf("accepted socket was not reused: first=%d second=%d", first_accepted_fd, fd));
  ASSERT_EQ(EESUCCESS, socket_close(fd));
  accepted_fd = -1;
  call_out("finish_test", 0);
}

void server_write(int fd) {
}

void server_accept_ready(int fd) {
  accepted_fd = socket_accept(fd, "server_read", "server_write");
  ASSERT2(accepted_fd >= 0, "socket_accept failed");
}

void begin_test() {
  string local;

  local = socket_address(server_fd, 1);
  client_fd = socket_create(STREAM, "client_read");
  ASSERT2(client_fd >= 0, "failed to create initial client socket");
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
  write("PACKAGE_SOCKETS not available, skipping stream text close/reuse test.\n");
}
#endif
