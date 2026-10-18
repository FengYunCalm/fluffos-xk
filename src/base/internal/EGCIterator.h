#ifndef FLUFFOS_SRC_BASE_INTERNAL_STRUTILS_CC_EGCSTRINGVIEW_H_
#define FLUFFOS_SRC_BASE_INTERNAL_STRUTILS_CC_EGCSTRINGVIEW_H_

#include <algorithm>
#include <cstdint>
#include <functional>
#include <cctype>
#include <locale>
#include <string>
#include <memory>
#include <unicode/utf8.h>
#include <unicode/uchar.h>
#include <unicode/brkiter.h>
#include <unicode/utext.h>
#include <unicode/unistr.h>

// Grapheme iterator with a byte-offset path for CR-free ASCII ranges.
// New buffers are classified once; subranges retain the previous ASCII/ICU
// path. ICU text is rebound on reset, even when classification is reused.
// operator->() also initializes ICU lazily for ASCII ranges.
class EGCIterator {
 private:
  bool ok_ = false;
  bool ascii_ = false;
  bool icu_ready_ = false;
  const char* src_;
  int32_t len_;
  UText text_ = UTEXT_INITIALIZER;
  bool text_open_ = false;
  std::string icu_source_;
  icu::UnicodeString unicode_text_;

  // True when the string is pure ASCII (so trivially valid UTF-8) AND contains
  // no CR.
  //
  // The CR exclusion is load-bearing, not caution: UAX #29 rule GB3 joins
  // CR x LF into ONE grapheme cluster, so "\r\n" is 2 bytes but 1 cluster and
  // the byte-offset==index identity breaks. CR is the only ASCII byte that can
  // join with a following character this way -- pinned exhaustively over all
  // 128x128 ASCII pairs by EgcResetTest.AsciiPairsMatchIcu. Excluding
  // every CR (rather than just CR immediately before LF) keeps this a single
  // flat scan; a lone CR merely falls back to ICU, which is still correct.
  static bool all_ascii(const char* src, int32_t slen) {
    // Only -1 is the NUL-terminated convention; other negative lengths are
    // invalid and must not make the empty scan look like ASCII.
    if (slen < 0)
      return false;
    unsigned char acc = 0;
    unsigned char cr = 0;
    for (int32_t i = 0; i < slen; i++) {
      auto c = static_cast<unsigned char>(src[i]);
      acc |= c;
      cr |= static_cast<unsigned char>(c == '\r');
    }
    return (acc & 0x80u) == 0 && cr == 0;
  }

  void clear_icu() {
    // ICU's UText overload may retain the wrapper and its backing bytes. Each
    // iterator owns its BreakIterator, so destroy that object before closing
    // the wrapper or releasing the backing storage.
    icu_ready_ = false;
    brk_.reset();
    unicode_text_.remove();
    if (text_open_) {
      utext_close(&text_);
      text_ = UTEXT_INITIALIZER;
      text_open_ = false;
    }
    icu_source_.clear();
  }

  static bool valid_utf8(const char* source, int32_t length) {
    auto const* bytes = reinterpret_cast<const unsigned char*>(source);
    for (int32_t index = 0; index < length;) {
      unsigned char const first = bytes[index];
      if (first <= 0x7f) {
        ++index;
        continue;
      }
      int required = 0;
      uint32_t codepoint = 0;
      uint32_t minimum = 0;
      if (first >= 0xc2 && first <= 0xdf) {
        required = 1;
        codepoint = first & 0x1f;
        minimum = 0x80;
      } else if (first >= 0xe0 && first <= 0xef) {
        required = 2;
        codepoint = first & 0x0f;
        minimum = 0x800;
      } else if (first >= 0xf0 && first <= 0xf4) {
        required = 3;
        codepoint = first & 0x07;
        minimum = 0x10000;
      } else {
        return false;
      }
      if (index + required >= length)
        return false;
      for (int offset = 1; offset <= required; ++offset) {
        unsigned char const continuation = bytes[index + offset];
        if ((continuation & 0xc0) != 0x80)
          return false;
        codepoint = (codepoint << 6) | (continuation & 0x3f);
      }
      if (codepoint < minimum || codepoint > 0x10ffff ||
          (codepoint >= 0xd800 && codepoint <= 0xdfff))
        return false;
      index += required + 1;
    }
    return true;
  }

