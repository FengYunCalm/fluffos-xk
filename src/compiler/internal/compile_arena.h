#ifndef FLUFFOS_SRC_COMPILER_INTERNAL_COMPILE_ARENA_H_
#define FLUFFOS_SRC_COMPILER_INTERNAL_COMPILE_ARENA_H_

// #1247 C-S1: compile-scope monotonic arena -- the sole owner of the
// compiler's scratch chunk pool. Explicit begin/end: error() throws a C++
// exception (simulate.cc:2325), so compile_file() pairs begin() with a
// DEFER end() (RAII, single point); begin() additionally drains any live
// chain left by an exception that unwound past the scope (safety net).
//
// Pool: one static 1MB base chunk plus up to kMaxRetainedStandardChunks
// retained 1MB standard chunks; oversize exact-fit chunks are always
// released at scope end. Allocation is a max_align_t-aligned monotonic
// bump that never crosses a chunk. Individual deallocation is a no-op.
//
// T3.1 (#1343 scratchpad arena): the pool is a ScratchArena instance rather
// than a single global, so a long-lived consumer (an interactive session
// driving repeated compiles) can own its own arena. The compiler keeps using
// the process-wide instance through the free functions below, which is
// exactly the previous behaviour. Consumption side:
//   * ScratchArena::begin() is "reset before use": it releases whatever the
//     previous cycle left live, so diagnostics of cycle N stay readable until
//     cycle N+1 starts (survival is reported by cross_cycle_bytes()).
//   * ScratchArenaBinding is a nested scope: destroying it releases every
//     chunk allocated inside it and rewinds the bump cursor of the chunk it
//     started in. Bindings must be destroyed innermost-first; an out-of-order
//     destruction is refused instead of corrupting the arena.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace compile_arena {

constexpr size_t kBaseChunkSize = 1u << 20;  // 1MB
constexpr size_t kMaxRetainedStandardChunks = 8;

struct Chunk;
class ScratchArenaBinding;

// One monotonic chunk pool. Instances are not thread-safe: each arena belongs
// to the thread that drives its cycles (the compiler arena is main-thread
// only, like the rest of the compiler).
class ScratchArena {
 public:
  // `static_base` selects zero-cost startup: the process-wide arena takes its
  // first chunk from a BSS array, every other arena allocates it on demand.
  explicit ScratchArena(bool static_base) noexcept;
  // Frees the arena's own memory: the live chain, the retained pool and (for
  // non-static arenas) the base chunk. The process-wide arena keeps its BSS
  // base, which is what makes allocation possible before/around main().
  ~ScratchArena();

  ScratchArena(const ScratchArena &) = delete;
  ScratchArena &operator=(const ScratchArena &) = delete;

  // Start a cycle: rewind to the base chunk and release what the previous
  // cycle left live (a leftover chain from an exception is part of that).
  void begin() noexcept;

  // End a cycle: release the live chain into the retained pool (bounded) and
  // rewind the base cursor.
  void end() noexcept;

  // Release everything, including the retained pool. The base chunk of the
  // process-wide arena is static and stays; other arenas drop theirs.
  void reset() noexcept;

  // Monotonic bump allocation, max_align_t aligned, never crossing a chunk.
  // Returns nullptr only for size 0 (callers treat 0 as no-op).
  void *alloc(size_t size);

  // Allocate len+1 bytes and NUL-terminate (len excludes the NUL).
  char *alloc_string(size_t len);

  // Observability.
  size_t cycle_bytes() const noexcept;        // bytes used in the current cycle
  size_t peak_cycle_bytes() const noexcept;   // high-water of cycle_bytes
  size_t bytes_live() const noexcept;         // bytes live right now
  size_t peak_live_bytes() const noexcept;    // high-water of bytes_live
  size_t cross_cycle_bytes() const noexcept;  // bytes begin() released last
  size_t bytes_allocated() const noexcept;    // lifetime total
  size_t chunk_mallocs() const noexcept;      // malloc() calls for chunks
  size_t reset_count() const noexcept;        // end() calls
  size_t retained_chunks() const noexcept;    // chunks in the retained pool
  size_t retained_heap_bytes() const noexcept;  // heap bytes of retained chunks
  size_t bindings_active() const noexcept;      // nesting depth right now
  size_t binding_rewinds() const noexcept;      // bindings that released
  size_t binding_order_violations() const noexcept;  // non-LIFO destructions
  // Marks dropped at a cycle boundary because their binding never released
  // them (out-of-order destruction, or a binding spanning a cycle). Their
  // storage is released with the rest of the cycle.
  size_t binding_marks_abandoned() const noexcept;

