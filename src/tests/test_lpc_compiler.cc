#include "test_lpc_support.h"

TEST_F(DriverTest, TestInMemoryCompileFile) {
  program_t* prog = nullptr;

  std::istringstream source("void test() {}");
  auto stream = std::make_unique<IStreamLexStream>(source);
  prog = compile_file(std::move(stream), "test");

  ASSERT_NE(prog, nullptr);
  deallocate_program(prog);
}

TEST_F(DriverTest, TestLpcVmRepresentativeWorkloadProbe) {
  auto mapping_number = [](mapping_t *map, const char *key) -> LPC_INT {
    auto *value = find_string_in_mapping(map, key);
    EXPECT_NE(value, nullptr) << key;
    EXPECT_EQ(value ? value->type : T_INVALID, T_NUMBER) << key;
    return value && value->type == T_NUMBER ? value->u.number : 0;
  };
  object_t *probe = clone_object_for_test("single/void");
  ASSERT_NE(probe, nullptr);

  constexpr LPC_INT kItemCount = 8;
#ifdef _WIN32
  // GetThreadTimes has coarse resolution; keep the measured workload above it.
  constexpr LPC_INT kIterations = 256;
#else
  constexpr LPC_INT kIterations = 2;
#endif
  push_number(kItemCount);
  push_number(kIterations);
  auto *result = safe_apply("benchmark_representative_lpc_workload", probe, 2,
                            ORIGIN_DRIVER);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, T_MAPPING);
  ASSERT_EQ(mapping_number(result->u.map, "item_count"), kItemCount);
  ASSERT_EQ(mapping_number(result->u.map, "iterations"), kIterations);
  ASSERT_GT(mapping_number(result->u.map, "checksum"), 0);
  ASSERT_GT(mapping_number(result->u.map, "cpu_total_ns"), 0);

  destruct_object_for_test(probe);
}

TEST_F(DriverTest, TestInMemoryCompileFileFail) {
  program_t* prog = nullptr;
  std::istringstream source("aksdljfaljdfiasejfaeslfjsaef");
  auto stream = std::make_unique<IStreamLexStream>(source);
  prog = compile_file(std::move(stream), "test");

  ASSERT_EQ(prog, nullptr);
}

TEST_F(DriverTest, TestValidLPC_FunctionDeafultArgument) {
  const char* source = R"(
// default case
void test1() {
}

// default case
void test2(int a, int b) {
  ASSERT_EQ(a, 1);
  ASSERT_EQ(b, 2);
}

// varargs
void test3(int a, int* b ...) {
  ASSERT_EQ(a, 1);
  ASSERT_EQ(b[0], 2);
  ASSERT_EQ(b[1], 3);
  ASSERT_EQ(b[2], 4);
  ASSERT_EQ(b[3], 5);
}

// can have multiple trailing arguments with a FP for calculating default value
void test4(int a, string b: (: "str" :), int c: (: 0 :)) {
  switch(a) {
    case 1: {
      ASSERT_EQ("str", b);
      ASSERT_EQ(0, c);
      break;
    }
    case 2: {
      ASSERT_EQ("aaa", b);
      ASSERT_EQ(0, c);
      break;
    }
    case 3: {
      ASSERT_EQ("bbb", b);
      ASSERT_EQ(3, c);
      break;
    }
  }
}

void do_tests() {
    test1();
    test2(1, 2);
    test3(1, 2, 3, 4, 5);
    // direct call
    test4(1);
    test4(2, "aaa");
    test4(3, "bbb", 3);
    // apply
    this_object()->test4(1);
    this_object()->test4(2, "aaa");
    this_object()->test4(3, "bbb", 3);
}
  )";
  std::istringstream iss(source);
  auto stream = std::make_unique<IStreamLexStream>(iss);
  auto *prog = compile_file(std::move(stream), "test");

  ASSERT_NE(prog, nullptr);
  dump_prog(prog, stdout, 1 | 2);
  deallocate_program(prog);
}


TEST_F(DriverTest, TestLPC_FunctionInherit) {
    // Load the inherited object first
    error_context_t econ{};
    save_context(&econ);
    try {
    auto obj = find_object("/single/tests/compiler/function");
    ASSERT_NE(obj , nullptr);

    auto obj2 = find_object("/single/tests/compiler/function_inherit");
    ASSERT_NE(obj2 , nullptr);

    auto obj3 = find_object("/single/tests/compiler/function_inherit_2");
    ASSERT_NE(obj3 , nullptr);

    dump_prog(obj3->prog, stdout, 1 | 2);
    } catch (...) {
        restore_context(&econ);
        FAIL();
    }
    pop_context(&econ);

}

namespace {
struct SimulTableSnapshot {
  simul_entry *names = nullptr;
  function_lookup_info_t *funcs = nullptr;
  int count = 0;
};

SimulTableSnapshot SaveSimulTable() {
  SimulTableSnapshot s;
  s.count = num_simul_efun;
  if (s.count) {
    // The production transaction frees the live arrays with FREE(). Keep
    // snapshot copies in the same allocator domain so a later test can use
    // them as live tables without crossing debugmalloc's ownership boundary.
    s.names = static_cast<simul_entry *>(
        DMALLOC(s.count * sizeof(simul_entry), TAG_SIMULS, "test_simul_snapshot_names"));
    s.funcs = static_cast<function_lookup_info_t *>(DMALLOC(
        s.count * sizeof(function_lookup_info_t), TAG_SIMULS, "test_simul_snapshot_funcs"));
    memcpy(s.names, simul_names, s.count * sizeof(simul_entry));
    memcpy(s.funcs, simuls, s.count * sizeof(function_lookup_info_t));
  }
  return s;
}

void RestoreSimulTable(const SimulTableSnapshot &s) {
  // Deactivate every currently active name (mirror of remove_simuls).
  for (int i = 0; i < num_simul_efun; i++) {
    ident_hash_elem_t *ihe = lookup_ident(simul_names[i].name);
    if (ihe) {
      if (ihe->dn.simul_num != -1) {
        ihe->sem_value--;
      }
      ihe->dn.simul_num = -1;
      ihe->token &= ~IHE_SIMUL;
      ihe->token |= IHE_ORPHAN;
    }
  }
  // Re-activate exactly the snapshot's active entries.
  for (int i = 0; i < s.count; i++) {
    if (!s.funcs[i].func) {
      continue;
    }
    ident_hash_elem_t *ihe = lookup_ident(s.names[i].name);
    if (!ihe) {
      // Same CHECK_ELEM trap as the reload path: lookup_ident() returns
      // NULL for sem_value == 0 entries, which the deactivation loop above
      // just produced. The perm-ident element itself always survives.
      ihe = find_or_add_perm_ident(s.names[i].name);
    }
    EXPECT_NE(ihe, nullptr) << "missing ident for " << s.names[i].name;
    if (!ihe) {
      continue;
    }
    ihe->token |= IHE_SIMUL;
    ihe->token &= ~IHE_ORPHAN;
    ihe->sem_value++;
    ihe->dn.simul_num = s.names[i].index;
  }
  // The snapshot takes ownership of the live-table role. The table currently
  // installed by the test is no longer reachable after this assignment, so
  // release it here; callers must first finish or rollback every prepared
  // activation that still owns an older table through old_names/old_funcs.
  if (simul_names != s.names) {
    FREE(simul_names);
  }
  if (simuls != s.funcs) {
    FREE(simuls);
  }
  // Deactivated names from the tests keep an inert perm-ident (token 0,
  // simul_num -1) -- harmless, matching failed-transaction semantics.
  simul_names = s.names;
  simuls = s.funcs;
  num_simul_efun = s.count;
}

int FindDispatchIndex(const char *name) {
  for (int i = 0; i < num_simul_efun; i++) {
    if (strcmp(simul_names[i].name, name) == 0) {
      return simul_names[i].index;
    }
  }
  return -1;
}

int FindProgramIndex(program_t *prog, const char *name) {
  for (int i = 0; i < prog->num_functions_defined + prog->last_inherited; i++) {
    function_t *f = find_func_entry(prog, i);
    if (strcmp(f->funcname, name) == 0) {
      return i;
    }
  }
  return -1;
}

void RemoveTestSimulIdent(const char *name) {
  auto *ident = lookup_perm_ident(name);
  if (!ident || ident->sem_value != 0 ||
      (ident->token & (IHE_RESWORD | IHE_EFUN | IHE_SIMUL))) {
    return;
  }
  const char *ident_name = ident->name;
  if (remove_perm_ident(ident)) {
    free_string(ident_name);
  }
}


// Prints the rendered diagnostic on a golden mismatch (the raw string is
// escaped in gtest output otherwise).
std::string actual_rendering(const std::string &text) {
  return "\n--- actual rendering ---\n" + text + "--- end ---\n";
}

program_t *CompileSimulProg(const std::string &src) {
  std::istringstream stream(src);
  return compile_file(std::make_unique<IStreamLexStream>(stream), "simul_reload_test");
}

int FindValidSimulSlot() {
  for (int i = 0; i < num_simul_efun; i++) {
    const char *name = simul_names[i].name;
    if (name && name[0] != '#') {
      return i;
    }
  }
  return -1;
}

// Locate a program variable's block slot by name (own variables first,
// inherited variables after). Returns -1 when absent.
int FindVariableSlot(program_t *prog, const char *name) {
  for (int i = 0; i < prog->num_variables_total; i++) {
    if (strcmp(prog->variable_table[i], name) == 0) {
      return i;
    }
  }
  return -1;
}
}  // namespace

TEST_F(DriverTest, TestSimulEfunReloadAddDropReadd) {
  ASSERT_GT(num_simul_efun, 0) << "config.test must load a simul_efun file";
  SimulTableSnapshot snap = SaveSimulTable();

  const int old_count = num_simul_efun;

  const char *kFoo = "simul_efun_xyz_foo";
  const char *kBar = "simul_efun_xyz_bar";
  ASSERT_EQ(lookup_ident(kFoo), nullptr) << "test name must not collide";

  // Program A: provides foo (a fresh name) -> middle insert at a sorted
  // position that is not the cumulative tail, plus a live-table survivor.
  const int survivor_slot = FindValidSimulSlot();
  ASSERT_GE(survivor_slot, 0);
  std::string survivor = simul_names[survivor_slot].name;
  std::string src_a = std::string("string ") + kFoo + "() { return \"a\"; }\n"
                      "string " + survivor + "() { return \"survivor\"; }\n";
  program_t *prog_a = CompileSimulProg(src_a);

  ASSERT_NE(prog_a, nullptr);

  simul_efun_prepared_t prep_a;
  simul_efuns_prepare(prog_a, &prep_a);
  ASSERT_EQ(prep_a.count, old_count + 1) << "foo inserted, survivor updated in place";
  simul_efuns_activate(&prep_a);

  // foo: fresh dispatch index = cumulative count; survivor: index preserved.
  int foo_idx = FindDispatchIndex(kFoo);
  int surv_idx = FindDispatchIndex(survivor.c_str());
  ASSERT_GE(foo_idx, old_count);
  ASSERT_EQ(surv_idx, snap.names[survivor_slot].index);
  // Survivor slot must hold the NEW program's function (the pre-fix bug
  // shifted dispatch slots on middle insert, making this a different or
  // null function).
  ASSERT_NE(simuls[surv_idx].func, nullptr);
  ASSERT_STREQ(simuls[surv_idx].func->funcname, survivor.c_str());
  // Function-table order is sorted by funcname, not declaration order:
  // locate foo's program index by name. The dispatch slot must hold the
  // new program's function AND its runtime index (call_direct relies on
  // both matching the caller's expectation).
  int foo_pidx = FindProgramIndex(prog_a, kFoo);
  ASSERT_GE(foo_pidx, 0);
  ASSERT_EQ(simuls[foo_idx].func, find_func_entry(prog_a, foo_pidx));
  ASSERT_EQ(simuls[foo_idx].index, foo_pidx);

  // Name must be present in the sorted table (position key existence).
  int foo_pos = -1;
  for (int i = 0; i < num_simul_efun; i++) {
    if (strcmp(simul_names[i].name, kFoo) == 0) {
      foo_pos = i;
    }
  }
  ASSERT_GE(foo_pos, 0);
  // The sorted-table position and the dispatch index are independent keys
  // (position = pointer-ordered table slot, dispatch = cumulative count);
  // they may coincide under some allocator layouts (ASan), so assert the
  // dispatch semantics (last injected = count-1) instead of inequality.
  ASSERT_EQ(foo_idx, num_simul_efun - 1);
  ident_hash_elem_t *ihe = lookup_ident(kFoo);
  ASSERT_NE(ihe, nullptr);
  ASSERT_TRUE(ihe->token & IHE_SIMUL);
  ASSERT_EQ(ihe->dn.simul_num, foo_idx);

  // Program B: drops foo (and the survivor). foo must stay in the table at
  // its dispatch index, inactive; its ident must become the compile-time
  // rejection state (IHE_ORPHAN, simul_num -1).
  std::string src_b = std::string("string ") + kBar + "() { return \"b\"; }\n";
  program_t *prog_b = CompileSimulProg(src_b);
  ASSERT_NE(prog_b, nullptr);
  simul_efun_prepared_t prep_b;
  simul_efuns_prepare(prog_b, &prep_b);
  simul_efuns_activate(&prep_b);

  ASSERT_EQ(FindDispatchIndex(kFoo), foo_idx) << "dropped name keeps its index";
  ASSERT_EQ(simuls[foo_idx].func, nullptr) << "dropped name stays inactive";
  // lookup_ident() returns NULL for sem_value == 0 entries (CHECK_ELEM in
  // lex.cc), which is exactly the post-drop state; the perm-ident element
  // itself survives in the compile-time rejection state (IHE_ORPHAN,
  // simul_num -1) and is reachable via find_or_add_perm_ident.
  ihe = find_or_add_perm_ident(kFoo);
  ASSERT_NE(ihe, nullptr);
  ASSERT_FALSE(ihe->token & IHE_SIMUL);
  ASSERT_TRUE(ihe->token & IHE_ORPHAN);
  ASSERT_EQ(ihe->dn.simul_num, -1);
  int bar_idx = FindDispatchIndex(kBar);
  ASSERT_EQ(bar_idx, old_count + 1) << "next cumulative index (foo took old_count, survivor updated in place)";
  int bar_pidx = FindProgramIndex(prog_b, kBar);
  ASSERT_GE(bar_pidx, 0);
  ASSERT_EQ(simuls[bar_idx].func, find_func_entry(prog_b, bar_pidx));
  ASSERT_EQ(simuls[bar_idx].index, bar_pidx);

  // Program C: re-adds foo. prepare() re-resolves the ident slot to the
  // existing perm-ident (dropped entries keep their ident hash element);
  // the same dispatch index must dispatch again.
  std::string src_c = std::string("string ") + kFoo + "() { return \"c\"; }\n";
  program_t *prog_c = CompileSimulProg(src_c);
  ASSERT_NE(prog_c, nullptr);
  simul_efun_prepared_t prep_c;
  simul_efuns_prepare(prog_c, &prep_c);
  simul_efuns_activate(&prep_c);

  ASSERT_EQ(FindDispatchIndex(kFoo), foo_idx) << "re-added name keeps its index";
  int foo_pidx_c = FindProgramIndex(prog_c, kFoo);
  ASSERT_GE(foo_pidx_c, 0);
  ASSERT_EQ(simuls[foo_idx].func, find_func_entry(prog_c, foo_pidx_c));
  ASSERT_EQ(simuls[foo_idx].index, foo_pidx_c);
  ihe = lookup_ident(kFoo);
  ASSERT_NE(ihe, nullptr);
  ASSERT_TRUE(ihe->token & IHE_SIMUL);
  ASSERT_FALSE(ihe->token & IHE_ORPHAN);
  ASSERT_EQ(ihe->dn.simul_num, foo_idx);

  // Finish the successful activations in reverse order so each prepared
  // object releases the older table it retained. RestoreSimulTable() then
  // releases the final active shadow table before installing the snapshot.
  simul_efuns_finish(&prep_c);
  simul_efuns_finish(&prep_b);
  simul_efuns_finish(&prep_a);
  RestoreSimulTable(snap);
  RemoveTestSimulIdent(kFoo);
  RemoveTestSimulIdent(kBar);
  deallocate_program(prog_a);
  deallocate_program(prog_b);
  deallocate_program(prog_c);
}

