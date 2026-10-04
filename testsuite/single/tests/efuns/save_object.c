int x = 0;
#ifndef __SENSIBLE_MODIFIERS__
static int z = 5;
#else
nosave int z = 5;
#endif
int y = 0x7fffffffffffffff;

void test_rename_failure(int flags) {
    string stem = "/save_object_rename_" + flags;
    string target = stem + (flags ? ".o.gz" : ".o");

    ASSERT_EQ(file_size(target), -1);
    ASSERT(mkdir(target));
    ASSERT(write_file(target + "/sentinel", "original target"));
    ASSERT_EQ(save_object(stem, flags), 0);
    ASSERT_EQ(file_size(target), -2);
    ASSERT_EQ(read_file(target + "/sentinel"), "original target");
    ASSERT_EQ(file_size(target + ".tmp"), -1);
    ASSERT(rm(target + "/sentinel"));
    ASSERT(rmdir(target));

    ASSERT(save_object(stem, flags) > 0);
    ASSERT(file_size(target) > 0);
    ASSERT(rm(target));
}

void do_tests() {
    ASSERT_EQ(24, save_object("/sf"));
    ASSERT_EQ(read_file("/sf.o") , "#" + __FILE__ + "\ny " + MAX_INT + "\n");
    save_object("/sf", 1);
    ASSERT_EQ(read_file("/sf.o"),  "#" + __FILE__ + "\nx 0\ny " + MAX_INT + "\n");

    test_rename_failure(0);
    test_rename_failure(2);

    // Fluffos new behavior.
    ASSERT_EQ(save_object(0), "#" + __FILE__ + "\ny " + MAX_INT + "\n");
    ASSERT_EQ(save_object(1), "#" + __FILE__ + "\nx 0\ny " + MAX_INT + "\n");
}
