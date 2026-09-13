// lpcshell (T3.4): interactive compile-and-diagnose shell.
//
// The interactive shape of the structured compiler diagnostics: a session that
// survives across evaluations, renders each problem with the rendering stack
// (T3.3) and silences the driver's traditional text while it does, so a user
// sees one report per problem instead of two.
//
// Scope is deliberately the compile loop and its diagnostics: no evaluation of
// the compiled program, no completion, no history file.
//
//   lpcshell <config_file>              interactive REPL
//   lpcshell <config_file> <file.c>...  compile files, exit 1 if any failed
//
// Interactive commands start with '#' at the start of a fresh input. '#' cannot
// start an LPC statement, so there is no ambiguity with source text.

#include "base/std.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "base/internal/rc.h"
#include "compiler/internal/LexStream.h"
#include "compiler/internal/compile_arena.h"
#include "compiler/internal/compiler.h"
#include "compiler/internal/diagnostic.h"
#include "compiler/internal/diagnostic_render.h"
#include "mainlib.h"
#include "vm/vm.h"

namespace {

// The interactive buffer is mirrored to this mudlib-relative path so the snippet
// renderer can quote the offending line (it reads from disk). The name is a
// long-standing gitignored testsuite scratch path.
constexpr const char *kBufferFile = "tmp_eval_file.c";

constexpr const char *kPrimaryPrompt = "> ";
constexpr const char *kContinuationPrompt = ".. ";

bool stdin_is_interactive() {
#ifdef _WIN32
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(fileno(stdin)) != 0;
#endif
}

struct Session {
  // Session-scope storage (T3.1): the session keeps the text of the evaluation
  // in progress here. The diagnostics module keeps its records in its own
  // arena, so both survive the compile they came from and are released when the
  // next evaluation starts.
  compile_arena::ScratchArena arena{false};
  compiler_diag::RenderStyle style = compiler_diag::RenderStyle::kClang;
  const char *retained_source = nullptr;
  size_t evaluations = 0;
  size_t failed_evaluations = 0;

  void retain(const std::string &source) {
    char *copy = arena.alloc_string(source.size());
    if (copy != nullptr) {
      std::memcpy(copy, source.data(), source.size());
    }
    retained_source = copy;
  }
};

// True when the accumulated text is a complete unit: no open bracket and no
// unfinished string or block comment. Quote characters inside comments do not
// count, and vice versa, so a mixed buffer stays predictable.
bool input_complete(const std::string &text) {
  int depth = 0;
  bool in_string = false;
  bool in_char = false;
  bool in_line_comment = false;
  bool in_block_comment = false;
  char previous = '\0';
  for (size_t i = 0; i < text.size(); i++) {
    char const c = text[i];
    if (in_line_comment) {
      if (c == '\n') {
        in_line_comment = false;
      }
    } else if (in_block_comment) {
      if (previous == '*' && c == '/') {
        in_block_comment = false;
      }
    } else if (in_string) {
      if (c == '"' && previous != '\\') {
        in_string = false;
      }
    } else if (in_char) {
      if (c == '\'' && previous != '\\') {
        in_char = false;
      }
    } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
      in_line_comment = true;
    } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '*') {
      in_block_comment = true;
    } else if (c == '"') {
      in_string = true;
    } else if (c == '\'') {
      in_char = true;
    } else if (c == '(' || c == '[' || c == '{') {
      depth++;
    } else if (c == ')' || c == ']' || c == '}') {
      depth--;
    }
    previous = c;
  }
  return depth <= 0 && !in_string && !in_char && !in_block_comment;
}

void write_buffer_file(const std::string &source) {
  std::ofstream out(kBufferFile, std::ios::binary | std::ios::trunc);
  if (out.good()) {
    out << source;
  }
}

