// I15 helper: this source is deliberately loaded for the first time from a
// simul_efun create() probe. It must not be compiled while dispatch indices
// are temporarily active.
int probe_value() { return 1; }
