void do_tests() {
    mapping m = allocate_mapping(random(1000));
    mixed x, y;

  ASSERT_EQ(sizeof(m), 0);
    
    foreach (x, y in m) {
	ASSERT(0); // shouldn't get here
    }
}
