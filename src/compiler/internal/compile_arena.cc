// #1247 C-S1: compile-scope monotonic arena implementation.
// T3.1 (#1343): instanced pools + nested-scope bindings.

#include "compile_arena.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "base/std.h"

namespace compile_arena {

struct Chunk {
  Chunk *next;      // retained-pool / live-chain link
  size_t size;      // usable bytes of data
  size_t used;      // bump cursor
  bool oversize;    // exact-fit chunk: always released at scope end
  char *data;       // points into the static base array or the malloc'd block
};

namespace {

// Static base chunk for the process-wide arena: 1MB in BSS, never freed, so
// compiler startup does not depend on the arena.
alignas(max_align_t) char g_base_data[kBaseChunkSize];
Chunk g_static_base{nullptr, kBaseChunkSize, 0, false, g_base_data};

constexpr size_t kChunkHeaderSize = sizeof(Chunk);

size_t align_up(size_t n) {
  constexpr size_t kAlign = alignof(max_align_t);
  return (n + kAlign - 1) & ~(kAlign - 1);
}

Chunk *new_chunk(size_t size, bool oversize, size_t *chunk_mallocs) {
  void *mem = DMALLOC(kChunkHeaderSize + size, TAG_COMPILER, "compile_arena");
  (*chunk_mallocs)++;
  Chunk *c = new (mem) Chunk;
  c->next = nullptr;
  c->size = size;
  c->used = 0;
  c->oversize = oversize;
  c->data = reinterpret_cast<char *>(mem) + kChunkHeaderSize;
  return c;
}

}  // namespace

ScratchArena::ScratchArena(bool static_base) noexcept
    : static_base_(static_base) {
  base_ = static_base_ ? &g_static_base : nullptr;
  current_ = base_;
}

ScratchArena::~ScratchArena() {
  // The compiler arena is destroyed at process exit; nothing allocates from it
  // afterwards. The static base chunk is BSS and stays, everything the arena
  // malloc'd is returned here so an exit-time leak checker sees no arena
  // memory.
  end();
  release_retained_pool();
  if (base_ && !static_base_) {
    FREE(base_);
    base_ = nullptr;
  }
  current_ = base_;
}

void ScratchArena::release_retained_pool() noexcept {
  Chunk *c = retained_;
  retained_ = nullptr;
  retained_count_ = 0;
  retained_heap_bytes_ = 0;
  while (c) {
    Chunk *next = c->next;
    FREE(c);
    c = next;
  }
}

Chunk *ScratchArena::acquire_standard() {
  if (retained_) {
    Chunk *c = retained_;
    retained_ = c->next;
    retained_count_--;
    retained_heap_bytes_ -= c->size;
    c->next = nullptr;
    c->used = 0;
    return c;
  }
  return new_chunk(kBaseChunkSize, false, &chunk_mallocs_);
}

void ScratchArena::release_chunk(Chunk *chunk) noexcept {
  if (chunk->oversize) {
    // Oversize chunks never enter the retained pool, so they never touch
    // retained_heap_bytes (only retained standard chunks are accounted).
    FREE(chunk);
    return;
  }
  if (retained_count_ < kMaxRetainedStandardChunks) {
    chunk->next = retained_;
    retained_ = chunk;
    retained_count_++;
    retained_heap_bytes_ += chunk->size;
    return;
  }
  FREE(chunk);
}

void ScratchArena::clear_bindings() noexcept {
  // Any binding created before this cycle boundary is stale: its mark index no
  // longer refers to a live scope. Bumping the generation makes its destructor
  // refuse instead of rewinding into the new cycle.
  binding_marks_abandoned_ += binding_marks_.size();
  binding_marks_.clear();
  bindings_active_ = 0;
  binding_generation_++;
}

void ScratchArena::begin() noexcept {
  // "Reset before use": release whatever the previous cycle left live (a
  // chain left behind by an error() exception counts as well). The released
  // amount is the cross-cycle survival metric a session reports.
  clear_bindings();
  cross_cycle_bytes_ = bytes_live_;
  Chunk *c = live_;
  live_ = nullptr;
  while (c) {
    Chunk *next = c->next;
    release_chunk(c);
    c = next;
  }
  if (base_) {
    base_->used = 0;
  }
  current_ = base_;
  cycle_bytes_ = 0;
  bytes_live_ = 0;
}

void ScratchArena::end() noexcept {
  clear_bindings();
  Chunk *c = live_;
  live_ = nullptr;
  while (c) {
    Chunk *next = c->next;
    release_chunk(c);
    c = next;
  }
  if (base_) {
    base_->used = 0;
  }
  current_ = base_;
  cycle_bytes_ = 0;
  bytes_live_ = 0;
  reset_count_++;
}

void ScratchArena::reset() noexcept {
  end();
  binding_order_violations_ = 0;
  binding_marks_abandoned_ = 0;
  release_retained_pool();
  if (base_ && !static_base_) {
    FREE(base_);
    base_ = nullptr;
  }
  current_ = base_;
  cross_cycle_bytes_ = 0;
}

void *ScratchArena::alloc(size_t size) {
  if (size == 0) {
    return nullptr;
  }
  if (base_ == nullptr) {
    // First use of a session arena: the base chunk is allocated on demand.
    base_ = new_chunk(kBaseChunkSize, false, &chunk_mallocs_);
    current_ = base_;
  }
  size_t need = align_up(size);
  Chunk *c = current_;
  if (c->used + need > c->size) {
    // Need a fresh chunk. Oversize (exact-fit) when the request exceeds a
    // standard chunk; otherwise take/allocate a standard chunk.
    if (need > kBaseChunkSize) {
      c = new_chunk(need, true, &chunk_mallocs_);
    } else {
      c = acquire_standard();
    }
    c->next = live_;
    live_ = c;
    current_ = c;
  }
  char *p = c->data + c->used;
  c->used += need;
  cycle_bytes_ += need;
  bytes_allocated_ += need;
  bytes_live_ += need;
  if (cycle_bytes_ > peak_cycle_bytes_) {
    peak_cycle_bytes_ = cycle_bytes_;
  }
  if (bytes_live_ > peak_live_bytes_) {
    peak_live_bytes_ = bytes_live_;
  }
  return p;
}

char *ScratchArena::alloc_string(size_t len) {
  char *p = static_cast<char *>(alloc(len + 1));
  if (p) {
    p[len] = '\0';
  }
  return p;
}

void ScratchArena::rewind_to(Chunk *chunk, size_t used, Chunk *live_head) noexcept {
  size_t released = 0;
  Chunk *c = live_;
  while (c && c != live_head) {
    Chunk *next = c->next;
    released += c->used;
    release_chunk(c);
    c = next;
  }
  live_ = live_head;
  if (chunk) {
    // The chunk the binding started in stays, but its bump cursor rewinds so
    // the storage handed out inside the binding becomes reusable.
    released += chunk->used > used ? chunk->used - used : 0;
    chunk->used = used;
  }
  current_ = chunk;
  cycle_bytes_ = cycle_bytes_ > released ? cycle_bytes_ - released : 0;
  bytes_live_ = bytes_live_ > released ? bytes_live_ - released : 0;
}

size_t ScratchArena::cycle_bytes() const noexcept { return cycle_bytes_; }
size_t ScratchArena::peak_cycle_bytes() const noexcept { return peak_cycle_bytes_; }
size_t ScratchArena::bytes_live() const noexcept { return bytes_live_; }
size_t ScratchArena::peak_live_bytes() const noexcept { return peak_live_bytes_; }
size_t ScratchArena::cross_cycle_bytes() const noexcept { return cross_cycle_bytes_; }
size_t ScratchArena::bytes_allocated() const noexcept { return bytes_allocated_; }
size_t ScratchArena::chunk_mallocs() const noexcept { return chunk_mallocs_; }
size_t ScratchArena::reset_count() const noexcept { return reset_count_; }
size_t ScratchArena::retained_chunks() const noexcept { return retained_count_; }
size_t ScratchArena::retained_heap_bytes() const noexcept {
  return retained_heap_bytes_;
}
size_t ScratchArena::bindings_active() const noexcept { return bindings_active_; }
size_t ScratchArena::binding_rewinds() const noexcept { return binding_rewinds_; }
size_t ScratchArena::binding_order_violations() const noexcept {
  return binding_order_violations_;
}
size_t ScratchArena::binding_marks_abandoned() const noexcept {
  return binding_marks_abandoned_;
}

ScratchArenaBinding::ScratchArenaBinding(ScratchArena &arena) noexcept
    : arena_(arena),
      mark_index_(arena.binding_marks_.size()),
      generation_(arena.binding_generation_) {
  arena_.binding_marks_.push_back(
      {arena_.current_, arena_.current_ ? arena_.current_->used : 0, arena_.live_});
  arena_.bindings_active_++;
}

ScratchArenaBinding::~ScratchArenaBinding() {
  if (!active_) {
    return;
  }
  active_ = false;
  if (generation_ != arena_.binding_generation_) {
    // The arena started a new cycle while this binding was alive, so its mark
    // is gone; the cycle boundary already released the storage.
    debug_message(
        "compile_arena: ScratchArenaBinding outlived its arena cycle; nothing "
        "to release.\n");
    return;
  }
  if (mark_index_ + 1 != arena_.binding_marks_.size()) {
    // Bindings must be destroyed innermost-first: an enclosing binding leaving
    // first would release storage an inner binding still owns. Refuse, report,
    // and leave the mark in place so the inner bindings keep working.
    arena_.binding_order_violations_++;
    debug_message(
        "compile_arena: ScratchArenaBinding destroyed out of order; arena left "
        "untouched.\n");
    return;
  }
  ScratchArena::BindingMark const mark = arena_.binding_marks_[mark_index_];
  size_t const before = arena_.bytes_live_;
  arena_.rewind_to(mark.chunk, mark.used, mark.live_head);
  bytes_released_ = before - arena_.bytes_live_;
  arena_.binding_marks_.pop_back();
  arena_.bindings_active_--;
  arena_.binding_rewinds_++;
}

ScratchArena &global() {
  static ScratchArena instance(true);
  return instance;
}

void begin() noexcept { global().begin(); }
void end() noexcept { global().end(); }
void *alloc(size_t size) { return global().alloc(size); }
char *alloc_string(size_t len) { return global().alloc_string(len); }

size_t cycle_bytes() noexcept { return global().cycle_bytes(); }
size_t peak_cycle_bytes() noexcept { return global().peak_cycle_bytes(); }
size_t chunk_mallocs() noexcept { return global().chunk_mallocs(); }
size_t reset_count() noexcept { return global().reset_count(); }
size_t retained_chunks() noexcept { return global().retained_chunks(); }
size_t retained_heap_bytes() noexcept { return global().retained_heap_bytes(); }

}  // namespace compile_arena
