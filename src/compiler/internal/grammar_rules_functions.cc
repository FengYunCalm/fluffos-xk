#include "base/std.h"

#include "vm/vm.h"
#include "vm/internal/base/machine.h"
#include "compiler/internal/compiler.h"
#include "compiler/internal/lex.h"
#include "compiler/internal/scratchpad.h"
#include "compiler/internal/generate.h"
#include "compiler/internal/grammar_rules.h"

#include <fmt/format.h>

extern int context;                   // FIXME
extern int func_present;              // FIXME
extern int compiling_async_function;  // parser state

// Snapshot of the function declaration's own line, taken in rule_func_type()
// (fired right after `type optional_star identifier` -- current_line is
// still on the function header, in the right file) and consumed by
// rule_func() once the whole body has been parsed. rule_func() can't just
// capture current_line itself: by the time it fires, parsing has moved on
// past the closing '}' (LALR needs a lookahead token to reduce), which can
// mean a completely different file if the function was the last thing in
// an #included header. Function declarations don't nest (LPC has no named
// function inside a function), so a single non-reentrant slot is safe --
// anonymous/lambda functions go through the expression rules instead and
// never touch this. (Upstream 06d23cfb #1293.)
static int pending_func_decl_line;

LPC_INT rule_func_type(LPC_INT type, LPC_INT optional_star, char* identifier) {
  int flags;
#ifdef SENSIBLE_MODIFIERS
  int acc_mod;
#endif
  pending_func_decl_line = current_line_base + current_line;
  func_present = 1;
  flags = PACKED_TYPE_MODS(type);

  flags |= global_modifiers;

#ifdef SENSIBLE_MODIFIERS
  acc_mod = (flags & DECL_ACCESS) & ~global_modifiers;
  if (acc_mod & (acc_mod - 1)) {
    char buf[256];
    char* end = EndOf(buf);
    char* p;

    p = strput(buf, end, "Multiple access modifiers (");
    p = get_type_modifiers(p, end, flags);
    p = strput(p, end, ") for function");
    yyerror("%s", buf);
  }
#endif

  if (!(flags & DECL_ACCESS))
    flags |= DECL_PUBLIC;
#ifdef SENSIBLE_MODIFIERS
  if (flags & DECL_NOSAVE) {
    yywarn("Illegal to declare nosave function.");
    flags &= ~DECL_NOSAVE;
  }
#endif
  type = PACK_TYPE_MODS(flags) | PACKED_TYPE_BASIC(type);
  // A named function starts a fresh runtime frame. Clear any locals left by
  // a preceding file-scope initializer without affecting __INIT's compile-
  // wide high-water mark.
  free_all_local_names(0);
  /* Handle type checking here so we know whether to typecheck
     'argument' */
  lpc_type_t const declared_type = PACKED_TYPE_BASIC(type);
  compiling_async_function = (flags & FUNC_ASYNC) != 0;
  if (declared_type) {
    if (CONFIG_INT(__RC_OLD_TYPE_BEHAVIOR__)) {
      exact_types = 0;
    } else {
      exact_types = declared_type | optional_star;
    }
  } else {
    if (pragmas & PRAGMA_STRICT_TYPES) {
      if (strcmp(identifier, "create") != 0)
        yyerror("\"#pragma strict_types\" requires type of function");
      else
        exact_types = TYPE_VOID; /* default for create() */
    } else
      exact_types = 0;
  }
  return type;
}

LPC_INT rule_func_proto(LPC_INT type, LPC_INT optional_star, char** identifier,
                        argument_t argument) {
  char* p = *identifier;
  *identifier = (char*)make_shared_string(*identifier);
  scratch_free(p);

  /* If we had nested functions, we would need to check */
  /* here if we have enough space for locals */

  /*
   * Define a prototype. If it is a real function, then the
   * prototype will be replaced below.
   */

  LPC_INT func_types = FUNC_PROTOTYPE;
  if (argument.flags & ARG_IS_VARARGS) {
    func_types |= (FUNC_TRUE_VARARGS | FUNC_VARARGS);
  }
  func_types |= PACKED_TYPE_MODS(type);

  define_new_function(*identifier, argument.num_arg, 0, func_types,
                      PACKED_TYPE_BASIC(type) | optional_star);
  /* This is safe since it is guaranteed to be in the
     function table, so it can't be dangling */
  free_string(*identifier);
  context = 0;

  return func_types;
}

