/* FILES
 *    - pcre.c
 *    - pcre_spec.c
 *    - pcre.h
 *
 * PCRE (Perl Compatible Regular Expressions)
 *    Efuns using system library of PCRE (http://www.pcre.org/)
 *    The PCRE library was created by Philip Hazel at the
 *    University of Cambridge
 *
 *    For regular expressions syntactics:
 *       http://perldoc.perl.org/perlre.html
 *
 *
 * AUTHOR
 *    Volothamp @ Final Realms
 *         nfa106 [a] gmail.com
 *
 * HISTORY
 *    June-July 2009:
 *       Package created
 *
 *
 * DESCRIPTION
 *
 *   This package introduces the following efuns:
 *
 *      string pcre_version();
 *         - returns the version of the compiled PCRE library used
 *
 *      mixed pcre_match(string | string *, string, void | int);
 *         - analog with regexp(string | string *, string, void | int);
 *           for backwards compatibility reasons but utilizing the PCRE
 *           library.
 *
 *      mixed *pcre_assoc(string, string *, mixed *, mixed | void);
 *         - analog with reg_assoc(string, string *, mixed *, mixed | void);
 *           for backwards compatibility reasons but utilizing the PCRE
 *           library.
 *
 *      string *pcre_extract(string subject, string pattern);
 *         - returns an array of captured groups specified in pattern
 *
 *      string pcre_replace(string subject, string pattern, string
 **replacement);
 *         - returns a string where all captured groups have been replaced by
 *the
 *           elements of the replacement array. Number of subgroups and the size
 *of
 *           the replacement array must match.
 *
 *      string pcre_replace_callback(string subject, string pattern, function |
 *string fun, object ob);
 *         - returns a string where all captured groups have been replaced by
 *the
 *           return value of function pointe fun or function fun in object ob.
 */
// TODO
// study pattern
// extra options when matching, greedy, lazy, possesive, etc.
// match_all ? see previous
// store reg->error & reg->erroffset before error(..)

#include <thirdparty/scope_guard/scope_guard.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include "base/package_api.h"

#include "pcre.h"
#include "include/pcre_flags.h"
#include "vm/internal/base/mapping.h"

// Prototype declarations
static void pcre_free_memory(pcre_t *p);
static pcre2_code *pcre_local_compile(pcre_t *p);
static int pcre_local_exec(pcre_t *p);
static int pcre_exec_at(pcre_t *p, PCRE2_SIZE offset, uint32_t extra_flags);
static int pcre_magic(pcre_t *p);
static int pcre_query_match(pcre_t *p);
static void pcre_throw_match_error(int rc);
static inline uint32_t compute_compile_options(int flags);
static inline uint32_t compute_exec_options(int flags);
static int pcre_match_single(svalue_t *str, const char *pattern, int pcre_flags);
static array_t *pcre_match(array_t *v, const char *pattern, int flag, int pcre_flags);
static array_t *pcre_assoc(svalue_t *str, array_t *pat, array_t *tok, svalue_t *def, int pcre_flags);
static char *pcre_get_replace(pcre_t *run, array_t *replacements);
static array_t *pcre_get_substrings(pcre_t *run, bool include_names);

namespace {

struct pcre_replace_segment_t {
  PCRE2_SIZE start;
  PCRE2_SIZE end;
  size_t replacement_length;
  const char *replacement;
};

static bool pcre_capture_participated(const pcre_t *run, int group) {
  if (group < 0 || static_cast<PCRE2_SIZE>(group) >
                      (std::numeric_limits<PCRE2_SIZE>::max() - 1) / 2) {
    return false;
  }
  PCRE2_SIZE const ovec_index = static_cast<PCRE2_SIZE>(group) * 2;
  return ovec_index + 1 < run->ovecsize && run->ovector[ovec_index] != PCRE2_UNSET &&
         run->ovector[ovec_index + 1] != PCRE2_UNSET &&
         run->ovector[ovec_index + 1] >= run->ovector[ovec_index] &&
         run->ovector[ovec_index + 1] <= run->s_length;
}

static PCRE2_SIZE pcre_checked_length(size_t length) {
  if (length > static_cast<size_t>(std::numeric_limits<PCRE2_SIZE>::max())) {
    error("PCRE subject is too large.\n");
  }
  return static_cast<PCRE2_SIZE>(length);
}

static PCRE2_SIZE pcre_subject_length(const pcre_t *run) { return run->s_length; }

static PCRE2_SIZE pcre_advance_after_empty_match(const char *subject,
                                                  PCRE2_SIZE subject_length,
                                                  PCRE2_SIZE offset) {
  if (offset >= subject_length) {
    return subject_length;
  }

  // PCRE2 is compiled in UTF-8 mode. Advance by one complete code point so a
  // zero-length match cannot be retried at the same byte offset.
  PCRE2_SIZE next = offset + 1;
  while (next < subject_length &&
         (static_cast<unsigned char>(subject[next]) & 0xc0) == 0x80) {
    ++next;
  }
  return next;
}

static std::vector<pcre_replace_segment_t> pcre_build_replace_segments(const pcre_t *run,  // #1247-equivalent PCRE-1/2 (non-overlapping segments)
                                                                         array_t *replacements,
                                                                         size_t *result_size) {
  std::vector<pcre_replace_segment_t> segments;
  size_t size = static_cast<size_t>(run->s_length);
  PCRE2_SIZE previous_end = run->ovector[0];

  for (int group = 1; group < run->rc; ++group) {
    if (!pcre_capture_participated(run, group)) {
      continue;
    }

    PCRE2_SIZE const start = run->ovector[group * 2];
    PCRE2_SIZE const end = run->ovector[group * 2 + 1];
    if (start < previous_end) {
      continue;  // Nested capture: the outer segment owns this range.
    }

    size_t const old_length = static_cast<size_t>(end - start);
    size_t const replacement_length = SVALUE_STRLEN(&replacements->item[group - 1]);
    if (size < old_length) {
      error("Invalid PCRE capture range.\n");
    }
    size -= old_length;
    if (replacement_length > std::numeric_limits<size_t>::max() - size) {
      error("PCRE replacement result is too large.\n");
    }
    size += replacement_length;
    previous_end = end;
    segments.push_back({start, end, replacement_length, replacements->item[group - 1].u.string});
  }

  *result_size = size;
  return segments;
}

}  // namespace

// Caching functions
static int pcre_cache_pattern(struct pcre_cache_t *table, pcre_t *run,
                              const char *pattern, uint32_t compile_flags);
