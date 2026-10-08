// Include the implementation to corrupt private records without adding runtime hooks.
// This ELF test supplies its symbols; libdriver does not link a second copy.
#include "vm/internal/object_store.cc"

#include <array>
#include <new>
#include <thread>

#include <gtest/gtest.h>

namespace {
thread_local int allocations_before_failure = -1;

std::array<size_t, 21> summary_values(const OwnerLocalBridgeSummary& summary) {
  return {summary.live_records,
          summary.object_refs,
          summary.object_ref_indexes,
          summary.destructed_records,
          summary.live_path_index_entries,
          summary.destructed_path_index_entries,
          summary.orphan_records,
          summary.owner_local_to_global_mismatch_records,
          summary.global_to_owner_local_record_mismatch_records,
          summary.global_records,
          summary.global_live_records,
          summary.global_destructed_records,
          summary.global_to_owner_local_mismatch_records,
          summary.owner_local_record_index_ready,
          summary.owner_local_canonical_record_ready,
          summary.owner_local_to_global_bridge_consistent,
          summary.global_record_bridge_consistent,
          summary.global_to_owner_local_bridge_consistent,
          summary.global_record_bridge_retirement_ready,
          summary.global_live_object_bridge_retirement_ready,
          summary.global_bridge_consistent};
}

OwnerLocalBridgeSummary consistent_summary(size_t live, size_t dead) {
  OwnerLocalBridgeSummary result;
  result.live_records = live;
  result.object_refs = live;
  result.object_ref_indexes = live;
  result.destructed_records = dead;
  result.live_path_index_entries = live;
  result.destructed_path_index_entries = dead;
  result.global_records = live + dead;
  result.global_live_records = live;
  result.global_destructed_records = dead;
  result.global_record_bridge_retirement_ready = true;
  result.global_live_object_bridge_retirement_ready = true;
  return result;
}

void mark_record_mismatch(OwnerLocalBridgeSummary& result, size_t local, size_t global) {
  result.orphan_records = local;
  result.owner_local_to_global_mismatch_records = local;
  result.global_to_owner_local_record_mismatch_records = global;
  result.global_to_owner_local_mismatch_records = global;
  result.owner_local_to_global_bridge_consistent = local == 0;
  result.global_record_bridge_consistent = false;
  result.global_to_owner_local_bridge_consistent = global == 0;
  result.global_record_bridge_retirement_ready = false;
  result.global_live_object_bridge_retirement_ready = false;
  result.global_bridge_consistent = false;
}

class ObjectStoreSummaryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(object_records.empty());
    ASSERT_TRUE(owner_shards.empty());
  }

  void TearDown() override {
    ObjectStoreWriteLock lock(object_store_directory_mutex);
    owner_shards.clear();
    object_records.clear();
    object_migration_traces.clear();
  }

  void add_record(size_t index, const char* owner, bool destructed = false) {
    ObjectStoreWriteLock lock(object_store_directory_mutex);
    const ObjectRecord record{index + 1, owner, 7, "fixture/" + std::to_string(index), destructed};
    auto* object = &objects_[index];
    object_records[object] = record;
    auto& shard = shard_for_owner(owner);
    if (destructed) {
      shard.destructed_records[record.object_id] = record;
      shard.destructed_path_index[record.object_path] = record.object_id;
    } else {
      shard.local_records[record.object_id] = record;
      shard.local_objects[record.object_id] = object;
      shard.local_object_index[object] = record.object_id;
      shard.object_directory.insert(record.object_id);
      shard.object_path_index[record.object_path] = record.object_id;
    }
  }

  OwnerLocalBridgeSummary summarize() {
    ObjectStoreReadLock lock(object_store_directory_mutex);
    return owner_local_bridge_summary_locked();
  }

  void expect_summary(const OwnerLocalBridgeSummary& expected) {
    EXPECT_EQ(summary_values(summarize()), summary_values(expected));
  }

  // Summary checks pointer identities, never object fields or VM references.
  std::array<object_t, 4> objects_{};
};

