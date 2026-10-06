void do_the_nasty_deed() {
    write("all tests finished, shutting down.\n");
    shutdown(0);
    ASSERT(0);
}

void do_tests() {
  // The test controller calls shutdown only after its completion barrier.
  ASSERT(function_exists("do_the_nasty_deed", this_object()));
}