TEST_F(DriverTest, TestCompileDiagnosticsRecordPositionSeverityAndContext) {
  // T3.2 golden: a syntax error is recorded with the file, the offending line
  // and the caret column, and the rendered text path is untouched (the full
  // testsuite run is the text-path regression).
  std::string const bad_source = "void create() {\n  int x = ;\n}\n";
  compiler_diag::clear();
  program_t *bad_prog = nullptr;
  {
    std::istringstream stream(bad_source);
    bad_prog = compile_file(std::make_unique<IStreamLexStream>(stream), "diag_syntax_test");
  }
  EXPECT_EQ(bad_prog, nullptr) << "a syntax error must not produce a program";
  ASSERT_GE(compiler_diag::size(), 1u);
  const auto *diag = compiler_diag::last();
  ASSERT_NE(diag, nullptr);
  EXPECT_EQ(diag->severity, compiler_diag::Severity::kError);
  ASSERT_NE(diag->message, nullptr);
  EXPECT_NE(std::string(diag->message).find("syntax error"), std::string::npos)
      << "message: " << diag->message;
  ASSERT_NE(diag->snippet.position.file, nullptr);
  EXPECT_STREQ(diag->snippet.position.file, "diag_syntax_test");
  EXPECT_EQ(diag->snippet.position.line, 2);
  EXPECT_EQ(diag->snippet.end_line, 2);
  // The column is the lexer's position when it reported, i.e. one past the
  // ';' it could not use (the same position the traditional caret uses).
  auto const semicolon = bad_source.find(';', bad_source.find('\n') + 1);
  auto const line_start = bad_source.find('\n') + 1;
  EXPECT_EQ(diag->snippet.position.column,
            static_cast<int>(semicolon - line_start) + 2)
      << "column should be the lexer position after the unexpected ';'";
  EXPECT_TRUE(diag->snippet.show_context);
  EXPECT_TRUE(diag->ranges.empty());
  EXPECT_TRUE(diag->fixits.empty());
  EXPECT_TRUE(diag->expansions.empty());
  EXPECT_EQ(compiler_diag::records_outside_scope(), 0u);

  // A warning keeps its severity and does not fail the compile.
  std::string const warned_source =
      "#pragma no_such_pragma\nvoid create() { }\n";
  compiler_diag::clear();
  program_t *warned_prog = nullptr;
  {
    std::istringstream stream(warned_source);
    warned_prog = compile_file(std::make_unique<IStreamLexStream>(stream), "diag_warning_test");
  }
  ASSERT_NE(warned_prog, nullptr);
  ASSERT_EQ(compiler_diag::size(), 1u);
  const compiler_diag::Diagnostic *warning = &compiler_diag::at(0);
  EXPECT_EQ(warning->severity, compiler_diag::Severity::kWarning);
  ASSERT_NE(warning->message, nullptr);
  EXPECT_NE(std::string(warning->message).find("Unknown #pragma"), std::string::npos)
      << "message: " << warning->message;
  ASSERT_NE(warning->snippet.position.file, nullptr);
  EXPECT_STREQ(warning->snippet.position.file, "diag_warning_test");
  EXPECT_EQ(warning->snippet.position.line, 1);
  EXPECT_GT(warning->snippet.position.column, 0);
  deallocate_program(warned_prog);

  // Opening a new compile scope drops the previous records (the arena cycle
  // that owns their strings restarts).
  {
    std::istringstream stream("void create() { }\n");
    program_t *fresh = compile_file(std::make_unique<IStreamLexStream>(stream), "diag_clean_test");
    ASSERT_NE(fresh, nullptr);
    EXPECT_EQ(compiler_diag::size(), 0u)
        << "a clean compile starts with no diagnostics";
    deallocate_program(fresh);
  }

  // A runtime report (smart_log from the driver, not the compiler) is not a
  // compile diagnostic: the runtime path does not even attempt a record, so
  // neither the list nor the out-of-scope counter moves.
  compiler_diag::clear();
  size_t const outside_before = compiler_diag::records_outside_scope();
  smart_log("driver", 0, "runtime report, no lexer position\n", 1);
  EXPECT_EQ(compiler_diag::size(), 0u);
  EXPECT_EQ(compiler_diag::records_outside_scope(), outside_before);
}

TEST_F(DriverTest, TestCompilerDiagnosticsTreatPrecomposedPercentMessagesAsText) {
  // Overload warnings are assembled from program filenames.  A filename may
  // contain '%', so the completed warning must be passed as data rather than
  // interpreted as a printf format string.
  compiler_diag::clear();
  const char *base_path = "clone/diag_percent%base.c";
  PathCleanupGuard base_file_guard{base_path};
  std::ofstream base_file(base_path, std::ios::trunc);
  ASSERT_TRUE(base_file.good());
  base_file << "void dummy() { }\n";
  base_file.close();
  ASSERT_TRUE(base_file.good());
  object_t *base = load_object_for_test("clone/diag_percent%base");
  ASSERT_NE(base, nullptr);
  std::istringstream stream(
      "inherit \"/clone/diag_percent%base\";\n"
      "inherit \"/clone/diag_percent%base\";\n"
      "void create() { }\n");
  program_t *program = compile_file(std::make_unique<IStreamLexStream>(stream),
                                     "clone/diag_percent%root");
  ASSERT_NE(program, nullptr);

  bool found_percent_filename = false;
  for (size_t i = 0; i < compiler_diag::size(); i++) {
    const auto &diagnostic = compiler_diag::at(i);
    if (diagnostic.message != nullptr &&
        std::string(diagnostic.message).find("diag_percent%base") != std::string::npos) {
      found_percent_filename = true;
      break;
    }
  }
  EXPECT_TRUE(found_percent_filename)
      << "the duplicate-inherit warning must preserve the percent-containing filename";
  deallocate_program(program);
  destruct_object_for_test(base);
}

TEST_F(DriverTest, TestDiagnosticRenderingGoldenStylesAndSnippets) {
  // T3.3 golden: read_source_line() plus both render styles over a fixture the
  // renderer can read back from disk. Fixtures live under the testsuite's
  // gitignored log/ directory and are removed again.
  auto fixture_path = std::string("log/diag_render_fixture.c");
  auto fixture = std::string("// diag render fixture\n") + "void create() {\n  int x = ;\n}\n";
  {
    // Plain stdio: the driver's write_file() applies mudlib path permissions,
    // and this fixture only exists so the renderer can read a source line back.
    std::ofstream out(fixture_path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.good());
    out << fixture;
    ASSERT_TRUE(out.good());
  }

  struct FixtureGuard {
    std::string path;
    ~FixtureGuard() { remove(path.c_str()); }
  } fixture_guard{fixture_path};
  // The compiler reports files relative to the mudlib root, i.e. the path
  // write_file() sees (without the leading slash).
  std::string const source_name = fixture_path;

  compiler_diag::clear();
  program_t *prog = nullptr;
  {
    std::istringstream stream(fixture);
    prog = compile_file(std::make_unique<IStreamLexStream>(stream), source_name.c_str());
  }
  EXPECT_EQ(prog, nullptr);
  ASSERT_GE(compiler_diag::size(), 1u);
  const compiler_diag::Diagnostic *first = compiler_diag::last();
  ASSERT_NE(first, nullptr);
  const compiler_diag::Diagnostic &diag = *first;

  // read_source_line(): the reported line, the first line, an empty tail line
  // and a missing file.
  char line[compiler_diag::kMaxSnippetLineLength + 1];
  ASSERT_TRUE(compiler_diag::read_source_line(source_name.c_str(), 1, line, sizeof(line)));
  EXPECT_EQ(std::string(line), "// diag render fixture");
  ASSERT_TRUE(compiler_diag::read_source_line(source_name.c_str(), 2, line, sizeof(line)));
  EXPECT_EQ(std::string(line), "void create() {");
  ASSERT_TRUE(compiler_diag::read_source_line(source_name.c_str(), 3, line, sizeof(line)));
  EXPECT_EQ(std::string(line), "  int x = ;");
  EXPECT_FALSE(compiler_diag::read_source_line(source_name.c_str(), 5, line, sizeof(line)))
      << "the file ends after line 4's newline: no text to render";
  EXPECT_FALSE(compiler_diag::read_source_line("log/definitely_missing_file.c", 1, line,
                                               sizeof(line)));
  EXPECT_FALSE(compiler_diag::read_source_line(source_name.c_str(), 0, line, sizeof(line)));

  // Traditional style: the shape the driver prints today, rebuilt from the
  // record (the driver's own text path is unchanged and pinned by the full
  // testsuite run).
  // The caret column is the lexer position, one past the ';' it could not use:
  // two spaces of indent plus (column - 1) spaces before the caret, matching
  // what prepare_logs() prints today.
  auto const traditional =
      compiler_diag::render_diagnostic(diag, compiler_diag::RenderStyle::kTraditional);
  EXPECT_EQ(traditional,
            "/log/diag_render_fixture.c line 3: syntax error, unexpected ';'\n"
            "  int x = ;\n"
            "             ^\n")
      << actual_rendering(traditional);

  // Structured style: location, severity, snippet, caret.
  auto const clang = compiler_diag::render_diagnostic(diag, compiler_diag::RenderStyle::kClang);
  EXPECT_EQ(clang,
            "log/diag_render_fixture.c:3:12: error: syntax error, unexpected ';'\n"
            "    int x = ;\n"
            "             ^\n")
      << actual_rendering(clang);

  // A diagnostic inside a macro body carries the expansion chain and renders a
  // note per step.
  std::string const macro_source =
      "#define BAD_EXPR int x = ;\n"
      "void create() {\n"
      "  BAD_EXPR\n"
      "}\n";
  compiler_diag::clear();
  program_t *macro_prog = nullptr;
  {
    std::istringstream stream(macro_source);
    macro_prog = compile_file(std::make_unique<IStreamLexStream>(stream), "diag_macro_test");
  }
  EXPECT_EQ(macro_prog, nullptr);
  ASSERT_GE(compiler_diag::size(), 1u);
  const compiler_diag::Diagnostic *macro_diag = compiler_diag::last();
  ASSERT_NE(macro_diag, nullptr);
  ASSERT_EQ(macro_diag->expansions.size(), 1u);
  EXPECT_STREQ(macro_diag->expansions[0].message, "BAD_EXPR");
  ASSERT_NE(macro_diag->expansions[0].position.file, nullptr);
  EXPECT_STREQ(macro_diag->expansions[0].position.file, "diag_macro_test");
  EXPECT_EQ(macro_diag->expansions[0].position.line, 3)
      << "the expansion site is the line that used the macro";
  auto const macro_clang =
      compiler_diag::render_diagnostic(*macro_diag, compiler_diag::RenderStyle::kClang);
  EXPECT_NE(macro_clang.find("note: expanded from macro 'BAD_EXPR'"), std::string::npos)
      << actual_rendering(macro_clang);
  EXPECT_NE(macro_clang.find("diag_macro_test:3:"), std::string::npos)
      << actual_rendering(macro_clang);
}