TEST_F(ObjectStoreSummaryTest, Empty) {
  OwnerLocalBridgeSummary actual;
  allocations_before_failure = 0;
  try {
    actual = summarize();
  } catch (...) {
    allocations_before_failure = -1;
    throw;
  }
  allocations_before_failure = -1;
  EXPECT_EQ(summary_values(actual), summary_values(consistent_summary(0, 0)));
}

TEST_F(ObjectStoreSummaryTest, LiveAndTombstoneAcrossOwners) {
  add_record(0, "owner/a");
  add_record(1, "owner/b");
  add_record(2, "owner/a", true);
  add_record(3, "owner/b", true);
  expect_summary(consistent_summary(2, 2));
}

TEST_F(ObjectStoreSummaryTest, MissingLocalRecord) {
  add_record(0, "owner/a");
  {
    ObjectStoreWriteLock lock(object_store_directory_mutex);
    owner_shards.at("owner/a").local_records.erase(1);
  }
  auto expected = consistent_summary(1, 0);
  expected.live_records = 0;
  mark_record_mismatch(expected, 0, 1);
  expected.owner_local_record_index_ready = false;
  expected.owner_local_canonical_record_ready = false;
  expected.owner_local_to_global_bridge_consistent = false;
  expect_summary(expected);
}

TEST_F(ObjectStoreSummaryTest, ReverseIndexMismatch) {
  add_record(0, "owner/a");
  {
    ObjectStoreWriteLock lock(object_store_directory_mutex);
    owner_shards.at("owner/a").local_object_index.erase(&objects_[0]);
  }
  auto expected = consistent_summary(1, 0);
  expected.object_ref_indexes = 0;
  expected.global_to_owner_local_mismatch_records = 1;
  expected.owner_local_canonical_record_ready = false;
  expected.owner_local_to_global_bridge_consistent = false;
  expected.global_to_owner_local_bridge_consistent = false;
  expected.global_live_object_bridge_retirement_ready = false;
  expected.global_bridge_consistent = false;
  expect_summary(expected);
}

TEST_F(ObjectStoreSummaryTest, EpochMismatch) {
  add_record(0, "owner/a");
  {
    ObjectStoreWriteLock lock(object_store_directory_mutex);
    owner_shards.at("owner/a").local_records.at(1).owner_epoch++;
  }
  auto expected = consistent_summary(1, 0);
  mark_record_mismatch(expected, 1, 1);
  expect_summary(expected);
}

TEST_F(ObjectStoreSummaryTest, MissingGlobalRecord) {
  add_record(0, "owner/a");
  {
    ObjectStoreWriteLock lock(object_store_directory_mutex);
    object_records.erase(&objects_[0]);
  }
  auto expected = consistent_summary(1, 0);
  expected.global_records = 0;
  expected.global_live_records = 0;
  mark_record_mismatch(expected, 1, 0);
  expect_summary(expected);
}

TEST_F(ObjectStoreSummaryTest, DuplicateIdRetainsFirstTraversalMatch) {
  add_record(0, "owner/a");
  {
    ObjectStoreWriteLock lock(object_store_directory_mutex);
    auto duplicate = object_records.at(&objects_[0]);
    duplicate.owner_epoch++;
    object_records.emplace(&objects_[1], duplicate);
    const auto* first = find_global_record_by_object_id_locked(1);
    auto& shard = owner_shards.at("owner/a");
    shard.local_records.at(1) = *first;
    auto* matching_object = first == &object_records.at(&objects_[0]) ? &objects_[0] : &objects_[1];
    shard.local_objects.at(1) = matching_object;
    shard.local_object_index.clear();
    shard.local_object_index.emplace(matching_object, 1);
  }
  auto expected = consistent_summary(1, 0);
  expected.global_records = 2;
  expected.global_live_records = 2;
  mark_record_mismatch(expected, 0, 1);
  expect_summary(expected);
}

