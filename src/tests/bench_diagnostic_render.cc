// T3.3 baseline: cost of rendering one structured diagnostic.
//
// The renderer runs when a consumer asks for a diagnostic (the interactive
// shell prints one per compile, a tool renders a stored list), so the number to
// watch is per-case latency, not throughput. Two shapes are measured because
// they exercise different paths: a snippet read from disk (read_source_line's
// memchr scan) and an expansion note chain (no snippet I/O).

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "compiler/internal/diagnostic.h"
#include "compiler/internal/diagnostic_render.h"

namespace {

compiler_diag::Diagnostic make_snippet_case() {
  compiler_diag::Diagnostic diagnostic;
  diagnostic.severity = compiler_diag::Severity::kError;
  diagnostic.message = "syntax error, unexpected ';'";
  diagnostic.snippet.position.file = "etc/config.test";
  diagnostic.snippet.position.line = 1;
  diagnostic.snippet.position.column = 12;
  diagnostic.snippet.end_line = 1;
  diagnostic.snippet.end_column = 12;
  diagnostic.snippet.show_context = true;
  return diagnostic;
}

compiler_diag::Diagnostic make_expansion_case() {
  compiler_diag::Diagnostic diagnostic;
  diagnostic.severity = compiler_diag::Severity::kWarning;
  diagnostic.message = "Unused local variable 'value'";
  diagnostic.snippet.position.file = "no/such/file.c";
  diagnostic.snippet.position.line = 42;
  diagnostic.snippet.position.column = 7;
  diagnostic.snippet.show_context = true;
  for (int i = 0; i < 3; i++) {
    compiler_diag::Expansion expansion;
    expansion.message = i == 0 ? "XK_OUTER" : (i == 1 ? "XK_MIDDLE" : "XK_INNER");
    expansion.position.file = "single/master.c";
    expansion.position.line = 10 + i;
    expansion.position.column = 3;
    diagnostic.expansions.push_back(expansion);
  }
  return diagnostic;
}

void measure(const char *name, const compiler_diag::Diagnostic &diagnostic,
             compiler_diag::RenderStyle style, int iterations) {
  size_t sink = 0;
  auto const start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; i++) {
    std::string const rendered = compiler_diag::render_diagnostic(diagnostic, style);
    sink += rendered.size();
  }
  auto const elapsed = std::chrono::steady_clock::now() - start;
  double const ns_per_case =
      static_cast<double>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count()) /
      static_cast<double>(iterations);
  printf("%-34s %10.1f ns/case  (%zu)\n", name, ns_per_case, sink / static_cast<size_t>(iterations));
}

}  // namespace

int main() {
  constexpr int kIterations = 20000;
  auto const snippet_case = make_snippet_case();
  auto const expansion_case = make_expansion_case();

  printf("render_diagnostic, %d iterations per case\n", kIterations);
  measure("snippet/clang", snippet_case, compiler_diag::RenderStyle::kClang, kIterations);
  measure("snippet/traditional", snippet_case,
          compiler_diag::RenderStyle::kTraditional, kIterations);
  measure("expansion/clang", expansion_case, compiler_diag::RenderStyle::kClang, kIterations);
  measure("expansion/traditional", expansion_case,
          compiler_diag::RenderStyle::kTraditional, kIterations);
  return 0;
}
