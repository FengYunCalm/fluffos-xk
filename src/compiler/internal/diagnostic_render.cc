// T3.3 (E4): structured diagnostic rendering.

#include "compiler/internal/diagnostic_render.h"

#include <cstdio>
#include <cstring>

#include <fmt/format.h>

#include "base/internal/strutils.h"  // for trim

namespace compiler_diag {

namespace {

// Appends the source line and a caret under the recorded column. The line is
// read from disk; when it cannot be read the snippet is skipped instead of
// pointing at the wrong text.
void append_snippet(std::string &out, const Diagnostic &diagnostic, bool trim_line) {
  const char *file = diagnostic.snippet.position.file;
  int const line = diagnostic.snippet.position.line;
  if (file == nullptr || line <= 0) {
    return;
  }
  char source[kMaxSnippetLineLength + 1];
  if (!read_source_line(file, line, source, sizeof(source))) {
    return;
  }

  bool const truncated = std::strlen(source) == kMaxSnippetLineLength;
  // The traditional shape trims the line the way prepare_logs() does, so the
  // reconstructed output matches what the driver printed. The structured shape
  // keeps the indentation, so the caret lines up with the printed text.
  std::string text = trim_line ? trim(std::string(source)) : std::string(source);
  out += fmt::format("  {}{}\n", text, truncated ? "..." : "");

  int column = diagnostic.snippet.position.column;
  if (column <= 0) {
    return;
  }
  // The recorded column is the lexer's position when it reported, which is one
  // past the last consumed character; the caret marks the last consumed
  // character so it lines up with the token the message is about.
  int caret = column - 1;
  if (caret < 0) {
    caret = 0;
  }
  out += fmt::format("  {}^\n", std::string(static_cast<size_t>(caret), ' '));
}

// One note per macro expansion step: a diagnostic inside a macro body is
// otherwise impossible to place (the file/line belong to the macro definition).
void append_expansions(std::string &out, const Diagnostic &diagnostic,
                       RenderStyle style) {
  for (auto const &expansion : diagnostic.expansions) {
    const char *file = expansion.position.file ? expansion.position.file : "?";
    if (style == RenderStyle::kClang) {
      out += fmt::format("{}:{}:{}: note: expanded from macro '{}'\n", file,
                         expansion.position.line, expansion.position.column,
                         expansion.message ? expansion.message : "?");
    } else {
      out += fmt::format("  in expansion of macro '{}' at {} line {}\n",
                         expansion.message ? expansion.message : "?", file,
                         expansion.position.line);
    }
  }
}

}  // namespace

bool read_source_line(const char *file, int line, char *buf, size_t buflen) {
  if (file == nullptr || line <= 0 || buf == nullptr || buflen == 0) {
    return false;
  }
  buf[0] = '\0';
  while (*file == '/') {
    file++;
  }
  FILE *fp = fopen(file, "rb");
  if (fp == nullptr) {
    return false;
  }

  constexpr size_t kScanSize = 8192;
  char scan[kScanSize];
  int current_line = 1;
  bool collecting = (line == 1);
  size_t collected = 0;

  while (size_t n = fread(scan, 1, sizeof(scan), fp)) {
    char *p = scan;
    char *end = scan + n;
    while (p < end) {
      char *nl = static_cast<char *>(std::memchr(p, '\n', static_cast<size_t>(end - p)));
      char *segment_end = nl ? nl : end;
      if (collecting) {
        size_t const take = static_cast<size_t>(segment_end - p);
        size_t const room = buflen - 1 - collected;
        size_t const copy = take < room ? take : room;
        if (copy != 0) {
          std::memcpy(buf + collected, p, copy);
          collected += copy;
        }
        if (collected >= buflen - 1) {
          // Enough of the line (it is truncated anyway): stop reading.
          buf[collected] = '\0';
          fclose(fp);
          return true;
        }
        if (nl != nullptr) {
          buf[collected] = '\0';
          fclose(fp);
          return true;
        }
      }
      if (nl != nullptr) {
        current_line++;
        p = nl + 1;
        if (current_line == line) {
          collecting = true;
        }
      } else {
        p = end;
      }
    }
  }
  fclose(fp);
  if (!collecting) {
    return false;
  }
  buf[collected] = '\0';
  return collected != 0;
}

std::string render_snippet(const Diagnostic &diagnostic) {
  std::string out;
  append_snippet(out, diagnostic, false);
  return out;
}

std::string render_diagnostic(const Diagnostic &diagnostic, RenderStyle style) {
  const char *file = diagnostic.snippet.position.file ? diagnostic.snippet.position.file : "?";
  const char *message = diagnostic.message ? diagnostic.message : "";

  std::string out;
  if (style == RenderStyle::kClang) {
    int const column = diagnostic.snippet.position.column > 0
                           ? diagnostic.snippet.position.column
                           : 1;
    out += fmt::format("{}:{}:{}: {}: {}", file, diagnostic.snippet.position.line, column,
                       diagnostic.severity == Severity::kWarning ? "warning" : "error", message);
    if (out.empty() || out.back() != '\n') {
      out += '\n';
    }
    append_snippet(out, diagnostic, false);
    append_expansions(out, diagnostic, style);
    return out;
  }

  out += fmt::format("/{} line {}: {}{}\n", file, diagnostic.snippet.position.line,
                     diagnostic.severity == Severity::kWarning ? "Warning: " : "", message);
  if (diagnostic.snippet.show_context) {
    append_snippet(out, diagnostic, true);
    append_expansions(out, diagnostic, style);
  }
  return out;
}

}  // namespace compiler_diag
