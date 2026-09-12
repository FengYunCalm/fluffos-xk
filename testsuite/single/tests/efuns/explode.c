void do_tests() {
  string tmp = "this is a test";
  mixed *ret;

  // Edge cases
  ASSERT_EQ(({ "abcd" }), explode("abcd", " and "));
  ASSERT_EQ(({ "abcd" }), explode_reversible("abcd", " and "));
  ASSERT_EQ(({ }), explode("", " and "));
  ASSERT_EQ(({ }), explode_reversible("", " and "));
  ASSERT_EQ(({ " an" }), explode(" an", " and "));
  ASSERT_EQ(({ " an" }), explode_reversible(" an", " and "));

  ret = explode(tmp, "");
  ASSERT_EQ(({ "t", "h", "i", "s", " ", "i", "s", " ", "a", " ", "t", "e", "s", "t"}), ret);
  ASSERT_EQ(ret, explode_reversible(tmp, ""));
  ASSERT_EQ(tmp, implode(explode_reversible(tmp, ""), ""));

  ret = explode(tmp, " ");
  ASSERT_EQ(({ "this", "is", "a", "test" }), ret);
  ASSERT_EQ(ret, explode_reversible(tmp, " "));
  ASSERT_EQ(tmp, implode(explode_reversible(tmp, " "), " "));

  ret = explode(" " + tmp, " ");
#ifndef __REVERSIBLE_EXPLODE_STRING__
  ASSERT_EQ(({ "this", "is", "a", "test" }), ret);
#else
  ASSERT_EQ(({ "", "this", "is", "a", "test" }), ret);
#endif
  ASSERT_EQ(({ "", "this", "is", "a", "test" }), explode_reversible(" " + tmp, " "));
  ASSERT_EQ(" " + tmp, implode(explode_reversible(" " + tmp, " "), " "));

  ret = explode("     " + tmp, " ");
#ifndef __REVERSIBLE_EXPLODE_STRING__
#ifdef __SANE_EXPLODE_STRING__
  ASSERT_EQ(({ "", "", "", "", "this", "is", "a", "test" }), ret);
#else
  ASSERT_EQ(({ "this", "is", "a", "test" }), ret);
#endif
#else
  ASSERT_EQ(({ "", "", "", "", "", "this", "is", "a", "test" }), ret);
#endif
  ASSERT_EQ(({ "", "", "", "", "", "this", "is", "a", "test" }), explode_reversible("     " + tmp, " "));
  ASSERT_EQ("     " + tmp, implode(explode_reversible("     " + tmp, " "), " "));

  tmp = "this  is  a  test  ";
  ret = explode(tmp, "  ");
#ifndef __REVERSIBLE_EXPLODE_STRING__
  ASSERT_EQ(({ "this", "is", "a", "test" }), ret);
#else
  ASSERT_EQ(({ "this", "is", "a", "test", "" }), ret);
#endif
  ASSERT_EQ(tmp, implode(explode_reversible(tmp, "  "), "  "));
  ASSERT_EQ(({ "this", "is", "a", "test", "" }), explode_reversible(tmp, "  "));


  ret = explode("  " + tmp, "  ");
#ifndef __REVERSIBLE_EXPLODE_STRING__
  ASSERT_EQ(({ "this", "is", "a", "test" }), ret);
#else
  ASSERT_EQ(({ "", "this", "is", "a", "test", "" }), ret);
#endif
  ASSERT_EQ(({ "", "this", "is", "a", "test", "" }), explode_reversible("  " + tmp, "  "));

  ret = explode("      " + tmp, "  ");
#ifndef __REVERSIBLE_EXPLODE_STRING__
#ifdef __SANE_EXPLODE_STRING__
  ASSERT_EQ(({ "", "", "this", "is", "a", "test" }), ret);
#else
  ASSERT_EQ(({ "this", "is", "a", "test" }), ret);
#endif
#else
  ASSERT_EQ(({ "", "", "", "this", "is", "a", "test", "" }), ret);
#endif
  ASSERT_EQ(({ "", "", "", "this", "is", "a", "test", "" }), explode_reversible("      " + tmp, "  "));

  tmp = "..x.y..z..";
#ifndef __REVERSIBLE_EXPLODE_STRING__
#ifdef __SANE_EXPLODE_STRING__
  ASSERT_EQ(({ "", "x", "y", "", "z", "" }), explode(tmp, "."));
#else
  ASSERT_EQ(({ "x", "y", "", "z", "" }), explode(tmp, "."));
#endif
#else
  ASSERT_EQ(({ "", "", "x", "y", "", "z", "", "" }), explode(tmp, "."));
#endif
  ASSERT_EQ(({ "", "", "x", "y", "", "z", "", "" }), explode_reversible(tmp, "."));

  ASSERT_EQ(
      ({"lh15970183750", "abcdefghigk", "werert"}),
      explode("lh15970183750║abcdefghigk║werert", "║"));

  // Many ASCII tokens exercise the iterator subrange fast path without
  // exceeding the configured array limit.
  {
    int n = 8000;
    string many = repeat_string("abcdefghij ", n);
    mixed *parts = explode(many, " ");
    ASSERT_EQ(n, sizeof(parts));
    ASSERT_EQ("abcdefghij", parts[0]);
    ASSERT_EQ("abcdefghij", parts[<1]);
    ASSERT_EQ(many, implode(explode_reversible(many, " "), " "));
  }

  // Empty delimiters split grapheme clusters; ASCII input should not force
  // an ICU walk for every byte.
  {
    int n = 8000;
    string many = repeat_string("a", n);
    mixed *chars = explode(many, "");
    ASSERT_EQ(n, sizeof(chars));
    ASSERT_EQ("a", chars[0]);
    ASSERT_EQ("a", chars[<1]);
    ASSERT_EQ(many, implode(chars, ""));
  }

  // A multi-byte delimiter must fit entirely inside the counted haystack;
  // trailing-delimiter trimming leaves bytes beyond that range in the C
  // string and must not let a strstr-based search consume them.
  ASSERT_EQ(({ "ab-" }), explode("ab---", "--"));
  ASSERT_EQ(({ "你-" }), explode("你---", "--"));
  ASSERT_EQ(({ "你-", "" }), explode("你-----", "--"));
  ASSERT_EQ(({ "你-", "", "" }), explode_reversible("你-----", "--"));
  ASSERT_EQ("你-----", implode(explode_reversible("你-----", "--"), "--"));
  ASSERT_EQ(({ "café text\n", "" }), explode("café text\n\n\n\n\n", "\n\n"));
  ASSERT_EQ(({ "a\r\nb-", "" }), explode("a\r\nb-----", "--"));

  {
    int n = 8000;
    string many = "x" + repeat_string(" ", n);
    mixed *parts = explode(many, " ");
    ASSERT_EQ(n, sizeof(parts));
    ASSERT_EQ("x", parts[0]);
    ASSERT_EQ("", parts[<1]);
  }
}
