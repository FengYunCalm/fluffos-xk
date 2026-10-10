#include "test_lpc_support.h"

TEST(StrUtilsTest, TrimCharsetMatchesUtf8Scalars) {
  const std::string book = "\xE3\x80\x8A" "\xE4\xB8\x89" "\xE5\xAD\x97" "\xE7\xBB\x8F" "\xE3\x80\x8B";
  const std::string ideographic_space = "\xE3\x80\x80";
  const std::string wrapped = ideographic_space + book + ideographic_space;

  EXPECT_EQ(book, trim(book, ideographic_space));
  EXPECT_EQ(book, trim(wrapped, ideographic_space));
  EXPECT_EQ(book + ideographic_space, ltrim(wrapped, ideographic_space));
  EXPECT_EQ(ideographic_space + book, rtrim(wrapped, ideographic_space));
  EXPECT_EQ("\xE4\xB8\x89" "\xE5\xAD\x97" "\xE7\xBB\x8F",
            trim(book, "\xE3\x80\x8A" "\xE3\x80\x8B"));

  EXPECT_EQ(wrapped, trim(wrapped, ""));
  EXPECT_EQ(wrapped, trim(wrapped));

  const std::string invalid_prefix = "\x80x";
  const std::string invalid_suffix = "x\x80";
  const std::string invalid = "\x80";
  EXPECT_EQ(invalid_prefix, ltrim(invalid_prefix, "x"));
  EXPECT_EQ(invalid_suffix, rtrim(invalid_suffix, "x"));
  EXPECT_EQ(invalid, trim(invalid, invalid));
}

TEST(MudlibStatsTest, ArraySizeUpdatesAreAtomicAcrossVmThreads) {
  mudlib_stats_t stats{};
  statgroup_t group{&stats, nullptr};
  constexpr int kThreadCount = 8;
  constexpr int kUpdatesPerThread = 5000;
  std::vector<std::thread> workers;
  workers.reserve(kThreadCount);

  for (int i = 0; i < kThreadCount; i++) {
    workers.emplace_back([&group] {
      for (int update = 0; update < kUpdatesPerThread; update++) {
        add_array_size(&group, 1);
      }
    });
  }
  for (auto &worker : workers) {
    worker.join();
  }

  ASSERT_EQ(stats.size_array, kThreadCount * kUpdatesPerThread);
}

TEST(StrallocAccountingTest, UintMaxLengthPreservesWraparoundAndEvaluatesOnce) {
  StringStatsSnapshot snapshot;

  unsigned int evaluations = 0;
  const auto uint_max_length = [&evaluations]() {
    evaluations++;
    return std::numeric_limits<unsigned int>::max();
  };

  ADD_NEW_STRING(uint_max_length(), 0);
  EXPECT_EQ(evaluations, 1u);
  EXPECT_EQ(num_distinct_strings.load(std::memory_order_relaxed),
            snapshot.num_distinct_strings_value + 1);
  EXPECT_EQ(bytes_distinct_strings.load(std::memory_order_relaxed),
            snapshot.bytes_distinct_strings_value);

  SUB_NEW_STRING(uint_max_length(), 0);
  EXPECT_EQ(evaluations, 2u);
  EXPECT_EQ(num_distinct_strings.load(std::memory_order_relaxed),
            snapshot.num_distinct_strings_value);
  EXPECT_EQ(bytes_distinct_strings.load(std::memory_order_relaxed),
            snapshot.bytes_distinct_strings_value);

  ADD_STRING(uint_max_length());
  EXPECT_EQ(evaluations, 3u);
  EXPECT_EQ(allocd_strings.load(std::memory_order_relaxed), snapshot.allocd_strings_value + 1);
  EXPECT_EQ(allocd_bytes.load(std::memory_order_relaxed), snapshot.allocd_bytes_value);

  SUB_STRING(uint_max_length());
  EXPECT_EQ(evaluations, 4u);
  EXPECT_EQ(allocd_strings.load(std::memory_order_relaxed), snapshot.allocd_strings_value);
  EXPECT_EQ(allocd_bytes.load(std::memory_order_relaxed), snapshot.allocd_bytes_value);
}

TEST(StrallocAccountingTest, ShrinkingCountedStringSubtractsTheRemovedBytes) {
  StringStatsSnapshot snapshot;

  char *value = new_string(8, "stralloc shrink accounting test");
  std::memset(value, 'x', 8);
  value[8] = '\0';
  EXPECT_EQ(bytes_distinct_strings.load(std::memory_order_relaxed),
            snapshot.bytes_distinct_strings_value + 9);
  EXPECT_EQ(allocd_bytes.load(std::memory_order_relaxed), snapshot.allocd_bytes_value + 9);

  value = extend_string(value, 0);
  value[0] = '\0';
  EXPECT_EQ(bytes_distinct_strings.load(std::memory_order_relaxed),
            snapshot.bytes_distinct_strings_value + 1);
  EXPECT_EQ(allocd_bytes.load(std::memory_order_relaxed), snapshot.allocd_bytes_value + 1);

  FREE_MSTR(value);
  EXPECT_EQ(num_distinct_strings.load(std::memory_order_relaxed),
            snapshot.num_distinct_strings_value);
  EXPECT_EQ(bytes_distinct_strings.load(std::memory_order_relaxed),
            snapshot.bytes_distinct_strings_value);
  EXPECT_EQ(allocd_strings.load(std::memory_order_relaxed), snapshot.allocd_strings_value);
  EXPECT_EQ(allocd_bytes.load(std::memory_order_relaxed), snapshot.allocd_bytes_value);
}

