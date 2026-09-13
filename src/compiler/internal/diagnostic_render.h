#ifndef FLUFFOS_SRC_COMPILER_INTERNAL_DIAGNOSTIC_RENDER_H_
#define FLUFFOS_SRC_COMPILER_INTERNAL_DIAGNOSTIC_RENDER_H_

// T3.3 (E4): rendering for structured compile diagnostics.
//
// Two styles, chosen per call so the driver's existing output stays exactly as
// it is and a consumer (interactive shell, tool, test) can ask for the
// structured shape:
//
//   * kTraditional - "/file line N: message" plus, when the diagnostic was
//     recorded with context, the source line and a caret. This is the shape
//     prepare_logs() produces today, reconstructed from the record.
//   * kClang - "file:line:col: error: message" plus a source snippet with a
//     caret and, for diagnostics reported inside a macro body, one note per
//     expansion step.
//
// Snippet text comes from read_source_line(), which reads the file relative to
// the driver's working directory (the mudlib root), so the rendered snippet
// shows the file as it is on disk. A diagnostic reported from a macro body
// therefore points at the expansion site text rather than the expanded lexer
// buffer - that is what makes the note chain useful.

#include <cstddef>
#include <string>

#include "compiler/internal/diagnostic.h"

namespace compiler_diag {

enum class RenderStyle {
  kTraditional = 0,
  kClang = 1,
};

// Longest source line read for a snippet (upstream parity: longer lines are
// cut and the renderer marks the cut).
constexpr size_t kMaxSnippetLineLength = 512;

// Reads 1-based `line` of `file` into `buf` (NUL terminated, at most
// buflen-1 bytes). Returns false when the file cannot be read or has fewer
// lines. This is the memchr-based scan of the upstream rendering stack: an 8K
// read buffer, no per-character fgetc (the slow version cost 45.4M calls in one
// testsuite run to serve 153 diagnostics). A missing file is not an error.
bool read_source_line(const char *file, int line, char *buf, size_t buflen);

// Renders one diagnostic (with a trailing newline). Never throws, and renders
// what it can when the snippet is unavailable.
std::string render_diagnostic(const Diagnostic &diagnostic, RenderStyle style);

// Renders the snippet lines only (source line + caret), as used by both styles;
// empty when the line is unavailable.
std::string render_snippet(const Diagnostic &diagnostic);

}  // namespace compiler_diag

#endif /* FLUFFOS_SRC_COMPILER_INTERNAL_DIAGNOSTIC_RENDER_H_ */
