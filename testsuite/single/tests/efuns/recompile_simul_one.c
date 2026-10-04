// Minimal simul_efun reload regression for the cumulative dispatch table.
int do_tests() {
  object sefun = find_object("/single/simul_efun");
  ASSERT(sefun);
  ASSERT_EQ("dGVzdA==", base64encode("test"));

  if (!get_config(326)) {
    mixed err = catch(recompile_object(sefun));
    ASSERT2(stringp(err), "expected disabled");
    return;
  }

  ASSERT_EQ(1, recompile_object(sefun));
  ASSERT_EQ("dGVzdA==", base64encode("test"));
  write("one simul reload ok\n");
}
