#include <globals.h>

private mapping test_states = ([]);
private int next_token;
private int scheduling;
private int running;
private int finishing;
private int failed;
private mapping expected_runtime_error;
private string executing_case;
private mapping test_scopes = ([]);
private mapping excluded_cases = ([]);

int execute(string fun);

private string case_key(string name) {
  if (strlen(name) > 4 && name[<4..] == ".lpc") {
    return name[0..<5];
  }
  if (strlen(name) > 2 && name[<2..] == ".c") {
    return name[0..<3];
  }
  return name;
}

private void init_scopes() {
  mapping capabilities = ([ "async": 0, "db": 0, "sqlite": 0, "modern_ed": 0,
                            "profile": 0, "sockets": 0, "trace": 0, "tls_reload": 0,
                            "interactive": !!this_player() && interactive(this_player()) ]);
  string contents = read_file("/etc/test-scopes.tsv");

#ifdef __PACKAGE_ASYNC__
  capabilities["async"] = 1;
#endif
#ifdef __PACKAGE_DB__
  capabilities["db"] = 1;
#endif
#ifdef __USE_SQLITE3__
  capabilities["sqlite"] = 1;
#endif
#ifndef __OLD_ED__
  capabilities["modern_ed"] = 1;
#endif
#ifdef __PROFILE_FUNCTIONS__
  capabilities["profile"] = 1;
#endif
#ifdef __PACKAGE_SOCKETS__
  capabilities["sockets"] = 1;
#endif
#if efun_defined(trace_start)
  capabilities["trace"] = 1;
#endif
#if efun_defined(sys_reload_tls)
  capabilities["tls_reload"] = 1;
#endif
  if (!contents) {
    error("Missing test-scopes.tsv.\n");
  }
  foreach (string name in sort_array(keys(capabilities), 1)) {
    write(sprintf("T> cap %s=%d\n", name, capabilities[name]));
  }
  foreach (string line in explode(contents, "\n")) {
    string *fields;
    string *missing = ({});
    string kind;

    line = trim(line);
    if (line == "" || line[0] == '#') {
      continue;
    }
    fields = explode(line, "\t");
    if (sizeof(fields) != 4 || fields[3] == "" || test_scopes[fields[0]] ||
        member_array(fields[1], ({ "runtime", "fixture", "coverage_gap" })) == -1 ||
        (file_size(fields[0] + ".lpc") < 0 && file_size(fields[0] + ".c") < 0)) {
      error("Invalid or stale test-scopes.tsv row.\n");
    }
    foreach (string feature in fields[2] == "-" ? ({}) : explode(fields[2], ",")) {
      if (undefinedp(capabilities[feature])) {
        error("Unknown test capability: " + feature + "\n");
      }
      if (!capabilities[feature]) {
        missing += ({ feature });
      }
    }
    test_scopes[fields[0]] = fields;
    kind = fields[1] != "runtime" ? fields[1] : (sizeof(missing) ? "unavailable" : "");
    if (kind != "") {
      excluded_cases[fields[0]] = ({ kind, fields[3] });
    }
  }
}

int is_running() {
  return running;
}

void record_failure(string reason) {
  if (!running) {
    return;
  }
  failed = 1;
  write("T> fail " + reason + "\n");
  shutdown(-1);
}

void expect_runtime_error(function operation, string program, string message) {
  mapping state = test_states[case_key(file_name(previous_object()))];
  mapping expectation;
  mixed failure;

  if (!state || state["done"] || expected_runtime_error) {
    error("Expected runtime error requires an active, non-nested case scope.\n");
  }
  expectation = ([ "program": program, "message": message, "seen": 0 ]);
  expected_runtime_error = expectation;
  failure = catch(evaluate(operation));
  expected_runtime_error = 0;
  if (failure) {
    error(failure);
  }
  if (!expectation["seen"]) {
    error("Expected runtime error was not observed.\n");
  }
}

int consume_expected_error(string program, string message) {
  mapping state = test_states[executing_case || ""];

  // A expects failure and B checks survival. The master still rejects assertions.
  if (state && (state["kind"] == "A" || state["kind"] == "B")) {
    return 1;
  }
  if (!expected_runtime_error || expected_runtime_error["seen"] ||
      expected_runtime_error["program"] != program ||
      expected_runtime_error["message"] != message) {
    return 0;
  }
  expected_runtime_error["seen"] = 1;
  return 1;
}

void record_assertion(object caller) {
  mapping state;

  if (!running || caller == this_object()) {
    return;
  }
  // Helper and inherited assertions belong to the executing case, not their source file.
  state = test_states[executing_case || (caller ? case_key(file_name(caller)) : "")];
  if (!state) {
    return;
  }
  if (state["done"]) {
    record_failure("assertion after case completion: " + state["name"]);
    return;
  }
  state["assertions"]++;
}

private void finish_run() {
  if (failed) {
    return;
  }
  write("Checks succeeded.\n");
  "/single/tests/efuns/shutdown"->do_the_nasty_deed();
}