void rule_func(parse_node_t** function, LPC_INT type, LPC_INT optional_star, char* identifier,
               argument_t argument, LPC_INT* func_types, parse_node_t** block_or_semi) {
  /* Either a prototype or a block */
  if (*block_or_semi) {
    int fun;

    *func_types &= ~FUNC_PROTOTYPE;
    if ((*block_or_semi)->kind != NODE_RETURN && ((*block_or_semi)->kind != NODE_TWO_VALUES ||
                                                  (*block_or_semi)->r.expr->kind != NODE_RETURN)) {
      parse_node_t* replacement;
      CREATE_STATEMENTS(replacement, *block_or_semi, 0);
      CREATE_RETURN(replacement->r.expr, 0);
      *block_or_semi = replacement;
    }

    // Creating functions for argument defaults
    fun = define_new_function(identifier, argument.num_arg, max_num_locals - argument.num_arg,
                              *func_types, PACKED_TYPE_BASIC(type) | optional_star);
    if (fun != -1) {
      *function = new_node_no_line();
      (*function)->kind = NODE_FUNCTION;
      // Safety net alongside the explicit-return line fix in the return
      // rules: a function whose body has NO explicit return (an implicit
      // `return 0;`/`return;` gets synthesized above by this rule itself,
      // via CREATE_RETURN's default new_node_no_line()) can still reach
      // codegen with every one of its statements' lines equal to 0 -- e.g.
      // an empty body, or one whose only statement the optimizer discards
      // entirely (a side-effect-free expression statement). Stamping the
      // function's own declaration line here (captured early, in
      // rule_func_type(), before parsing could drift into a different file --
      // see pending_func_decl_line's comment) makes i_generate_node()'s
      // existing `if (expr->line && ...)` guard call switch_to_line() at
      // function entry regardless of what's inside.
      (*function)->line = pending_func_decl_line;
      (*function)->v.number = fun;
      (*function)->l.number = max_num_locals;
      (*function)->r.expr = *block_or_semi;

      if (!(*func_types & FUNC_TRUE_VARARGS) && argument.num_arg) {
        bool have_default_args = false;
        auto default_args_limit = sizeof(FUNCTION_DEF(fun)->default_args_findex) /
                                  sizeof(FUNCTION_DEF(fun)->default_args_findex[0]);
        for (int i = 0; i < argument.num_arg; i++) {
          auto local = locals_ptr[i];
          if (local.funcptr_default) {
            have_default_args = true;
            if (i > default_args_limit || argument.num_arg > default_args_limit) {
              yyerror("Functions with default arguments can only have %zu args",
                      default_args_limit);
              return;
            }
            FUNCTION_DEF(fun)->min_arg--;
            // The helper's name must be unique across the whole inherit
            // chain: it is defined DECL_NOMASK, so a parent and child both
            // defining foo(int a: (: ... :)) with colliding helper names
            // is an "Illegal to redefine 'nomask' function" compile error.
            // A wall-clock timestamp only disambiguated compiles that
            // happened in DIFFERENT seconds -- overriding an inherited
            // default-arg function failed whenever parent and child compiled
            // within the same second (the normal case), and compiles were
            // unreproducible byte-wise. A process-global counter is unique
            // for every compile in the process and deterministic given
            // compile order.
            static uint64_t default_arg_seq = 0;
            auto funcname = fmt::format(FMT_STRING("#__{}_{}_{}"), ++default_arg_seq, identifier,
                                        local.ihe->name);
            // the funcnum here will change in epilog().
            auto funcnum = define_new_function(
                funcname.c_str(), 0, 0,
                (*func_types & DECL_ACCESS) | DECL_NOMASK,  // same access as origin function
                type_of_locals_ptr[locals_ptr[i].runtime_index]);
            FUNCTION_DEF(fun)->default_args_findex[i] = FUNCTION_TEMP(funcnum)->u.index;
            // debug_message("function %s (%d), new def arg function %s (%d)\n", identifier, fun, funcname.c_str(), funcnum);

            parse_node_t* node_return;
            CREATE_RETURN(node_return, local.funcptr_default);

            auto* node_func = new_node_no_line();
            node_func->kind = NODE_FUNCTION;
            node_func->v.number = funcnum;
            node_func->l.number = 0;
            node_func->r.expr = node_return;

            auto* newnode = *function;
            CREATE_TWO_VALUES(*function, 0, newnode, node_func);
          } else {
            if (i > 0) {
              if (i > default_args_limit)
                continue;
              auto prev = FUNCTION_DEF(fun)->default_args_findex[i - 1];
              if (prev != 0) {
                yyerror(
                    "Function arguments with default value closure must be specified continuously.");
                return;
              }
            }
          }
        }
      }
    } else
      *function = 0;
  } else
    *function = 0;
  free_all_local_names(!!(*block_or_semi));
  compiling_async_function = 0;
}
