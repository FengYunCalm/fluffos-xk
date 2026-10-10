void do_tests() {
  string default_report;
  string report;
  string path = "/u11_dumpallobj";

  rm("/OBJ_DUMP");
  dumpallobj();
  default_report = read_file("/OBJ_DUMP") || "";
  ASSERT(sizeof(default_report) > 0);
  ASSERT(strsrch(default_report, base_name(this_object())[1..]) >= 0);
  ASSERT(rm("/OBJ_DUMP"));

  rm(path);
  dumpallobj(path);
  report = read_file(path) || "";
  ASSERT(sizeof(report) > 0);
  ASSERT(strsrch(report, base_name(this_object())[1..]) >= 0);
  ASSERT(rm(path));
}
