// PCRE efun flags (kept in high bits to avoid legacy flag collisions).
// The numeric contract is stable; pcre.cc maps these bits to PCRE2 options.
#ifndef SRC_INCLUDE_PCRE_FLAGS_H_
#define SRC_INCLUDE_PCRE_FLAGS_H_

#define PCRE_DEFAULT 0

// Compile-time options
#define PCRE_I (1 << 16)  // PCRE2_CASELESS
#define PCRE_M (1 << 17)  // PCRE2_MULTILINE
#define PCRE_S (1 << 18)  // PCRE2_DOTALL
#define PCRE_U (1 << 19)  // PCRE2_UNGREEDY
#define PCRE_X (1 << 20)  // PCRE2_EXTENDED

// Exec-time option
#define PCRE_A (1 << 21)  // PCRE2_ANCHORED

#endif  // SRC_INCLUDE_PCRE_FLAGS_H_