static pcre_cache_bucket_t *pcre_get_cached_pattern(struct pcre_cache_t *table,
                                                     const char *pattern,
                                                     uint32_t compile_flags);
static mapping_t *pcre_get_cache();
int pcrecachesize = 0;
// Entries detached during an active match stay rooted until the last borrower
// releases them. This keeps the shared pattern alive across reentrant cache
// eviction and callback execution.
static pcre_cache_bucket_t *pcre_detached_cache = nullptr;
// Globals
struct pcre_cache_t pcre_cache = {{nullptr}};

// efuns
void f_pcre_version() {
  static char version[32];
  static bool initialized = false;
  if (!initialized) {
    snprintf(version, sizeof(version), "%d.%d", PCRE2_MAJOR, PCRE2_MINOR);
    initialized = true;
  }
  push_constant_string(version);
}

void f_pcre_match() {
  if (st_num_arg < 2 || st_num_arg > 4) {
    error("pcre_match() requires 2 to 4 arguments\n");
  }
  array_t *v;
  int flag = 0;
  int pcre_flags = 0;
  // The subject (1st arg) is at sp - st_num_arg + 1 no matter how many
  // optional trailing args (flag, pcre_flags) follow; reading (sp - 1) only
  // located it for the 2-arg form, so every 3-/4-arg call misidentified the
  // mode and ran the wrong branch over the subject's union bits.
  bool is_string = ((sp - st_num_arg + 1)->type == T_STRING);

  // optional 4th arg: pcre_flags
  if (st_num_arg > 3) {
    if (sp->type != T_NUMBER) {
      error("Bad argument 4 to pcre_match()\n");
    }
    pcre_flags = (sp--)->u.number;
    st_num_arg--;
  }

  // optional 3rd arg:
  if (st_num_arg == 3) {
    if (sp->type != T_NUMBER) {
      error("Bad argument 3 to pcre_match()\n");
    }
    if (is_string) {
      pcre_flags = (sp--)->u.number;
    } else {
      flag = (sp--)->u.number;
    }
    st_num_arg--;
  }

  if (sp->type != T_STRING) {
    error("Bad pattern argument to pcre_match()\n");
  }
  if (is_string) {
    if ((sp - 1)->type != T_STRING) {
      error("Bad subject argument to pcre_match()\n");
    }
    flag = pcre_match_single((sp - 1), sp->u.string, pcre_flags);

    free_string_svalue(sp--);
    free_string_svalue(sp);
    put_number(flag);
  } else {
    if ((sp - 1)->type != T_ARRAY) {
      error("Bad subject argument to pcre_match()\n");
    }
    v = pcre_match((sp - 1)->u.arr, sp->u.string, flag, pcre_flags);

    free_string_svalue(sp--);
    free_array(sp->u.arr);
    sp->u.arr = v;
  }
}

void f_pcre_assoc() {
  if (st_num_arg < 3 || st_num_arg > 5) {
    error("pcre_assoc() requires 3 to 5 arguments\n");
  }
  svalue_t *arg;
  array_t *vec;
  int pcre_flags = 0;

  if (st_num_arg == 5) {
    if (sp->type != T_NUMBER) {
      error("Bad argument 5 to pcre_assoc()\n");
    }
    pcre_flags = sp->u.number;
    sp--;
    st_num_arg--;
  }

  arg = sp - st_num_arg + 1;

  if (arg->type != T_STRING) {
    error("Bad argument 1 to pcre_assoc()\n");
  }
  if ((arg + 1)->type != T_ARRAY) {
    error("Bad argument 2 to pcre_assoc()\n");
  }
  if ((arg + 2)->type != T_ARRAY) {
    error("Bad argument 3 to pcre_assoc()\n");
  }

  vec = pcre_assoc(arg, (arg + 1)->u.arr, (arg + 2)->u.arr,
                   st_num_arg > 3 ? (arg + 3) : &const0, pcre_flags);

  if (st_num_arg == 4) {
    pop_3_elems();
  } else {
    pop_2_elems();
  }

  free_string_svalue(sp);

  sp->type = T_ARRAY;
  sp->u.arr = vec;
}

void f_pcre_extract() {
  if (st_num_arg < 2 || st_num_arg > 4) {
    error("pcre_extract() requires 2 to 4 arguments\n");
  }
  pcre_t *run;
  array_t *ret;
  int include_names = 0;
  int pcre_flags = 0;

  if (st_num_arg >= 4) {
    if (sp->type != T_NUMBER) {
      error("Bad argument 4 to pcre_extract()\n");
    }
    pcre_flags = sp->u.number;
    sp--;
    st_num_arg--;
  }

  svalue_t *arg = sp - st_num_arg + 1;

  if (arg->type != T_STRING || (arg + 1)->type != T_STRING) {
    error("Bad subject or pattern argument to pcre_extract()\n");
  }

  if (st_num_arg == 3) {
    if ((arg + 2)->type != T_NUMBER) {
      error("Bad argument 3 to pcre_extract()\n");
    }
    include_names = (arg + 2)->u.number != 0;
  } else if (st_num_arg != 2) {
    error("pcre_extract() requires 2 or 3 arguments\n");
  }

  run = (pcre_t *)DCALLOC(1, sizeof(pcre_t), TAG_TEMPORARY, "f_pcre_extract : run");
  run->pattern = (arg + 1)->u.string;
  run->subject = arg->u.string;
  run->s_length = pcre_checked_length(SVALUE_STRLEN(arg));
  run->ovector = nullptr;
  run->ovecsize = 0;
  run->compile_flags = compute_compile_options(pcre_flags);
  run->exec_flags = compute_exec_options(pcre_flags);
  DEFER { pcre_free_memory(run); };

  if (pcre_magic(run) < 0) {
    error("PCRE compilation failed at offset %zu: %s\n",
          static_cast<size_t>(run->erroffset), run->error);
  }

  if (run->rc == PCRE2_ERROR_NOMATCH) {
    pop_n_elems(st_num_arg);
    push_refed_array(&the_null_array);
    return;
  }
  if (run->rc < 0 || run->rc == 0) {
    pcre_throw_match_error(run->rc);
  }
  if (static_cast<PCRE2_SIZE>(run->rc) > run->ovecsize / 2) {
    error("Too many substrings.\n");
  }

  ret = pcre_get_substrings(run, include_names);
  pop_n_elems(st_num_arg);

  push_refed_array(ret);
}