  friend class ScratchArenaBinding;

 private:
  // One nested scope: where the binding started (chunk + bump cursor) and the
  // live-chain head at that moment. Marks live in the arena, not in the
  // binding objects, so a binding that is destroyed out of order cannot leave
  // the arena pointing at freed memory.
  struct BindingMark {
    Chunk *chunk;
    size_t used;
    Chunk *live_head;
  };

  Chunk *acquire_standard();
  void release_chunk(Chunk *chunk) noexcept;
  void rewind_to(Chunk *chunk, size_t used, Chunk *live_head) noexcept;
  void clear_bindings() noexcept;
  void release_retained_pool() noexcept;

  Chunk *base_ = nullptr;      // first chunk (static BSS or owned)
  bool static_base_ = false;   // true when base_ points at the BSS array
  Chunk *live_ = nullptr;      // chunks allocated in the current cycle
  Chunk *retained_ = nullptr;  // retained standard chunks (LIFO)
  size_t retained_count_ = 0;
  size_t retained_heap_bytes_ = 0;
  Chunk *current_ = nullptr;   // chunk the bump cursor points into

  size_t cycle_bytes_ = 0;
  size_t peak_cycle_bytes_ = 0;
  size_t bytes_live_ = 0;
  size_t peak_live_bytes_ = 0;
  size_t cross_cycle_bytes_ = 0;
  size_t bytes_allocated_ = 0;
  size_t chunk_mallocs_ = 0;
  size_t reset_count_ = 0;
  size_t bindings_active_ = 0;
  size_t binding_rewinds_ = 0;
  size_t binding_order_violations_ = 0;
  size_t binding_marks_abandoned_ = 0;
  uint64_t binding_generation_ = 1;
  std::vector<BindingMark> binding_marks_;
};

// Nested scope inside one arena cycle. Destruction releases the chunks
// allocated since construction (and with them any allocation made inside).
// Must be destroyed before any enclosing binding; a non-LIFO destruction is
// refused (it releases nothing, counts as a violation and leaves the arena
// usable), because rewinding past a live inner binding would hand out storage
// that binding still owns.
class ScratchArenaBinding {
 public:
  explicit ScratchArenaBinding(ScratchArena &arena) noexcept;
  ~ScratchArenaBinding();

  ScratchArenaBinding(const ScratchArenaBinding &) = delete;
  ScratchArenaBinding &operator=(const ScratchArenaBinding &) = delete;

  size_t bytes_released() const noexcept { return bytes_released_; }

 private:
  ScratchArena &arena_;
  size_t mark_index_;
  uint64_t generation_;
  size_t bytes_released_ = 0;
  bool active_ = true;
};

// The process-wide arena the compiler allocates from.
ScratchArena &global();

// Compile-scope API (delegates to global()); unchanged call surface for the
// lexer, parser and grammar callbacks.
void begin() noexcept;
void end() noexcept;
void *alloc(size_t size);
char *alloc_string(size_t len);

// Observability (mud_status()).
size_t cycle_bytes() noexcept;           // bytes used in the current scope
size_t peak_cycle_bytes() noexcept;      // high-water of cycle_bytes
size_t chunk_mallocs() noexcept;         // malloc() calls for chunks
size_t reset_count() noexcept;           // end() calls
size_t retained_chunks() noexcept;       // chunks in the retained pool
size_t retained_heap_bytes() noexcept;   // heap bytes of retained chunks

}  // namespace compile_arena

#endif /* FLUFFOS_SRC_COMPILER_INTERNAL_COMPILE_ARENA_H_ */