TEST(OwnerFutureStoreTest, TtlReapsPayloadFreeTerminalRecords) {
  OwnerFutureStore store;
  // Fake monotonic clock: TTL boundaries must be deterministic and must not
  // depend on the CI process uptime.
  uint64_t fake_now_ns = 1'000'000'000ULL;  // 1s
  store.set_clock_for_test([&fake_now_ns]() { return fake_now_ns; });

  auto record = owner_future_store_test_record(1, 101);
  store.admit_pending(record);
  ASSERT_TRUE(store.complete_for_task(101, "completed", "result", "").has_value());
  // Record is terminal with no payload: age it beyond TTL via the fake clock.
  const uint64_t aged_at_ns = fake_now_ns;  // terminalized at fake now
  {
    auto aged = owner_future_store_test_record(1, 101);
    aged.state = "completed";
    aged.terminal_at_ns = aged_at_ns;
    store.take(1);  // consume existing terminal record
    ASSERT_TRUE(store.restore_terminal_checked(aged));
  }

  // Boundary TTL-1: record must still be retained.
  fake_now_ns = aged_at_ns + OwnerFutureStore::kTerminalTtlNs - 1;
  store.reap_expired_terminal();
  ASSERT_TRUE(store.poll(1).has_value());
  ASSERT_EQ(store.reaped_terminal_count(), 0u);

  // Boundary TTL: record must be reaped at exactly the TTL boundary.
  fake_now_ns = aged_at_ns + OwnerFutureStore::kTerminalTtlNs;
  store.reap_expired_terminal();
  ASSERT_EQ(store.poll(1), std::nullopt);
  ASSERT_GE(store.reaped_terminal_count(), 1u);
  ASSERT_EQ(store.size(), 0);

  // Boundary TTL+1 on a fresh record: same deterministic reap.
  auto record2 = owner_future_store_test_record(2, 202);
  store.admit_pending(record2);
  ASSERT_TRUE(store.complete_for_task(202, "completed", "result", "").has_value());
  const uint64_t record2_terminal_at_ns = fake_now_ns;
  {
    auto aged = owner_future_store_test_record(2, 202);
    aged.state = "completed";
    aged.terminal_at_ns = record2_terminal_at_ns;
    store.take(2);
    ASSERT_TRUE(store.restore_terminal_checked(aged));
  }
  fake_now_ns = record2_terminal_at_ns + OwnerFutureStore::kTerminalTtlNs + 1;
  store.reap_expired_terminal();
  ASSERT_EQ(store.poll(2), std::nullopt);
  ASSERT_EQ(store.size(), 0);
}

TEST(OwnerFutureStoreTest, TtlKeepsPayloadBearingTerminalRecords) {
  OwnerFutureStore store;
  uint64_t fake_now_ns = 1'000'000'000ULL;
  store.set_clock_for_test([&fake_now_ns]() { return fake_now_ns; });
  auto record = owner_future_store_test_record(1, 101);
  store.admit_pending(record);
  auto completion = store.complete_for_task(101, "completed", "result", "");
  ASSERT_TRUE(completion.has_value());
  // Give the completed record a payload: TTL must never silently drop it.
  {
    auto payload = owner_future_store_test_record(1, 101);
    payload.state = "completed";
    payload.terminal_at_ns = fake_now_ns;
    payload.result = std::make_shared<VMFrozenValue>();
    store.take(1);
    ASSERT_TRUE(store.restore_terminal_checked(payload));
  }
  fake_now_ns += OwnerFutureStore::kTerminalTtlNs + 1;
  store.reap_expired_terminal();
  ASSERT_TRUE(store.poll(1).has_value());
  ASSERT_EQ(store.reaped_terminal_count(), 0u);
}

TEST(OwnerFutureStoreTest, CapacityRejectsNewPendingBeyondTerminalCap) {
  OwnerFutureStore store;
  const auto cap = OwnerFutureStore::kMaxTerminalRecords;
  // Fill the store with payload-bearing terminal records (never auto-reaped).
  for (size_t i = 0; i < cap; i++) {
    auto record = owner_future_store_test_record(i + 1, i + 1);
    record.state = "completed";
    record.terminal_at_ns = 1;
    record.result = std::make_shared<VMFrozenValue>();
    ASSERT_TRUE(store.restore_terminal_checked(record));
  }
  ASSERT_EQ(store.terminal_record_count(), cap);
  // One more pending submission must be rejected: no free lifecycle slot.
  auto rejected = owner_future_store_test_record(cap + 1, cap + 1);
  ASSERT_FALSE(store.admit_pending(rejected));
  ASSERT_EQ(store.capacity_reject_count(), 1u);
  ASSERT_EQ(store.poll(cap + 1), std::nullopt);
}

TEST(OwnerFutureStoreTest, KeepsTaskLookupUntilTerminalTake) {
  OwnerFutureStore store;
  store.admit_pending(owner_future_store_test_record(1, 101));

  auto pending_take = store.take(1);
  ASSERT_TRUE(pending_take.found);
  ASSERT_FALSE(pending_take.consumed);
  ASSERT_TRUE(store.complete_for_task(101, "completed", "result", "").has_value());

  auto terminal_take = store.take(1);
  ASSERT_TRUE(terminal_take.found);
  ASSERT_TRUE(terminal_take.consumed);
  ASSERT_FALSE(store.complete_for_task(101, "completed", "result", "").has_value());
  ASSERT_EQ(store.size(), 0);
}

TEST(OwnerFutureStoreTest, ReindexesOverwriteAndDuplicateTaskIds) {
  OwnerFutureStore store;
  store.admit_pending(owner_future_store_test_record(1, 101));
  store.admit_pending(owner_future_store_test_record(1, 202));
  ASSERT_FALSE(store.complete_for_task(101, "completed", "stale", "").has_value());

  store.admit_pending(owner_future_store_test_record(2, 202));
  ASSERT_TRUE(store.complete_for_task(202, "completed", "first", "").has_value());
  ASSERT_TRUE(store.complete_for_task(202, "completed", "second", "").has_value());
  ASSERT_FALSE(store.complete_for_task(202, "completed", "extra", "").has_value());

  ASSERT_TRUE(store.take(1).consumed);
  ASSERT_TRUE(store.take(2).consumed);
  ASSERT_EQ(store.size(), 0);
}

TEST(OwnerFutureStoreTest, PendingTaskLookupTracksTerminalTransition) {
  OwnerFutureStore store;
  store.admit_pending(owner_future_store_test_record(7, 707));
  ASSERT_TRUE(store.has_pending_for_task(707));
  ASSERT_TRUE(store.fail_terminal(7, "cancelled", true, false).changed);
  ASSERT_FALSE(store.has_pending_for_task(707));
  ASSERT_TRUE(store.take(7).consumed);
  ASSERT_FALSE(store.has_pending_for_task(707));
}