TEST_F(DriverTest, TestDiagnosticRenderingTwentyCaseGolden) {
  // T3.3 golden set: 20 compile scenarios, each rendered in both styles. The
  // assertions pin the structured contract (location line the compiler
  // reported, severity, message, snippet equal to the file's line, caret
  // offset) instead of a wall of hand-copied strings; the exact byte shapes of
  // both styles are pinned by TestDiagnosticRenderingGoldenStylesAndSnippets.
  struct Case {
    const char *name;
    const char *source;
    compiler_diag::Severity severity;
    const char *message_substring;
    int line;
  };
  std::vector<Case> const cases = {
      {"syntax_semicolon", "void create() {\n  int x = ;\n}\n",
       compiler_diag::Severity::kError, "syntax error", 2},
      {"missing_paren", "void create() {\n  if (1 return;\n}\n",
       compiler_diag::Severity::kError, "syntax error", 2},
      {"unknown_type", "void create() {\n  nosuchtype x;\n}\n",
       compiler_diag::Severity::kError, "", 2},
      {"bad_return", "int create() {\n  return \"text\";\n}\n",
       compiler_diag::Severity::kError, "", 2},
      {"bad_expression", "void create() {\n  int x = 1 +;\n}\n",
       compiler_diag::Severity::kError, "syntax error", 2},
      {"missing_brace", "void create() {\n  int x = 1;\n",
       compiler_diag::Severity::kError, "", 3},
      {"stray_brace", "void create() { }\n}\n",
       compiler_diag::Severity::kError, "", 2},
      {"unknown_pragma", "#pragma xk_unknown_pragma\nvoid create() { }\n",
       compiler_diag::Severity::kWarning, "Unknown #pragma", 1},
      // The unused-local warning is reported when the function ends.
      {"unused_local", "void create() {\n  int unused = 1;\n}\n",
       compiler_diag::Severity::kWarning, "Unused local variable", 3},
      {"arg_count", "int helper(int a) { return a; }\nvoid create() {\n  helper();\n}\n",
       compiler_diag::Severity::kError, "Wrong number of arguments to 'helper'", 3},
      {"macro_object", "#define XK_MACRO int y = ;\nvoid create() {\n  XK_MACRO\n}\n",
       compiler_diag::Severity::kError, "syntax error", 3},
      {"macro_nested",
       "#define XK_INNER int z = ;\n#define XK_OUTER XK_INNER\nvoid create() {\n  XK_OUTER\n}\n",
       compiler_diag::Severity::kError, "syntax error", 4},
      {"macro_function",
       "#define XK_FN(a) int a = ;\nvoid create() {\n  XK_FN(w);\n}\n",
       compiler_diag::Severity::kError, "syntax error", 3},
      {"macro_arity", "#define XK_ONE(a) (a)\nvoid create() {\n  XK_ONE();\n}\n",
       compiler_diag::Severity::kError, "", 3},
      {"bad_include", "#include \"definitely_missing_header_xyz.h\"\nvoid create() { }\n",
       compiler_diag::Severity::kError, "Cannot #include", 1},
      {"switch_overlap",
       "void create() {\n  int i = 2;\n  switch (i) { case 1..3: break; case 2..4: break; }\n}\n",
       compiler_diag::Severity::kError, "", 3},
      {"bad_cast", "void create() {\n  string s = (string) ;\n}\n",
       compiler_diag::Severity::kError, "syntax error", 2},
      {"mapping_syntax", "void create() {\n  mapping m = ([ 1 : ]);\n}\n",
       compiler_diag::Severity::kError, "syntax error", 2},
      {"array_syntax", "void create() {\n  int *a = ({ 1 2 });\n}\n",
       compiler_diag::Severity::kError, "syntax error", 2},
      {"double_assign", "void create() {\n  int = 3;\n}\n",
       compiler_diag::Severity::kError, "", 2},
  };
  ASSERT_EQ(cases.size(), 20u);

  auto path_for = [](const char *name) {
    return "log/diag_golden_" + std::string(name) + ".c";
  };
  for (auto const &test_case : cases) {
    std::string const path = path_for(test_case.name);
    {
      std::ofstream out(path, std::ios::binary | std::ios::trunc);
      ASSERT_TRUE(out.good()) << path;
      out << test_case.source;
      ASSERT_TRUE(out.good());
    }
    struct CaseGuard {
      std::string path;
      ~CaseGuard() { remove(path.c_str()); }
    } guard{path};

    compiler_diag::clear();
    program_t *prog = nullptr;
    {
      std::istringstream stream(test_case.source);
      prog = compile_file(std::make_unique<IStreamLexStream>(stream), path.c_str());
    }
    if (prog != nullptr) {
      deallocate_program(prog);
    }
    ASSERT_GE(compiler_diag::size(), 1u)
        << test_case.name << ": scenario must produce a diagnostic";
    // The first record with the severity the scenario is about: an unrelated
    // warning (an unused local) must not shadow it.
    const compiler_diag::Diagnostic *diag = nullptr;
    for (size_t i = 0; i < compiler_diag::size(); i++) {
      if (compiler_diag::at(i).severity == test_case.severity) {
        diag = &compiler_diag::at(i);
        break;
      }
    }
    if (diag == nullptr) {
      FAIL() << test_case.name << ": no diagnostic with the expected severity";
    }
    ASSERT_NE(diag, nullptr) << test_case.name;
    ASSERT_NE(diag->message, nullptr) << test_case.name;
    EXPECT_EQ(diag->severity, test_case.severity) << test_case.name;
    if (test_case.message_substring[0] != '\0') {
      EXPECT_NE(std::string(diag->message).find(test_case.message_substring), std::string::npos)
          << test_case.name << " message: " << diag->message;
    } else {
      EXPECT_GT(std::strlen(diag->message), 0u) << test_case.name;
    }
    ASSERT_NE(diag->snippet.position.file, nullptr) << test_case.name;
    EXPECT_EQ(diag->snippet.position.line, test_case.line) << test_case.name;
    EXPECT_GT(diag->snippet.position.column, 0) << test_case.name;

    // The snippet must be the file's own line (the renderer reads it back from
    // disk) and the caret must sit at the recorded column in both styles.
    char line_text[compiler_diag::kMaxSnippetLineLength + 1];
    bool const have_line = compiler_diag::read_source_line(path.c_str(), test_case.line,
                                                           line_text, sizeof(line_text));
    auto const clang = compiler_diag::render_diagnostic(*diag, compiler_diag::RenderStyle::kClang);
    EXPECT_NE(clang.find(std::string(":") + std::to_string(test_case.line) + ":"),
              std::string::npos)
        << test_case.name << ": " << clang;
    if (have_line) {
      EXPECT_NE(clang.find(line_text), std::string::npos)
          << test_case.name << ": snippet line missing: " << clang;
    } else {
      // End-of-file diagnostics point past the last line: there is no text to
      // quote, so the renderer keeps the location line alone.
      EXPECT_EQ(clang.find("  \n"), std::string::npos)
          << test_case.name << ": unexpected snippet: " << clang;
    }
    auto const traditional =
        compiler_diag::render_diagnostic(*diag, compiler_diag::RenderStyle::kTraditional);
    EXPECT_NE(traditional.find("line " + std::to_string(test_case.line) + ":"),
              std::string::npos)
        << test_case.name << ": " << traditional;
  }
}

TEST_F(DriverTest, TestSimulEfunDroppedNameCallSiteErrors) {
  // E3 v2 contract: when a simul_efun name disappears from the table, an
  // already-compiled call site (a live sindex) must get the stable runtime
  // error, not a null call or an out-of-bounds dispatch read.
  ASSERT_GT(num_simul_efun, 0) << "config.test must load a simul_efun file";
  SimulTableSnapshot snap = SaveSimulTable();

  const char *kFoo = "simul_efun_dropped_xyz";
  ASSERT_EQ(lookup_ident(kFoo), nullptr) << "test name must not collide";

  program_t *prog_a = CompileSimulProg(std::string("string ") + kFoo + "() { return \"a\"; }\n");
  ASSERT_NE(prog_a, nullptr);
  simul_efun_prepared_t prep_a;
  simul_efuns_prepare(prog_a, &prep_a);
  simul_efuns_activate(&prep_a);

  int foo_idx = FindDispatchIndex(kFoo);
  int foo_pidx = FindProgramIndex(prog_a, kFoo);
  ASSERT_GE(foo_idx, 0);
  ASSERT_GE(foo_pidx, 0);
  ASSERT_EQ(simuls[foo_idx].func, find_func_entry(prog_a, foo_pidx));

  // The next program drops foo: its dispatch slot survives with func null.
  program_t *prog_b = CompileSimulProg("string simul_efun_dropped_other() { return \"b\"; }\n");
  ASSERT_NE(prog_b, nullptr);
  simul_efun_prepared_t prep_b;
  simul_efuns_prepare(prog_b, &prep_b);
  simul_efuns_activate(&prep_b);
  ASSERT_EQ(FindDispatchIndex(kFoo), foo_idx) << "dropped name keeps its index";
  ASSERT_EQ(simuls[foo_idx].func, nullptr) << "dropped name stays inactive";

  auto *saved_current_object = current_object;
  current_object = master_ob;
  error_context_t econ{};
  save_context(&econ);
  bool errored = false;
  push_number(0);  // the dropped simul_efun's single argument
  try {
    call_simul_efun(static_cast<unsigned short>(foo_idx), 1);
  } catch (...) {
    errored = true;
    restore_context(&econ);
  }
  pop_context(&econ);
  current_object = saved_current_object;
  EXPECT_TRUE(errored) << "a dropped simul_efun must error at the call site";

  // The message itself is a fixed literal at the only raise site; pin it so
  // the runtime error cannot be renamed into something unrecognizable.
  auto simul_source = read_source_file_for_test("../src/vm/internal/simul_efun.cc");
  ASSERT_FALSE(simul_source.empty());
  ASSERT_NE(simul_source.find("error(\"Function is no longer a simul_efun.\\n\");"),
            std::string::npos)
      << "call_simul_efun() must keep erroring for a dropped dispatch slot";

  simul_efuns_finish(&prep_b);
  simul_efuns_finish(&prep_a);
  RestoreSimulTable(snap);
  RemoveTestSimulIdent(kFoo);
  RemoveTestSimulIdent("simul_efun_dropped_other");
  deallocate_program(prog_a);
  deallocate_program(prog_b);
}

