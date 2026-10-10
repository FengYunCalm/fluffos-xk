void do_tests() {
  mapping before = debug_levels();
  int was_set = before["LPC"] != 0;

  set_debug_level("LPC");
  ASSERT(debug_levels()["LPC"] != 0);
  clear_debug_level("LPC");
  ASSERT_EQ(0, debug_levels()["LPC"]);

  if (was_set) {
    set_debug_level("LPC");
  }
}