TEST(OwnerFutureStoreTest, PendingCountTracksRecordTransitions) {
  OwnerFutureStore store;
  store.admit_pending(owner_future_store_test_record(8, 808));
  ASSERT_EQ(store.pending_count(), 1);

  store.admit_pending(owner_future_store_test_record(8, 909));
  ASSERT_EQ(store.pending_count(), 1);
  ASSERT_TRUE(
      store.complete_for_task(909, "completed", "result", "").has_value());
  ASSERT_EQ(store.pending_count(), 0);

  // A terminal id cannot be re-admitted as pending (its lifecycle slot is
  // occupied until take/reap); a fresh id admits normally.
  ASSERT_FALSE(store.admit_pending(owner_future_store_test_record(8, 1001)));
  ASSERT_TRUE(store.admit_pending(owner_future_store_test_record(9, 1001)));
  ASSERT_EQ(store.pending_count(), 1);
  ASSERT_TRUE(store.fail_terminal(9, "cancelled", true, false).changed);
  ASSERT_EQ(store.pending_count(), 0);
  ASSERT_TRUE(store.take(8).consumed);
  ASSERT_TRUE(store.take(9).consumed);
  ASSERT_EQ(store.pending_count(), 0);
}

TEST(OwnerFutureStoreTest, RejectsNonTerminalCompletionStateWithoutCounterDrift) {
  OwnerFutureStore direct_store;
  direct_store.admit_pending(owner_future_store_test_record(9, 909));
  EXPECT_FALSE(direct_store.complete(9, "pending", "result", "").has_value());
  EXPECT_EQ(direct_store.state(9), OwnerFutureState::kPending);
  EXPECT_EQ(direct_store.pending_count(), 1);
  EXPECT_EQ(direct_store.completed_count(), 0u);
  EXPECT_EQ(direct_store.failed_count(), 0u);
  EXPECT_FALSE(direct_store.take(9).consumed);
  ASSERT_TRUE(direct_store.complete(9, "completed", "result", "").has_value());
  EXPECT_EQ(direct_store.pending_count(), 0);
  EXPECT_EQ(direct_store.completed_count(), 1u);
  EXPECT_TRUE(direct_store.take(9).consumed);

  OwnerFutureStore task_store;
  task_store.admit_pending(owner_future_store_test_record(10, 1010));
  EXPECT_FALSE(
      task_store.complete_for_task(1010, "cancelled", "", "invalid state").has_value());
  EXPECT_EQ(task_store.state(10), OwnerFutureState::kPending);
  EXPECT_EQ(task_store.pending_count(), 1);
  EXPECT_EQ(task_store.completed_count(), 0u);
  EXPECT_EQ(task_store.failed_count(), 0u);
}

// F03/R2-F05: the lifecycle-slot reservation contract. Admission reserves
// the terminal slot a pending will occupy, so after admitting `cap` pendings
// every completion succeeds and every admitted future id stays queryable:
// no quota path may ever turn a successfully admitted id into unknown.
TEST(OwnerFutureStoreTest, TerminalCapHoldsAcrossPendingToTerminalTransitions) {
  OwnerFutureStore store;
  const auto cap = OwnerFutureStore::kMaxTerminalRecords;
  const uint64_t count = cap;
  std::vector<uint64_t> future_ids;
  for (uint64_t i = 0; i < count; i++) {
    auto record = owner_future_store_test_record(i + 1, i + 1);
    ASSERT_TRUE(store.admit_pending(record)) << "admission " << i << " must succeed";
    future_ids.push_back(i + 1);
  }
  ASSERT_EQ(store.pending_count(), static_cast<int64_t>(count));
  ASSERT_EQ(store.terminal_record_count(), 0u);

  // The (cap+1)-th pending has no free lifecycle slot: admission rejects
  // atomically (no record created, no side effect).
  auto extra = owner_future_store_test_record(cap + 1, cap + 1);
  ASSERT_FALSE(store.admit_pending(extra));
  ASSERT_GE(store.capacity_reject_count(), 1u);
  ASSERT_EQ(store.poll(cap + 1), std::nullopt);

  uint64_t quota_rejected = 0;
  for (auto future_id : future_ids) {
    auto completion = store.complete(future_id, "completed", "result", "");
    ASSERT_TRUE(completion.has_value());
    if (completion->quota_rejected) {
      quota_rejected++;
    }
  }
  // Every admitted future completes inside its reserved slot: no quota
  // rejection at completion time, terminal count exactly at cap.
  ASSERT_EQ(quota_rejected, 0u);
  ASSERT_EQ(store.terminal_record_count(), cap);
  ASSERT_EQ(store.pending_count(), 0);

  // The key invariant: no successfully admitted id is ever unknown until
  // take/TTL/cancellation, and none may remain pending.
  for (auto future_id : future_ids) {
    const auto state = store.state(future_id);
    ASSERT_NE(state, OwnerFutureState::kUnknown)
        << "future " << future_id << " must stay queryable after admission";
    ASSERT_NE(state, OwnerFutureState::kPending)
        << "future " << future_id << " must be terminal";
  }
  // Every retained terminal record is consumable via take().
  size_t consumed = 0;
  for (auto future_id : future_ids) {
    auto taken = store.take(future_id);
    if (taken.consumed) {
      consumed++;
    }
  }
  ASSERT_EQ(consumed, cap);
  ASSERT_EQ(store.size(), 0);
}

