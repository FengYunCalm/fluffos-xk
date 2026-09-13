void do_tests() {
    ASSERT(member_array('c', "foo") == -1);
    ASSERT(member_array('b', "abar") == 1);
    ASSERT(member_array('y', "xyzzy") == 1);
    ASSERT(member_array('y', "xyzzy", 2) == 4);
    ASSERT(member_array(2, ({ 1, 2, 3 })) == 1);
    ASSERT(member_array("foo", ({ 1, "foo", 3 })) == 1);

    // flag 1: prefix match.
    ASSERT(member_array("fo", ({ "bar", "foo", 3 }), 0, 1) == 1);
    ASSERT(member_array("fo", ({ "bar", "foo", 3 })) == -1);

    // flag 2: search backwards from the end.
    ASSERT(member_array("foo", ({ "foo", "bar", "foo" }), 0, 2) == 2);

    // flag 4: 'item' is a predicate called with each element; the first
    // element it accepts wins. (flag 4 used to be ignored entirely.)
    ASSERT(member_array((: $1 > 1 :), ({ 1, 2, 3 }), 0, 4) == 1);
    ASSERT(member_array((: $1 > 9 :), ({ 1, 2, 3 }), 0, 4) == -1);
    ASSERT(member_array((: $1 == "b" :), ({ "a", "b", "c" }), 0, 4) == 1);
    ASSERT(member_array((: $1 == "b" :), ({ "a", "b", "c" }), 2, 4) == -1);
}
