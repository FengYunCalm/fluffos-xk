void do_tests() {
  object master = find_object("/single/master");
  string path = "/u11_set_hide";
  string report;

  set_hide(1);
  master->set_hide_audit_mode(1);
  rm(path);
  dumpallobj(path);
  report = read_file(path) || "";
  master->set_hide_audit_mode(0);
  set_hide(0);
  rm(path);

  ASSERT(strsrch(report, base_name(this_object())) < 0);
}