void f_pcre_replace() {
  if (st_num_arg < 3 || st_num_arg > 4) {
    error("pcre_replace() requires 3 or 4 arguments\n");
  }
  pcre_t *run;
  array_t *replacements;

  char *ret;
  int pcre_flags = 0;

  if (st_num_arg >= 4) {
    if (sp->type != T_NUMBER) {
      error("Bad argument 4 to pcre_replace()\n");
    }
    pcre_flags = (sp--)->u.number;
    st_num_arg--;
  }

  // The spec's types are compile-time only: a `mixed` value reaches these
  // slots unchecked, and the code below reads .u.string/.u.arr raw.
  if ((sp - 2)->type != T_STRING) {
    error("Bad argument 1 to pcre_replace()\n");
  }
  if ((sp - 1)->type != T_STRING) {
    error("Bad argument 2 to pcre_replace()\n");
  }
  if (sp->type != T_ARRAY) {
    error("Bad argument 3 to pcre_replace()\n");
  }

  run = (pcre_t *)DCALLOC(1, sizeof(pcre_t), TAG_TEMPORARY, "f_pcre_replace: run");
  DEFER { pcre_free_memory(run); };

  run->ovector = nullptr;
  run->ovecsize = 0;
  run->pattern = (sp - 1)->u.string;
  run->subject = (sp - 2)->u.string;
  replacements = sp->u.arr;

  for (int i = 0; i < replacements->size; ++i) {
    if (replacements->item[i].type != T_STRING) {
      error("Non-string found in PCRE replacement array.\n");
    }
  }

  run->s_length = pcre_checked_length(SVALUE_STRLEN(sp - 2));
  run->compile_flags = compute_compile_options(pcre_flags);
  run->exec_flags = compute_exec_options(pcre_flags);

  if (pcre_magic(run) < 0) {
    error("PCRE compilation failed at offset %zu: %s\n",
          static_cast<size_t>(run->erroffset), run->error);
  }

  if (run->rc == PCRE2_ERROR_NOMATCH) {
    pop_2_elems();
    return;
  }
  if (run->rc < 0 || run->rc == 0) {
    pcre_throw_match_error(run->rc);
  }

  if (static_cast<PCRE2_SIZE>(run->rc) > run->ovecsize / 2) {
    error("Too many substrings.\n");
  }
  if ((run->rc - 1) != replacements->size) {
    int const tmp = run->rc - 1;
    error(
        "Number of captured substrings and replacements do not match, "
        "%d vs %d.\n",
        tmp, replacements->size);
  }

  if (run->rc == 1) {
    /* No captured substrings, return subject */
    pop_2_elems();
    return;
    // push_malloced_string(run->subject);
  }

  ret = pcre_get_replace(run, replacements);

  pop_3_elems();
  push_malloced_string(ret);
}

// string pcre_replace_callback(string, string, function)
void f_pcre_replace_callback() {
  int num_arg = st_num_arg, i;
  if (num_arg < 3) {
    error("pcre_replace_callback() requires at least 3 arguments\n");
  }
  char *ret;
  pcre_t *run;
  svalue_t *arg;
  array_t *arr, *r;
  function_to_call_t ftc;
  int pcre_flags = 0;

  // A numeric final argument is the legacy optional flags slot. Other final
  // arguments remain callback-bound values and are passed to the callback.
  if (num_arg >= 4 && sp->type == T_NUMBER) {
    pcre_flags = sp->u.number;
    sp--;
    st_num_arg--;
    num_arg--;
  }

  arg = sp - num_arg + 1;

  if (arg->type != T_STRING || (arg + 1)->type != T_STRING) {
    error("Bad subject or pattern argument to pcre_replace_callback()\n");
  }
  if (arg[2].type != T_FUNCTION && arg[2].type != T_STRING) {
    error("Illegal third argument to pcre_replace_callback()\n");
  }

  run = (pcre_t *)DCALLOC(1, sizeof(pcre_t), TAG_TEMPORARY, "f_pcre_replace: run");
  run->ovector = nullptr;
  run->ovecsize = 0;
  run->subject = arg->u.string;
  run->pattern = (arg + 1)->u.string;

  run->s_length = pcre_checked_length(SVALUE_STRLEN(arg));
  run->compile_flags = compute_compile_options(pcre_flags);
  run->exec_flags = compute_exec_options(pcre_flags);
  DEFER { pcre_free_memory(run); };

  if (pcre_magic(run) < 0) {
    error("PCRE compilation failed at offset %zu: %s\n",
          static_cast<size_t>(run->erroffset), run->error);
  }

  if (run->rc == PCRE2_ERROR_NOMATCH) {
    pop_n_elems(num_arg - 1);
    return;
  }
  if (run->rc < 0 || run->rc == 0) {
    pcre_throw_match_error(run->rc);
  }

  if (static_cast<PCRE2_SIZE>(run->rc) > run->ovecsize / 2) {
    error("Too many substrings.\n");
  }

  arr = pcre_get_substrings(run, false);

  if (arg[2].type == T_FUNCTION || arg[2].type == T_STRING) {
    process_efun_callback(2, &ftc, F_PCRE_REPLACE_CALLBACK);
  } else {  // 0
    error("Illegal third argument (0) to pcre_replace_callback");
  }

  r = allocate_array(run->rc - 1);  // can't use the empty variant in case we error below

  push_refed_array(r);
  push_refed_array(arr);
  error_context_t econ;

  save_context(&econ);
  try {
    for (i = 0; i < run->rc - 1; i++) {
      svalue_t *v;
      push_svalue(arr->item + i);
      push_number(i);
      v = call_efun_callback(&ftc, 2);

      /* Mimic behaviour of map(string, function) when function pointer returns
       null,
       ie return the input.  */
      if (v && v->type == T_STRING && v->u.string != nullptr) {
        assign_svalue_no_free(&r->item[i], v);
      } else {
        assign_svalue_no_free(&r->item[i], &arr->item[i]);
      }
    }
  } catch (const char *) {
    restore_context(&econ);
    /* condition was restored to where it was when we came in */
    pop_context(&econ);
    error("error in callback!\n");
  }
  pop_context(&econ);
  ret = pcre_get_replace(run, r);

  pop_n_elems(num_arg + 2);  // refed arrays
  push_malloced_string(ret);
}

void f_pcre_cache() {
  mapping_t *m = nullptr;
  m = pcre_get_cache();
  if (!m) {
    push_number(0);
  } else {
    push_refed_mapping(m);
  }
}

