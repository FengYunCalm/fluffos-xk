// T3.2 (#1343): structured compile diagnostic record.

#include "diagnostic.h"

#include <cstring>
#include <vector>

#include "compiler/internal/compile_arena.h"

namespace compiler_diag {

namespace {

// Diagnostics outlive the compile scope that produced them: an interactive
// session (or a test) renders them after compile_file() returned, i.e. after
// the compiler's arena cycle ended and its storage became reusable. They
// therefore live in their own session arena, reset by begin_scope() - the
// "reset before use" shape T3.1 provides.
compile_arena::ScratchArena g_diag_arena(false, TAG_COMPILE_DIAGNOSTICS,
                                         "compile_diagnostics");

std::vector<Diagnostic> g_compile_diags;
// Non-zero while the parser of a compile scope is running: records are only
// valid inside the arena cycle compile_file() opens, so anything reported
// outside one is dropped instead of storing pointers into recycled storage.
int g_scope_depth = 0;
size_t g_records_outside_scope = 0;

// Arena-backed copy: the record outlives the parser's stack buffers but not the
// compile scope (see the header).
const char *arena_dup(const char *text, size_t len);

const char *arena_dup(const char *text) {
  if (text == nullptr) {
    return nullptr;
  }
  return arena_dup(text, std::strlen(text));
}

const char *arena_dup(const char *text, size_t len) {
  if (text == nullptr) {
    return nullptr;
  }
  char *copy = g_diag_arena.alloc_string(len);
  if (copy != nullptr) {
    std::memcpy(copy, text, len);
  }
  return copy;
}

Position make_position(const char *file, int line, int column) {
  Position position;
  position.file = arena_dup(file);
  position.line = line;
  position.column = column;
  return position;
}

}  // namespace

void begin_scope() {
  // Reset before use: this releases the previous scope's records (their
  // storage is in the session arena) and starts a fresh cycle.
  g_diag_arena.begin();
  g_compile_diags.clear();
  g_scope_depth++;
}

void end_scope() {
  if (g_scope_depth > 0) {
    g_scope_depth--;
  }
}

bool scope_active() { return g_scope_depth > 0; }

size_t records_outside_scope() { return g_records_outside_scope; }

void clear() { g_compile_diags.clear(); }

void record(const Source &source) {
  if (g_scope_depth == 0) {
    // Reported outside a compile scope (no arena cycle to own the strings).
    g_records_outside_scope++;
    return;
  }
  Diagnostic diagnostic;
  diagnostic.severity = source.severity;
  diagnostic.message = arena_dup(source.message);
  diagnostic.snippet.position = make_position(source.file, source.line, source.column);
  diagnostic.snippet.end_line = source.line;
  diagnostic.snippet.end_column = source.column;
  diagnostic.snippet.show_context = source.show_context;
  for (int i = 0; i < source.expansion_count; i++) {
    const ExpansionSite &site = source.expansion_sites[i];
    Expansion expansion;
    expansion.message = arena_dup(site.name);
    expansion.position = make_position(site.file, site.line, site.column);
    diagnostic.expansions.push_back(std::move(expansion));
  }
  g_compile_diags.push_back(std::move(diagnostic));
}

size_t size() { return g_compile_diags.size(); }

const Diagnostic &at(size_t index) { return g_compile_diags[index]; }

const Diagnostic *last() {
  return g_compile_diags.empty() ? nullptr : &g_compile_diags.back();
}

}  // namespace compiler_diag