TEST_F(DriverTest, TestSimulEfunReloadCreateFailureRollback) {
  ASSERT_GT(num_simul_efun, 0) << "config.test must load a simul_efun file";
  SimulTableSnapshot snap = SaveSimulTable();
  const int old_count = num_simul_efun;
  program_t *old_prog = simul_efun_ob->prog;
  ASSERT_NE(old_prog, nullptr);
  // The live tables themselves (snap holds deep copies; rollback must
  // restore the ORIGINAL pointers, not copies).
  simul_entry *orig_names = simul_names;
  function_lookup_info_t *orig_funcs = simuls;

  const char *kBad = "simul_efun_xyz_bad";
  const int survivor_slot = FindValidSimulSlot();
  ASSERT_GE(survivor_slot, 0);
  std::string survivor = simul_names[survivor_slot].name;
  ASSERT_NE(survivor, kBad);
  int surv_sem_before = 0;
  ident_hash_elem_t *pre = find_or_add_perm_ident(survivor.c_str());
  if (pre) surv_sem_before = pre->sem_value;

  // The file-level initializer calls boom() -> the implicit #global_init#
  // runs during the create phase (call___INIT) and raises -> the whole
  // transaction must roll back to the pre-swap state.
  std::string src = std::string("string ") + kBad + "() { return \"bad\"; }\n"
                    "string " + survivor + "() { return \"survivor2\"; }\n"
                    "int boom() { error(\"boom\"); return 0; }\n"
                    "int g = boom();\n";
  program_t *prog = CompileSimulProg(src);
  ASSERT_NE(prog, nullptr);

  // Drive the transaction through the same entry point f_recompile_object
  // uses (pin + kind + dispatch prepare + snapshot all live there).
  const uint64_t old_generation = simul_efun_ob->prog_generation;
  const unsigned int old_program_ref = old_prog->ref;
  const unsigned int new_program_ref = prog->ref;

  RecompilePrepared prep;
  prep.staged = StagedProgram(prog);
  // This synthetic sefun source intentionally omits the production
  // inherit graph; the direct transaction test uses the documented fallback
  // classification and still exercises the migration allocation contract.
  start_recompile_transaction(simul_efun_ob, RecompileTargetKind::SimulEfun, &prep);
  ASSERT_EQ(prep.targets.size(), 1u) << "simul_efun_ob must be the only target";
  prepare_variable_migrations(&prep);
  ASSERT_EQ(prep.migrations.size(), prep.targets.size())
      << "SimulEfun must prepare one variable block per target";
  ASSERT_EQ(simul_efun_ob->prog, old_prog);
  ASSERT_EQ(simul_efun_ob->prog_generation, old_generation);
  ASSERT_EQ(simul_efun_ob->variables.layout_id, program_layout_digest(old_prog));
  ASSERT_EQ(old_prog->ref, old_program_ref + 1u) << "transaction pin must keep old program alive";
  ASSERT_EQ(prog->ref, new_program_ref);

  // Segment 1: no-fail swap. New dispatch table live.
  prep.commit_swap();
  ASSERT_EQ(simul_efun_ob->prog, prog);
  ASSERT_EQ(simul_efun_ob->prog_generation, old_generation + 1u);
  ASSERT_EQ(simul_efun_ob->variables.count, static_cast<uint32_t>(prog->num_variables_total));
  ASSERT_EQ(simul_efun_ob->variables.layout_id, program_layout_digest(prog));
  ASSERT_EQ(prog->ref, new_program_ref + 2u)
      << "commit pin and target reservation must be accounted before create";
  ASSERT_EQ(prep.simuls.state, simul_efun_prepared_t::State::Activated);
  int bad_idx = FindDispatchIndex(kBad);
  ASSERT_GE(bad_idx, old_count);
  // lookup_ident() HIDES inert/orphan idents: CHECK_ELEM gates on
  // sem_value != 0 (lex.cc), so an inactivated name returns nullptr no
  // matter what string is passed. find_or_add_perm_ident() bypasses the
  // gate (strcmp + no sem_value check) and is the only way to reach an
  // inert entry.
  ident_hash_elem_t *ihe = find_or_add_perm_ident(kBad);
  ASSERT_NE(ihe, nullptr);
  ASSERT_TRUE(ihe->token & IHE_SIMUL);
  ASSERT_EQ(ihe->dn.simul_num, bad_idx);

  // Segment 2: create phase fails (__INIT raises). run_create_guarded()
  // owns the error-context dance (restore -> pop -> rollback) and must
  // report the failure.
  ASSERT_FALSE(prep.run_create_guarded())
      << "file-level init error must surface as a failed guarded create";

  // Old dispatch tables restored BY POINTER (the exact pre-swap arrays).
  ASSERT_EQ(simul_names, orig_names);
  ASSERT_EQ(simuls, orig_funcs);
  ASSERT_EQ(num_simul_efun, old_count);
  // Survivor ident restored to the active state with its original index
  // and semantic counter.
  ihe = find_or_add_perm_ident(survivor.c_str());
  ASSERT_NE(ihe, nullptr);
  ASSERT_TRUE(ihe->token & IHE_SIMUL);
  ASSERT_FALSE(ihe->token & IHE_ORPHAN);
  ASSERT_EQ(ihe->dn.simul_num, snap.names[survivor_slot].index);
  ASSERT_EQ(ihe->sem_value, surv_sem_before);
  // The failed transaction's fresh name and temporary dispatch slot are
  // withdrawn; unlike a formerly live dropped name, it must not leave an
  // orphan identifier behind.
  ihe = lookup_perm_ident(kBad);
  ASSERT_EQ(ihe, nullptr);
  ASSERT_LT(FindDispatchIndex(kBad), 0);
  // Target object back on the old program; dispatch entries still point at
  // old-program functions with consistent runtime indices.
  ASSERT_EQ(simul_efun_ob->prog, old_prog);
  int s_idx = snap.names[survivor_slot].index;
  ASSERT_NE(simuls[s_idx].func, nullptr);
  int s_pidx = FindProgramIndex(old_prog, survivor.c_str());
  ASSERT_GE(s_pidx, 0);
  ASSERT_EQ(simuls[s_idx].func, find_func_entry(old_prog, s_pidx));
  ASSERT_EQ(simuls[s_idx].index, s_pidx);

  // Rollback ends in the Finalized state: the destructor cleanup is a
  // no-op and the new tables were freed.
  ASSERT_EQ(prep.simuls.state, simul_efun_prepared_t::State::Finalized);
  ASSERT_EQ(simul_efun_ob->prog, old_prog);
  ASSERT_EQ(simul_efun_ob->prog_generation, old_generation);
  ASSERT_EQ(simul_efun_ob->variables.count,
            static_cast<uint32_t>(old_prog->num_variables_total));
  ASSERT_EQ(simul_efun_ob->variables.layout_id, program_layout_digest(old_prog));
  ASSERT_EQ(old_prog->ref, old_program_ref) << "rollback must release only its transaction pin";
  ASSERT_EQ(prog->ref, new_program_ref)
      << "rollback must release commit and target reservations, not the staged owner ref";

  RestoreSimulTable(snap);
  // prep goes out of scope: staged.prog deallocates (its initial ref), all
  // pins were released by rollback().
}

TEST_F(DriverTest, TestSimulEfunReloadRollbackKeepsDroppedInert) {
  ASSERT_GT(num_simul_efun, 0);
  SimulTableSnapshot snap = SaveSimulTable();
  const char *kFoo = "simul_efun_xyz_foo2";  // unique vs other reload tests
  const int survivor_slot = FindValidSimulSlot();
  ASSERT_GE(survivor_slot, 0);
  std::string survivor = simul_names[survivor_slot].name;
  int surv_sem_before = 0;
  ident_hash_elem_t *pre = find_or_add_perm_ident(survivor.c_str());
  if (pre) surv_sem_before = pre->sem_value;

  // Reload A: adds foo (live). Reload B: drops foo (inert in the dispatch
  // table, ident in compile-time rejection state) -- the pre-transaction
  // state a later rollback must restore exactly.
  std::string src_a = std::string("string ") + kFoo + "() { return \"a\"; }\n"
                      "string " + survivor + "() { return \"survivorA\"; }\n";
  program_t *prog_a = CompileSimulProg(src_a);
  ASSERT_NE(prog_a, nullptr);
  simul_efun_prepared_t prep_a;
  simul_efuns_prepare(prog_a, &prep_a);
  simul_efuns_activate(&prep_a);
  int foo_idx = FindDispatchIndex(kFoo);
  ASSERT_GE(foo_idx, 0);

  std::string src_b = "string simul_efun_xyz_only() { return \"b\"; }\n";
  program_t *prog_b = CompileSimulProg(src_b);
  ASSERT_NE(prog_b, nullptr);
  simul_efun_prepared_t prep_b;
  simul_efuns_prepare(prog_b, &prep_b);
  simul_efuns_activate(&prep_b);
  ASSERT_EQ(simuls[foo_idx].func, nullptr) << "foo dropped by reload B";
  ident_hash_elem_t *ihe = find_or_add_perm_ident(kFoo);
  ASSERT_NE(ihe, nullptr);
  ASSERT_FALSE(ihe->token & IHE_SIMUL);
  ASSERT_TRUE(ihe->token & IHE_ORPHAN);
  ASSERT_EQ(ihe->dn.simul_num, -1);

  // Transaction C: re-adds foo, then its create phase fails. Rollback must
  // leave foo exactly as reload B left it -- NOT re-activated.
  std::string src_c = std::string("string ") + kFoo + "() { return \"c\"; }\n"
                      "int boom() { error(\"boom\"); return 0; }\n"
                      "int g = boom();\n";
  program_t *prog_c = CompileSimulProg(src_c);
  ASSERT_NE(prog_c, nullptr);
  RecompilePrepared prep_c;
  prep_c.staged = StagedProgram(prog_c);
  // As above, this is a minimal dispatch-table fixture rather than a full
  // production simul_efun source; prepare_variable_migrations() supplies the
  // direct-entry fallback classification.
  start_recompile_transaction(simul_efun_ob, RecompileTargetKind::SimulEfun, &prep_c);
  ASSERT_EQ(prep_c.targets.size(), 1u);
  prepare_variable_migrations(&prep_c);
  ASSERT_EQ(prep_c.migrations.size(), prep_c.targets.size());
  prep_c.commit_swap();
  ASSERT_EQ(simul_efun_ob->variables.count,
            static_cast<uint32_t>(prog_c->num_variables_total));
  ASSERT_EQ(simul_efun_ob->variables.layout_id, program_layout_digest(prog_c));
  // foo live again on the new table.
  ihe = find_or_add_perm_ident(kFoo);
  ASSERT_NE(ihe, nullptr);
  ASSERT_TRUE(ihe->token & IHE_SIMUL);
  ASSERT_EQ(ihe->dn.simul_num, foo_idx);

  // create fails -> guarded wrapper rolls back and reports failure.
  ASSERT_FALSE(prep_c.run_create_guarded());

  // foo: back to inert orphan (the pre-transaction state). Regression
  // target: the pre-fix rollback re-activated every old-table entry,
  // including dropped names, making the compiler emit F_SIMUL_EFUN for a
  // dispatch slot whose func was null.
  ASSERT_EQ(simuls[foo_idx].func, nullptr) << "foo stays dropped after rollback";
  ihe = find_or_add_perm_ident(kFoo);
  ASSERT_NE(ihe, nullptr);
  ASSERT_FALSE(ihe->token & IHE_SIMUL);
  ASSERT_TRUE(ihe->token & IHE_ORPHAN);
  ASSERT_EQ(ihe->dn.simul_num, -1);
  // Reload B's live name re-activates (it was in the old table with a
  // live func, so rollback step 2 restores it).
  int only_idx = FindDispatchIndex("simul_efun_xyz_only");
  ASSERT_GE(only_idx, 0);
  ihe = find_or_add_perm_ident("simul_efun_xyz_only");
  ASSERT_NE(ihe, nullptr);
  ASSERT_TRUE(ihe->token & IHE_SIMUL);
  ASSERT_EQ(ihe->dn.simul_num, only_idx);
  // The survivor was already dropped by reload B (not in prog_b), so the
  // rollback target state keeps it orphaned: B's post-activate table has
  // a nil func for it and step 2's guard skips nil-func entries.
  ihe = find_or_add_perm_ident(survivor.c_str());
  ASSERT_NE(ihe, nullptr);
  ASSERT_TRUE(ihe->token & IHE_ORPHAN);
  ASSERT_EQ(ihe->dn.simul_num, -1);
  ASSERT_NE(simuls[only_idx].func, nullptr);

  // Release the shadow tables the intermediate activates left behind.
  simul_efuns_finish(&prep_a);
  simul_efuns_finish(&prep_b);
  RestoreSimulTable(snap);
  RemoveTestSimulIdent(kFoo);
  RemoveTestSimulIdent("simul_efun_xyz_only");
  deallocate_program(prog_a);
  deallocate_program(prog_b);
  // prog_c deallocates via the prep_c destructor.
}

TEST_F(DriverTest, TestSimulEfunReloadInitCreateOrder) {
  // The probe object's file-level initializer (compiled into #global_init#)
  // sets probe_order to 1; create() promotes it to 2. call_create() -- the
  // exact call run_create() issues per target -- must leave it at 2, i.e.
  // __INIT strictly before create.
  object_t *probe = load_object_for_test("single/recompile_probe");
  ASSERT_NE(probe, nullptr);
  int slot = FindVariableSlot(probe->prog, "probe_order");
  ASSERT_GE(slot, 0);
  // load_object ran the create phase already; reset and run it again. Direct
  // VM entry points require an active error context, just like the production
  // transaction's run_create_guarded() wrapper.
  probe->variables.data[slot].u.number = 0;
  error_context_t econ{};
  save_context(&econ);
  try {
    call_create(probe, 0);
    pop_context(&econ);
  } catch (...) {
    restore_context(&econ);
    FAIL() << "call_create failed for recompile probe";
  }
  ASSERT_EQ(probe->variables.data[slot].u.number, 2)
      << "create phase must run __INIT (probe_mark) before create()";
}

TEST_F(DriverTest, TestMasterReloadSuccess) {
  ASSERT_NE(master_ob, nullptr);
  program_t *orig_prog = master_ob->prog;
  ASSERT_NE(orig_prog, nullptr);
  orig_prog->ref++;  // pin for the restore below

  // First reload: full transaction, success path (master.c has no runtime
  // initializers or create(): the create phase is a no-op).
  RecompilePrepared prep;
  prep.staged = compile_program_for_recompile(master_ob);
  ASSERT_NE(prep.staged.prog, nullptr);
  prep.old_layout = describe_recompile_layout(orig_prog);
  prep.new_layout = describe_recompile_layout(prep.staged.prog);
  prep.admission_diff = classify_recompile_layout(prep.old_layout, prep.new_layout);
  ASSERT_TRUE(prep.admission_diff.migratable());
  start_recompile_transaction(master_ob, RecompileTargetKind::Master, &prep);
  ASSERT_EQ(prep.targets.size(), 1u) << "master_ob must be the only target";
  prepare_variable_migrations(&prep);
  ASSERT_EQ(prep.migrations.size(), prep.targets.size());

  prep.commit_swap();
  ASSERT_TRUE(prep.run_create_guarded())
      << "master.c has no runtime initializers: create phase must succeed";
  prep.commit_finish();

  ASSERT_EQ(master_ob->prog, prep.staged.prog)
      << "master reload must swap in the new program";
  // The apply lookup table for the new program must be ready (every master
  // apply resolves through it).
  lookup_entry_s e = apply_cache_lookup("valid_recompile_object", prep.staged.prog);
  // progp points at the program DEFINING the apply (the inherited valid.c
  // program here), not at the queried program.
  ASSERT_NE(e.progp, nullptr);
  ASSERT_NE(e.funp, nullptr);

  // Authorization apply through the standard master path hits the NEW
  // program (§1.3.2 verification point).
  push_object(master_ob);
  svalue_t *ret = safe_apply_master_ob(APPLY_VALID_RECOMPILE_OBJECT, 1);
  ASSERT_NE(ret, nullptr);
  ASSERT_TRUE(MASTER_APPROVED(ret));

  // Immediate second reload: authorization apply must resolve through the
  // program the first reload just published.
  program_t *first_prog = prep.staged.prog;
  {
    RecompilePrepared prep2;
    prep2.staged = compile_program_for_recompile(master_ob);
    ASSERT_NE(prep2.staged.prog, nullptr);
    prep2.old_layout = describe_recompile_layout(master_ob->prog);
    prep2.new_layout = describe_recompile_layout(prep2.staged.prog);
    prep2.admission_diff = classify_recompile_layout(prep2.old_layout, prep2.new_layout);
    ASSERT_TRUE(prep2.admission_diff.migratable());
    start_recompile_transaction(master_ob, RecompileTargetKind::Master, &prep2);
    ASSERT_EQ(prep2.targets.size(), 1u);
    prepare_variable_migrations(&prep2);
    ASSERT_EQ(prep2.migrations.size(), prep2.targets.size());
    prep2.commit_swap();
    ASSERT_TRUE(prep2.run_create_guarded());
    prep2.commit_finish();
    ASSERT_EQ(master_ob->prog, prep2.staged.prog);
    lookup_entry_s e2 = apply_cache_lookup("valid_recompile_object", prep2.staged.prog);
    ASSERT_NE(e2.progp, nullptr);
    ASSERT_NE(e2.funp, nullptr);
    // prep2 destructor deallocates prep2.staged.prog (its initial ref; the
    // master object's ref to it stays live until the restore below).
  }

  // Restore: point master back at the original program, transferring the
  // master object's reference from the last reload to orig_prog, then drop
  // our pin.
  program_t *last_prog = master_ob->prog;
  master_ob->prog = orig_prog;
  orig_prog->ref++;   // the object's reference
  free_prog(&last_prog);  // the object's reference to the last reload
  free_prog(&orig_prog);  // our pin
}