// Internal functions utilized by the efuns
static inline uint32_t compute_compile_options(int flags) {
  uint32_t opts = PCRE2_UTF;
  if (flags & PCRE_I) opts |= PCRE2_CASELESS;
  if (flags & PCRE_M) opts |= PCRE2_MULTILINE;
  if (flags & PCRE_S) opts |= PCRE2_DOTALL;
  if (flags & PCRE_U) opts |= PCRE2_UNGREEDY;
  if (flags & PCRE_X) opts |= PCRE2_EXTENDED;
  return opts;
}

static inline uint32_t compute_exec_options(int flags) {
  return (flags & PCRE_A) ? PCRE2_ANCHORED : 0;
}

static void pcre_set_compile_error(pcre_t *p, int errorcode) {
  int const length = pcre2_get_error_message(errorcode, reinterpret_cast<PCRE2_UCHAR *>(p->error),
                                             sizeof(p->error));
  if (length < 0) {
    snprintf(p->error, sizeof(p->error), "PCRE2 error %d", errorcode);
    return;
  }
  p->error[sizeof(p->error) - 1] = '\0';
}

static void pcre_throw_match_error(int rc) {
  char message[256];
  int const length = pcre2_get_error_message(rc, reinterpret_cast<PCRE2_UCHAR *>(message),
                                             sizeof(message));
  if (length < 0) {
    snprintf(message, sizeof(message), "PCRE2 error %d", rc);
  } else {
    message[sizeof(message) - 1] = '\0';
  }
  error("PCRE matching failed (%d): %s\n", rc, message);
}

static pcre2_code *pcre_local_compile(pcre_t *p) {
  int errorcode = 0;
  p->erroffset = 0;
  p->re = pcre2_compile(reinterpret_cast<PCRE2_SPTR>(p->pattern), PCRE2_ZERO_TERMINATED,
                         p->compile_flags, &errorcode, &p->erroffset, nullptr);
  if (p->re == nullptr) {
    pcre_set_compile_error(p, errorcode);
  }
  return p->re;
}

static void pcre_load_pattern_info(pcre_t *p) {
  p->namecount = 0;
  p->name_entry_size = 0;
  p->name_table = nullptr;
  if (pcre2_pattern_info(p->re, PCRE2_INFO_NAMECOUNT, &p->namecount) != 0 ||
      pcre2_pattern_info(p->re, PCRE2_INFO_NAMEENTRYSIZE, &p->name_entry_size) != 0 ||
      pcre2_pattern_info(p->re, PCRE2_INFO_NAMETABLE, &p->name_table) != 0) {
    error("Unable to query PCRE2 pattern metadata.\n");
  }
}

static int pcre_exec_at(pcre_t *p, PCRE2_SIZE offset, uint32_t extra_flags) {
  pcre2_match_context *match_context = pcre2_match_context_create(nullptr);
  if (match_context == nullptr) {
    error("Unable to allocate PCRE2 match context.\n");
  }
  DEFER { pcre2_match_context_free(match_context); };

  pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(p->re, nullptr);
  if (match_data == nullptr) {
    error("Unable to allocate PCRE2 match data.\n");
  }
  DEFER { pcre2_match_data_free(match_data); };

  int const rc = pcre2_match(p->re, reinterpret_cast<PCRE2_SPTR>(p->subject), p->s_length, offset,
                             p->exec_flags | extra_flags, match_data, match_context);
  p->rc = rc;
  if (p->ovector != nullptr) {
    FREE(p->ovector);
    p->ovector = nullptr;
  }
  p->ovecsize = 0;

  if (rc >= 0) {
    uint32_t const count = pcre2_get_ovector_count(match_data);
    if (count > std::numeric_limits<PCRE2_SIZE>::max() / 2) {
      error("PCRE2 capture vector is too large.\n");
    }
    p->ovecsize = static_cast<PCRE2_SIZE>(count) * 2;
    p->ovector = static_cast<PCRE2_SIZE *>(DCALLOC(static_cast<size_t>(p->ovecsize),
                                                    sizeof(PCRE2_SIZE), TAG_TEMPORARY,
                                                    "pcre_exec_at: ovector"));
    PCRE2_SIZE const *source = pcre2_get_ovector_pointer(match_data);
    memcpy(p->ovector, source, static_cast<size_t>(p->ovecsize) * sizeof(PCRE2_SIZE));
  }

  return rc;
}

static int pcre_local_exec(pcre_t *p) { return pcre_exec_at(p, 0, 0); }

static int pcre_prepare(pcre_t *p) {
  pcre_cache_bucket_t *entry = pcre_get_cached_pattern(&pcre_cache, p->pattern, p->compile_flags);
  if (entry != nullptr) {
    p->cache_entry = entry;
    p->re = entry->compiled_pattern;
  } else if (pcre_local_compile(p) != nullptr) {
    if (pcre_cache_pattern(&pcre_cache, p, p->pattern, p->compile_flags) != 0) {
      // The run retains ownership when the cache cannot accept the code.
      p->cache_entry = nullptr;
    }
  }

  if (p->re == nullptr) {
    return -1;
  }
  pcre_load_pattern_info(p);
  return 1;
}

static int pcre_magic(pcre_t *p) {
  if (pcre_prepare(p) < 0) {
    return -1;
  }
  pcre_local_exec(p);
  return 1;
}

static int pcre_query_match(pcre_t *p) {
  if (p->rc == PCRE2_ERROR_NOMATCH) {
    return 0;
  }
  if (p->rc <= 0) {
    pcre_throw_match_error(p->rc);
  }
  return 1;
}

static void pcre_free_match_arrays(std::vector<array_t *> *matches) {
  for (array_t *match : *matches) {
    if (match != nullptr) {
      free_array(match);
    }
  }
  matches->clear();
}

