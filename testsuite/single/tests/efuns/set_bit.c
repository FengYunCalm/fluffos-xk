void do_tests() {
  string str = set_bit("", 100);

  ASSERT(catch(set_bit("", -2)));
  ASSERT(test_bit(str, 100));
  for (int i = 0; i < 200; i++) {
    if (i != 100) {
      ASSERT(!test_bit(str, i));
    }
  }

  str = clear_bit(str, 100);
  ASSERT(!test_bit(str, 100));
}