TEST_F(ObjectStoreSummaryTest, MissingPathIndex) {
  add_record(0, "owner/a");
  {
    ObjectStoreWriteLock lock(object_store_directory_mutex);
    owner_shards.at("owner/a").object_path_index.clear();
  }
  auto expected = consistent_summary(1, 0);
  expected.live_path_index_entries = 0;
  mark_record_mismatch(expected, 0, 1);
  expected.owner_local_record_index_ready = false;
  expected.owner_local_canonical_record_ready = false;
  expected.owner_local_to_global_bridge_consistent = false;
  expect_summary(expected);
}

TEST_F(ObjectStoreSummaryTest, TombstoneShadowedByLivePath) {
  add_record(0, "owner/a");
  add_record(1, "owner/b", true);
  {
    ObjectStoreWriteLock lock(object_store_directory_mutex);
    auto& record = object_records.at(&objects_[1]);
    record.object_path = "fixture/0";
    auto& shard = owner_shards.at("owner/b");
    shard.destructed_records.at(2) = record;
    shard.destructed_path_index.clear();
  }
  auto expected = consistent_summary(1, 1);
  expected.destructed_path_index_entries = 0;
  expect_summary(expected);
}

TEST_F(ObjectStoreSummaryTest, OwnerMigrationThenDestruction) {
  add_record(0, "owner/a");
  objects_[0].obname = "fixture/0";
  objects_[0].vm_owner_id = "owner/b";
  objects_[0].vm_owner_epoch = 8;
  {
    ObjectStoreWriteLock lock(object_store_directory_mutex);
    write_owner_local_lifecycle_record_locked(&objects_[0]);
    EXPECT_EQ(object_migration_traces.size(), 1);
  }
  expect_summary(consistent_summary(1, 0));
  {
    ObjectStoreWriteLock lock(object_store_directory_mutex);
    write_owner_local_lifecycle_record_locked(&objects_[0], true);
  }
  expect_summary(consistent_summary(0, 1));
}

TEST_F(ObjectStoreSummaryTest, AllocationFailureReleasesLockWithoutPublishingState) {
  add_record(0, "owner/a");
  add_record(1, "owner/a");
  for (int fail_after = 0; fail_after < 3; ++fail_after) {
    SCOPED_TRACE(fail_after);
    allocations_before_failure = fail_after;
    EXPECT_THROW(summarize(), std::bad_alloc);
    EXPECT_EQ(allocations_before_failure, -1);
    {
      ObjectStoreWriteLock lock(object_store_directory_mutex, std::try_to_lock);
      EXPECT_TRUE(lock.owns_lock());
    }
    expect_summary(consistent_summary(2, 0));
  }
}

TEST_F(ObjectStoreSummaryTest, ConcurrentReadersAndEpochWriter) {
  add_record(0, "owner/a");
  add_record(1, "owner/b", true);
  const auto expected = summary_values(consistent_summary(1, 1));
  std::atomic<bool> mismatch{false};
  auto reader = [&] {
    for (int i = 0; i < 128; ++i) {
      if (summary_values(summarize()) != expected) {
        mismatch.store(true);
      }
    }
  };
  std::thread first(reader);
  std::thread second(reader);
  for (int i = 0; i < 128; ++i) {
    ObjectStoreWriteLock lock(object_store_directory_mutex);
    object_records.at(&objects_[0]).owner_epoch++;
    owner_shards.at("owner/a").local_records.at(1).owner_epoch++;
  }
  first.join();
  second.join();
  EXPECT_FALSE(mismatch.load());
}
}  // namespace

extern "C" void* __real__Znwm(size_t size);
extern "C" void* __wrap__Znwm(size_t size) {
  if (allocations_before_failure == 0) {
    allocations_before_failure = -1;
    throw std::bad_alloc();
  }
  if (allocations_before_failure > 0) {
    --allocations_before_failure;
  }
  return __real__Znwm(size);
}
