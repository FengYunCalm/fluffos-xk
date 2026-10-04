/* PCRE2 match offsets are byte offsets into the subject. Each capture uses
 * two entries in the copied ovector: start and end. PCRE2_UNSET marks an
 * optional capture that did not participate. */
#ifndef PACKAGS_PCRE_H
#define PACKAGS_PCRE_H

#define PCRE_CACHE_SIZE 256
#ifndef PCRE2_CODE_UNIT_WIDTH
#define PCRE2_CODE_UNIT_WIDTH 8
#endif

#include <cstddef>
#include <cstdint>

#include <pcre2.h>

struct pcre_cache_bucket_t;

typedef struct {
  pcre2_code *re;
  char error[256];
  const char *pattern;
  const char *subject;
  PCRE2_SPTR name_table;
  PCRE2_SIZE s_length;
  PCRE2_SIZE erroffset;
  PCRE2_SIZE start_offset;
  uint32_t namecount;
  uint32_t name_entry_size;
  uint32_t compile_flags;
  uint32_t exec_flags;
  PCRE2_SIZE *ovector;
  PCRE2_SIZE ovecsize;
  int rc;
  struct pcre_cache_bucket_t *cache_entry;
} pcre_t;

struct pcre_cache_bucket_t {
  pcre2_code *compiled_pattern;
  const char *pattern;
  uint32_t compile_flags;
  size_t size;
  unsigned int borrowers;
  int detached;
  struct pcre_cache_bucket_t *next;
  struct pcre_cache_bucket_t *detached_next;
};

struct pcre_cache_t {
  struct pcre_cache_bucket_t *buckets[PCRE_CACHE_SIZE];
};

#ifdef DEBUGMALLOC_EXTENSIONS
void mark_pcre_cache();
#endif

#endif
