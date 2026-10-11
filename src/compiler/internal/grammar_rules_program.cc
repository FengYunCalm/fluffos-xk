#include "base/std.h"

#include "vm/vm.h"
#include "vm/internal/base/machine.h"
#include "compiler/internal/compiler.h"
#include "compiler/internal/lex.h"
#include "compiler/internal/scratchpad.h"
#include "compiler/internal/generate.h"
#include "compiler/internal/grammar_rules.h"

void rule_program(parse_node_t* $$) {
  comp_trees[TREE_MAIN] = $$;
}

bool rule_inheritence(parse_node_t** $$, int $1, char* $3) {
  object_t* ob;
  inherit_t inherit;
  int initializer;
#ifdef SENSIBLE_MODIFIERS
  int acc_mod;
#endif

  $1 |= global_modifiers;

#ifdef SENSIBLE_MODIFIERS
  acc_mod = ($1 & DECL_ACCESS) & ~global_modifiers;
  if (acc_mod & (acc_mod - 1)) {
    char buf[256];
    char* end = EndOf(buf);
    char* p;

    p = strput(buf, end, "Multiple access modifiers (");
    p = get_type_modifiers(p, end, acc_mod);
    p = strput(p, end, ") for inheritance");
    yyerror("%s", buf);
  }
#endif

  if (!($1 & DECL_ACCESS))
    $1 |= DECL_PUBLIC;
  if (var_defined) {
    yyerror("Illegal to inherit after defining global variables.");
    inherit_file = 0;
    return true;
  }
  ob = find_object2($3);
  if (ob == 0) {
    inherit_file = alloc_cstring($3, "inherit");
    /* Return back to load_object() */
    return true;
  }
  scratch_free($3);
  inherit.prog = ob->prog;

  if (mem_block[A_INHERITS].current_size) {
    inherit_t* prev_inherit = INHERIT(NUM_INHERITS - 1);

    inherit.function_index_offset = prev_inherit->function_index_offset +
                                    prev_inherit->prog->num_functions_defined +
                                    prev_inherit->prog->last_inherited;
    if (prev_inherit->prog->num_functions_defined &&
        prev_inherit->prog->function_table[prev_inherit->prog->num_functions_defined - 1]
                .funcname[0] == APPLY___INIT_SPECIAL_CHAR)
      inherit.function_index_offset--;
  } else
    inherit.function_index_offset = 0;

  inherit.variable_index_offset = mem_block[A_VAR_TEMP].current_size / sizeof(variable_t);
  inherit.type_mod = $1;
  add_to_mem_block(A_INHERITS, (char*)&inherit, sizeof inherit);

  /* The following has to come before copy_vars - Sym */
  copy_structures(ob->prog);
  copy_variables(ob->prog, $1);
  initializer = copy_functions(ob->prog, $1);
  if (initializer >= 0) {
    parse_node_t *node, *newnode;
    /* initializer is an index into the object we're
       inheriting's function table; this finds the
       appropriate entry in our table and generates
       a call to it */
    node = new_node_no_line();
    node->kind = NODE_CALL_2;
    node->r.expr = 0;
    node->v.number = F_CALL_INHERITED;
    node->l.number = initializer | ((NUM_INHERITS - 1) << 16);
    node->type = TYPE_ANY;

    /* The following illustrates a distinction between */
    /* macros and funcs...newnode is needed here - Sym */
    newnode = comp_trees[TREE_INIT];
    CREATE_TWO_VALUES(comp_trees[TREE_INIT], 0, newnode, node);
    comp_trees[TREE_INIT] = pop_value(comp_trees[TREE_INIT]);
  }
  *$$ = 0;

  return false;
}
