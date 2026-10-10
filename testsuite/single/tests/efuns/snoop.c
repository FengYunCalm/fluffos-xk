void do_tests() {
  ASSERT(snoop(this_object()) == this_object());
  ASSERT(catch(snoop(this_object(), this_object())));
}
