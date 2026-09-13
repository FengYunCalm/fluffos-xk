// L7 stress: repeated recompile_object() interleaved with N worker clones
// doing cross-object VM work on every round. Each reload round must quiesce
// without timeout and without failures; the contract runs under ASan
// (build-recompile-asan + etc/config.recompile) to prove zero sanitizer
// reports under load.
//
// ftest model note: the test frame is synchronous -- call_out / heart_beat
// are scheduled but never advanced (same as the sibling contract test
// recompile_object.c, which schedules them as probes only). True multi-owner
// thread concurrency is covered by owner_runtime_bench (C++ side); this LPC
// contract covers cross-object context switching against reload rounds.
// Full contract runs with etc/config.recompile (enable recompile object : 1);
// under etc/config.test (default off) it verifies the disabled error only.

#define N_WORKERS 4
#define ROUNDS 5
#define PER_ROUND 200
// L7 extension (E3 v2): rounds that hot-reload the master object, the target the
// v1 gate used to reject. The master is the object every apply goes through, so
// a swap here exercises the caches the v2 work rebuilt (apply cache, master
// applies) rather than a leaf object's variables.
#define MASTER_ROUNDS 3

int total_recompiles;
int master_recompiles;
// Kept as an assertion slot for future stress extensions (no increment
// path in the current synchronous form; ASSERT_EQ(0, failures) below is
// the canary).
int failures;

int run_reload_round(object blueprint) {
  for (int i = 0; i < PER_ROUND; i++) {
    int n = recompile_object(blueprint);
    if (n < 1) {
      return 0;
    }
    total_recompiles++;
  }
  return 1;
}

void do_tests_p6() {
  if (!get_config(326)) return;  // CFG_INT(70) = __RECOMPILE_OBJECT_ENABLED__

  object blueprint = load_object("/clone/recompile_blueprint");
  // N worker clones: each round they run VM work (owner/object context
  // switches) before the reload round, then we verify their state survived.
  mapping workers = ([]);
  for (int i = 0; i < N_WORKERS; i++) {
    workers[i] = new("/clone/recompile_blueprint");
  }

  int ok = 1;
  for (int r = 0; r < ROUNDS; r++) {
    foreach (object w in values(workers)) {
      w->set_value(w->get_value() + 1);
    }
    if (!run_reload_round(blueprint)) {
      ok = 0;
      break;
    }
    // Every round also attempts the master target. The reload must be refused
    // while the master's own program is executing (this test runs through
    // master applies), which is the protection that keeps an executing target
    // consistent rather than a v1 gate. The master-target rounds that can run
    // are driven from the driver side, where the master is not on the stack:
    // DriverTest.TestMasterReloadStressRounds (12 rounds, apply cache and
    // authorization apply verified after every swap).
    if (r < MASTER_ROUNDS) {
      mixed master_err = catch(recompile_object(master()));
      if (!stringp(master_err) || strsrch(master_err, "target program is executing") == -1) {
        ok = 0;
        break;
      }
      master_recompiles++;
    }
  }

  ASSERT_EQ(1, ok);
  ASSERT_EQ(ROUNDS * PER_ROUND, total_recompiles);
  ASSERT_EQ(MASTER_ROUNDS, master_recompiles);
  ASSERT_EQ(0, failures);
  ASSERT_EQ(7, blueprint->get_value());  // blueprint variables stable

  // Workers kept their per-round increments across every swap.
  foreach (object w in values(workers)) {
    ASSERT_EQ(7 + ROUNDS, w->get_value());  // 7 (blueprint default) + ROUNDS increments
  }
  foreach (object w in values(workers)) {
    destruct(w);
  }

  // Terminal proof line: shows the reload round actually ran (a silently
  // skipped do_tests_p6 would leave total_recompiles at 0).
  write(sprintf("recompile_stress: %d recompiles ok, %d master targets refused while executing,"
                " %d failures, %d workers\n",
                total_recompiles, master_recompiles, failures, N_WORKERS));
}

void do_tests() {
  if (!get_config(326)) {  // disabled config: stable rejection only
    mixed err = catch(recompile_object(this_object()));
    ASSERT2(stringp(err), "expected disabled");
    return;
  }
  do_tests_p6();
}