// Compiles `source` (reported as `compile_name`) and renders the recorded
// diagnostics. Returns true when nothing was reported as an error.
bool evaluate(Session &session, const std::string &source, const std::string &compile_name,
              bool mirror_to_buffer_file) {
  session.evaluations++;
  session.retain(source);
  compiler_diag::clear();
  if (mirror_to_buffer_file) {
    write_buffer_file(source);
  }

  program_t *prog = nullptr;
  {
    std::istringstream stream(source);
    prog = compile_file(std::unique_ptr<LexStream>(new IStreamLexStream(stream)),
                        compile_name.c_str());
  }
  if (prog != nullptr) {
    deallocate_program(prog);
  }

  size_t errors = 0;
  for (size_t i = 0; i < compiler_diag::size(); i++) {
    compiler_diag::Diagnostic const &diagnostic = compiler_diag::at(i);
    if (diagnostic.severity == compiler_diag::Severity::kError) {
      errors++;
    }
    fputs(compiler_diag::render_diagnostic(diagnostic, session.style).c_str(), stdout);
  }

  bool const ok = errors == 0;
  if (!ok) {
    session.failed_evaluations++;
  }
  return ok;
}

bool is_known_command(const std::string &name) {
  return name.empty() || name == "help" || name == "quit" || name == "exit" ||
         name == "style" || name == "diagnostics" || name == "buffer" || name == "arena" ||
         name == "reset";
}

// A preprocessor directive is source text, not a command: '#define' and friends
// are written in the same shell as programs, and '#' is only a command marker
// for the shell's own words.
bool is_preprocessor_directive(const std::string &line) {
  static const char *const kDirectives[] = {
      "define", "undef",   "include", "if",    "ifdef", "ifndef", "elif",
      "else",   "endif",   "pragma",  "echo",  "line",  "error",  "warn",
  };
  std::string name = line.substr(1);
  size_t const space = name.find_first_of(" \t");
  if (space != std::string::npos) {
    name = name.substr(0, space);
  }
  for (auto const *directive : kDirectives) {
    if (name == directive) {
      return true;
    }
  }
  return false;
}

void print_help() {
  fputs(
      "#help                    this text\n"
      "#style clang|traditional diagnostic rendering style\n"
      "#diagnostics             re-render the last compile's diagnostics\n"
      "#buffer                  show the retained input of the last evaluation\n"
      "#arena                   session arena counters\n"
      "#reset                   reset the session arena\n"
      "#quit                    leave the shell\n",
      stdout);
}

void print_arena(const Session &session) {
  auto const &arena = session.arena;
  printf("session arena: live=%zu bytes, cross-cycle=%zu, retained=%zu chunks, allocs=%zu,"
         " rewinds=%zu, order-violations=%zu\n",
         arena.bytes_live(), arena.cross_cycle_bytes(), arena.retained_chunks(),
         arena.chunk_mallocs(), arena.binding_rewinds(), arena.binding_order_violations());
}

// Returns false when the shell should exit.
bool handle_command(Session &session, const std::string &command) {
  std::string name = command.substr(1);
  std::string argument;
  size_t const space = name.find_first_of(" \t");
  if (space != std::string::npos) {
    argument = name.substr(space + 1);
    name = name.substr(0, space);
  }

  if (name.empty() || name == "help") {
    print_help();
  } else if (name == "quit" || name == "exit") {
    return false;
  } else if (name == "style") {
    if (argument == "clang") {
      session.style = compiler_diag::RenderStyle::kClang;
      fputs("diagnostic style: clang\n", stdout);
    } else if (argument == "traditional") {
      session.style = compiler_diag::RenderStyle::kTraditional;
      fputs("diagnostic style: traditional\n", stdout);
    } else {
      fprintf(stdout, "#style expects 'clang' or 'traditional', got '%s'\n",
              argument.empty() ? "" : argument.c_str());
    }
  } else if (name == "diagnostics") {
    if (compiler_diag::size() == 0) {
      fputs("no diagnostics recorded\n", stdout);
    }
    for (size_t i = 0; i < compiler_diag::size(); i++) {
      fputs(compiler_diag::render_diagnostic(compiler_diag::at(i), session.style).c_str(),
            stdout);
    }
  } else if (name == "buffer") {
    if (session.retained_source == nullptr) {
      fputs("no evaluation yet\n", stdout);
    } else {
      printf("%s", session.retained_source);
      if (session.retained_source[0] != '\0' &&
          session.retained_source[std::strlen(session.retained_source) - 1] != '\n') {
        fputc('\n', stdout);
      }
    }
  } else if (name == "arena") {
    print_arena(session);
  } else if (name == "reset") {
    session.arena.begin();
    session.retained_source = nullptr;
    compiler_diag::clear();
    fputs("session state reset\n", stdout);
  }
  return true;
}

