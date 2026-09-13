void do_tests() {
  // control characters doesn't have width
  ASSERT_EQ(9, strwidth("what e\nver"));

  ASSERT_EQ(10 + 2 + 10, strwidth("欲穷千里目👩‍👩‍👧‍👧更上一层楼"));

  // The skin-tone modifier U+1F3FB..U+1F3FF merges into the preceding emoji
  // base, so the whole ZWJ cluster is one double-width glyph (this used to
  // count the modifier as a second 2-wide codepoint, returning 4).
  ASSERT_EQ(2, strwidth("\uD83E\uDD26\uD83C\uDFFB\u200D♂\uFE0F"));
}