static std::vector<array_t *> pcre_match_all(const char *subject, size_t subject_len,
                                             const char *pattern, int pcre_flags) {
  pcre_t *run = static_cast<pcre_t *>(DCALLOC(1, sizeof(pcre_t), TAG_TEMPORARY,
                                              "pcre_match_all: run"));
  run->pattern = pattern;
  run->subject = subject;
  run->s_length = pcre_checked_length(subject_len);
  run->compile_flags = compute_compile_options(pcre_flags);
  run->exec_flags = compute_exec_options(pcre_flags);

  DEFER { pcre_free_memory(run); };

  if (pcre_prepare(run) < 0) {
    error("PCRE compilation failed at offset %zu: %s\n", static_cast<size_t>(run->erroffset),
          run->error);
  }

  std::vector<array_t *> matches;
  SCOPE_FAIL { pcre_free_match_arrays(&matches); };

  PCRE2_SIZE const subject_length = pcre_subject_length(run);
  PCRE2_SIZE offset = 0;
  uint32_t extra_flags = 0;
  bool retry_after_empty_match = false;
  while (offset <= subject_length) {
    int const rc = pcre_exec_at(run, offset, extra_flags);
    if (rc == PCRE2_ERROR_NOMATCH) {
      if (!retry_after_empty_match || offset >= subject_length) {
        break;
      }
      offset = pcre_advance_after_empty_match(run->subject, subject_length, offset);
      extra_flags = 0;
      retry_after_empty_match = false;
      continue;
    }
    if (rc < 0 || rc == 0) {
      pcre_throw_match_error(rc);
    }

    array_t *match_array = allocate_array(rc);
    bool transferred = false;
    SCOPE_FAIL {
      if (!transferred) {
        free_array(match_array);
      }
    };
    matches.push_back(match_array);
    transferred = true;

    for (int i = 0; i < rc; ++i) {
      PCRE2_SIZE const start_offset = run->ovector[2 * i];
      PCRE2_SIZE const end_offset = run->ovector[2 * i + 1];
      size_t const length = pcre_capture_participated(run, i)
                                ? static_cast<size_t>(end_offset - start_offset)
                                : 0;
      if (length > std::numeric_limits<unsigned int>::max()) {
        error("PCRE capture is too large.\n");
      }
      char const *start = length == 0 ? "" : run->subject + start_offset;
      char *match_str = new_string(static_cast<unsigned int>(length), "pcre_match_all");
      if (length > 0) {
        memcpy(match_str, start, length);
      }
      match_str[length] = '\0';
      match_array->item[i].type = T_STRING;
      match_array->item[i].subtype = STRING_MALLOC;
      match_array->item[i].u.string = match_str;
    }

    PCRE2_SIZE const match_start = run->ovector[0];
    PCRE2_SIZE const match_end = run->ovector[1];
    if (match_end > match_start) {
      offset = match_end;
      extra_flags = 0;
      retry_after_empty_match = false;
    } else {
      if (match_end >= subject_length) {
        break;
      }
      offset = match_end;
      extra_flags = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
      retry_after_empty_match = true;
    }
  }

  return matches;
}

static int pcre_match_single(svalue_t *str, const char *pattern, int pcre_flags) {
  pcre_t *run;
  int ret;

  run = (pcre_t *)DCALLOC(1, sizeof(pcre_t), TAG_TEMPORARY, "pcre_match_single : run");
  run->ovector = nullptr;
  run->ovecsize = 0;
  run->pattern = pattern;
  run->subject = str->u.string;
  run->s_length = pcre_checked_length(SVALUE_STRLEN(str));
  run->compile_flags = compute_compile_options(pcre_flags);
  run->exec_flags = compute_exec_options(pcre_flags);

  DEFER { pcre_free_memory(run); };

  if (pcre_magic(run) < 0) {
    error("PCRE compilation failed at offset %zu: %s\n",
          static_cast<size_t>(run->erroffset), run->error);
  }

  ret = pcre_query_match(run);

  return ret;
}

static array_t *pcre_match(array_t *v, const char *pattern, int flag, int pcre_flags) {
  pcre_t *run;
  array_t *ret;
  svalue_t *sv1, *sv2;
  char *res;
  int num_match, size, match = !(flag & 2);

  if (!(size = v->size)) {
    return &the_null_array;
  }

  run = static_cast<pcre_t *>(DCALLOC(1, sizeof(pcre_t), TAG_TEMPORARY,
                                       "pcre_match : run"));
  run->pattern = pattern;
  run->compile_flags = compute_compile_options(pcre_flags);
  run->exec_flags = compute_exec_options(pcre_flags);

  DEFER { pcre_free_memory(run); };

  if (pcre_prepare(run) < 0) {
    error("PCRE compilation failed at offset %zu: %s\n",
          static_cast<size_t>(run->erroffset), run->error);
  }

  res = static_cast<char *>(DMALLOC(size, TAG_TEMPORARY, "pcre_match: res"));
  DEFER { FREE(res); };
  sv1 = v->item + size;
  num_match = 0;

  while (size--) {
    if ((--sv1)->type != T_STRING) {
      res[size] = 0;
      continue;
    }

    run->subject = sv1->u.string;
    run->s_length = pcre_checked_length(SVALUE_STRLEN(sv1));
    pcre_local_exec(run);

    if (pcre_query_match(run) != match) {
      res[size] = 0;
      continue;
    }

    res[size] = 1;
    num_match++;
  }

  flag &= 1;
  ret = allocate_empty_array(num_match << flag);
  sv2 = ret->item + (num_match << flag);
  size = v->size;

  while (size--) {
    if (res[size]) {
      if (flag) {
        (--sv2)->type = T_NUMBER;
        sv2->u.number = size + 1;
      }

      (--sv2)->type = T_STRING;

      sv1 = v->item + size;
      *sv2 = *sv1;

      if (sv1->subtype & STRING_COUNTED) {
        INC_COUNTED_REF(sv1->u.string);
        md_record_ref_journal(PTR_TO_NODET(sv1->u.string), true,
                              MSTR_REF(sv1->u.string), __CURRENT_FILE_LINE__);
        ADD_STRING(MSTR_SIZE(sv1->u.string));
      }

      if (!--num_match) {
        break;
      }
    }
  }
  return ret;
}

/* This is mostly copy/paste from reg_assoc, some parts are changed
 * TODO: rewrite with new logic
 */