private void finish_if_ready() {
  if (!running || scheduling || finishing || failed) {
    return;
  }
  foreach (mapping state in values(test_states)) {
    if (!state["done"]) {
      return;
    }
  }
  finishing = 1;
  call_out((: finish_run :), 0);
}

private void complete_case(string key) {
  mapping state = test_states[key];

  if (state["done"] || !state["returned"] || sizeof(state["tokens"])) {
    return;
  }
  if (state["kind"] == "C" && !state["assertions"]) {
    record_failure("ordinary case has no assertion coverage: " + state["name"]);
    return;
  }
  state["done"] = 1;
  write(sprintf("D> %s %s asserts=%d\n", state["kind"], state["name"],
                state["assertions"]));
  finish_if_ready();
}

int begin_async() {
  string key = case_key(file_name(previous_object()));
  mapping state = test_states[key];
  int token;

  if (!running || !state || state["done"]) {
    record_failure("async registration outside an active case");
    return 0;
  }
  token = ++next_token;
  state["tokens"][token] = 1;
  return token;
}

void complete_async(int token) {
  string key = case_key(file_name(previous_object()));
  mapping state = test_states[key];

  if (!state || !state["tokens"][token]) {
    record_failure("unknown or repeated async completion");
    return;
  }
  map_delete(state["tokens"], token);
  complete_case(key);
}

private void start_case(string name, string kind) {
  string key = case_key(name);

  if (test_states[key]) {
    error("Duplicate test identity: " + name + "\n");
  }
  test_states[key] = ([ "name": name, "kind": kind, "assertions": 0,
                        "tokens": ([]), "returned": 0, "done": 0 ]);
  executing_case = key;
  write(kind + "> " + name + "\n");
}

private void check_test_memory(string name) {
#if defined(__DEBUGMALLOC__) && defined(__DEBUGMALLOC_EXTENSIONS__) && defined(__PACKAGE_DEVELOP__)
  string leaks = check_memory();

  if (sizeof(filter(explode(leaks, "\n"), (: $1 && $1[0] :))) != 1) {
    write("After test: " + name + "\n" + leaks);
    error("LEAK\n");
  }
#endif
}

private string *source_files(string dir) {
  string *files = ({});

  // Source spelling is preserved; the extension-blind identity prefers .lpc.
  foreach (string file in sort_array(get_dir(dir + "*.c") + get_dir(dir + "*.lpc"), 1)) {
    if (strlen(file) > 2 && file[<2..] == ".c" &&
        file_size(dir + file[0..<3] + ".lpc") != -1) {
      continue;
    }
    files += ({ dir + file });
  }
  return files;
}

void recurse(string dir) {
  foreach (string file in source_files(dir)) {
    execute(file);
  }
  foreach (string subdir in sort_array(map(filter(get_dir(dir + "*", -1),
           (: $1[1] == -2 :)), (: $1[0] :)) - ({ ".", ".." }), 1)) {
    if (subdir == "fail" || subdir == "crasher") {
      foreach (string file in source_files(dir + subdir + "/")) {
        string key = case_key(file);

        start_case(file, subdir == "fail" ? "A" : "B");
        if (subdir == "fail") {
          ASSERT2(catch(load_object(file)), file + " loaded");
        } else {
          // Crasher coverage is survival, not ordinary assertion coverage.
          catch(load_object(file)->do_tests());
        }
        executing_case = 0;
        check_test_memory(file);
        test_states[key]["returned"] = 1;
        complete_case(key);
      }
      if (subdir == "fail") {
        cp("/log/compile", "/log/compile_fail");
        rm("/log/compile");
      }
    } else {
      recurse(dir + subdir + "/");
    }
  }
}

int execute(string fun) {
  object tp = this_player();
  string key;

  if (!fun || fun == "") {
    recurse("/single/tests/");
    return 1;
  }
  key = case_key(fun);
  if (excluded_cases[key]) {
    write("X> " + fun + "\t" + implode(excluded_cases[key], "\t") + "\n");
    return 1;
  }
  set_eval_limit(0x7fffffff);
  start_case(fun, "C");
  ASSERT_EQ(0, catch(fun->do_tests()));
  executing_case = 0;
  set_eval_limit(0x7fffffff);

  if (tp != this_player()) {
    error("Bad this_player() after calling " + fun + "\n");
  }
  check_test_memory(fun);
  if ("/single/master"->get_inherit_called() == 0) {
    error("MASTER valid inherit functions are not being called!\n");
  }
  test_states[key]["returned"] = 1;
  complete_case(key);
  return 1;
}

int main(string file) {
  if (running) {
    error("A test run is already active.\n");
  }
#if !(defined(__DEBUGMALLOC__) && defined(__DEBUGMALLOC_EXTENSIONS__) && defined(__PACKAGE_DEVELOP__))
  write("WARNING: Possible RELEASE build, check_memory() is not being executed.\n");
#endif
  running = 1;
  scheduling = 1;
  write("T> order lexical-v1\n");
  init_scopes();
  execute(file || "");
  if (!sizeof(test_states)) {
    record_failure("No applicable test cases were selected.");
  }
  scheduling = 0;
  finish_if_ready();
  return 1;
}
