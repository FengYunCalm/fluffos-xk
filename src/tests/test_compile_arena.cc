// T3.1 (#1343): ScratchArena / ScratchArenaBinding unit coverage.
//
// The arena is the compile-scope chunk pool; the interactive shape (a
// session that survives across cycles) adds two things this suite pins:
// a session arena that is not the process-wide one, and nested bindings
// whose destruction releases exactly the storage allocated inside them.

#include "compiler/internal/compile_arena.h"

#include <gtest/gtest.h>

#include <cstring>
#include <optional>
#include <string>

namespace {

using compile_arena::ScratchArena;
using compile_arena::ScratchArenaBinding;

// Writes a recognisable pattern so overlapping frees or a bogus rewind show
// up as corrupted content rather than a silent pass.
void write_pattern(void *p, size_t len, char seed) {
  auto *bytes = static_cast<char *>(p);
  for (size_t i = 0; i < len; i++) {
    bytes[i] = static_cast<char>(seed + (i % 7));
  }
}

bool pattern_intact(const void *p, size_t len, char seed) {
  auto *bytes = static_cast<const char *>(p);
  for (size_t i = 0; i < len; i++) {
    if (bytes[i] != static_cast<char>(seed + (i % 7))) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST(CompileArenaTest, SessionArenaIsIndependentFromTheCompilerArena) {
  ScratchArena session(false);
  ASSERT_EQ(session.bytes_live(), 0u);

  void *session_bytes = session.alloc(64);
  ASSERT_NE(session_bytes, nullptr);
  write_pattern(session_bytes, 64, 1);
  EXPECT_EQ(session.bytes_live(), 64u);
  EXPECT_EQ(session.chunk_mallocs(), 1u) << "the base chunk is allocated on first use";

  // The process-wide compiler scope keeps running its own cycles without
  // touching the session arena.
  compile_arena::begin();
  void *compiler_bytes = compile_arena::alloc(128);
  ASSERT_NE(compiler_bytes, nullptr);
  EXPECT_TRUE(pattern_intact(session_bytes, 64, 1))
      << "compiler allocation must not disturb session storage";
  compile_arena::end();

  EXPECT_EQ(session.bytes_live(), 64u) << "end() only affects the compiler arena";
  EXPECT_EQ(compile_arena::global().bytes_live(), 0u);
  EXPECT_TRUE(pattern_intact(session_bytes, 64, 1));
}

TEST(CompileArenaTest, BeginIsResetBeforeUseAndReportsCrossCycleSurvival) {
  ScratchArena session(false);
  session.begin();

  // Cycle 1 storage stays live until the next begin(): an interactive session
  // renders the diagnostics of cycle N before starting cycle N+1, so the
  // session protocol is "reset before use", not "release at the end".
  void *diagnostic = session.alloc(4096);
  ASSERT_NE(diagnostic, nullptr);
  write_pattern(diagnostic, 4096, 3);
  EXPECT_EQ(session.bytes_live(), 4096u);
  EXPECT_EQ(session.cross_cycle_bytes(), 0u);
  EXPECT_TRUE(pattern_intact(diagnostic, 4096, 3));

  // Cycle 2 releases cycle 1's storage and accounts it as survival.
  session.begin();
  EXPECT_EQ(session.cross_cycle_bytes(), 4096u);
  EXPECT_EQ(session.bytes_live(), 0u);
  EXPECT_EQ(session.cycle_bytes(), 0u);

  void *next_cycle = session.alloc(4096);
  EXPECT_EQ(next_cycle, diagnostic) << "the released storage is handed out again";
  EXPECT_EQ(session.chunk_mallocs(), 1u);
  session.end();
}

TEST(CompileArenaTest, BindingReleasesOnlyItsOwnStorage) {
  ScratchArena session(false);
  session.begin();

  void *outer = session.alloc(256);
  ASSERT_NE(outer, nullptr);
  write_pattern(outer, 256, 5);
  size_t const live_before_binding = session.bytes_live();

  {
    ScratchArenaBinding binding(session);
    EXPECT_EQ(session.bindings_active(), 1u);
    void *inner = session.alloc(1024);
    ASSERT_NE(inner, nullptr);
    write_pattern(inner, 1024, 9);
    EXPECT_EQ(session.bytes_live(), live_before_binding + 1024);
  }

  EXPECT_EQ(session.bindings_active(), 0u);
  EXPECT_EQ(session.binding_rewinds(), 1u);
  EXPECT_EQ(session.bytes_live(), live_before_binding)
      << "the binding must release exactly what it allocated";
  EXPECT_TRUE(pattern_intact(outer, 256, 5))
      << "storage allocated before the binding stays valid";

  session.end();
}

TEST(CompileArenaTest, BindingRewindsTheChunkItStartedIn) {
  ScratchArena session(false);
  session.begin();

  void *before = session.alloc(64);
  ASSERT_NE(before, nullptr);
  size_t const live_before = session.bytes_live();

  void *first = nullptr;
  {
    ScratchArenaBinding binding(session);
    // Small allocations stay inside the chunk the binding started in, so the
    // release has to rewind the bump cursor, not just drop whole chunks.
    first = session.alloc(64);
    void *second = session.alloc(64);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(session.bytes_live(), live_before + 128);
  }

  EXPECT_EQ(session.bytes_live(), live_before)
      << "the binding released its own two allocations";
  EXPECT_EQ(session.binding_rewinds(), 1u);

  // The rewound storage is handed out again at the binding's entry mark, which
  // is where its first allocation started.
  void *reused = session.alloc(64);
  EXPECT_EQ(reused, first) << "the rewound region restarts at the entry mark";
  EXPECT_EQ(session.bytes_live(), live_before + 64);
  session.end();
}

TEST(CompileArenaTest, NestedBindingsReleaseInnermostFirst) {
  ScratchArena session(false);
  session.begin();

  void *outer_data = session.alloc(128);
  ASSERT_NE(outer_data, nullptr);
  write_pattern(outer_data, 128, 11);
  size_t const live_at_outer_binding = session.bytes_live();

  {
    ScratchArenaBinding outer_binding(session);
    void *middle = session.alloc(128);
    ASSERT_NE(middle, nullptr);
    write_pattern(middle, 128, 13);
    size_t const live_at_inner_binding = session.bytes_live();

    {
      ScratchArenaBinding inner_binding(session);
      void *leaf = session.alloc(256);
      ASSERT_NE(leaf, nullptr);
      EXPECT_EQ(session.bindings_active(), 2u);
      EXPECT_EQ(session.bytes_live(), live_at_inner_binding + 256);
    }
    EXPECT_EQ(session.bindings_active(), 1u);
    EXPECT_EQ(session.bytes_live(), live_at_inner_binding)
        << "the inner binding must not release the middle allocation";
    EXPECT_TRUE(pattern_intact(middle, 128, 13));
  }

  EXPECT_EQ(session.bytes_live(), live_at_outer_binding);
  EXPECT_TRUE(pattern_intact(outer_data, 128, 11));
  EXPECT_EQ(session.binding_rewinds(), 2u);
  session.end();
}

TEST(CompileArenaTest, OutOfOrderBindingDestructionIsRefused) {
  ScratchArena session(false);
  session.begin();

  size_t live_with_outer = 0;
  size_t live_with_inner = 0;
  {
    std::optional<ScratchArenaBinding> outer_binding;
    outer_binding.emplace(session);
    void *outer_data = session.alloc(128);
    ASSERT_NE(outer_data, nullptr);
    live_with_outer = session.bytes_live();

    {
      ScratchArenaBinding inner_binding(session);
      void *inner_data = session.alloc(128);
      ASSERT_NE(inner_data, nullptr);
      live_with_inner = session.bytes_live();
      ASSERT_EQ(live_with_inner, live_with_outer + 128);

      // Destroying the outer binding while the inner one is alive is a
      // programming error: the arena must stay intact instead of releasing
      // storage the inner binding still owns.
      outer_binding.reset();
      EXPECT_EQ(session.bytes_live(), live_with_inner)
          << "a refused destruction must not release anything";
      EXPECT_EQ(session.bindings_active(), 2u);
      EXPECT_EQ(session.binding_order_violations(), 1u);
      EXPECT_EQ(session.binding_rewinds(), 0u);
    }

    // The inner binding released its own storage and left the arena usable.
    EXPECT_EQ(session.bindings_active(), 1u);
    EXPECT_EQ(session.bytes_live(), live_with_outer);
    EXPECT_EQ(session.binding_rewinds(), 1u);
  }

  // The abandoned outer mark is dropped at the cycle boundary (its storage is
  // released with the rest of the cycle), so nothing is stuck afterwards.
  session.end();
  EXPECT_EQ(session.binding_marks_abandoned(), 1u)
      << "the abandoned mark is dropped at the cycle boundary";
  EXPECT_EQ(session.bindings_active(), 0u);
  EXPECT_EQ(session.bytes_live(), 0u);
}

TEST(CompileArenaTest, OversizeChunksAreNotRetainedAndResetDropsThePool) {
  ScratchArena session(false);
  session.begin();
  void *huge = session.alloc(compile_arena::kBaseChunkSize + 4096);
  ASSERT_NE(huge, nullptr);
  EXPECT_EQ(session.retained_chunks(), 0u);
  session.end();
  EXPECT_EQ(session.retained_chunks(), 0u)
      << "an oversize chunk is released instead of retained";
  EXPECT_EQ(session.bytes_live(), 0u);

  // A standard cycle that overflows the base chunk retains the overflow chunk
  // for reuse (this is the C-S1 retention pool the compiler relies on)...
  session.begin();
  void *status = session.alloc(1024);
  ASSERT_NE(status, nullptr);
  void *standard = session.alloc(compile_arena::kBaseChunkSize);
  ASSERT_NE(standard, nullptr);
  session.end();
  EXPECT_EQ(session.retained_chunks(), 1u);
  EXPECT_GT(session.retained_heap_bytes(), 0u);

  // ...and reset() drops the pool together with the arena's own base chunk.
  size_t const mallocs_before_reset = session.chunk_mallocs();
  session.reset();
  EXPECT_EQ(session.retained_chunks(), 0u);
  EXPECT_EQ(session.retained_heap_bytes(), 0u);
  EXPECT_EQ(session.bytes_live(), 0u);
  EXPECT_EQ(session.chunk_mallocs(), mallocs_before_reset)
      << "reset() itself does not allocate";

  // The arena stays usable after a full reset.
  void *after_reset = session.alloc(64);
  ASSERT_NE(after_reset, nullptr);
  EXPECT_GT(session.chunk_mallocs(), mallocs_before_reset);
  session.reset();
}

TEST(CompileArenaTest, StringAllocationIsTerminatedAndAccountingMatches) {
  ScratchArena session(false);
  session.begin();

  char *empty = session.alloc_string(0);
  ASSERT_NE(empty, nullptr);
  EXPECT_EQ(empty[0], '\0');

  char *text = session.alloc_string(31);
  ASSERT_NE(text, nullptr);
  std::memcpy(text, "compile_arena", 13);
  text[13] = '\0';
  EXPECT_STREQ(text, "compile_arena");
  // Accounting is in max_align_t-aligned units: 31+NUL aligns to 32, the empty
  // string's single byte aligns to 16.
  EXPECT_EQ(session.bytes_live(), 32u + 16u);
  session.end();
  EXPECT_EQ(session.bytes_live(), 0u);
}