static array_t *pcre_assoc(svalue_t *str, array_t *pat, array_t *tok, svalue_t *def, int pcre_flags) {
  int i;
  size_t size;
  const char *tmp;
  array_t *ret;

  if ((size = pat->size) != tok->size) {
    error("Pattern and token array size must be identical.\n");
  }

  for (i = 0; i < size; i++)
    if (pat->item[i].type != T_STRING) {
      error("Non-string found in pattern array.\n");
    }

  ret = allocate_empty_array(2);

  if (size) {
    pcre_t **rgpp;
    struct RegMatch {
      int tok_i;
      const char *begin, *end;
      struct RegMatch *next;
    } *rmp = (struct RegMatch *)nullptr, *rmph = (struct RegMatch *)nullptr;
    int num_match = 0;
    svalue_t *sv1, *sv2, *sv;
    int regindex;
    pcre_t *tmpreg;
    PCRE2_SIZE laststart;

    rgpp = (pcre_t **)DCALLOC(size, sizeof(pcre_t *), TAG_TEMPORARY, "pcre_assoc : rgpp");
    bool completed = false;
    SCOPE_FAIL {
      if (completed) {
        return;
      }
      for (int cleanup_i = 0; cleanup_i < size; ++cleanup_i) {
        if (rgpp[cleanup_i] != nullptr) {
          pcre_free_memory(rgpp[cleanup_i]);
        }
      }
      FREE(rgpp);
      while (rmph != nullptr) {
        auto *cleanup_match = rmph;
        rmph = rmph->next;
        FREE(reinterpret_cast<char *>(cleanup_match));
      }
      free_array(ret);
    };

    for (i = 0; i < size; i++) {
      rgpp[i] = (pcre_t *)DCALLOC(1, sizeof(pcre_t), TAG_TEMPORARY, "pcre_assoc : rgpp[i]");
      rgpp[i]->pattern = pat->item[i].u.string;
      rgpp[i]->compile_flags = compute_compile_options(pcre_flags);
      rgpp[i]->exec_flags = compute_exec_options(pcre_flags);

      if (pcre_prepare(rgpp[i]) < 0) {
        const char *rerror = rgpp[i]->error;
        size_t const offset = static_cast<size_t>(rgpp[i]->erroffset);

        error("PCRE compilation failed at offset %zu: %s\n", offset, rerror);
      }
    }

    tmp = str->u.string;
    PCRE2_SIZE const totalsize = pcre_checked_length(SVALUE_STRLEN(str));
    PCRE2_SIZE used = 0;
    while (*tmp) {
      laststart = 0;
      regindex = -1;

      for (i = 0; i < size; i++) {
        rgpp[i]->subject = tmp;
        rgpp[i]->s_length = totalsize - used;

        pcre_local_exec(tmpreg = rgpp[i]);

        if (pcre_query_match(tmpreg)) {
          PCRE2_SIZE curr_temp_sz;

          curr_temp_sz = totalsize - used - tmpreg->ovector[0];
          if (!tmpreg->ovector[0]) {
            regindex = i;
            break;
          }
          if (laststart < curr_temp_sz) {
            laststart = curr_temp_sz;
            regindex = i;
          }
        }
      }

      if (regindex >= 0) {
        if (num_match == (std::numeric_limits<int>::max() - 1) / 2) {
          error("PCRE association result is too large.\n");
        }
        const char *rmpb_tmp, *rmpe_tmp;
        num_match++;

        if (rmp) {
          rmp->next = (struct RegMatch *)DMALLOC(sizeof(struct RegMatch), TAG_TEMPORARY,
                                                 "pcre_assoc : rmp->next");
          rmp = rmp->next;
        } else
          rmph = rmp = (struct RegMatch *)DMALLOC(sizeof(struct RegMatch), TAG_TEMPORARY,
                                                  "pcre_assoc : rmp");

        tmpreg = rgpp[regindex];

        rmpb_tmp = tmp + tmpreg->ovector[0];
        rmpe_tmp = tmp + tmpreg->ovector[1];

        rmp->begin = rmpb_tmp;
        rmp->end = tmp = rmpe_tmp;
        used += tmpreg->ovector[1];
        rmp->tok_i = regindex;
        rmp->next = (struct RegMatch *)nullptr;
      } else {
        break;
      }

      if (rmp->begin == tmp && (!*++tmp)) {
        break;
      }
    }

    sv = ret->item;
    sv->type = T_ARRAY;
    sv1 = (sv->u.arr = allocate_empty_array(2 * num_match + 1))->item;

    sv++;
    sv->type = T_ARRAY;
    sv2 = (sv->u.arr = allocate_empty_array(2 * num_match + 1))->item;

    rmp = rmph;

    tmp = str->u.string;

    while (num_match--) {
      char *svtmp;
      size_t const prefix_length = static_cast<size_t>(rmp->begin - tmp);
      if (prefix_length > std::numeric_limits<unsigned int>::max()) {
        error("PCRE association result is too large.\n");
      }

      sv1->type = T_STRING;
      sv1->subtype = STRING_MALLOC;
      sv1->u.string = svtmp =
          new_string(static_cast<unsigned int>(prefix_length), "pcre_assoc : sv1");
      memcpy(svtmp, tmp, prefix_length);
      svtmp[prefix_length] = 0;

      sv1++;
      assign_svalue_no_free(sv2++, def);
      tmp += prefix_length;

      size_t const match_length = static_cast<size_t>(rmp->end - rmp->begin);
      if (match_length > std::numeric_limits<unsigned int>::max()) {
        error("PCRE association result is too large.\n");
      }

      sv1->type = T_STRING;
      sv1->subtype = STRING_MALLOC;
      sv1->u.string = svtmp =
          new_string(static_cast<unsigned int>(match_length), "pcre_assoc : sv1");
      memcpy(svtmp, tmp, match_length);
      svtmp[match_length] = 0;

      tmp += match_length;

      sv1++;
      assign_svalue_no_free(sv2++, &tok->item[rmp->tok_i]);
      rmp = rmp->next;
    }

    sv1->type = T_STRING;
    sv1->subtype = STRING_MALLOC;
    sv1->u.string = string_copy(tmp, "pcre_assoc");
    assign_svalue_no_free(sv2, def);

    for (i = 0; i < size; i++) {
      pcre_free_memory(rgpp[i]);
    }

    FREE(rgpp);

    while ((rmp = rmph)) {
      rmph = rmp->next;
      FREE((char *)rmp);
    }
    completed = true;
    return ret;
  }
  svalue_t *temp;
  svalue_t *sv;

  (sv = ret->item)->type = T_ARRAY;
  temp = (sv->u.arr = allocate_empty_array(1))->item;
  assign_svalue_no_free(temp, str);
  sv = &ret->item[1];
  sv->type = T_ARRAY;
  assign_svalue_no_free((sv->u.arr = allocate_empty_array(1))->item, def);
  return ret;
}