// F03/R2-F05: single-payload and aggregate byte caps are hard limits; a
// breach is a deterministic rejection that keeps a payload-free FAILED
// tombstone in the reserved slot: the id stays queryable (poll/state/take)
// with a stable error until the consumer takes it. Never erased, never
// unknown.
TEST(OwnerFutureStoreTest, PayloadByteCapsRejectDeterministically) {
  OwnerFutureStore store;

  // Single-payload cap breach via a huge native string result.
  auto invalid_target_record = owner_future_store_test_record(1, 101);
  invalid_target_record.has_target_handle = true;
  ASSERT_TRUE(store.admit_pending(std::move(invalid_target_record)));
  std::string huge(OwnerFutureStore::kMaxSinglePayloadBytes + 1, 'x');
  auto single = store.complete_string_for_task(101, "native_string", std::move(huge));
  ASSERT_TRUE(single.has_value());
  ASSERT_TRUE(single->quota_rejected);
  ASSERT_EQ(single->target_status, VMObjectHandleResolveStatus::kInvalidHandle);
  ASSERT_STREQ(single->record.error.c_str(), "future_payload_single_byte_cap");
  ASSERT_EQ(store.byte_reject_count(), 1u);
  ASSERT_EQ(store.pending_count(), 0);
  // Tombstone retained: queryable as failed, payload-free, consumable.
  ASSERT_EQ(store.terminal_record_count(), 1u);
  ASSERT_EQ(store.size(), 1);
  ASSERT_EQ(store.state(1), OwnerFutureState::kFailed);
  ASSERT_EQ(store.poll(1)->error, "future_payload_single_byte_cap");
  ASSERT_EQ(store.terminal_payload_bytes(), 0);
  ASSERT_TRUE(store.take(1).consumed);
  ASSERT_EQ(store.size(), 0);
  ASSERT_EQ(store.state(1), OwnerFutureState::kUnknown);  // only after take

  // Aggregate byte cap: fill retained payloads near the cap, then complete
  // one more record whose payload would cross the aggregate limit.
  const uint64_t chunk = 1024 * 1024;  // 1 MiB each
  const uint64_t chunks_before =
      (OwnerFutureStore::kMaxTotalPayloadBytes / chunk) - 1;
  for (uint64_t i = 0; i < chunks_before; i++) {
    auto record = owner_future_store_test_record(i + 2, i + 1002);
    ASSERT_TRUE(store.admit_pending(record));
    std::string payload(chunk, 'y');
    auto completion = store.complete_string_for_task(i + 1002, "native_string", std::move(payload));
    ASSERT_TRUE(completion.has_value());
    ASSERT_FALSE(completion->quota_rejected);
  }
  ASSERT_GT(store.terminal_payload_bytes(), 0);
  ASSERT_EQ(store.peak_terminal_payload_bytes(), store.terminal_payload_bytes());

  // One more chunk would exceed kMaxTotalPayloadBytes (aggregate).
  auto record = owner_future_store_test_record(9999, 19999);
  ASSERT_TRUE(store.admit_pending(record));
  std::string crossing(chunk * 2, 'z');
  auto rejected =
      store.complete_string_for_task(19999, "native_string", std::move(crossing));
  ASSERT_TRUE(rejected.has_value());
  ASSERT_TRUE(rejected->quota_rejected);
  ASSERT_STREQ(rejected->record.error.c_str(), "future_payload_total_byte_cap");
  ASSERT_GE(store.byte_reject_count(), 2u);
  // The aggregate-breach tombstone is retained and queryable too.
  ASSERT_EQ(store.state(9999), OwnerFutureState::kFailed);
  ASSERT_TRUE(store.take(9999).consumed);

  // Counters settle exactly: every retained record is consumable and the
  // payload accounting returns to zero after take().
  for (uint64_t i = 0; i < chunks_before; i++) {
    ASSERT_TRUE(store.take(i + 2).consumed);
  }
  ASSERT_EQ(store.size(), 0);
  ASSERT_EQ(store.terminal_record_count(), 0u);
  ASSERT_EQ(store.terminal_payload_bytes(), 0);
  ASSERT_GT(store.peak_terminal_payload_bytes(), 0);
}

// F03: TTL reaping is bounded and only removes expired payload-free records;
// counters and the time index stay consistent across reap/take/overwrite.
TEST(OwnerFutureStoreTest, TtlReapCountersSettleAfterMixedOperations) {
  OwnerFutureStore store;
  uint64_t fake_now_ns = 1'000'000'000ULL;
  store.set_clock_for_test([&fake_now_ns]() { return fake_now_ns; });

  // Three payload-free terminal records at three ages.
  for (uint64_t i = 0; i < 3; i++) {
    auto record = owner_future_store_test_record(i + 1, i + 101);
    ASSERT_TRUE(store.admit_pending(record));
    ASSERT_TRUE(
        store.complete_for_task(i + 101, "completed", "result", "").has_value());
  }
  ASSERT_EQ(store.terminal_record_count(), 3u);
  fake_now_ns += OwnerFutureStore::kTerminalTtlNs + 1;
  store.reap_expired_terminal();
  ASSERT_EQ(store.terminal_record_count(), 0u);
  ASSERT_EQ(store.size(), 0);
  ASSERT_EQ(store.reaped_terminal_count(), 3u);
  ASSERT_EQ(store.oldest_terminal_age_ns(), 0u);

  // Mixed: one expired, one fresh, one payload-bearing (never reaped).
  auto expired = owner_future_store_test_record(10, 110);
  ASSERT_TRUE(store.admit_pending(expired));
  ASSERT_TRUE(store.complete_for_task(110, "completed", "result", "").has_value());
  auto payload = owner_future_store_test_record(12, 112);
  ASSERT_TRUE(store.admit_pending(payload));
  ASSERT_TRUE(store.complete_for_task(112, "completed", "result", "").has_value());
  {
    auto with_payload = owner_future_store_test_record(12, 112);
    with_payload.state = "completed";
    with_payload.terminal_at_ns = fake_now_ns;
    with_payload.result = std::make_shared<VMFrozenValue>();
    store.take(12);
    ASSERT_TRUE(store.restore_terminal_checked(with_payload));
  }
  // Advance past TTL, then create a fresh payload-free record.
  fake_now_ns += OwnerFutureStore::kTerminalTtlNs + 1;
  auto fresh = owner_future_store_test_record(11, 111);
  ASSERT_TRUE(store.admit_pending(fresh));
  ASSERT_TRUE(store.complete_for_task(111, "completed", "result", "").has_value());
  store.reap_expired_terminal();
  // Expired payload-free reaped; fresh stays; payload-bearing stays.
  ASSERT_EQ(store.terminal_record_count(), 2u);
  ASSERT_NE(store.poll(11), std::nullopt);
  ASSERT_NE(store.poll(12), std::nullopt);
  ASSERT_EQ(store.poll(10), std::nullopt);
  // The ALL-terminal index includes payload-bearing records, so the oldest
  // terminal here is the aged payload-bearing record 12 (age >= TTL); the
  // reapable-only index (record 11, age ~0) no longer drives the metric.
  ASSERT_GE(store.oldest_terminal_age_ns(), OwnerFutureStore::kTerminalTtlNs);
  // Consume the rest and verify exact counter return to zero.
  ASSERT_TRUE(store.take(11).consumed);
  ASSERT_TRUE(store.take(12).consumed);
  ASSERT_EQ(store.size(), 0);
  ASSERT_EQ(store.terminal_record_count(), 0u);
  ASSERT_EQ(store.terminal_payload_bytes(), 0);
}

