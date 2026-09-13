void do_tests() {
#ifndef __PACKAGE_CONTRIB__
  write("PACKAGE_CONTRIB not enabled, test did not run.\n");
#else
  mixed result = pluralize("a of b");

  ASSERT(stringp(result));
  ASSERT_EQ("cups of tea", pluralize("a cup of tea"));

  // Words ending in -ff take a plain -s (they used to become "bluves"),
  // while the staff -> staves exception survives.
  ASSERT_EQ("bluffs", pluralize("bluff"));
  ASSERT_EQ("chaffs", pluralize("chaff"));
  ASSERT_EQ("cliffs", pluralize("cliff"));
  ASSERT_EQ("staves", pluralize("staff"));

  // The Latin -is -> -es rule stays for thesis/axis but not for words like
  // penis/marquis (they used to become "penes"/"marques").
  ASSERT_EQ("theses", pluralize("thesis"));
  ASSERT_EQ("axes", pluralize("axis"));
  ASSERT_EQ("penises", pluralize("penis"));
  ASSERT_EQ("marquises", pluralize("marquis"));
#endif
}
