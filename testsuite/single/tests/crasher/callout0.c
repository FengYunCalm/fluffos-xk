int executed = 1;
private int completion_token;

void test() {
    int h = call_out("test", 0);
    ASSERT_EQ(0, find_call_out(h));
    write(sprintf("callout0: %d\n", executed));
  if (++executed > 3) {
    ASSERT(remove_call_out(h) >= 0);
    "/command/tests"->complete_async(completion_token);
  }
}
void do_tests() {
  completion_token = "/command/tests"->begin_async();
    call_out("test", 0.1);
}