// R2-F05: mapping key AND value string bytes are both metered by the frozen
// payload weight visitor (the old visitor counted only node structure for
// mappings, which could silently accept oversized mapping payloads). Runs
// under DriverTest because frozen string construction needs the driver's
// shared-string allocator.
// R2-F05: a quota-rejected tombstone is queryable (poll/state/take ->
// failed) until the consumer takes it or its payload-free TTL elapses; the
// id becomes unknown only through one of those legal reclamation paths.
TEST(OwnerFutureStoreTest, QuotaRejectedTombstoneQueryableUntilTakeOrTtl) {
  OwnerFutureStore store;
  uint64_t fake_now_ns = 1'000'000'000ULL;
  store.set_clock_for_test([&fake_now_ns]() { return fake_now_ns; });
  store.admit_pending(owner_future_store_test_record(1, 101));
  std::string huge(OwnerFutureStore::kMaxSinglePayloadBytes + 1, 'x');
  auto completion =
      store.complete_string_for_task(101, "native_string", std::move(huge));
  ASSERT_TRUE(completion.has_value());
  ASSERT_TRUE(completion->quota_rejected);
  ASSERT_EQ(store.state(1), OwnerFutureState::kFailed);
  ASSERT_NE(store.poll(1), std::nullopt);

  // Before TTL: the tombstone is not reaped.
  fake_now_ns += OwnerFutureStore::kTerminalTtlNs - 1;
  store.reap_expired_terminal();
  ASSERT_NE(store.poll(1), std::nullopt);
  ASSERT_EQ(store.state(1), OwnerFutureState::kFailed);

  // After TTL: payload-free tombstone is legally reaped; only then unknown.
  fake_now_ns += 2;
  store.reap_expired_terminal();
  ASSERT_EQ(store.poll(1), std::nullopt);
  ASSERT_EQ(store.state(1), OwnerFutureState::kUnknown);
  ASSERT_GE(store.reaped_terminal_count(), 1u);
}

// R2-F05: every completion entry point (complete / complete_for_task /
// complete_string_for_task / fail_terminal) shares the same accounting;
// counters return exactly to zero after take().
TEST(OwnerFutureStoreTest, AllCompletionEntriesShareAccounting) {
  OwnerFutureStore store;
  store.admit_pending(owner_future_store_test_record(1, 101));
  ASSERT_TRUE(store.complete(1, "completed", "r1", "").has_value());
  store.admit_pending(owner_future_store_test_record(2, 202));
  ASSERT_TRUE(store.complete_for_task(202, "completed", "r2", "").has_value());
  store.admit_pending(owner_future_store_test_record(3, 303));
  ASSERT_TRUE(store.complete_string_for_task(303, "r3", "native").has_value());
  store.admit_pending(owner_future_store_test_record(4, 404));
  ASSERT_TRUE(store.fail_terminal(4, "cancelled", true, false).changed);

  ASSERT_EQ(store.pending_count(), 0);
  ASSERT_EQ(store.terminal_record_count(), 4u);
  ASSERT_EQ(store.completed_count(), 3u);
  ASSERT_EQ(store.failed_count(), 1u);
  ASSERT_EQ(store.size(), 4);
  for (uint64_t id = 1; id <= 4; id++) {
    ASSERT_TRUE(store.take(id).consumed);
  }
  ASSERT_EQ(store.size(), 0);
  ASSERT_EQ(store.terminal_record_count(), 0u);
}

// R2-F05: concurrent admission/completion/take never yields negative
// counters or index drift; the store drains exactly.
TEST(OwnerFutureStoreTest, ConcurrentCompletionTakeReapKeepsCountersConsistent) {
  OwnerFutureStore store;
  constexpr int kThreads = 8;
  constexpr int kPerThread = 200;
  std::vector<std::thread> threads;
  std::atomic<int> failures{0};
  for (int t = 0; t < kThreads; t++) {
    threads.emplace_back([&store, t, &failures]() {
      for (int i = 0; i < kPerThread; i++) {
        const uint64_t id = static_cast<uint64_t>(t) * kPerThread + i + 1;
        auto record = owner_future_store_test_record(id, id);
        if (!store.admit_pending(record)) {
          failures.fetch_add(1, std::memory_order_relaxed);
          continue;
        }
        if (!store.complete(id, "completed", "r", "").has_value()) {
          failures.fetch_add(1, std::memory_order_relaxed);
        }
        if (!store.take(id).consumed) {
          failures.fetch_add(1, std::memory_order_relaxed);
        }
      }
    });
  }
  for (auto &th : threads) {
    th.join();
  }
  ASSERT_EQ(failures.load(), 0);
  ASSERT_EQ(store.size(), 0);
  ASSERT_EQ(store.pending_count(), 0);
  ASSERT_EQ(store.terminal_record_count(), 0u);
  ASSERT_EQ(store.completed_count(),
            static_cast<uint64_t>(kThreads * kPerThread));
}

TEST(OwnerTraceStoreTest, AssignsTraceIdToUnsequencedTaskTrace) {
  OwnerTraceStore store;
  OwnerTaskTrace trace;
  trace.owner_id = "owner/test/trace-store";
  trace.state = "observed";

  const auto trace_id = store.append_task(std::move(trace));
  const auto snapshot = store.task_snapshot(1);

  ASSERT_EQ(snapshot.events.size(), 1u);
  EXPECT_EQ(snapshot.total_traced, 1u);
  EXPECT_EQ(snapshot.events[0].trace_id, trace_id);
  EXPECT_EQ(snapshot.events[0].sequence, trace_id);
}