  bool setup_icu() {
    if (!brk_) {
      UErrorCode status = U_ZERO_ERROR;
      brk_.reset(icu::BreakIterator::createCharacterInstance(icu::Locale::getDefault(), status));
      if (!U_SUCCESS(status) || !brk_) {
        brk_.reset();
        return false;
      }
    }

    // Do not bind an empty range through ICU's UTF-8 UText adapter. Some
    // Windows ICU builds inspect the adapter's source even when its length is
    // zero. A stable empty UnicodeString preserves zero-boundary semantics.
    if (len_ == 0) {
      static const UChar empty_storage[1] = {0};
      static const icu::UnicodeString empty(false, empty_storage, 0);
      brk_->setText(empty);
      brk_->first();
      icu_ready_ = true;
      return true;
    }

    if (ascii_) {
      // ASCII byte offsets are identical to ICU's UTF-16 offsets. Use the
      // UnicodeString overload so ASCII never enters the Windows UTF-8 UText
      // provider.
      unicode_text_ = icu::UnicodeString(src_, len_, US_INV);
      brk_->setText(unicode_text_);
      brk_->first();
      icu_ready_ = true;
      return true;
    }

    static constexpr char empty_storage[] = "";
    const char* text_source = empty_storage;
    if (len_ > 0) {
      // Keep ICU away from interior pointers into a larger string. The
      // Windows UTF-8 UText provider has historically read beyond an
      // explicitly bounded subrange; this owned copy is NUL-terminated and
      // keeps the returned native offsets one-to-one with source bytes.
      icu_source_.assign(src_, static_cast<size_t>(len_));
      if (!valid_utf8(src_, len_)) {
        // ICU's UTF-8 UText provider can also mishandle a partial code point
        // at a bounded range edge. Preserve the existing one-byte/one-offset
        // fallback for malformed input.
        icu_source_.assign(static_cast<size_t>(len_), 'x');
      }
      text_source = icu_source_.data();
    } else if (len_ == -1) {
      // A negative length means a NUL-terminated source. Copy it as well so
      // an interior pointer never becomes ICU's backing storage.
      icu_source_.assign(src_);
      text_source = icu_source_.c_str();
    }

    UErrorCode status = U_ZERO_ERROR;
    utext_openUTF8(&text_, text_source, len_, &status);
    if (!U_SUCCESS(status)) {
      utext_close(&text_);
      text_ = UTEXT_INITIALIZER;
      return false;
    }
    text_open_ = true;

    status = U_ZERO_ERROR;
    brk_->setText(&text_, status);
    if (!U_SUCCESS(status)) {
      clear_icu();
      return false;
    }

    brk_->first();
    icu_ready_ = true;
    return true;
  }

 protected:
  std::unique_ptr<icu::BreakIterator> brk_;

  // Materialize the ICU iterator for a string that skipped setup because it
  // is ASCII. Must be called before ANY use of brk_.
  bool ensure_icu() {
    return icu_ready_ ? true : setup_icu();
  }

 public:
  [[nodiscard]] bool ok() const {
    return ok_;
  }
  // A false result may also describe an ASCII subrange retaining the ICU path.
  [[nodiscard]] bool is_ascii() const {
    return ascii_;
  }
  // The raw predicate, WITHOUT constructing an iterator. Callers that only
  // need the answer (u8_string_is_ascii_cached) use this.
  static bool scan_is_ascii(const char* src, int32_t slen) {
    return all_ascii(src, slen);
  }
  [[nodiscard]] const char* data() const {
    return src_;
  }
  [[nodiscard]] int32_t len() const {
    return len_;
  }
  // Virtual so derived iterators can invalidate cached cursor state when the
  // range changes. Base construction intentionally dispatches to this base
  // implementation, before derived members exist.
  virtual void reset(const char* src, int32_t slen) {
    const bool prev_ok = ok_;
    const bool prev_ascii = ascii_;
    const char* const prev_src = src_;
    const int32_t prev_len = len_;

    ok_ = false;
    clear_icu();
    ascii_ = false;
    src_ = src;
    len_ = slen;

    if (slen < -1)
      return;

    // A known subrange can retain either path without rescanning its bytes.
    // Compare integer addresses after checking lengths, not unrelated pointers.
    if (prev_ok && slen >= 0 && slen <= prev_len) {
      const auto src_u = reinterpret_cast<uintptr_t>(src);
      const auto prev_u = reinterpret_cast<uintptr_t>(prev_src);
      if (src_u >= prev_u &&
          src_u - prev_u <= static_cast<uintptr_t>(prev_len) - static_cast<uintptr_t>(slen)) {
        ascii_ = prev_ascii;
        ok_ = ascii_ || setup_icu();
        return;
      }
    }

    if (slen >= 0 && all_ascii(src, slen)) {
      // Pure ASCII is always well-formed UTF-8; defer ICU until something
      // actually asks for the underlying break iterator.
      ascii_ = true;
      ok_ = true;
      return;
    }

    ok_ = setup_icu();
  }
  auto operator->() {
    ensure_icu();
    return brk_.get();
  }

  EGCIterator(const char* src, int32_t slen) : src_(src), len_(slen) {
    reset(src, slen);
  }

  virtual ~EGCIterator() {
    clear_icu();
  }
};
#endif  // FLUFFOS_SRC_BASE_INTERNAL_STRUTILS_CC_EGCSTRINGVIEW_H_
