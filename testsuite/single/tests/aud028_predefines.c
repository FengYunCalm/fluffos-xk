void do_tests() {
  ASSERT(stringp(__VERSION__) && sizeof(__VERSION__) > 0);
  ASSERT(stringp(__ARCH__) && sizeof(__ARCH__) > 0);
  ASSERT(stringp(__COMPILER__) && sizeof(__COMPILER__) > 0);
  ASSERT(stringp(__CXXFLAGS__));
}