TEST(OwnerSchedulerStateTest, RemovesOneQueuedTaskWithoutDroppingNeighbors) {
  OwnerSchedulerState state;
  const auto runnable = [](const OwnerMailboxTask &task) {
    return task.task_type == "executor_probe";
  };
  OwnerMailboxTask first{};
  first.task_id = 11;
  first.sequence = 1;
  first.owner_id = "owner/test/scheduler/remove-task";
  first.task_type = "executor_probe";
  OwnerMailboxTask removed_candidate = first;
  removed_candidate.task_id = 12;
  removed_candidate.sequence = 2;
  OwnerMailboxTask last = first;
  last.task_id = 13;
  last.sequence = 3;

  ASSERT_TRUE(state.enqueue_owner_task(std::move(first),
                                       "owner/test/scheduler/remove-task",
                                       false, runnable));
  ASSERT_FALSE(state.enqueue_owner_task(std::move(removed_candidate),
                                        "owner/test/scheduler/remove-task",
                                        false, runnable));
  ASSERT_FALSE(state.enqueue_owner_task(std::move(last),
                                        "owner/test/scheduler/remove-task",
                                        false, runnable));

  OwnerMailboxTask removed{};
  ASSERT_TRUE(state.remove_owner_task(12, &removed));
  ASSERT_EQ(removed.task_id, 12u);
  ASSERT_EQ(state.mailbox_total_depth(), 2);
  ASSERT_FALSE(state.remove_owner_task(12, &removed));

  OwnerMailboxTask next{};
  ASSERT_TRUE(state.pop_next_schedulable_task(&next, false, runnable).found);
  ASSERT_EQ(next.task_id, 11u);
  ASSERT_TRUE(state.pop_next_schedulable_task(&next, false, runnable).found);
  ASSERT_EQ(next.task_id, 13u);
  ASSERT_EQ(state.mailbox_total_depth(), 0);
}

TEST(OwnerSchedulerStateTest, ForcedReleaseClearsClaimsAndRestoresRunnableOwner) {
  OwnerSchedulerState state;
  const auto runnable = [](const OwnerMailboxTask &task) {
    return task.task_type == "executor_probe";
  };
  OwnerMailboxTask first{};
  first.task_id = 1;
  first.sequence = 1;
  first.owner_epoch = 1;
  first.owner_id = "owner/test/scheduler/forced-release";
  first.task_type = "executor_probe";
  OwnerMailboxTask second = first;
  second.task_id = 2;
  second.sequence = 2;

  ASSERT_TRUE(state.enqueue_owner_task(
      std::move(first), second.owner_id, false, runnable));
  ASSERT_FALSE(state.enqueue_owner_task(
      std::move(second), "owner/test/scheduler/forced-release", false,
      runnable));

  OwnerMailboxTask claimed{};
  const auto claim = state.pop_next_schedulable_task(
      &claimed, true, runnable);
  ASSERT_TRUE(claim.found);
  ASSERT_EQ(claimed.task_id, 1u);
  ASSERT_EQ(state.active_owner_count(), 1);
  ASSERT_EQ(state.active_claim_count(), 1);
  const auto claimed_owner = claimed.owner_id;
  state.push_front_owner_task(claimed_owner, std::move(claimed));

  const auto claims = state.active_claim_count();
  const auto releases = state.release_all_active_owners(runnable);
  ASSERT_EQ(releases, claims);
  ASSERT_EQ(state.active_owner_count(), 0);
  ASSERT_EQ(state.active_claim_count(), 0);
  ASSERT_EQ(state.runnable_owner_count(runnable), 1);

  OwnerMailboxTask restarted{};
  const auto restarted_claim = state.pop_next_schedulable_task(
      &restarted, true, runnable);
  ASSERT_TRUE(restarted_claim.found);
  ASSERT_EQ(restarted.task_id, 1u);
}

TEST_F(DriverTest, TestPromisePassThroughDeliveryIsDeferredAndRefcounted) {
  clear_tick_events();
  struct TickQueueGuard {
    ~TickQueueGuard() { clear_tick_events(); }
  } tick_queue_guard;

  promise_t* source = promise_alloc();
  promise_t* next = promise_alloc();
  next->ref++;  // keep an observer reference after the reaction consumes one
  promise_add_reaction(source, nullptr, nullptr, next, nullptr);

  add_gametick_event(0, [&] {
    svalue_t value = const0;
    value.type = T_NUMBER;
    value.u.number = 42;
    ASSERT_EQ(promise_settle(source, &value, 0), 1);
    EXPECT_EQ(next->state, PROMISE_PENDING);
  });

  ASSERT_EQ(next->state, PROMISE_PENDING);
  ASSERT_EQ(run_tick_events_for_test(), 2u);
  ASSERT_EQ(next->state, PROMISE_FULFILLED);
  ASSERT_EQ(next->result.type, T_NUMBER);
  ASSERT_EQ(next->result.u.number, 42);
  ASSERT_EQ(pending_promise_deliveries(), 0u);

  free_promise(source);
  free_promise(next);
}

#ifdef DEBUG
TEST_F(DriverTest, ForeachTemporariesRestoredOnUnwind) {
  auto* object = load_object_for_test("single/tests/compiler/foreach_unwind");
  ASSERT_NE(object, nullptr);
  current_object = master_ob;

  const int before = stack_in_use_as_temporary;
  const auto context_before = vm_context().execution.stack_in_use_as_temporary;
  ASSERT_EQ(before, context_before);

  auto invoke_number = [&](const char* method, int expected) {
    auto* result = safe_apply(method, object, 0, ORIGIN_DRIVER);
    if (result == nullptr) {
      ADD_FAILURE() << method << " unexpectedly failed";
      return;
    }
    if (result->type != T_NUMBER) {
      ADD_FAILURE() << method << " returned svalue type " << result->type;
      vm_apply_return_clear();
      return;
    }
    EXPECT_EQ(result->u.number, expected) << method;
    vm_apply_return_clear();
    EXPECT_EQ(stack_in_use_as_temporary, before) << method;
    EXPECT_EQ(vm_context().execution.stack_in_use_as_temporary, context_before) << method;
  };

  // Catching an error unwinds one loop before F_EXIT_FOREACH can run.
  invoke_number("caught_in_foreach", 1);
  // The same boundary must restore both nested loop counters.
  invoke_number("caught_in_nested_foreach", 1);
  // A later normal statement must still execute the DEBUG stack check path.
  invoke_number("normal_foreach", 6);
  // Returning from an open loop follows the ordinary F_EXIT_FOREACH path.
  invoke_number("return_inside_foreach", 11);

  // safe_apply()/restore_context() uses the same boundary and must not leak a
  // temporary count when the error escapes the LPC function entirely.
  auto* failed = safe_apply("error_in_foreach", object, 0, ORIGIN_DRIVER);
  EXPECT_EQ(failed, nullptr);
  vm_apply_return_clear();
  EXPECT_EQ(stack_in_use_as_temporary, before);
  EXPECT_EQ(vm_context().execution.stack_in_use_as_temporary, context_before);
}
#endif  // DEBUG