static array_t *pcre_get_substrings(pcre_t *run, bool include_names) {
  if (run->rc <= 0) {
    error("PCRE substring extraction requires a successful match.\n");
  }

  size_t const base_size = static_cast<size_t>(run->rc - 1);
  if (base_size > std::numeric_limits<unsigned int>::max() ||
      (include_names && base_size == std::numeric_limits<unsigned int>::max())) {
    error("Too many PCRE capture groups.\n");
  }
  unsigned int const ret_size = static_cast<unsigned int>(base_size + (include_names ? 1 : 0));
  array_t *ret = allocate_empty_array(ret_size);
  mapping_t *name_map = nullptr;
  bool name_map_published = false;
  bool completed = false;
  SCOPE_FAIL {
    if (!completed) {
      if (!name_map_published && name_map != nullptr) {
        free_mapping(name_map);
      }
      free_array(ret);
    }
  };

  for (size_t i = 1; i <= base_size; ++i) {
    PCRE2_SIZE start = 0;
    PCRE2_SIZE end = 0;
    if (pcre_capture_participated(run, static_cast<int>(i))) {
      start = run->ovector[i * 2];
      end = run->ovector[i * 2 + 1];
    }
    size_t const length = static_cast<size_t>(end - start);
    if (length > std::numeric_limits<unsigned int>::max()) {
      error("PCRE capture is too large.\n");
    }
    char *match = new_string(static_cast<unsigned int>(length), "pcre get substrings");
    if (length > 0) {
      memcpy(match, run->subject + start, length);
    }
    match[length] = '\0';
    ret->item[i - 1].type = T_STRING;
    ret->item[i - 1].subtype = STRING_MALLOC;
    ret->item[i - 1].u.string = match;
  }

  if (include_names) {
    name_map = allocate_mapping(run->namecount);
    for (uint32_t name_index = 0; name_index < run->namecount; ++name_index) {
      PCRE2_SPTR entry = run->name_table + name_index * run->name_entry_size;
      uint32_t const group = (static_cast<uint32_t>(entry[0]) << 8) |
                             static_cast<uint32_t>(entry[1]);
      if (group == 0 || group > base_size) {
        continue;
      }

      uint32_t const ovec_index = group * 2;
      if (static_cast<PCRE2_SIZE>(ovec_index + 1) >= run->ovecsize ||
          !pcre_capture_participated(run, static_cast<int>(group))) {
        continue;
      }

      svalue_t key;
      key.type = T_STRING;
      key.subtype = STRING_SHARED;
      key.u.string = make_shared_string(reinterpret_cast<const char *>(entry + 2));
      svalue_t *slot = find_for_insert(name_map, &key, 1);
      free_string(key.u.string);
      if (slot != nullptr) {
        assign_svalue(slot, &ret->item[group - 1]);
      }
    }

    ret->item[ret_size - 1].type = T_MAPPING;
    ret->item[ret_size - 1].subtype = 0;
    ret->item[ret_size - 1].u.map = name_map;
    name_map_published = true;
  }

  completed = true;
  return ret;
}

static char *pcre_get_replace(pcre_t *run, array_t *replacements) {
  size_t ret_sz = 0;
  auto const segments = pcre_build_replace_segments(run, replacements, &ret_sz);
  auto const configured_max = CONFIG_INT(__MAX_STRING_LENGTH__);
  size_t const max_string_length = configured_max > 0 ? static_cast<size_t>(configured_max) : 0;

  if (ret_sz > max_string_length || ret_sz > std::numeric_limits<unsigned int>::max()) {
    error("Maximum string length exceeded in PCRE replacement.\n");
  }

  char *ret = new_string(static_cast<unsigned int>(ret_sz), "pcre get replace");
  size_t ret_pos = 0;
  size_t subject_pos = 0;
  size_t const match_start = static_cast<size_t>(run->ovector[0]);
  size_t const match_end = static_cast<size_t>(run->ovector[1]);

  if (match_start > 0) {
    memcpy(ret, run->subject, match_start);
    ret_pos = match_start;
    subject_pos = match_start;
  }

  for (auto const &segment : segments) {
    size_t const prefix_length = static_cast<size_t>(segment.start) - subject_pos;
    if (prefix_length > 0) {
      memcpy(ret + ret_pos, run->subject + subject_pos, prefix_length);
      ret_pos += prefix_length;
    }
    if (segment.replacement_length > 0) {
      memcpy(ret + ret_pos, segment.replacement, segment.replacement_length);
      ret_pos += segment.replacement_length;
    }
    subject_pos = static_cast<size_t>(segment.end);
  }

  size_t const matched_tail_length = static_cast<size_t>(match_end) - subject_pos;
  if (matched_tail_length > 0) {
    memcpy(ret + ret_pos, run->subject + subject_pos, matched_tail_length);
    ret_pos += matched_tail_length;
  }

  size_t const suffix_length = static_cast<size_t>(run->s_length) - match_end;
  if (suffix_length > 0) {
    memcpy(ret + ret_pos, run->subject + match_end, suffix_length);
    ret_pos += suffix_length;
  }

  ret[ret_pos] = '\0';
  return ret;
}

static void pcre_destroy_cache_entry(pcre_cache_bucket_t *entry) {
  if (entry == nullptr) {
    return;
  }
  pcre2_code_free(entry->compiled_pattern);
  free_string(entry->pattern);
  FREE(entry);
}

static void pcre_untrack_detached_cache_entry(pcre_cache_bucket_t *entry) {
  pcre_cache_bucket_t **link = &pcre_detached_cache;
  while (*link != nullptr && *link != entry) {
    link = &(*link)->detached_next;
  }
  if (*link == nullptr) {
    error("PCRE detached cache entry is not rooted.\n");
  }
  *link = entry->detached_next;
  entry->detached_next = nullptr;
}

static void pcre_release_cache_entry(pcre_cache_bucket_t *entry) {
  if (entry == nullptr) {
    return;
  }
  if (entry->borrowers == 0) {
    error("PCRE cache borrower underflow.\n");
  }
  --entry->borrowers;
  if (entry->detached && entry->borrowers == 0) {
    pcre_untrack_detached_cache_entry(entry);
    pcre_destroy_cache_entry(entry);
  }
}

static void pcre_free_memory(pcre_t *p) {
  if (p == nullptr) {
    return;
  }
  if (p->ovector != nullptr) {
    FREE(p->ovector);
  }
  if (p->cache_entry != nullptr) {
    pcre_release_cache_entry(p->cache_entry);
  } else if (p->re != nullptr) {
    pcre2_code_free(p->re);
  }
  FREE(p);
}

static void pcre_detach_cache_entry(struct pcre_cache_t *table, unsigned int bucket,
                                    pcre_cache_bucket_t *previous,
                                    pcre_cache_bucket_t *entry) {
  if (previous == nullptr) {
    table->buckets[bucket] = entry->next;
  } else {
    previous->next = entry->next;
  }
  entry->next = nullptr;
  entry->detached = 1;
  --pcrecachesize;
  entry->detached_next = nullptr;
  if (entry->borrowers == 0) {
    pcre_destroy_cache_entry(entry);
  } else {
    entry->detached_next = pcre_detached_cache;
    pcre_detached_cache = entry;
  }
}