TEST_F(DriverTest, TestMasterReloadPublicEntryOutsideMasterApply) {
  ASSERT_NE(master_ob, nullptr);
  struct RecompileConfigGuard {
    int saved;
    ~RecompileConfigGuard() { CONFIG_INT(__RECOMPILE_OBJECT_ENABLED__) = saved; }
  } config_guard{CONFIG_INT(__RECOMPILE_OBJECT_ENABLED__)};
  CONFIG_INT(__RECOMPILE_OBJECT_ENABLED__) = 1;

  program_t *original_prog = master_ob->prog;
  ASSERT_NE(original_prog, nullptr);
  original_prog->ref++;
  struct MasterProgramGuard {
    object_t *target;
    program_t *original;
    ~MasterProgramGuard() {
      if (!target || (target->flags & O_DESTRUCTED) || !target->prog) {
        free_prog(&original);
        return;
      }
      if (target->prog != original) {
        program_t *current = target->prog;
        target->prog = original;
        original->ref++;
        free_prog(&current);
      }
      free_prog(&original);
    }
  } master_guard{master_ob, original_prog};

  object_t* probe = load_object_for_test("clone/recompile_lifecycle_probe");
  ASSERT_NE(probe, nullptr);
  struct ProbeGuard {
    object_t* object;
    ~ProbeGuard() { destruct_object_for_test(object); }
  } probe_guard{probe};

  auto invoke_mode = [probe](int mode) -> int {
    push_number(mode);
    auto* result = safe_apply("run_master_recompile_mode", probe, 1, ORIGIN_DRIVER);
    if (result == nullptr || result->type != T_NUMBER) {
      vm_apply_return_clear();
      return -1;
    }
    int value = static_cast<int>(result->u.number);
    vm_apply_return_clear();
    return value;
  };

  auto query_last_mode = [probe]() -> int {
    auto* result = safe_apply("query_last_mode", probe, 0, ORIGIN_DRIVER);
    if (result == nullptr || result->type != T_NUMBER) {
      vm_apply_return_clear();
      return -1;
    }
    int value = static_cast<int>(result->u.number);
    vm_apply_return_clear();
    return value;
  };

  auto call_escaped_fp = [probe]() -> int {
    auto* result = safe_apply("call_escaped_fp", probe, 0, ORIGIN_DRIVER);
    if (result == nullptr || result->type != T_NUMBER) {
      vm_apply_return_clear();
      return -1;
    }
    int value = static_cast<int>(result->u.number);
    vm_apply_return_clear();
    return value;
  };

  // Every real lifecycle mutation entry used by master::create() must be
  // rejected before it can destruct, rebind, move, or publish the target.
  // The call originates from the already-loaded probe, not master::flag(), so
  // this exercises the public recompile_object() entry without an outer
  // master authorization frame.
  for (int mode = 1; mode <= 7; mode++) {
    ASSERT_EQ(invoke_mode(mode), 0) << "lifecycle mode " << mode;
    ASSERT_EQ(query_last_mode(), mode) << "lifecycle mode " << mode;
    ASSERT_FALSE(master_ob->flags & O_DESTRUCTED) << "lifecycle mode " << mode;
    ASSERT_EQ(master_ob->prog, original_prog) << "lifecycle mode " << mode;
  }

  // A non-simul transaction must not turn the simul-only compilation barrier
  // into a global ban: master::create() may load an unrelated object.
  ASSERT_EQ(invoke_mode(8), 1);
  ASSERT_EQ(query_last_mode(), 8);
  ASSERT_FALSE(master_ob->flags & O_DESTRUCTED);

  // I05: a function pointer created and bound during failed master create
  // escapes into a non-target object. It stays Invalid after rollback and a
  // later successful reload; generation restoration alone cannot revive it.
  ASSERT_EQ(invoke_mode(9), 0);
  ASSERT_EQ(call_escaped_fp(), 1);
  ASSERT_EQ(invoke_mode(0), 1);
  ASSERT_EQ(call_escaped_fp(), 1);
  ASSERT_EQ(invoke_mode(10), 1);
  ASSERT_EQ(call_escaped_fp(), 0);

  // A later public call must still be able to publish master successfully
  // after every rejected lifecycle attempt.
  ASSERT_EQ(invoke_mode(0), 1);
}

// L7 extension (E3 v2): master-target rounds under repetition. The single
// TestMasterReloadSuccess above proves one swap; this proves the master stays
// functionally reloadable round after round - every round must publish a new
// program, keep the apply lookup resolvable for it and keep the standard master
// authorization apply working. It is the driver-side counterpart of the LPC
// contract's note that a reload cannot run while the target program is
// executing.
TEST_F(DriverTest, TestMasterReloadStressRounds) {
  ASSERT_NE(master_ob, nullptr);
  program_t *orig_prog = master_ob->prog;
  ASSERT_NE(orig_prog, nullptr);
  orig_prog->ref++;  // pin for the restore below

  constexpr int kRounds = 12;
  std::vector<program_t *> published;

  for (int round = 0; round < kRounds; round++) {
    RecompilePrepared prep;
    prep.staged = compile_program_for_recompile(master_ob);
    ASSERT_NE(prep.staged.prog, nullptr) << "round " << round;
    prep.old_layout = describe_recompile_layout(master_ob->prog);
    prep.new_layout = describe_recompile_layout(prep.staged.prog);
    prep.admission_diff = classify_recompile_layout(prep.old_layout, prep.new_layout);
    ASSERT_TRUE(prep.admission_diff.migratable()) << "round " << round;

    start_recompile_transaction(master_ob, RecompileTargetKind::Master, &prep);
    ASSERT_EQ(prep.targets.size(), 1u) << "round " << round;
    prepare_variable_migrations(&prep);
    prep.commit_swap();
    ASSERT_TRUE(prep.run_create_guarded()) << "round " << round;
    prep.commit_finish();

    ASSERT_EQ(master_ob->prog, prep.staged.prog) << "round " << round;
    published.push_back(prep.staged.prog);

    // The rebuilt apply cache must resolve for the program this round published.
    lookup_entry_s entry = apply_cache_lookup("valid_recompile_object", prep.staged.prog);
    ASSERT_NE(entry.funp, nullptr) << "round " << round;

    // And the master must still answer applies: the standard authorization
    // path is what every reload depends on.
    push_object(master_ob);
    svalue_t *ret = safe_apply_master_ob(APPLY_VALID_RECOMPILE_OBJECT, 1);
    ASSERT_NE(ret, nullptr) << "round " << round;
    ASSERT_TRUE(MASTER_APPROVED(ret)) << "round " << round;
    free_svalue(ret, "TestMasterReloadStressRounds");
    *ret = const0;
  }
  ASSERT_EQ(published.size(), static_cast<size_t>(kRounds));

  // Restore the original master program (transfers the object's reference and
  // drops our pin), exactly like the single-reload test.
  program_t *last_prog = master_ob->prog;
  master_ob->prog = orig_prog;
  orig_prog->ref++;
  free_prog(&last_prog);
  free_prog(&orig_prog);
}

// Quiescence accounting: a successful begin/end pair must increment
// attempts and success (and never timeouts), proving the counters are
// live on the success path (the pre-fix guard rejected before counting,
// so attempts stayed 0 forever and quiescence failures were invisible).
TEST_F(DriverTest, TestRecompileLifecycleBoundary) {
  ASSERT_EQ(vm_recompile_execution_context(), nullptr);
  object_t *target = master_ob;
  ASSERT_NE(target, nullptr);

  RecompilePrepared prepared;
  add_ref(target, "TestRecompileLifecycleBoundary");
  RecompileTarget snapshot;
  snapshot.ob = target;
  snapshot.old_generation = target->prog_generation;
  prepared.targets.push_back(snapshot);
  prepared.kind = RecompileTargetKind::SimulEfun;

  {
    RecompileExecutionContext context;
    ASSERT_EQ(vm_recompile_execution_context(), &context);

    context.attach_prepared(&prepared);
    EXPECT_EQ(context.phase(), RecompileExecutionPhase::Prepared);
    EXPECT_TRUE(context.blocks_target_lifecycle(target));
    EXPECT_TRUE(context.blocks_clone_source(target));

    context.mark_swapped();
    EXPECT_EQ(context.phase(), RecompileExecutionPhase::Swapped);
    EXPECT_TRUE(context.blocks_new_compile())
        << "simul dispatch activation must close compilation";

    context.mark_finished();
    EXPECT_EQ(context.phase(), RecompileExecutionPhase::Finished);
    EXPECT_FALSE(context.blocks_target_lifecycle(target));
  }

  ASSERT_EQ(vm_recompile_execution_context(), nullptr);
}

TEST_F(DriverTest, TestRecompileQuiesceSnapshotSerializesClaimAndTimeout) {
  auto &coordinator = owner_runtime_coordinator();
  const auto before = coordinator.snapshot();
  {
    std::lock_guard<std::mutex> lock(coordinator.mutex());
    coordinator.claim_begin_locked();
  }

  const auto quiesce = vm_owner_recompile_quiesce_begin(std::chrono::milliseconds(1));
  ASSERT_FALSE(quiesce.ok);
  ASSERT_EQ(quiesce.failure, OwnerRecompileQuiesceFailure::kTimeout);
  coordinator.claim_end();

  const auto after = coordinator.snapshot();
  ASSERT_EQ(after.active_owner_claims, 0u);
  ASSERT_GE(after.quiesce_attempts, before.quiesce_attempts + 1);
  ASSERT_GE(after.quiesce_timeouts, before.quiesce_timeouts + 1);
}

TEST_F(DriverTest, TestRecompileQuiesceCountsSuccess) {
  auto &coordinator = owner_runtime_coordinator();
  const auto before = coordinator.snapshot();

  auto quiesce = vm_owner_recompile_quiesce_begin(std::chrono::milliseconds(200));
  ASSERT_TRUE(quiesce.ok) << "main-thread quiesce with zero claims must succeed";
  ASSERT_EQ(quiesce.failure, OwnerRecompileQuiesceFailure::kNone);
  vm_owner_recompile_quiesce_end(quiesce.epoch);
  // begin() returns the CURRENT epoch as a handle; end() advances it, so the
  // pair must leave the epoch exactly one ahead of the handle.
  const auto after = coordinator.snapshot();
  ASSERT_EQ(after.recompile_epoch, quiesce.epoch + 1);

  ASSERT_GE(after.quiesce_attempts, before.quiesce_attempts + 1);
  ASSERT_GE(after.quiesce_success, before.quiesce_success + 1);
  ASSERT_EQ(after.quiesce_timeouts, before.quiesce_timeouts);
}

// #1247 B-S1: ObjectVariableBlock lifecycle and program_layout_digest.
// The block's count is the sole payload-length source; layout_id comes
// exclusively from program_layout_digest().
TEST_F(DriverTest, TestObjectVariableBlockLifecycle) {
  const char *src =
      "int x;\n"
      "string s = \"abc\";\n"
      "void do_tests() { x = 1; }\n";
  program_t *prog = CompileSimulProg(src);
  ASSERT_NE(prog, nullptr);
  ASSERT_GE(prog->num_variables_total, 2);

  ObjectVariableBlock b;
  obj_vars_init(&b, prog);
  ASSERT_EQ(b.count, static_cast<uint32_t>(prog->num_variables_total));
  ASSERT_NE(b.data, nullptr);
  ASSERT_EQ(b.layout_id, program_layout_digest(prog));
  // deterministic: same program, same digest
  ASSERT_EQ(program_layout_digest(prog), b.layout_id);
  for (uint32_t i = 0; i < b.count; i++) {
    ASSERT_EQ(b.data[i].type, T_NUMBER);
    ASSERT_EQ(b.data[i].u.number, 0) << "fresh block slots must be const0";
  }
  // layout_id differs for a genuinely different layout
  const char *src2 = "int x;\nint y;\nstring s;\n";
  program_t *prog2 = CompileSimulProg(src2);
  ASSERT_NE(prog2, nullptr);
  ASSERT_NE(program_layout_digest(prog2), program_layout_digest(prog))
      << "different variable layouts must not share a digest";

  // clear zeroes contents without releasing the payload
  b.data[0].type = T_NUMBER;
  b.data[0].u.number = 42;
  obj_vars_clear(&b);
  ASSERT_EQ(b.data[0].u.number, 0);
  ASSERT_NE(b.data, nullptr);
  ASSERT_EQ(b.count, static_cast<uint32_t>(prog->num_variables_total));

  // move transfers the whole payload into an empty block
  svalue_t *moved_data = b.data;
  ObjectVariableBlock dst;
  obj_vars_move(&dst, &b);
  ASSERT_EQ(dst.count, static_cast<uint32_t>(prog->num_variables_total));
  ASSERT_EQ(dst.data, moved_data);
  ASSERT_EQ(b.data, nullptr);
  ASSERT_EQ(b.count, 0u);

  // swap exchanges payloads wholesale
  ObjectVariableBlock other;
  obj_vars_init(&other, prog2);
  void *dst_data = dst.data;
  void *other_data = other.data;
  uint64_t dst_id = dst.layout_id, other_id = other.layout_id;
  obj_vars_swap(&dst, &other);
  ASSERT_EQ(dst.data, other_data);
  ASSERT_EQ(other.data, dst_data);
  ASSERT_EQ(dst.layout_id, other_id);
  ASSERT_EQ(other.layout_id, dst_id);

  obj_vars_destroy(&dst);
  obj_vars_destroy(&other);
  ASSERT_EQ(dst.data, nullptr);
  ASSERT_EQ(dst.count, 0u);

  free_prog(&prog2);
  free_prog(&prog);
}

