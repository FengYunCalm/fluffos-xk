// LPC tokenizer driven by lpc-grammar.json (generated from grammar.y /
// src/compiler/internal/lex.cc by tools/lpc-syntax/generate_ebnf.py --
// regenerate with the generate_ebnf CMake target; never hand-edit the JSON).
//
// Token kinds: comment, directive, string, textblock, char, number,
// keyword, type, modifier, efunkw, identifier, operator, punctuation,
// functional, whitespace, unknown.
//
// A spanning token that reaches end-of-input without its terminator
// (string missing its closing '"', char literal missing its closing
// "'", '/*' comment missing its '*/', text block missing its terminator
// line) is emitted with `unterminated: true`. Driver ground truth
// (src/compiler/internal/lex.cc): every one of these is a hard lexerror at
// <<EOF>> -- the driver never assigns such a file a meaning, so consumers
// that rewrite source (format.mjs) must refuse it.

import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));
export const grammar = JSON.parse(readFileSync(join(here, 'lpc-grammar.json'), 'utf8'));

const KEYWORDS = new Set(grammar.keywords);
const TYPES = new Set(grammar.typeKeywords);
const MODIFIERS = new Set(grammar.modifierKeywords);
// Longest-match order comes pre-sorted from the generator.
const OPERATORS = grammar.operators;
const PUNCT = new Set(grammar.punctuation);

const isIdentStart = (c) => /[A-Za-z_]/.test(c);
const isIdentChar = (c) => /[A-Za-z0-9_]/.test(c);
const isDigit = (c) => /[0-9]/.test(c);
const isLexWs = (c) => c === ' ' || c === '\t' || c === '\r' || c === '\n' || c === '\v' || c === '\f';

// True when `src[i]` is a '(' immediately (modulo lex.cc's whitespace class:
// space/tab/CR/LF/VT/FF) followed by "::" -- i.e. a bare parent-call guard
// like `(::name(...))`, not a functional-literal open. Mirrors lex.cc's
// `"("{WS}*"::"` rule, which LPC_YYLESS(1)s back to just '(' so "::" is
// re-scanned as L_COLON_COLON.
function isParentCallOpenParen(src, i) {
  let j = i + 1;
  while (j < src.length && isLexWs(src[j])) j++;
  return src[j] === ':' && src[j + 1] === ':';
}

// Skip one quoted string or character span while scanning preprocessor
// directives. The driver treats quoted content as opaque for comment search.
export function skipStringSpan(src, i, quote = '"') {
  let j = i + 1;
  while (j < src.length && src[j] !== quote) {
    if (src[j] === '\\') j++;
    j++;
  }
  return Math.min(j + 1, src.length);
}

export function skipCharSpan(src, i) {
  // Mirror lex.cc's char-literal grammar EXACTLY: after the opening
  // quote, the body is ONE unit -- either a single raw byte (any byte,
  // *including a literal quote*, per <SC_CHAR_BODY>[^\\]) or one escape
  // sequence -- and then a closing quote is required. Escapes are
  // variable-length (`'\x41'` hex, `'\101'` octal), which is why this
  // can't assume a fixed 2-char width (the old fixed width made
  // directive scanning swallow following source), but it must NOT
  // "scan to the next quote" like skipStringSpan either: that misreads
  // the valid MudOS-ism `'''` (quote char, body is a raw `'`) as an
  // empty `''` plus a stray `'` that then opens a bogus literal running
  // to the next quote anywhere on the line -- in one real mudlib that
  // next quote sat inside a trailing `//'` comment, and the formatter
  // merged the case label, the comment, and the following statement
  // into one line, silently deleting the statement on recompile.
  let j = i + 1;
  if (j >= src.length) return j;
  if (src[j] === '\\') {
    j++; // the escape introducer; now classify per lex.cc's rules
    const e = src[j];
    if (e === undefined) return j;
    if (e === 'x') {
      // "\\x"[0-9a-fA-F]+ (or bare "\\x", an error the driver still
      // consumes as just the two chars before the close-quote check)
      j++;
      while (j < src.length && /[0-9A-Fa-f]/.test(src[j])) j++;
    } else if (e >= '0' && e <= '7') {
      // "\\"[0-7]+ octal, maximal munch
      while (j < src.length && src[j] >= '0' && src[j] <= '7') j++;
    } else if (e === '\r' && src[j + 1] === '\n') {
      j += 2; // "\\\r\n" escaped newline
    } else {
      j++; // "\\." -- simple/unknown escapes are exactly one char
    }
  } else {
    j++; // one raw body byte -- including a literal `'` or newline
  }
  // Closing quote. If it's missing the driver reports an error and
  // pushes the offending byte back for the next scan (LPC_YYLESS(0));
  // mirror that by ending the span here so the byte re-lexes normally.
  if (j < src.length && src[j] === "'") j++;
  return j;
}

