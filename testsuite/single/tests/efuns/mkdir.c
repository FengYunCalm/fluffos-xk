void do_tests() {
  string path = "/u11_mkdir";

  rmdir(path);
  ASSERT_EQ(-1, file_size(path));
  ASSERT_EQ(1, mkdir(path));
  ASSERT_EQ(-2, file_size(path));
  ASSERT_EQ(0, mkdir(path));
  ASSERT_EQ(1, rmdir(path));
  ASSERT_EQ(-1, file_size(path));
}