// #1247 B-S1: an object created through the loader carries a block whose
// count matches the compiled program and whose layout_id equals the
// program's digest; destructing the object releases the payload.
TEST_F(DriverTest, TestObjectBlockMatchesLoadedProgram) {
  object_t *ob = load_object_for_test("single/void");
  ASSERT_NE(ob, nullptr);
  ASSERT_EQ(ob->variables.count, static_cast<uint32_t>(ob->prog->num_variables_total));
  ASSERT_EQ(ob->variables.layout_id, program_layout_digest(ob->prog));
  ASSERT_NE(ob->variables.data, nullptr);
  destruct_object_for_test(ob);
}

// #1247 B-S1: replace_program() must build a fresh payload for the target
// program and swap blocks (never move slots inside a fixed-size object).
// Fixtures: rp_a (1 var), rp_mid (inherits rp_a + 1 var), rp_leaf
// (inherits rp_mid + 1 var). Layouts: mid=[a,m], leaf=[a,m,l].
TEST_F(DriverTest, TestReplaceProgramBlockMovePrefix) {
  object_t *a = load_object_for_test("single/tests/efuns/rp_a");
  object_t *mid = load_object_for_test("single/tests/efuns/rp_mid");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(mid, nullptr);
  ASSERT_EQ(mid->variables.count, 2u);
  ASSERT_STREQ(variable_name(mid->prog, 0), "a");
  ASSERT_STREQ(variable_name(mid->prog, 1), "m");

  // mid's own variable carries a value; replace mid with rp_a (offset 0:
  // the new layout is a prefix of the old).
  mid->variables.data[1].u.number = 7;
  // #1247: mirror the production allocation (replace_program.cc:222
    // DMALLOC) -- FREE() in replace_programs() would mismatch operator new.
    auto *entry = static_cast<replace_ob_t *>(
        DMALLOC(sizeof(replace_ob_t), TAG_TEMPORARY, "test_replace_program"));
  entry->ob = mid;
  entry->new_prog = a->prog;
  entry->var_offset = 0;
  entry->next = obj_list_replace;
  obj_list_replace = entry;
  replace_programs();

  ASSERT_EQ(mid->prog, a->prog);
  ASSERT_EQ(mid->variables.count, 1u);
  ASSERT_EQ(mid->variables.layout_id, program_layout_digest(a->prog));
  ASSERT_EQ(mid->variables.data[0].u.number, 0) << "a is a fresh slot (prefix)";
  ASSERT_EQ(obj_list_replace, nullptr);

  destruct_object_for_test(mid);
  destruct_object_for_test(a);
}

TEST_F(DriverTest, TestReplaceProgramBlockMoveOffset) {
  object_t *a = load_object_for_test("single/tests/efuns/rp_a");
  object_t *leaf = load_object_for_test("single/tests/efuns/rp_leaf");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(leaf, nullptr);
  ASSERT_EQ(leaf->variables.count, 3u);
  ASSERT_STREQ(variable_name(leaf->prog, 0), "a");
  ASSERT_STREQ(variable_name(leaf->prog, 1), "m");
  ASSERT_STREQ(variable_name(leaf->prog, 2), "l");

  // leaf -> rp_mid: the target's variables start at offset 1 in the old
  // block ([a, m, l] -> [m, l]); head slot a is dropped.
  leaf->variables.data[1].u.number = 11;  // m
  leaf->variables.data[2].u.number = 22;  // l
  // #1247: mirror the production allocation (replace_program.cc:222
    // DMALLOC) -- FREE() in replace_programs() would mismatch operator new.
    auto *entry = static_cast<replace_ob_t *>(
        DMALLOC(sizeof(replace_ob_t), TAG_TEMPORARY, "test_replace_program"));
  entry->ob = leaf;
  entry->new_prog = a->prog;  // placeholder, replaced below via search
  entry->var_offset = 1;
  // Need the real mid program: load rp_mid object and use its prog.
  object_t *mid = load_object_for_test("single/tests/efuns/rp_mid");
  ASSERT_NE(mid, nullptr);
  entry->new_prog = mid->prog;
  entry->next = obj_list_replace;
  obj_list_replace = entry;
  replace_programs();

  ASSERT_EQ(leaf->prog, mid->prog);
  ASSERT_EQ(leaf->variables.count, 2u);
  ASSERT_EQ(leaf->variables.layout_id, program_layout_digest(mid->prog));
  ASSERT_EQ(leaf->variables.data[0].u.number, 11) << "m survived the move";
  ASSERT_EQ(leaf->variables.data[1].u.number, 22) << "l survived the move";
  ASSERT_EQ(obj_list_replace, nullptr);

  destruct_object_for_test(mid);
  destruct_object_for_test(leaf);
  destruct_object_for_test(a);
}

// #1247 B-S2: digest/descriptor equivalence -- program_layout_digest must
// equal the FNV of the canonical descriptor serialization for every
// compiled program, and two programs with the same digest must have
// identical serializations (the B-S2 bijection gate, no whitelist).
TEST_F(DriverTest, TestProgramLayoutDigestMatchesDescriptor) {
  const char *src =
      "class Point { int x; int y; }\n"
      "class Point p;\n"
      "int q;\n"
      "void do_tests() { q = 1; }\n";
  program_t *prog = CompileSimulProg(src);
  ASSERT_NE(prog, nullptr);

  RecompileLayout layout = describe_recompile_layout(prog);
  ASSERT_EQ(layout.num_variables_total, prog->num_variables_total);
  std::string canonical = canonical_layout_serialization(layout);
  ASSERT_EQ(program_layout_digest(prog), layout_serialization_digest(canonical))
      << "digest(prog) must equal digest(canonical(describe(prog)))";

  // Same program compiled again must give the same digest and serialization.
  program_t *prog2 = CompileSimulProg(src);
  ASSERT_NE(prog2, nullptr);
  RecompileLayout layout2 = describe_recompile_layout(prog2);
  ASSERT_EQ(program_layout_digest(prog2), program_layout_digest(prog));
  ASSERT_EQ(canonical_layout_serialization(layout2), canonical);

  // Distinct layouts must not share a digest or serialization.
  const char *src3 =
      "class Point { int x; }\n"
      "class Point p;\n"
      "int q;\n"
      "void do_tests() { q = 1; }\n";
  program_t *prog3 = CompileSimulProg(src3);
  ASSERT_NE(prog3, nullptr);
  ASSERT_NE(program_layout_digest(prog3), program_layout_digest(prog))
      << "class schema change must change the digest";
  ASSERT_NE(canonical_layout_serialization(describe_recompile_layout(prog3)), canonical);

  free_prog(&prog3);
  free_prog(&prog2);
  free_prog(&prog);
}

// #1247 B-S2: every program the test driver has compiled so far must be
// consistent under (digest, canonical serialization): same digest implies
// same serialization. Exercises the full compiled corpus in one pass.
TEST_F(DriverTest, TestLayoutDigestBijectionOverCorpus) {
  std::map<uint64_t, std::string> digest_to_serial;
  for (object_t *ob = obj_list; ob; ob = ob->next_all) {
    if (!ob->prog) continue;
    RecompileLayout layout = describe_recompile_layout(ob->prog);
    std::string serial = canonical_layout_serialization(layout);
    uint64_t digest = program_layout_digest(ob->prog);
    if (digest != layout_serialization_digest(serial)) {
      std::cerr << "DIGEST MISMATCH " << ob->prog->filename << "\n  digest=" << digest
                << " canonical=" << layout_serialization_digest(serial) << "\n  serial=" << serial
                << "\n";
    }
    ASSERT_EQ(digest, layout_serialization_digest(serial))
        << "digest/canonical mismatch for " << ob->prog->filename;
    auto it = digest_to_serial.find(digest);
    if (it != digest_to_serial.end()) {
      ASSERT_EQ(it->second, serial)
          << "digest collision between different layouts: " << ob->prog->filename;
    } else {
      digest_to_serial[digest] = serial;
    }
  }
  ASSERT_GT(digest_to_serial.size(), 0u) << "corpus must contain programs";
}

// #1247 B-S2: same-name variables on different inherit paths are distinct
// identities; renaming is delete+add (no value preservation); reordering
// keeps values and is migratable; type changes, inherit edge type_mod
// changes and class schema changes are rejected.
TEST_F(DriverTest, TestClassifyLayoutRules) {
  // old: {v1, v2} in that order; new: reordered {v2, v1} plus one added
  // variable. All identities on the same path.
  RecompileLayout old_l;
  old_l.num_variables_total = 2;
  old_l.variables = {
      {0, "/p", "v1", 0, 0},
      {1, "/p", "v2", 0, 0},
  };
  RecompileLayout new_l;
  new_l.num_variables_total = 3;
  new_l.variables = {
      {0, "/p", "v2", 0, 0},
      {1, "/p", "v1", 0, 0},
      {2, "/p", "v3", 0, 0},
  };
  RecompileLayoutDiff diff = classify_recompile_layout(old_l, new_l);
  ASSERT_TRUE(diff.flags & kReordered);
  ASSERT_TRUE(diff.flags & kAddedVariable);
  ASSERT_EQ(diff.added.size(), 1u);
  ASSERT_EQ(diff.added[0].name_text, "v3");
  ASSERT_EQ(diff.removed.size(), 0u);
  ASSERT_EQ(diff.matches.size(), 2u);
  ASSERT_TRUE(diff.migratable()) << "reorder+add must be migratable";

  // Type change on a matched variable must reject.
  RecompileLayout type_new = new_l;
  for (auto &v : type_new.variables) {
    if (v.name_text == "v1") v.effective_decl_type = 1;
  }
  RecompileLayoutDiff type_diff = classify_recompile_layout(old_l, type_new);
  ASSERT_TRUE(type_diff.flags & kTypeChanged);
  ASSERT_FALSE(type_diff.migratable());

  // Rename: old v1 disappears, new v4 appears.
  RecompileLayout rename_new;
  rename_new.num_variables_total = 2;
  rename_new.variables = {
      {0, "/p", "v4", 0, 0},
      {1, "/p", "v2", 0, 0},
  };
  RecompileLayoutDiff rename_diff = classify_recompile_layout(old_l, rename_new);
  ASSERT_TRUE(rename_diff.flags & kAddedVariable);
  ASSERT_TRUE(rename_diff.flags & kRemovedVariable);
  ASSERT_EQ(rename_diff.matches.size(), 1u);
  ASSERT_EQ(rename_diff.matches[0].identity.name_text, "v2");
  ASSERT_TRUE(rename_diff.migratable());

  // Same name on different inherit paths must NOT match.
  RecompileLayout path_new;
  path_new.num_variables_total = 2;
  path_new.variables = {
      {0, "/other", "v1", 0, 0},
      {1, "/p", "v2", 0, 0},
  };
  RecompileLayoutDiff path_diff = classify_recompile_layout(old_l, path_new);
  ASSERT_TRUE(path_diff.flags & kAddedVariable);
  ASSERT_TRUE(path_diff.flags & kRemovedVariable);
  ASSERT_EQ(path_diff.matches.size(), 1u);

  // Duplicate identity within one side must reject.
  RecompileLayout dup_l = old_l;
  dup_l.variables[1].inherit_path = "/p";
  dup_l.variables[1].name_text = "v1";
  RecompileLayoutDiff dup_diff = classify_recompile_layout(dup_l, new_l);
  ASSERT_TRUE(dup_diff.flags & kDuplicateIdentity);
  ASSERT_FALSE(dup_diff.migratable());

  // Inherit edge type_mod change must reject.
  RecompileLayout inherit_old;
  inherit_old.num_variables_total = 1;
  inherit_old.inherits = {{0, "/sub", "sub", 0, 123}};
  inherit_old.variables = {{0, "/sub", "sv", 0, 0}};
  RecompileLayout inherit_new = inherit_old;
  inherit_new.inherits[0].type_mod = 1;
  RecompileLayoutDiff inherit_diff = classify_recompile_layout(inherit_old, inherit_new);
  ASSERT_TRUE(inherit_diff.flags & kInheritChanged);
  ASSERT_FALSE(inherit_diff.migratable());

  // Class schema change must reject (same variable, different schema).
  RecompileLayout class_old;
  class_old.num_variables_total = 1;
  class_old.classes = {{"/p", "C", {{"m", 0}}, 111}};
  class_old.variables = {{0, "/p", "c", 0, 111}};
  RecompileLayout class_new = class_old;
  class_new.classes[0].schema_digest = 222;
  class_new.variables[0].class_schema_digest = 222;
  RecompileLayoutDiff class_diff = classify_recompile_layout(class_old, class_new);
  ASSERT_TRUE(class_diff.flags & kClassSchemaChanged);
  ASSERT_FALSE(class_diff.migratable());

  // Combined: add + remove + type change keeps ALL flags.
  RecompileLayout comb_new;
  comb_new.num_variables_total = 2;
  comb_new.variables = {
      {0, "/p", "v2", 5, 0},   // type changed
      {1, "/p", "v9", 0, 0},   // added
  };
  RecompileLayoutDiff comb_diff = classify_recompile_layout(old_l, comb_new);
  ASSERT_TRUE(comb_diff.flags & kAddedVariable);
  ASSERT_TRUE(comb_diff.flags & kRemovedVariable);
  ASSERT_TRUE(comb_diff.flags & kTypeChanged);
  ASSERT_FALSE(comb_diff.migratable());
  ASSERT_GE(comb_diff.reject_reasons.size(), 1u);
}