// Bytes consumed by a C phase-2 splice at `k`, or 0.
function backslashNewlineLen(src, k) {
  if (src[k] !== '\\') return 0;
  if (src[k + 1] === '\n') return 2;
  if (src[k + 1] === '\r' && src[k + 2] === '\n') return 3;
  return 0;
}

function peekAfterSplices(src, k) {
  for (;;) {
    const n = backslashNewlineLen(src, k);
    if (!n) return k;
    k += n;
  }
}

// A directive's raw '\n' terminator, scanning from `j` (inside the
// directive, past its '#'). Comments are recognized after splices (C
// phase 2 then 3): a block comment opened on a directive line is
// invisible whitespace and may close on a LATER physical line
// (docs/lpc/preprocessor/) -- the newline(s) inside it don't end the
// directive; keep scanning past the comment's close for the directive's
// REAL terminating newline instead. A '//' comment runs to the logical
// newline (a trailing '\' continues it). A string/char literal's own
// comment-shaped content must not be misread as a real comment start,
// hence routing through skipStringSpan/skipCharSpan.
function directiveLineEnd(src, j) {
  let k = j;
  while (k < src.length && src[k] !== '\n') {
    const c = src[k];
    // String/char spans are only skipped WITHIN the physical line: the
    // driver's directive capture is strictly line-based (quotes never
    // extend a directive -- an unterminated '"' or a bare "'" in a
    // #define body is legal and inert), so a span that would run past
    // the newline means the directive ends at that newline instead.
    // Without the bound, `#define Q it'` swallowed the entire next
    // source line into the directive token, and `#define BAD "abc`
    // swallowed everything up to the next '"' anywhere in the file. (A
    // backslash-newline inside the span still continues the directive:
    // the caller's '\'-continuation check sees the '\' before this
    // returned newline, matching the driver's splice-first folding.)
    if (c === '"' || c === "'") {
      const e = c === "'" ? skipCharSpan(src, k) : skipStringSpan(src, k);
      const nl = src.indexOf('\n', k);
      if (nl >= 0 && nl < e) return nl;
      k = e;
      continue;
    }
    if (c === '/') {
      const after = peekAfterSplices(src, k + 1);
      if (src[after] === '*') {
        let p = after + 1;
        while (p < src.length) {
          const n = backslashNewlineLen(src, p);
          if (n) { p += n; continue; }
          if (src[p] === '*') {
            const close = peekAfterSplices(src, p + 1);
            if (src[close] === '/') { k = close + 1; break; }
          }
          p++;
        }
        if (p >= src.length) return src.length;
        continue;
      }
      if (src[after] === '/') {
        let p = after + 1;
        while (p < src.length) {
          const n = backslashNewlineLen(src, p);
          if (n) { p += n; continue; }
          if (src[p] === '\n') { k = p; break; }
          p++;
        }
        if (p >= src.length) return src.length;
        continue;
      }
    }
    k++;
  }
  return k;
}