// Caching functions. A run borrows a cache entry until pcre_free_memory(), so
// an eviction cannot free code that is still executing or serving a callback.
static int pcre_cache_pattern(struct pcre_cache_t *table, pcre_t *run,
                              const char *pattern, uint32_t compile_flags) {
  if (run == nullptr || run->re == nullptr) {
    return -1;
  }

  const auto *shared_pattern = make_shared_string(pattern);
  unsigned int const bucket =
      (HASH(BLOCK(shared_pattern)) ^ compile_flags) % PCRE_CACHE_SIZE;
  pcre_cache_bucket_t *node = table->buckets[bucket];
  while (node != nullptr) {
    if (shared_pattern == node->pattern && node->compile_flags == compile_flags) {
      pcre2_code_free(run->re);
      run->re = node->compiled_pattern;
      run->cache_entry = node;
      ++node->borrowers;
      free_string(shared_pattern);
      return 0;
    }
    node = node->next;
  }

  size_t code_size = 0;
  if (pcre2_pattern_info(run->re, PCRE2_INFO_SIZE, &code_size) != 0) {
    free_string(shared_pattern);
    return -1;
  }

  if (pcrecachesize >= 2 * PCRE_CACHE_SIZE) {
    for (unsigned int evict_bucket = 0; evict_bucket < PCRE_CACHE_SIZE; ++evict_bucket) {
      pcre_cache_bucket_t *previous = nullptr;
      pcre_cache_bucket_t *tail = table->buckets[evict_bucket];
      if (tail == nullptr) {
        continue;
      }
      while (tail->next != nullptr) {
        previous = tail;
        tail = tail->next;
      }
      pcre_detach_cache_entry(table, evict_bucket, previous, tail);
      break;
    }
  }

  node = static_cast<pcre_cache_bucket_t *>(DCALLOC(1, sizeof(pcre_cache_bucket_t),
                                                     TAG_PCRE_CACHE,
                                                     "pcre_cache_pattern: node"));
  if (node == nullptr) {
    free_string(shared_pattern);
    return -1;
  }
  node->pattern = shared_pattern;
  node->compile_flags = compile_flags;
  node->compiled_pattern = run->re;
  node->size = code_size;
  node->borrowers = 1;
  node->detached = 0;
  node->detached_next = nullptr;
  node->next = table->buckets[bucket];
  table->buckets[bucket] = node;
  ++pcrecachesize;
  run->cache_entry = node;
  return 0;
}

static pcre_cache_bucket_t *pcre_get_cached_pattern(struct pcre_cache_t *table,
                                                     const char *pattern,
                                                     uint32_t compile_flags) {
  const auto *shared_pattern = make_shared_string(pattern);
  unsigned int const bucket =
      (HASH(BLOCK(shared_pattern)) ^ compile_flags) % PCRE_CACHE_SIZE;
  pcre_cache_bucket_t *node = table->buckets[bucket];
  pcre_cache_bucket_t *previous = nullptr;

  while (node != nullptr) {
    if (shared_pattern == node->pattern && node->compile_flags == compile_flags) {
      if (previous != nullptr) {
        previous->next = node->next;
        node->next = table->buckets[bucket];
        table->buckets[bucket] = node;
      }
      ++node->borrowers;
      free_string(shared_pattern);
      return node;
    }
    previous = node;
    node = node->next;
  }
  free_string(shared_pattern);
  return nullptr;
}

static mapping_t *pcre_get_cache() {
  int size = 0, i;
  mapping_t *ret;
  struct pcre_cache_bucket_t *node;

  // Calculate size for mapping
  for (i = 0; i < PCRE_CACHE_SIZE; i++) {
    if (pcre_cache.buckets[i] != nullptr) {
      node = pcre_cache.buckets[i];

      while (node) {
        node = node->next;
        size++;
      }
    }
  }

  ret = allocate_mapping(size);

  for (i = 0; i < PCRE_CACHE_SIZE; i++) {
    if (pcre_cache.buckets[i] != nullptr) {
      node = pcre_cache.buckets[i];

      while (node) {
        size_t keylen = strlen(node->pattern) + 16;
        char *key = (char *)DMALLOC(keylen, TAG_TEMPORARY, "pcre_cache key");
        snprintf(key, keylen, "%s|0x%x", node->pattern, node->compile_flags);
        add_mapping_pair(ret, key, node->size);
        FREE(key);
        node = node->next;
      }
    }
  }
  return ret;
}

#ifdef DEBUGMALLOC_EXTENSIONS
static void mark_pcre_cache_chain(pcre_cache_bucket_t *node) {
  while (node != nullptr) {
    DO_MARK(node, TAG_PCRE_CACHE);
    EXTRA_REF(BLOCK(node->pattern))++;
    node = node->next;
  }
}

void mark_pcre_cache() {
  for (auto &bucket : pcre_cache.buckets) {
    mark_pcre_cache_chain(bucket);
  }

  for (auto *node = pcre_detached_cache; node != nullptr; node = node->detached_next) {
    DO_MARK(node, TAG_PCRE_CACHE);
    EXTRA_REF(BLOCK(node->pattern))++;
  }
}
#endif

void f_pcre_match_all() {
  array_t *v;

  if (st_num_arg < 2 || st_num_arg > 3) {
    error("pcre_match_all() requires 2 or 3 arguments\n");
  }

  int pcre_flags = 0;
  if (st_num_arg >= 3) {
    if (sp->type != T_NUMBER) {
      error("Bad argument 3 to pcre_match_all()\n");
    }
    pcre_flags = (sp--)->u.number;
    st_num_arg--;
  }

  if ((sp - 1)->type != T_STRING || sp->type != T_STRING) {
    error("Bad subject or pattern argument to pcre_match_all()\n");
  }

  const auto *pattern = (sp)->u.string;
  const auto *subject = (sp - 1)->u.string;
  auto subject_len = SVALUE_STRLEN(sp - 1);

  auto matches = pcre_match_all(subject, subject_len, pattern, pcre_flags);
  SCOPE_FAIL { pcre_free_match_arrays(&matches); };

  if (matches.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    error("PCRE match_all result is too large.\n");
  }

  pop_2_elems();

  v = allocate_array(static_cast<int>(matches.size()));
  for (size_t i = 0; i < matches.size(); ++i) {
    array_t *match_array = matches[i];
    if (match_array == nullptr) {
      error("Invalid PCRE match_all result.\n");
    }
    v->item[i].type = T_ARRAY;
    v->item[i].u.arr = match_array;
    matches[i] = nullptr;
  }

  matches.clear();
  push_refed_array(v);
}
