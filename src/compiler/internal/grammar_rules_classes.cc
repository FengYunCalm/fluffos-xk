#include "base/std.h"

#include "vm/vm.h"
#include "vm/internal/base/machine.h"
#include "compiler/internal/compiler.h"
#include "compiler/internal/lex.h"
#include "compiler/internal/scratchpad.h"
#include "compiler/internal/generate.h"
#include "compiler/internal/grammar_rules.h"

ident_hash_elem_t* rule_define_class(LPC_INT* $$, char* $3) {
  ident_hash_elem_t* ihe;

  ihe = find_or_add_ident(PROG_STRING(*$$ = store_prog_string($3)), FOA_GLOBAL_SCOPE);
  if (ihe->dn.class_num == -1) {
    ihe->sem_value++;
    ihe->dn.class_num = mem_block[A_CLASS_DEF].current_size / sizeof(class_def_t);
    if (ihe->dn.class_num > CLASS_NUM_MASK) {
      yyerror("Too many classes, max is %d.\n", CLASS_NUM_MASK + 1);
    }

    scratch_free($3);
    return nullptr;
  } else {
    return ihe;
  }
}

void rule_define_class_members(struct ident_hash_elem_t* $2, LPC_INT $5) {
  class_def_t* sd;
  class_member_entry_t* sme;
  int i, raise_error = 0;

  /* check for a redefinition */
  if ($2 != 0) {
    sd = CLASS($2->dn.class_num);
    if (sd->size != current_number_of_locals) {
      raise_error = 1;
    } else {
      i = sd->size;
      sme = (class_member_entry_t*)mem_block[A_CLASS_MEMBER].block + sd->index;
      while (i--) {
        /* check for matching names and types */
        if (strcmp(PROG_STRING(sme[i].membername), locals_ptr[i].ihe->name) != 0 ||
            sme[i].type != (type_of_locals_ptr[i] & ~LOCAL_MODS)) {
          raise_error = 1;
          break;
        }
      }
    }
  }

  if (raise_error) {
    yyerror("Illegal to redefine class '%s',", PROG_STRING($5));
  } else {
    sd = (class_def_t*)allocate_in_mem_block(A_CLASS_DEF, sizeof(class_def_t));
    i = sd->size = current_number_of_locals;
    sd->index = mem_block[A_CLASS_MEMBER].current_size / sizeof(class_member_entry_t);
    sd->classname = $5;

    sme = (class_member_entry_t*)allocate_in_mem_block(
        A_CLASS_MEMBER, sizeof(class_member_entry_t) * current_number_of_locals);

    while (i--) {
      sme[i].membername = store_prog_string(locals_ptr[i].ihe->name);
      sme[i].type = type_of_locals_ptr[i] & ~LOCAL_MODS;
    }
  }
  free_all_local_names(0);
}