TEST_F(DriverTest, TestAsyncAwaitResumesAfterYieldAndCatchesRejection) {
  clear_tick_events();
  struct AsyncProbeGuard {
    object_t* object = nullptr;
    promise_t* source = nullptr;
    std::vector<promise_t*> results;
    ~AsyncProbeGuard() {
      clear_tick_events();
      if (object != nullptr) {
        destruct_object_for_test(object);
      }
      if (source != nullptr) {
        free_promise(source);
      }
      for (auto* result : results) {
        free_promise(result);
      }
      vm_apply_return_clear();
    }
  } guard;

  guard.object = load_object_for_test("single/async_phase2_probe");
  ASSERT_NE(guard.object, nullptr);

  auto invoke = [&](const char* method) -> promise_t* {
    auto* result = safe_apply(method, guard.object, 0, ORIGIN_DRIVER);
    EXPECT_NE(result, nullptr);
    if (result == nullptr || result->type != T_PROMISE) {
      vm_apply_return_clear();
      return nullptr;
    }
    auto* promise = result->u.prom;
    promise->ref++;  // retain a C++ observer reference after clearing apply_ret_value
    vm_apply_return_clear();
    guard.results.push_back(promise);
    return promise;
  };

  auto drain_backend_events = [&] {
    for (int pass = 0; pass < 32; pass++) {
      if (tick_event_queue_size_for_test() != 0) {
        ASSERT_GT(run_tick_events_for_test(), 0u);
      }
      if (walltime_event_queue_size_for_test() != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        ASSERT_EQ(event_base_loop(g_event_base, EVLOOP_NONBLOCK), 0);
      }
      if (tick_event_queue_size_for_test() == 0 &&
          walltime_event_queue_size_for_test() == 0) {
        return;
      }
    }
    ADD_FAILURE() << "async probe did not drain backend events";
  };

  auto* suspended = invoke("suspend_once");
  ASSERT_NE(suspended, nullptr);
  ASSERT_EQ(suspended->state, PROMISE_PENDING);
  drain_backend_events();
  ASSERT_EQ(suspended->state, PROMISE_FULFILLED);
  ASSERT_EQ(suspended->result.type, T_NUMBER);
  ASSERT_EQ(suspended->result.u.number, 42);

  auto* function_pointer = invoke("call_function_pointer_probe");
  ASSERT_NE(function_pointer, nullptr);
  ASSERT_EQ(function_pointer->state, PROMISE_PENDING);
  drain_backend_events();
  ASSERT_EQ(function_pointer->state, PROMISE_FULFILLED);
  ASSERT_EQ(function_pointer->result.type, T_NUMBER);
  ASSERT_EQ(function_pointer->result.u.number, 42);

  auto* simul = invoke("call_simul_probe");
  ASSERT_NE(simul, nullptr);
  ASSERT_EQ(simul->state, PROMISE_PENDING);
  drain_backend_events();
  ASSERT_EQ(simul->state, PROMISE_FULFILLED);
  ASSERT_EQ(simul->result.type, T_NUMBER);
  ASSERT_EQ(simul->result.u.number, 84);

  auto* external = invoke("call_external_probe");
  ASSERT_NE(external, nullptr);
  ASSERT_EQ(external->state, PROMISE_PENDING);
  drain_backend_events();
  ASSERT_EQ(external->state, PROMISE_FULFILLED);
  ASSERT_EQ(external->result.type, T_NUMBER);
  ASSERT_EQ(external->result.u.number, 42);

  auto* inherited = invoke("call_inherited_probe");
  ASSERT_NE(inherited, nullptr);
  ASSERT_EQ(inherited->state, PROMISE_PENDING);
  drain_backend_events();
  ASSERT_EQ(inherited->state, PROMISE_FULFILLED);
  ASSERT_EQ(inherited->result.type, T_NUMBER);
  ASSERT_EQ(inherited->result.u.number, 7);

  program_t* rejection_owner_program = guard.object->prog;
  const unsigned int rejection_owner_program_ref_before = rejection_owner_program->ref;
  guard.source = promise_alloc();
  guard.source->ref++;  // retain the source while the async call owns its argument
  push_refed_promise(guard.source);
  auto* result = safe_apply("catch_rejection", guard.object, 1, ORIGIN_DRIVER);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_PROMISE);
  auto* caught = result->u.prom;
  caught->ref++;
  EXPECT_EQ(rejection_owner_program->ref, rejection_owner_program_ref_before + 2u)
      << "rejected suspended frames must hold both program pins";
  vm_apply_return_clear();
  guard.results.push_back(caught);
  ASSERT_EQ(caught->state, PROMISE_PENDING);

  svalue_t reason = const0;
  reason.type = T_STRING;
  reason.subtype = STRING_CONSTANT;
  reason.u.string = const_cast<char*>("expected rejection");
  ASSERT_EQ(promise_settle(guard.source, &reason, 1), 1);
  drain_backend_events();
  ASSERT_EQ(caught->state, PROMISE_FULFILLED);
  ASSERT_EQ(caught->result.type, T_STRING);
  ASSERT_STREQ(caught->result.u.string, "expected rejection");
  EXPECT_EQ(rejection_owner_program->ref, rejection_owner_program_ref_before)
      << "rejection delivery must release both program pins";
}