int run_repl(Session &session) {
  std::string buffer;
  // Directives are not a program on their own: they stay pending so the program
  // that uses them compiles as one unit. A blank line submits them anyway.
  bool only_directives = false;
  print_help();
  bool const interactive = stdin_is_interactive();

  for (;;) {
    if (interactive) {
      fputs(buffer.empty() ? kPrimaryPrompt : kContinuationPrompt, stdout);
      fflush(stdout);
    }
    std::string line;
    if (!std::getline(std::cin, line)) {
      break;
    }

    // A '#' command is only recognised at the start of a fresh input, and only
    // for the shell's own words: a preprocessor directive is source text.
    if (buffer.empty() && !line.empty() && line[0] == '#') {
      std::string name = line.substr(1);
      size_t const space = name.find_first_of(" \t");
      if (space != std::string::npos) {
        name = name.substr(0, space);
      }
      if (!is_known_command(name) && !is_preprocessor_directive(line)) {
        fprintf(stdout, "unknown command '#%s' (try #help)\n", name.c_str());
        continue;
      }
      if (is_known_command(name)) {
        if (!handle_command(session, line)) {
          return 0;
        }
        continue;
      }
      // Falls through: the directive is compiled as part of the program.
    }

    bool const blank = line.empty();
    bool const line_is_directive =
        !line.empty() && line[0] == '#' && is_preprocessor_directive(line);
    only_directives = (buffer.empty() || only_directives) && line_is_directive && !blank;
    buffer += line;
    buffer += '\n';
    if (blank && only_directives && !buffer.empty()) {
      // Escape hatch: a blank line submits directives that are waiting for a
      // program.
    } else if (only_directives) {
      continue;
    } else if (!input_complete(buffer)) {
      continue;
    }

    bool const ok = evaluate(session, buffer, kBufferFile, true);
    if (ok) {
      printf("compiled: %zu bytes\n", buffer.size());
    } else {
      fputs("compile failed\n", stdout);
    }
    session.arena.begin();
    buffer.clear();
    only_directives = false;
  }
  return 0;
}

int run_batch(Session &session, const std::vector<std::string> &files) {
  int failures = 0;
  for (auto const &file : files) {
    std::ifstream input(file, std::ios::binary);
    if (!input) {
      fprintf(stderr, "lpcshell: cannot read %s\n", file.c_str());
      failures++;
      continue;
    }
    std::string const source((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
    if (!evaluate(session, source, file, false)) {
      failures++;
    }
    session.arena.begin();
  }
  return failures == 0 ? 0 : 1;
}

void print_usage() {
  fprintf(stderr,
          "Usage: lpcshell config_file [file.c ...]\n"
          "       lpcshell config_file            interactive session\n");
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage();
    return 1;
  }

  init_main(argv[1]);

  // The shell renders the structured diagnostics itself; the driver keeps its
  // traditional text path for every other consumer.
  compiler_set_text_log_suppressed(true);

  Session session;
  std::vector<std::string> files;
  for (int i = 2; i < argc; i++) {
    files.emplace_back(argv[i]);
  }

  int status = files.empty() ? run_repl(session) : run_batch(session, files);

  compiler_set_text_log_suppressed(false);
  remove(kBufferFile);
  return status;
}