export function tokenize(src) {
  const toks = [];
  let i = 0;
  let line = 1;
  let col = 1;
  let atLineStart = true;
  // Track absolute character offsets independently from the scanner index so
  // token spans remain stable for formatter diagnostics.
  let pos = 0;

  const push = (kind, text) => {
    toks.push({ kind, text, line, col, start: pos, end: pos + text.length });
    pos += text.length;
    for (const ch of text) {
      if (ch === '\n') { line++; col = 1; } else { col++; }
    }
    if (kind !== 'whitespace' && kind !== 'comment') atLineStart = false;
  };

  const readWhile = (pred) => {
    let j = i;
    while (j < src.length && pred(src[j])) j++;
    return src.slice(i, j);
  };

  while (i < src.length) {
    const c = src[i];

    // whitespace (newline re-arms directive detection)
    if (c === ' ' || c === '\t' || c === '\r' || c === '\n') {
      const t = readWhile((ch) => ch === ' ' || ch === '\t' || ch === '\r' || ch === '\n');
      if (t.includes('\n')) atLineStart = true;
      push('whitespace', t);
      i += t.length;
      continue;
    }

    // comments
    if (c === '/' && src[i + 1] === '/') {
      let j = src.indexOf('\n', i);
      if (j < 0) j = src.length;
      push('comment', src.slice(i, j));
      i = j;
      continue;
    }
    if (c === '/' && src[i + 1] === '*') {
      let j = src.indexOf('*/', i + 2);
      const closed = j >= 0;
      j = closed ? j + 2 : src.length;
      push('comment', src.slice(i, j));
      if (!closed) toks[toks.length - 1].unterminated = true;
      i = j;
      continue;
    }

    // preprocessor directive: '#' at line start. Splice first (C phase 2),
    // then comments (phase 3): '\'-continuations join, a '//' that ends
    // with '\' continues the comment, and an unclosed block comment does
    // not end the directive (see directiveLineEnd).
    if (c === '#' && atLineStart) {
      let j = i;
      for (;;) {
        let nl = directiveLineEnd(src, j);
        if (nl >= src.length) { j = src.length; break; }
        let k = nl - 1;
        while (k > j && src[k] === '\r') k--;
        if (src[k] === '\\') { j = nl + 1; continue; }
        j = nl;
        break;
      }
      push('directive', src.slice(i, j));
      i = j;
      continue;
    }

    // text blocks: @TERM / @@TERM ... TERM at line start
    if (c === '@' && isIdentStart(src[i + 1] === '@' ? src[i + 2] ?? '' : src[i + 1] ?? '')) {
      const arr = src[i + 1] === '@';
      let j = i + (arr ? 2 : 1);
      let term = '';
      while (j < src.length && isIdentChar(src[j])) { term += src[j]; j++; }
      const endRe = new RegExp(`^${term}(?![A-Za-z0-9_])`, 'm');
      const rest = src.slice(j);
      const m = endRe.exec(rest);
      let end;
      if (m) end = j + m.index + term.length;
      else end = src.length;
      push('textblock', src.slice(i, end));
      if (!m) toks[toks.length - 1].unterminated = true;
      i = end;
      continue;
    }

    // strings -- the loop exits either ON the closing quote (j indexes
    // it, j < length) or past end-of-input; the latter is the driver's
    // "End of file in string" lexerror, flagged for consumers.
    if (c === '"') {
      let j = i + 1;
      while (j < src.length && src[j] !== '"') {
        if (src[j] === '\\') j++;
        j++;
      }
      push('string', src.slice(i, Math.min(j + 1, src.length)));
      if (j >= src.length) toks[toks.length - 1].unterminated = true;
      i = Math.min(j + 1, src.length);
      continue;
    }

    // char literal -- one body byte or escape, then the closing quote,
    // exactly as lex.cc scans it (see skipCharSpan for the full rule;
    // notably `'''` is a VALID quote-char literal, not an empty `''`).
    if (c === "'") {
      const j = skipCharSpan(src, i);
      const span = src.slice(i, j);
      push('char', span);
      // A char literal without its closing quote is a driver lexerror
      // whether it hits EOF ("End of file in char") or not (push-back
      // recovery, compile still fails): flag it so formatLPC refuses the
      // file (skipCharSpan ends the span without the close in that case).
      if (!(span.length >= 2 && span.endsWith("'"))) {
        toks[toks.length - 1].unterminated = true;
      }
      i = j;
      continue;
    }

    // numbers: 0x/0b, underscores, reals
    if (isDigit(c)) {
      let j = i;
      if (c === '0' && (src[j + 1] === 'x' || src[j + 1] === 'X')) {
        j += 2;
        while (j < src.length && /[0-9A-Fa-f_]/.test(src[j])) j++;
      } else if (c === '0' && (src[j + 1] === 'b' || src[j + 1] === 'B')) {
        j += 2;
        while (j < src.length && /[01_]/.test(src[j])) j++;
      } else {
        while (j < src.length && /[0-9_]/.test(src[j])) j++;
        // Float: optional fraction (or trailing dot -- never consuming
        // the ".." range operator), then an optional exponent; a bare
        // exponent ("1e3") is a float too. "1e" with no digits is
        // NUMBER(1) IDENT(e), so the exponent needs a lookahead digit.
        if (src[j] === '.' && src[j + 1] !== '.') {
          j++;
          while (j < src.length && /[0-9_]/.test(src[j])) j++;
        }
        if ((src[j] === 'e' || src[j] === 'E') &&
            (isDigit(src[j + 1] ?? '') ||
             ((src[j + 1] === '+' || src[j + 1] === '-') && isDigit(src[j + 2] ?? '')))) {
          j++;
          if (src[j] === '+' || src[j] === '-') j++;
          while (j < src.length && /[0-9_]/.test(src[j])) j++;
        }
      }
      push('number', src.slice(i, j));
      i = j;
      continue;
    }

    // identifiers / keywords ($N parameters too)
    if (isIdentStart(c) || (c === '$' && isDigit(src[i + 1] ?? ''))) {
      let j = i + (c === '$' ? 1 : 0);
      while (j < src.length && isIdentChar(src[j])) j++;
      const word = src.slice(i, j);
      let kind = 'identifier';
      if (KEYWORDS.has(word)) kind = word === 'efun' ? 'efunkw' : 'keyword';
      else if (TYPES.has(word)) kind = 'type';
      else if (MODIFIERS.has(word)) kind = 'modifier';
      push(kind, word);
      i = j;
      continue;
    }

    // functional open/close before operators ("(:", ":)")
    //
    // "(::" (optionally with whitespace between the '(' and the "::") is
    // NOT a functional-literal open -- it's an ordinary '(' followed by
    // the "::" scope-resolution operator, as in a bare parent-call guard
    // `if (::name(...))`. lex.cc's own "("{WS}*"::" rule exists for
    // exactly this: it returns just '(' and pushes the rest back so "::"
    // scans as its own token next (see the "(::" longest-match guard
    // comment there). Without this guard, greedily matching '(' + ':' as
    // "(:" leaves a lone ':' behind, corrupting the token stream (`::` ->
    // `: :`) and everything the formatter builds on top of it.
    if (c === '(' && src[i + 1] === ':') {
      if (isParentCallOpenParen(src, i)) {
        // Bare '(' -- do not let the operator table below match "(:" as a
        // functional-literal open either; emit just the paren and let "::"
        // (and any whitespace between them) scan on the next iterations.
        push('punctuation', '(');
        i += 1;
        continue;
      }
      push('functional', '(:'); i += 2; continue;
    }
    if (c === ':' && src[i + 1] === ')') { push('functional', ':)'); i += 2; continue; }

    // operators, longest-match from the grammar contract
    let matched = false;
    for (const op of OPERATORS) {
      if (src.startsWith(op, i)) {
        push('operator', op);
        i += op.length;
        matched = true;
        break;
      }
    }
    if (matched) continue;

    if (PUNCT.has(c)) {
      push('punctuation', c);
      i++;
      continue;
    }

    push('unknown', c);
    i++;
  }
  return toks;
}
