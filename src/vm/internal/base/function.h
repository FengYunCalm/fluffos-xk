#ifndef FUNCTION_H
#define FUNCTION_H

#include <cstdint>

class RecompileExecutionContext;

/* It is usually better to include "lpc_incl.h" instead of including this
   directly */

/* FP_LOCAL */
typedef struct {
  short index;
  /* E3 P2: the program this index belongs to at creation/bind time. It is
   * also the func_ref accounting object: dealloc_funp() decrements THIS
   * program's func_ref, never the owner's current program (which can be
   * hot-replaced). NULL for non-FP_LOCAL uses of the union slot. */
  struct program_t *prog;
} local_ptr_t;

/* FP_SIMUL */
typedef local_ptr_t simul_ptr_t;

/* FP_EFUN */
typedef local_ptr_t efun_ptr_t;

/* FP_FUNCTIONAL */
struct functional_t {
  /* these two must come first */
  unsigned char num_arg;
  unsigned char num_local;
  short fio;
  struct program_t *prog;
  int offset;
  short vio;
  // char lpccode[80];
};

/* common header */
struct funptr_hdr_t {
  uint32_t ref;
  short type; /* FP_* is used */
#ifdef DEBUGMALLOC_EXTENSIONS
  int extra_ref;
#endif
  struct object_t *owner;
  struct array_t *args;
  /* E3 P2/I01: owner->prog_generation at creation/bind time. Every kind
   * snapshots it through initialize_funp_header; FP_LOCAL and FP_FUNCTIONAL
   * compare it before dispatch and report a stable stale-pointer error after
   * recompile_object() swaps programs. */
  uint64_t owner_gen;
  /* I05: a newborn pointer is staged until the transaction succeeds; a
   * failed transaction changes it permanently to Invalid. */
  enum class lifecycle_state : uint8_t { Live, StagedBorn, Invalid };
  lifecycle_state state;
  /* Every live funptr is linked into its owner's weak registry. */
  bool registry_linked;
  struct funptr_t *registry_prev;
  struct funptr_t *registry_next;
  /* The shared recompile birth journal holds one temporary reference. */
  RecompileExecutionContext *birth_context;
  struct funptr_t *birth_prev;
  struct funptr_t *birth_next;
  /* Existing target pointers receive a separate prepare pin. This link is
   * independent of the owner registry so destruct cleanup can unlink the
   * owner without losing the transaction's reference. */
  RecompileExecutionContext *transaction_pin_context;
  struct funptr_t *transaction_pin_prev;
  struct funptr_t *transaction_pin_next;
};

struct funptr_t {
  funptr_hdr_t hdr;
  union {
    efun_ptr_t efun;
    local_ptr_t local;
    simul_ptr_t simul;
    functional_t functional;
  } f;
};

union string_or_func {
  funptr_t *f;
  const char *s;
};

void initialize_funp_header(funptr_t *, short, struct object_t *, bool retain_owner_ref = true);
void funptr_register(funptr_t *) noexcept;
void funptr_unlink(funptr_t *) noexcept;
void funptr_detach_owner(funptr_t *) noexcept;
void funptr_detach_all_for_object(struct object_t *) noexcept;
void dealloc_funp(funptr_t *);
void push_refed_funp(funptr_t *);
void push_funp(funptr_t *);
void free_funp(funptr_t *);
int merge_arg_lists(int, struct array_t *, int);
funptr_t *make_efun_funp(int, struct svalue_t *);
funptr_t *make_lfun_funp(int, struct svalue_t *);
funptr_t *make_simul_funp(int, struct svalue_t *);
funptr_t *make_functional_funp(short, short, short, struct svalue_t *, int);

#endif