// #1247 B-S3: BlueprintFamily layout migration. The staged program drops x
// and adds z; matched y keeps its old value, the new z keeps its
// initializer (InitThenMigrate policy), the removed x is gone.
TEST_F(DriverTest, TestRecompileMigrationAddRemovePreserves) {
  object_t *bp = load_object_for_test("single/tests/efuns/b3_bp");
  ASSERT_NE(bp, nullptr);
  ASSERT_EQ(bp->variables.count, 2u);
  ASSERT_EQ(bp->variables.data[0].u.number, 100);
  ASSERT_EQ(bp->variables.data[1].u.number, 200);

  const char *v2 = "int z = 300;\nint y;\n";
  program_t *prog2 = CompileSimulProg(v2);
  ASSERT_NE(prog2, nullptr);

  RecompilePrepared prep;
  prep.staged = StagedProgram(prog2);
  prep.old_layout = describe_recompile_layout(bp->prog);
  prep.new_layout = describe_recompile_layout(prog2);
  ASSERT_FALSE(recompile_layouts_match(prep.old_layout, prep.new_layout, nullptr));

  RecompileLayoutDiff diff = classify_recompile_layout(prep.old_layout, prep.new_layout);
  ASSERT_TRUE(diff.migratable()) << "add+remove must migrate";
  ASSERT_EQ(diff.added.size(), 1u);
  ASSERT_EQ(diff.added[0].name_text, "z");
  ASSERT_EQ(diff.removed.size(), 1u);
  ASSERT_EQ(diff.removed[0].name_text, "x");
  ASSERT_EQ(diff.matches.size(), 1u);
  ASSERT_EQ(diff.matches[0].identity.name_text, "y");

  start_recompile_transaction(bp, RecompileTargetKind::BlueprintFamily, &prep);
  prepare_variable_migrations(&prep);
  ASSERT_EQ(prep.migrations.size(), prep.targets.size())
      << "layout change must build one migration per target";

  prep.commit_swap();
  ASSERT_TRUE(prep.run_create_guarded());
  prep.commit_finish();

  ASSERT_EQ(bp->prog, prog2);
  ASSERT_EQ(bp->variables.count, 2u);
  ASSERT_EQ(bp->variables.layout_id, program_layout_digest(prog2));
  // z (slot 0) keeps the initializer; y (slot 1) recovered the old value.
  ASSERT_EQ(bp->variables.data[0].u.number, 300);
  ASSERT_EQ(bp->variables.data[1].u.number, 200);

  destruct_object_for_test(bp);
}

// #1247 B-S3: reordering keeps values by stable identity.
TEST_F(DriverTest, TestRecompileMigrationReorder) {
  object_t *bp = load_object_for_test("single/tests/efuns/b3_bp");
  ASSERT_NE(bp, nullptr);
  const char *v2 = "int y;\nint x;\n";  // swapped slots
  program_t *prog2 = CompileSimulProg(v2);
  ASSERT_NE(prog2, nullptr);

  RecompilePrepared prep;
  prep.staged = StagedProgram(prog2);
  prep.old_layout = describe_recompile_layout(bp->prog);
  prep.new_layout = describe_recompile_layout(prog2);
  RecompileLayoutDiff diff = classify_recompile_layout(prep.old_layout, prep.new_layout);
  ASSERT_TRUE(diff.flags & kReordered);
  ASSERT_TRUE(diff.migratable());

  start_recompile_transaction(bp, RecompileTargetKind::BlueprintFamily, &prep);
  prepare_variable_migrations(&prep);
  prep.commit_swap();
  ASSERT_TRUE(prep.run_create_guarded());
  prep.commit_finish();

  // New layout [y, x]: y recovered 200, x recovered 100.
  ASSERT_EQ(bp->variables.count, 2u);
  ASSERT_EQ(bp->variables.data[0].u.number, 200);
  ASSERT_EQ(bp->variables.data[1].u.number, 100);
  destruct_object_for_test(bp);
}

// #1247 B-S3: __INIT failure during migration rolls the whole transaction
// back: program, variable payload and values are the pre-swap state.
TEST_F(DriverTest, TestRecompileMigrationInitFailureRollsBack) {
  object_t *bp = load_object_for_test("single/tests/efuns/b3_bp");
  ASSERT_NE(bp, nullptr);
  program_t *old_prog = bp->prog;
  const char *v2 = "int z;\nint boom() { error(\"boom\"); return 0; }\nint g = boom();\n";
  program_t *prog2 = CompileSimulProg(v2);
  ASSERT_NE(prog2, nullptr);

  RecompilePrepared prep;
  prep.staged = StagedProgram(prog2);
  prep.old_layout = describe_recompile_layout(bp->prog);
  prep.new_layout = describe_recompile_layout(prog2);
  ASSERT_TRUE(classify_recompile_layout(prep.old_layout, prep.new_layout).migratable());

  start_recompile_transaction(bp, RecompileTargetKind::BlueprintFamily, &prep);
  prepare_variable_migrations(&prep);
  prep.commit_swap();
  // __INIT raises during state preparation -> guarded create fails ->
  // rollback.
  ASSERT_FALSE(prep.run_create_guarded());

  ASSERT_EQ(bp->prog, old_prog);
  ASSERT_EQ(bp->variables.count, 2u);
  ASSERT_EQ(bp->variables.data[0].u.number, 100);
  ASSERT_EQ(bp->variables.data[1].u.number, 200);
  ASSERT_EQ(bp->variables.layout_id, program_layout_digest(old_prog));
  // No leaked migration state: rollback released both payloads.
  ASSERT_TRUE(prep.migrations.empty());
  destruct_object_for_test(bp);
}

// I03 fail-first: a new staged program may add clean_up() even when the old
// program did not define it. The commit must derive the new flag from the
// staged apply table rather than reusing the old snapshot.
TEST_F(DriverTest, TestRecompileDerivedFlagsUseStagedProgram) {
  object_t *bp = load_object_for_test("single/tests/efuns/b3_bp");
  ASSERT_NE(bp, nullptr);
  ASSERT_EQ(function_exists(APPLY_CLEAN_UP, bp, 1), nullptr);
  bp->flags &= ~O_WILL_CLEAN_UP;
  const unsigned short old_flags = bp->flags;
  program_t *old_prog = bp->prog;

  program_t *new_prog = CompileSimulProg(
      "int x = 100;\nint y = 200;\nvoid clean_up(int inherited) {}\n");
  ASSERT_NE(new_prog, nullptr);

  RecompilePrepared prep;
  prep.staged = StagedProgram(new_prog);
  prep.old_layout = describe_recompile_layout(old_prog);
  prep.new_layout = describe_recompile_layout(new_prog);
  ASSERT_TRUE(recompile_layouts_match(prep.old_layout, prep.new_layout, nullptr));
  start_recompile_transaction(bp, RecompileTargetKind::BlueprintFamily, &prep);
  prepare_variable_migrations(&prep);
  ASSERT_TRUE(prep.migrations.empty());
  prep.commit_swap();

  EXPECT_NE(bp->flags & O_WILL_CLEAN_UP, 0u);
  prep.rollback();
  EXPECT_EQ(bp->prog, old_prog);
  EXPECT_EQ(bp->flags, old_flags);
  destruct_object_for_test(bp);
}

// I03: rollback must restore the exact old derived-bit snapshot, not infer it
// again from the old program. A one-shot clean_up sweep may have cleared the
// bit while the method remains present.
TEST_F(DriverTest, TestRecompileDerivedFlagsRestoreOldSnapshot) {
  object_t *ob = load_object_for_test("clone/clean_up_deadline");
  ASSERT_NE(ob, nullptr);
  ASSERT_NE(function_exists(APPLY_CLEAN_UP, ob, 1), nullptr);
  ASSERT_NE(ob->flags & O_WILL_CLEAN_UP, 0u);
  ob->flags &= ~O_WILL_CLEAN_UP;
  const unsigned short old_flags = ob->flags;
  program_t *old_prog = ob->prog;

  program_t *new_prog = CompileSimulProg("void create() {}\n");
  ASSERT_NE(new_prog, nullptr);
  RecompilePrepared prep;
  prep.staged = StagedProgram(new_prog);
  prep.old_layout = describe_recompile_layout(old_prog);
  prep.new_layout = describe_recompile_layout(new_prog);
  prep.admission_diff = classify_recompile_layout(prep.old_layout, prep.new_layout);
  ASSERT_TRUE(prep.admission_diff.migratable());
  start_recompile_transaction(ob, RecompileTargetKind::BlueprintFamily, &prep);
  prepare_variable_migrations(&prep);
  ASSERT_TRUE(prep.migrations.empty());
  prep.commit_swap();

  EXPECT_EQ(ob->flags & O_WILL_CLEAN_UP, 0u);
  prep.rollback();
  EXPECT_EQ(ob->prog, old_prog);
  EXPECT_EQ(ob->flags, old_flags);
  destruct_object_for_test(ob);
}

// #1247 B-S3: exact-layout BlueprintFamily reload creates NO migration and
// runs NO __INIT (v1 semantics preserved).
TEST_F(DriverTest, TestRecompileCreateNonLpcExceptionRollsBack) {
  object_t *bp = load_object_for_test("single/tests/efuns/b3_bp");
  ASSERT_NE(bp, nullptr);
  program_t *old_prog = bp->prog;
  const uint64_t old_generation = bp->prog_generation;
  const unsigned short old_flags = bp->flags;
  const uint32_t old_count = bp->variables.count;
  ASSERT_EQ(old_count, 2u);
  const LPC_INT old_x = bp->variables.data[0].u.number;
  const LPC_INT old_y = bp->variables.data[1].u.number;
  const unsigned int old_ref = old_prog->ref;

  program_t *new_prog = CompileSimulProg("int x = 900;\nint y = 901;\n");
  ASSERT_NE(new_prog, nullptr);
  const unsigned int new_ref = new_prog->ref;

  RecompilePrepared prep;
  prep.staged = StagedProgram(new_prog);
  prep.old_layout = describe_recompile_layout(old_prog);
  prep.new_layout = describe_recompile_layout(new_prog);
  ASSERT_TRUE(recompile_layouts_match(prep.old_layout, prep.new_layout, nullptr));

  start_recompile_transaction(bp, RecompileTargetKind::BlueprintFamily, &prep);
  prepare_variable_migrations(&prep);
  ASSERT_TRUE(prep.migrations.empty());
  prep.commit_swap();
  ASSERT_EQ(bp->prog, new_prog);
  ASSERT_NE(bp->prog_generation, old_generation);

  // The injected C++ exception also corrupts the execution registers before
  // it escapes. run_create_guarded() must restore the exact VM boundary before
  // rollback, not merely repair the value/control stacks.
  VMExecutionState failure_entry = vm_context_capture_execution();
  failure_entry.current_object = bp;
  failure_entry.current_prog = new_prog;
  {
    VMExecutionScope failure_scope(vm_context(), failure_entry);
    prep.create_failure_hook_for_test = &throw_recompile_non_lpc_exception_for_test;
    ASSERT_FALSE(prep.run_create_guarded());
    ASSERT_EQ(current_object, bp);
    ASSERT_EQ(current_prog, new_prog);
    ASSERT_EQ(vm_context().execution.current_object, bp);
    ASSERT_EQ(vm_context().execution.current_prog, new_prog);
  }

  ASSERT_EQ(bp->prog, old_prog);
  ASSERT_EQ(bp->prog_generation, old_generation);
  ASSERT_EQ(bp->flags, old_flags);
  ASSERT_EQ(bp->variables.count, old_count);
  ASSERT_EQ(bp->variables.data[0].u.number, old_x);
  ASSERT_EQ(bp->variables.data[1].u.number, old_y);
  ASSERT_EQ(bp->variables.layout_id, program_layout_digest(old_prog));
  ASSERT_EQ(old_prog->ref, old_ref);
  ASSERT_EQ(new_prog->ref, new_ref);
  ASSERT_TRUE(prep.migrations.empty());

  destruct_object_for_test(bp);
}

TEST_F(DriverTest, TestRecompileExactNoMigrationForBlueprint) {
  object_t *bp = load_object_for_test("single/tests/efuns/b3_bp");
  ASSERT_NE(bp, nullptr);
  const char *v2 = "int x = 100;\nint y = 200;\n";  // identical layout
  program_t *prog2 = CompileSimulProg(v2);
  ASSERT_NE(prog2, nullptr);

  RecompilePrepared prep;
  prep.staged = StagedProgram(prog2);
  prep.old_layout = describe_recompile_layout(bp->prog);
  prep.new_layout = describe_recompile_layout(prog2);
  ASSERT_TRUE(recompile_layouts_match(prep.old_layout, prep.new_layout, nullptr));

  start_recompile_transaction(bp, RecompileTargetKind::BlueprintFamily, &prep);
  prepare_variable_migrations(&prep);
  ASSERT_TRUE(prep.migrations.empty()) << "exact layout must not migrate";
  prep.commit_swap();
  ASSERT_TRUE(prep.run_create_guarded());
  prep.commit_finish();

  // Values untouched (no __INIT, no copy).
  ASSERT_EQ(bp->variables.count, 2u);
  ASSERT_EQ(bp->variables.data[0].u.number, 100);
  ASSERT_EQ(bp->variables.data[1].u.number, 200);
  destruct_object_for_test(bp);
}

