// E3 recompile_object() v1 contract (v0.4 §5/§11): same-layout hot swap of a
// blueprint and its clones, no __INIT, stale funptr rejection, executing
// guard, disabled and permission failures, heartbeat/call_out survival,
// repeated reloads.
// Full contract runs with etc/config.recompile (enable recompile object : 1);
// under etc/config.test (default off) it verifies the disabled error only.

// Self-reload probe: a family member calling recompile_object() on its own
// blueprint must hit the "target program is executing" guard.
object family_blueprint;
int self_reload() {
  mixed err = catch(recompile_object(family_blueprint));
  if (stringp(err) && strsrch(err, "target program is executing") != -1) {
    return 1;
  }
  return 0;
}

int hb_count = 0;
int callout_count = 0;
void heart_beat() { hb_count++; }
void on_callout() { callout_count++; }

void do_tests_p6() {
  if (!get_config(326)) return;  // CFG_INT(70) = __RECOMPILE_OBJECT_ENABLED__

  object blueprint = load_object("/clone/recompile_blueprint");
  object worker = new("/clone/recompile_blueprint");
  worker->set_heart_beat(1);
  call_out("on_callout", 1);

  // Repeated recompiles: generations, refcounts and queues stay stable.
  int ok = 1;
  for (int i = 0; i < 200; i++) {
    int n = recompile_object(blueprint);
    if (n < 1) { ok = 0; break; }
  }
  ASSERT_EQ(1, ok);
  ASSERT_EQ(7, worker->get_value());  // variables preserved across all swaps

  destruct(worker);
}

void do_tests() {
  // Default-off (config.test): the efun must stably reject.
  if (!get_config(326)) {  // CFG_INT(70) = __RECOMPILE_OBJECT_ENABLED__ (70 + 256 base)
    mixed err = catch(recompile_object(this_object()));
    ASSERT2(stringp(err), "expected disabled");
    return;
  }

  // The blueprint family lives in /clone/recompile_blueprint; THIS object
  // (running do_tests) is not a member, so the swap below is legal.
  object blueprint = load_object("/clone/recompile_blueprint");
  object clone_a = new("/clone/recompile_blueprint");
  object clone_b = new("/clone/recompile_blueprint");
  clone_a->set_value(100);
  clone_b->set_value(200);
  ASSERT_EQ(1, blueprint->version());
  ASSERT_EQ(100, clone_a->get_value());

  // Old funptr created on a family member before the swap must go stale
  // afterwards (the owner's generation is bumped by the swap).
  function old_fp = clone_a->make_fp();
  function old_functional_fp = clone_a->make_functional_fp();
  ASSERT_EQ(1, old_fp());
  ASSERT_EQ(1, old_functional_fp());

  int n = recompile_object(blueprint);
  ASSERT_EQ(3, n);  // blueprint + 2 clones

  // The scoped transaction context is already Entered while the master
  // authorization hook runs. A nested public recompile must fail there, while
  // the outer transaction still completes normally.
  object master = load_object("/single/master");
  master->set_recompile_nested_probe(1);
  int nested_count = recompile_object(blueprint);
  ASSERT_EQ(3, nested_count);
  if (master->query_recompile_nested_probe_result() != 1) {
    OUTPUT("nested recompile error: " + master->query_recompile_nested_probe_error());
  }
  ASSERT_EQ(1, master->query_recompile_nested_probe_result());
  master->set_recompile_nested_probe(0);

  // Variables preserved on every member of the family.
  ASSERT_EQ(100, clone_a->get_value());
  ASSERT_EQ(200, clone_b->get_value());
  ASSERT_EQ(7, blueprint->get_value());

  // Old funptr is now stale: stable error, no re-resolution against the
  // new program.
  mixed err = catch(old_fp());
  ASSERT2(stringp(err), "expected stale function pointer error");
  mixed functional_err = catch(old_functional_fp());
  ASSERT2(stringp(functional_err),
          "expected stale functional pointer error");
  function rebound_functional_fp = bind(old_functional_fp, this_object());
  mixed rebound_functional_err = catch(rebound_functional_fp());
  ASSERT2(stringp(rebound_functional_err),
          "bind must preserve stale functional pointer state");

  // A funptr created after the swap snapshots the new generation.
  function new_fp = clone_a->make_fp();
  function new_functional_fp = clone_a->make_functional_fp();
  ASSERT_EQ(1, new_fp());
  ASSERT_EQ(1, new_functional_fp());

  // Executing guard: a family member reloading its own blueprint must be
  // rejected with "target program is executing" (top-level frame).
  object self = new("/clone/recompile_blueprint");
  self->set_family_blueprint(blueprint);
  ASSERT_EQ(1, self->self_reload());

  destruct(self);
  destruct(clone_b);
  destruct(clone_a);
  destruct(blueprint);

  // ftest itself runs inside master::flag(). Reloading master from this
  // active master frame must be rejected before swap; accepting it would
  // free the program that still owns the outer interpreter frame.
  mixed master_error = catch(recompile_object(master));
  ASSERT2(stringp(master_error) &&
              strsrch(master_error, "target program is executing") != -1,
          "master reload from the active master frame must be rejected");

  // Exercise the trace/error path after the rejection. The old implementation
  // freed the active master program and then failed here in get_svalue_trace().
  mixed trace_probe_error = catch(error("trace probe after master rejection\\n"));
  ASSERT2(stringp(trace_probe_error), "trace after active-master rejection must be safe");

  // Simul dispatch activation has an additional escape: no unrelated LPC
  // source may be lazily compiled while temporary simul indices are live.
  object simul = load_object("/single/simul_efun");
  simul->set_recompile_simul_probe(1);
  mixed simul_error = catch(recompile_object(simul));
  ASSERT2(stringp(simul_error), "simul lazy compile must be rejected");
  simul->set_recompile_simul_probe(0);

  // The same transaction barrier must cover every lifecycle entry used by
  // simul_efun::create(), not only the lazy-compile escape above. The helper
  // is outside the target so mode 8 exercises reverse destruction.
  object lifecycle_probe = load_object("/clone/recompile_lifecycle_probe");
  simul->set_recompile_simul_probe_helper(lifecycle_probe);
  for (int mode = 2; mode <= 8; mode++) {
    simul->set_recompile_simul_probe(mode);
    mixed lifecycle_error = catch(recompile_object(simul));
    ASSERT2(stringp(lifecycle_error),
            "simul lifecycle mutation must be rejected");
    simul->set_recompile_simul_probe(0);
    ASSERT_EQ(1, simul->recompile_simul_lifecycle_alive());
  }
  // I05: a function pointer born during a failing transaction escapes through
  // a non-target owner. Rollback must invalidate it permanently; a later
  // successful retry must not clear that state by restoring the generation.
  simul->set_recompile_simul_probe(9);
  mixed i05_error = catch(recompile_object(simul));
  ASSERT2(stringp(i05_error), "I05 create failure must roll back");
  ASSERT_EQ(1, lifecycle_probe->call_escaped_fp());
  simul->set_recompile_simul_probe(0);
  ASSERT_EQ(1, recompile_object(simul));
  ASSERT_EQ(1, lifecycle_probe->call_escaped_fp());
  simul->set_recompile_simul_probe(10);
  ASSERT_EQ(1, recompile_object(simul));
  ASSERT_EQ(0, lifecycle_probe->call_escaped_fp());

  destruct(lifecycle_probe);

  do_tests_p6();
}
