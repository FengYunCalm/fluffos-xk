// I15 helper: call a target lifecycle operation from a distinct object so
// the transaction guard is tested at the real entry point. The mode lives
// outside the transaction target so __INIT cannot reset the probe itself.
int mode = 0;
int last_mode = 0;

void set_mode(int value) {
  mode = value;
}

int query_mode() {
  return mode;
}

void set_last_mode(int value) {
  last_mode = value;
}

int query_last_mode() {
  return last_mode;
}

string last_recompile_error;
function escaped_fp;

void store_escaped_fp(function fp) {
  escaped_fp = fp;
}

int call_escaped_fp() {
  if (!functionp(escaped_fp)) return -1;
  mixed caught = catch(escaped_fp(123));
  return stringp(caught);
}

// Driver-side tests call this object from a non-master LPC frame. Keeping the
// public efun call here avoids confusing the master::flag() test harness frame
// with the short-lived authorization hook frame.
int run_master_recompile_mode(int value) {
  object target = master();
  target->set_recompile_create_probe(value);
  mixed caught = catch(recompile_object(target));
  last_recompile_error = caught ? sprintf("%O", caught) : "";
  return caught ? 0 : 1;
}

string query_last_recompile_error() {
  return last_recompile_error;
}

void destroy_target(object target) {
  destruct(target);
}
