#ifndef FLUFFOS_SRC_COMPILER_INTERNAL_DIAGNOSTIC_H_
#define FLUFFOS_SRC_COMPILER_INTERNAL_DIAGNOSTIC_H_

// T3.2 (#1343): structured compile diagnostics.
//
// The compiler has always reported problems by formatting text
// (prepare_logs() -> debug_message() + the master's log_error apply). That
// text is lossy: a consumer (an interactive shell, a tool rendering errors
// after the compile finished, a test asserting on the exact position) cannot
// recover position, severity, context or macro provenance from it.
//
// A Diagnostic is the structured record of one report: what was wrong, where,
// plus the optional machine-applicable and provenance parts that the renderer
// (T3.3) prints as a snippet with carets and expansion notes.
//
// Ownership: strings are copied into the compile arena (T3.1), so a record
// stays readable until the next compile scope begins - exactly the window an
// interactive session needs (render after the cycle, before the next one).
// Records are only produced by the compiler's own yyerror()/yyywarn(); a
// runtime error that reports through smart_log() (apply.cc) has no position
// and is deliberately not recorded here.

#include <cstddef>
#include <vector>

namespace compiler_diag {

enum class Severity : unsigned char {
  kError = 0,
  kWarning = 1,
};

// 0 means "unknown" for line/column, so a record from a context without a
// lexer position stays distinguishable from line 0 of a file.
struct Position {
  const char *file = nullptr;  // arena string, without the leading '/'
  int line = 0;
  int column = 0;  // 1-based column of the caret, 0 when unknown
};

struct Range {
  Position begin;
  int end_line = 0;
  int end_column = 0;
};

// A machine-applicable replacement (unused by the parser today; carried so
// the renderer and tools see the same shape as the upstream model).
struct Fixit {
  Range range;
  const char *replacement = nullptr;
};

// One step of a macro expansion chain: the position of the expansion site and
// the ranges it covers, so a diagnostic inside a macro body can be explained.
struct Expansion {
  const char *message = nullptr;
  Position position;
  std::vector<Range> ranges;
};

// Where the diagnostic points. `show_context` mirrors PRAGMA_ERROR_CONTEXT:
// when it is off, the renderer prints just the location line.
struct Snippet {
  Position position;
  int end_line = 0;
  int end_column = 0;
  bool show_context = true;
};

struct Diagnostic {
  Severity severity = Severity::kError;
  const char *message = nullptr;  // arena string, without a trailing newline
  Snippet snippet;
  std::vector<Range> ranges;
  std::vector<Fixit> fixits;
  std::vector<Expansion> expansions;
};

// What the compiler is allowed to report as a compile diagnostic: the parse
// and code generation of the file being compiled. Diagnostics produced while
// the driver runs LPC during compilation (master applies, create()) go through
// the runtime path and are not part of this list.
// One active macro expansion step (name + expansion site), supplied by the
// lexer. Kept as a neutral struct so this module does not depend on the lexer.
struct ExpansionSite {
  const char *name;
  const char *file;
  int line;
  int column;
};

struct Source {
  const char *file;  // current_file, without the leading '/'
  int line;          // current_line
  int column;        // 1-based caret column, 0 when unknown
  Severity severity;
  const char *message;
  bool show_context;
  // Outermost-first macro expansion frames, or null when not inside a macro.
  const ExpansionSite *expansion_sites = nullptr;
  int expansion_count = 0;
};

// Compile scope: open before the parser runs (compile_file()), close when it
// returns. Opening drops the previous scope's records; recording outside a
// scope is ignored and counted (no arena cycle would own the strings).
void begin_scope();
void end_scope();
bool scope_active();
size_t records_outside_scope();

// Drop all recorded diagnostics without touching the scope depth.
void clear();

// Append one diagnostic; strings are copied into the compile arena.
void record(const Source &source);

// Recorded diagnostics, in report order.
size_t size();
const Diagnostic &at(size_t index);

// Last recorded diagnostic, or nullptr when none was recorded. Convenience for
// tests and for callers that only care about the newest record.
const Diagnostic *last();

}  // namespace compiler_diag

#endif /* FLUFFOS_SRC_COMPILER_INTERNAL_DIAGNOSTIC_H_ */