// #1247 B-S3: Master/SimulEfun policy -- Always migration + MigrateThenInit
// on an exact reload. master.c's nosave int has_error = 0 has a file-level
// initializer, so __INIT always resets it to 0. The order proof: if the
// migration copy ran AFTER __INIT (wrong order), the copied old value 7
// would survive; with MigrateThenInit the copy runs first and __INIT's
// reset wins, so the final value must be 0.
TEST_F(DriverTest, TestMasterExactReloadMigrateThenInit) {
  int slot = FindVariableSlot(master_ob->prog, "has_error");
  ASSERT_GE(slot, 0);
  master_ob->variables.data[slot].u.number = 7;

  RecompilePrepared prep;
  prep.staged = compile_program_for_recompile(master_ob);
  ASSERT_NE(prep.staged.prog, nullptr);
  prep.old_layout = describe_recompile_layout(master_ob->prog);
  prep.new_layout = describe_recompile_layout(prep.staged.prog);
  ASSERT_TRUE(recompile_layouts_match(prep.old_layout, prep.new_layout, nullptr))
      << "master exact reload required";

  start_recompile_transaction(master_ob, RecompileTargetKind::Master, &prep);
  prepare_variable_migrations(&prep);
  ASSERT_EQ(prep.migrations.size(), prep.targets.size())
      << "Master policy migrates even on exact layout";
  prep.commit_swap();
  ASSERT_TRUE(prep.run_create_guarded());
  prep.commit_finish();

  // __INIT (file-level initializer) ran AFTER the migration copy and its
  // reset won: final value is 0, proving MigrateThenInit ordering.
  slot = FindVariableSlot(master_ob->prog, "has_error");
  ASSERT_GE(slot, 0);
  ASSERT_EQ(master_ob->variables.data[slot].u.number, 0)
      << "MigrateThenInit: __INIT must run after the copy and its write must win";
}

TEST_F(DriverTest, TestTelnetLinemodeShortSubnegotiation) {
  // #1247 TELNET-1: a 1-byte LINEMODE sub-negotiation must not read buf[1]
  // (upstream read buf[1] after only checking size == 0).
  interactive_t ip{};
  const char mode_byte[] = {static_cast<char>(LM_MODE)};
  on_telnet_subnegotiation(TELNET_TELOPT_LINEMODE, mode_byte, 1, &ip);
  const char do_byte[] = {static_cast<char>(TELNET_DO)};
  on_telnet_subnegotiation(TELNET_TELOPT_LINEMODE, do_byte, 1, &ip);
  const char will_byte[] = {static_cast<char>(TELNET_WILL)};
  on_telnet_subnegotiation(TELNET_TELOPT_LINEMODE, will_byte, 1, &ip);
  on_telnet_subnegotiation(TELNET_TELOPT_LINEMODE, nullptr, 0, &ip);
}

TEST_F(DriverTest, TestTelnetZmpArgcVariants) {
  // #1247 TELNET-3: argc-1 element array filled at item[i-1] must not
  // overrun for argc 1/2/3 (upstream wrote item[1..argc-1]).
  interactive_t ip{};
  ip.ob = load_object_for_test("/single/void");
  ASSERT_NE(ip.ob, nullptr);
  const char *argv1[] = {"cmd"};
  on_telnet_do_zmp(argv1, 1, &ip);
  const char *argv2[] = {"cmd", "a"};
  on_telnet_do_zmp(argv2, 2, &ip);
  const char *argv3[] = {"cmd", "a", "b"};
  on_telnet_do_zmp(argv3, 3, &ip);
}

#ifndef _WIN32
namespace {
// The mudlib root is the driver's working directory (config.test's "mudlib
// directory : ." is resolved against it), so the lpcc CLI contract must be
// exercised with testsuite/ as the child's CWD. Both paths are derived from
// the running test binary (<build>/src/tests/lpc_tests) so any build
// directory works.
std::string LpccTestBuildDir() {
  static const std::string build_dir = [] {
    std::filesystem::path exe;
#ifdef __APPLE__
    uint32_t size = 0;
    if (_NSGetExecutablePath(nullptr, &size) != -1 || size == 0) {
      return std::string();
    }
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
      return std::string();
    }
    exe = std::filesystem::path(buffer.c_str());
#else
    std::error_code ec;
    exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) {
      return std::string();
    }
#endif
    // <build>/src/tests/lpc_tests -> <build>
    return exe.parent_path().parent_path().parent_path().string();
  }();
  return build_dir;
}

struct LpccCliResult {
  int exit_code = -1;
  std::string output;
};

LpccCliResult RunLpccCli(const std::string &args) {
  LpccCliResult result;
  const auto build_dir = LpccTestBuildDir();
  if (build_dir.empty()) {
    return result;
  }
  const auto mudlib = std::filesystem::path(build_dir).parent_path() / "testsuite";
  const auto lpcc = std::filesystem::path(build_dir) / "bin" / "lpcc";
  if (!std::filesystem::exists(lpcc) || !std::filesystem::exists(mudlib / "etc" / "config.test")) {
    return result;
  }
  // trace_lpcc.json lands in the child's CWD; it is a gitignored artifact of
  // the documented lpcc invocation from testsuite/.
  const std::string command = "cd '" + mudlib.string() + "' && '" + lpcc.string() + "' " + args +
                              " 2>&1";
  FILE *pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    return result;
  }
  char buffer[4096];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result.output += buffer;
  }
  const int status = pclose(pipe);
  result.exit_code = WEXITSTATUS(status);
  return result;
}
}  // namespace

// T3.4: runs the interactive shell with `input` on stdin. Prompt output is
// tty-only, so piped runs see exactly the diagnostics and command output.
LpccCliResult RunLpcshellCli(const std::string &args, const std::string &input) {
  LpccCliResult result;
  const auto build_dir = LpccTestBuildDir();
  if (build_dir.empty()) {
    return result;
  }
  const auto mudlib = std::filesystem::path(build_dir).parent_path() / "testsuite";
  const auto shell = std::filesystem::path(build_dir) / "bin" / "lpcshell";
  if (!std::filesystem::exists(shell) || !std::filesystem::exists(mudlib / "etc" / "config.test")) {
    return result;
  }
  const auto input_path = std::filesystem::path(build_dir) / "lpcshell_input.txt";
  {
    std::ofstream out(input_path, std::ios::binary | std::ios::trunc);
    if (!out.good()) {
      return result;
    }
    out << input;
  }
  const std::string command = "cd '" + mudlib.string() + "' && '" + shell.string() + "' " + args +
                              " < '" + input_path.string() + "' 2>&1";
  FILE *pipe = popen(command.c_str(), "r");
  if (pipe != nullptr) {
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      result.output += buffer;
    }
    int const status = pclose(pipe);
    result.exit_code = WEXITSTATUS(status);
  }
  std::error_code ec;
  std::filesystem::remove(input_path, ec);
  return result;
}

TEST_F(DriverTest, TestLpcshellInteractiveContract) {
  const std::string cfg = "etc/config.test";

  // Error, structured rendering, recovery, diagnostics replay and clean exit.
  {
    const std::string input =
        "void create() {\n  int x = ;\n}\n#diagnostics\n#style traditional\n"
        "#diagnostics\n#nope\nvoid create() { }\n#quit\n";
    auto const result = RunLpcshellCli(cfg, input);
    if (result.exit_code == -1) {
      GTEST_SKIP() << "lpcshell is not built in this build directory";
    }
    EXPECT_EQ(result.exit_code, 0) << result.output;
    // clang style: location, snippet and caret for the failing line.
    EXPECT_NE(result.output.find("tmp_eval_file.c:2:12: error: syntax error"), std::string::npos)
        << result.output;
    EXPECT_NE(result.output.find("  int x = ;"), std::string::npos) << result.output;
    EXPECT_NE(result.output.find("compile failed"), std::string::npos) << result.output;
    // #diagnostics replays the same record, in the traditional shape after the
    // style switch, without the driver printing a second copy.
    EXPECT_NE(result.output.find("/tmp_eval_file.c line 2: syntax error"), std::string::npos)
        << result.output;
    size_t const occurrences = [&result] {
      size_t count = 0, pos = 0;
      while ((pos = result.output.find("syntax error, unexpected ';'", pos)) != std::string::npos) {
        count++;
        pos += 1;
      }
      return count;
    }();
    // Three renders: the compile's own report, then two #diagnostics replays.
    EXPECT_EQ(occurrences, 3u) << result.output;
    // Error recovery: the valid program after the failure compiles.
    EXPECT_NE(result.output.find("compiled:"), std::string::npos) << result.output;
    // Unknown commands are reported and the shell keeps running.
    EXPECT_NE(result.output.find("unknown command '#nope'"), std::string::npos) << result.output;
  }

  // Multi-line continuation: an unbalanced brace keeps the input open.
  {
    auto const result = RunLpcshellCli(cfg, "void create() {\n}\n#quit\n");
    ASSERT_NE(result.exit_code, -1) << result.output;
    EXPECT_EQ(result.exit_code, 0) << result.output;
    EXPECT_NE(result.output.find("compiled:"), std::string::npos) << result.output;
    EXPECT_EQ(result.output.find("syntax error"), std::string::npos) << result.output;
  }

  // Batch mode: a compiling file exits 0, a broken one exits 1 with a
  // diagnostic that names the host path (testsuite sources are not standalone:
  // they use the testsuite's simul_efun helpers, so the fixtures live in the
  // build directory).
  {
    const auto build_dir = LpccTestBuildDir();
    ASSERT_FALSE(build_dir.empty());

    const auto good = std::filesystem::path(build_dir) / "lpcshell_good.c";
    {
      std::ofstream out(good, std::ios::binary | std::ios::trunc);
      ASSERT_TRUE(out.good());
      out << "void create() { }\nint run_twice(int value) { return value * 2; }\n";
    }
    auto const good_result = RunLpcshellCli(cfg + " " + good.string(), "");
    std::error_code ec;
    std::filesystem::remove(good, ec);
    ASSERT_NE(good_result.exit_code, -1) << good_result.output;
    EXPECT_EQ(good_result.exit_code, 0) << good_result.output;
    EXPECT_EQ(good_result.output.find("error:"), std::string::npos) << good_result.output;

    const auto broken = std::filesystem::path(build_dir) / "lpcshell_broken.c";
    {
      std::ofstream out(broken, std::ios::binary | std::ios::trunc);
      ASSERT_TRUE(out.good());
      out << "void create() {\n  int x = ;\n}\n";
    }
    auto const bad = RunLpcshellCli(cfg + " " + broken.string(), "");
    std::filesystem::remove(broken, ec);
    ASSERT_NE(bad.exit_code, -1) << bad.output;
    EXPECT_EQ(bad.exit_code, 1) << bad.output;
    EXPECT_NE(bad.output.find(":2:12: error: syntax error"), std::string::npos) << bad.output;
  }
}

TEST_F(DriverTest, TestLpccCliArgumentMatrix) {
  const std::string cfg = "etc/config.test";
  const std::string good = "/single/tests/efuns/has_cycle";
  const std::string missing = "/single/tests/efuns/does_not_exist";

  struct Case {
    std::string args;
    int exit_code;
    bool expects_usage;
  };
  const std::vector<Case> cases = {
      // argc 1/2 and any other arity that matches no mode: usage + exit 1.
      {"", 1, true},
      {cfg, 1, true},
      {"--owner-audit --format=json " + cfg, 1, true},
      // The owner-audit scanner reads the LPC file from the host filesystem
      // (relative to the child's CWD), not through the mudlib path rules.
      {"--owner-audit --format=json " + cfg + " single/tests/efuns/has_cycle.c", 0, false},
      {cfg + " " + good + " extra", 1, true},
      // argc 3: compile one file (0) or fail to load it (1).
      {cfg + " " + good, 0, false},
      {cfg + " " + missing, 1, false},
      // --batch: batch exit code is 1 when any file fails.
      {"--batch " + cfg + " " + good, 0, false},
      {"--batch " + cfg + " " + missing, 1, false},
      {"--batch " + cfg + " " + good + " " + missing, 1, false},
  };

  if (LpccTestBuildDir().empty()) {
    GTEST_SKIP() << "cannot resolve the build directory from /proc/self/exe";
  }
  if (!std::filesystem::exists(std::filesystem::path(LpccTestBuildDir()) / "bin" / "lpcc")) {
    GTEST_SKIP() << "lpcc is not built";
  }

  for (const auto &test_case : cases) {
    const auto result = RunLpccCli(test_case.args);
    ASSERT_NE(result.exit_code, -1)
        << "lpcc could not be spawned for args: [" << test_case.args << "]";
    EXPECT_EQ(result.exit_code, test_case.exit_code)
        << "args: [" << test_case.args << "]\n"
        << result.output;
    if (test_case.expects_usage) {
      EXPECT_NE(result.output.find("Usage: lpcc"), std::string::npos)
          << "args: [" << test_case.args << "]\n"
          << result.output;
    }
  }

  // The batch path reports per-file results on stdout.
  const auto batch = RunLpccCli("--batch " + cfg + " " + good + " " + missing);
  EXPECT_NE(batch.output.find("PASS /single/tests/efuns/has_cycle"), std::string::npos)
      << batch.output;
  EXPECT_NE(batch.output.find("FAIL /single/tests/efuns/does_not_exist"), std::string::npos)
      << batch.output;
}

TEST_F(DriverTest, TestLpccUnknownConfigFailsCleanly) {
  // An unknown first argument is not a flag error: argc 3 means "compile
  // argv[2] against the config in argv[1]", and an unreadable config must fail
  // with the driver's own config error rather than a crash or a hang.
  const auto result = RunLpccCli("--unknown-flag etc/config.test");
  ASSERT_NE(result.exit_code, -1);
  EXPECT_NE(result.exit_code, 0);
  EXPECT_NE(result.output.find("config file"), std::string::npos) << result.output;
}
#endif  // !_WIN32
