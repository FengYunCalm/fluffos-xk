// I15 negative-space fixture: this object is unrelated to the master target.
// Loading it from a Master transaction must remain allowed; the new-compile
// barrier is specific to an active SimulEfun dispatch transaction.
int probe_value() {
  return 19;
}