TEST_F(DriverTest, TestRecompileSafetyCoroutineGeneration64AndProgramPin) {
  clear_tick_events();
  object_t* object = load_object_for_test("single/async_phase2_probe");
  ASSERT_NE(object, nullptr);

  program_t* owner_program = object->prog;
  const unsigned int owner_program_ref_before = owner_program->ref;
  object->prog_generation =
      static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 17u;

  auto* result = safe_apply("suspend_once", object, 0, ORIGIN_DRIVER);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_PROMISE);
  auto* promise = result->u.prom;
  promise->ref++;
  vm_apply_return_clear();
  ASSERT_EQ(promise->state, PROMISE_PENDING);
  EXPECT_EQ(owner_program->ref, owner_program_ref_before + 2u)
      << "defining-program and owner-top-program pins must both be held";

  for (int pass = 0; pass < 32 && promise->state == PROMISE_PENDING; pass++) {
    if (tick_event_queue_size_for_test() != 0) {
      ASSERT_GT(run_tick_events_for_test(), 0u);
    }
    if (walltime_event_queue_size_for_test() != 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      ASSERT_EQ(event_base_loop(g_event_base, EVLOOP_NONBLOCK), 0);
    }
    if (tick_event_queue_size_for_test() == 0 &&
        walltime_event_queue_size_for_test() == 0 &&
        promise->state == PROMISE_PENDING) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  }

  EXPECT_EQ(promise->state, PROMISE_FULFILLED);
  if (promise->state == PROMISE_FULFILLED) {
    ASSERT_EQ(promise->result.type, T_NUMBER);
    EXPECT_EQ(promise->result.u.number, 42);
  } else {
    promise->handled = true;
  }
  EXPECT_EQ(owner_program->ref, owner_program_ref_before)
      << "all completion paths must release both program pins";
  free_promise(promise);
  destruct_object_for_test(object);
}

TEST_F(DriverTest, TestRecompileSafetyCoroutineRejectsRecompile) {
  clear_tick_events();
  object_t* object = load_object_for_test("single/async_phase2_probe");
  ASSERT_NE(object, nullptr);

  auto* result = safe_apply("suspend_once", object, 0, ORIGIN_DRIVER);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_PROMISE);
  auto* promise = result->u.prom;
  promise->ref++;
  vm_apply_return_clear();
  ASSERT_EQ(promise->state, PROMISE_PENDING);

  program_t* old_program = object->prog;
  RecompilePrepared prepared;
  prepared.staged = compile_program_for_recompile(object);
  ASSERT_NE(prepared.staged.prog, nullptr);
  prepared.old_layout = describe_recompile_layout(old_program);
  prepared.new_layout = describe_recompile_layout(prepared.staged.prog);
  ASSERT_TRUE(recompile_layouts_match(prepared.old_layout, prepared.new_layout, nullptr));
  start_recompile_transaction(object, RecompileTargetKind::BlueprintFamily, &prepared);
  prepare_variable_migrations(&prepared);
  ASSERT_TRUE(prepared.migrations.empty());
  prepared.commit_swap();
  ASSERT_NE(object->prog, old_program);
  ASSERT_TRUE(prepared.run_create_guarded());
  prepared.commit_finish();

  for (int pass = 0; pass < 32 && promise->state == PROMISE_PENDING; pass++) {
    if (tick_event_queue_size_for_test() != 0) {
      ASSERT_GT(run_tick_events_for_test(), 0u);
    }
    if (walltime_event_queue_size_for_test() != 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      ASSERT_EQ(event_base_loop(g_event_base, EVLOOP_NONBLOCK), 0);
    }
    if (tick_event_queue_size_for_test() == 0 &&
        walltime_event_queue_size_for_test() == 0 &&
        promise->state == PROMISE_PENDING) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  }

  ASSERT_EQ(promise->state, PROMISE_REJECTED);
  ASSERT_EQ(promise->result.type, T_STRING);
  ASSERT_STREQ(promise->result.u.string,
               "*async function owner was recompiled while suspended");
  promise->handled = true;
  free_promise(promise);
  destruct_object_for_test(object);
}

TEST_F(DriverTest, TestRecompileSafetyCoroutineRejectsReplaceProgram) {
  clear_tick_events();
  object_t* object = load_object_for_test("single/async_phase2_probe");
  object_t* replacement = load_object_for_test("single/async_phase2_base");
  ASSERT_NE(object, nullptr);
  ASSERT_NE(replacement, nullptr);

  auto* result = safe_apply("suspend_once", object, 0, ORIGIN_DRIVER);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_PROMISE);
  auto* promise = result->u.prom;
  promise->ref++;
  vm_apply_return_clear();
  ASSERT_EQ(promise->state, PROMISE_PENDING);

  auto* entry = static_cast<replace_ob_t *>(
      DMALLOC(sizeof(replace_ob_t), TAG_TEMPORARY, "test_async_replace_program"));
  entry->ob = object;
  entry->new_prog = replacement->prog;
  entry->var_offset = 0;
  entry->next = obj_list_replace;
  obj_list_replace = entry;
  replace_programs();
  ASSERT_EQ(object->prog, replacement->prog);

  for (int pass = 0; pass < 32 && promise->state == PROMISE_PENDING; pass++) {
    if (tick_event_queue_size_for_test() != 0) {
      ASSERT_GT(run_tick_events_for_test(), 0u);
    }
    if (walltime_event_queue_size_for_test() != 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      ASSERT_EQ(event_base_loop(g_event_base, EVLOOP_NONBLOCK), 0);
    }
    if (tick_event_queue_size_for_test() == 0 &&
        walltime_event_queue_size_for_test() == 0 &&
        promise->state == PROMISE_PENDING) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  }

  ASSERT_EQ(promise->state, PROMISE_REJECTED);
  ASSERT_EQ(promise->result.type, T_STRING);
  ASSERT_STREQ(promise->result.u.string,
               "*async function owner's program was replaced while suspended");
  promise->handled = true;
  free_promise(promise);
  destruct_object_for_test(object);
  destruct_object_for_test(replacement);
}
