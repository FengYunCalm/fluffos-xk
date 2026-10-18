#ifndef FLUFFOS_SRC_BASE_INTERNAL_STRUTILS_CC_EGCSTRINGVIEW_H_
#define FLUFFOS_SRC_BASE_INTERNAL_STRUTILS_CC_EGCSTRINGVIEW_H_

#include <algorithm>
#include <cstdint>
#include <functional>
#include <cctype>
#include <locale>
#include <string>
#include <memory>
#include <utility>
#include <unicode/utf8.h>
#include <unicode/uchar.h>
#include <unicode/brkiter.h>
#include <unicode/utext.h>
#include <unicode/unistr.h>

#include <stack>

class BreakIteratorPool {
  class EmptyUText {
   public:
    EmptyUText() {
      UErrorCode status = U_ZERO_ERROR;
      utext_openUTF8(&text_, empty_storage_, 0, &status);
      initialized_ = U_SUCCESS(status);
    }
    ~EmptyUText() {
      if (initialized_)
        utext_close(&text_);
    }
    UText* get() {
      return initialized_ ? &text_ : nullptr;
    }

   private:
    static constexpr char empty_storage_[1] = {0};
    UText text_ = UTEXT_INITIALIZER;
    bool initialized_ = false;
  };

  class BreakIteratorPoolInner {
   public:
    // Declare the stable empty UText before the iterator stack so the stack
    // is destroyed first. Pooled BreakIterators can retain this wrapper.
    EmptyUText empty_text_;
    std::stack<std::unique_ptr<icu::BreakIterator>> stack_;
  };

  struct ReturnToPool_Deleter {
    explicit ReturnToPool_Deleter(std::weak_ptr<BreakIteratorPoolInner> origin)
        : origin_(std::move(origin)) {}
    void operator()(icu::BreakIterator* ptr) {
      auto origin = origin_.lock();
      if (origin) {
        origin->stack_.emplace(ptr);
      } else {
        delete ptr;
      }
    }

   private:
    std::weak_ptr<BreakIteratorPoolInner> origin_;
  };

 private:
  std::shared_ptr<BreakIteratorPoolInner> inner_;

 public:
  UText* empty_text() {
    return inner_->empty_text_.get();
  }

 private:
  void add() {
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::BreakIterator> ptr(
        icu::BreakIterator::createCharacterInstance(icu::Locale::getDefault(), status));
    if (!U_SUCCESS(status)) {
      ptr.reset();
    }
    inner_->stack_.emplace(std::move(ptr));
  }

 public:
  using item_type = std::unique_ptr<icu::BreakIterator, ReturnToPool_Deleter>;

  explicit BreakIteratorPool(int initial) : inner_(std::make_shared<BreakIteratorPoolInner>()) {
    for (int i = 0; i < initial; i++)
      add();
  }

  item_type accquire() {
    if (inner_->stack_.empty())
      add();
    auto res = std::unique_ptr<icu::BreakIterator, ReturnToPool_Deleter>(
        inner_->stack_.top().release(), ReturnToPool_Deleter(inner_));
    inner_->stack_.pop();
    return res;
  }
};

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

  static BreakIteratorPool* pool() {
    // thread_local: the pool's stack is not synchronized, and FluffOS_XK's
    // owner threads run VM code concurrently (the upstream tree is
    // single-threaded and tolerates a process-wide pool; we do not). Local
    // pre-P2 versions had thread_local here; P2's wholesale file swap lost
    // it and reintroduced a data race (R5, TestStringIndexHandlesConcurrent
    // EgcIterators SIGSEGV).
    static thread_local BreakIteratorPool pool(32);
    return &pool;
  }

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

  static UText* empty_icu_text() {
    return pool()->empty_text();
  }

  bool bind_empty_icu() {
    auto* const empty_text = empty_icu_text();
    if (empty_text == nullptr)
      return false;
    UErrorCode status = U_ZERO_ERROR;
    brk_->setText(empty_text, status);
    return U_SUCCESS(status);
  }

  void clear_icu() {
    if (icu_ready_) {
      // BreakIterator may retain the UText wrapper on Windows even though the
      // API documentation describes setText() as copying it. Detach it while
      // the wrapper is still valid, then close the wrapper below. The empty
      // wrapper belongs to the pool and outlives its pooled iterators.
      bind_empty_icu();
      icu_ready_ = false;
    }
    if (text_open_) {
      utext_close(&text_);
      text_ = UTEXT_INITIALIZER;
      text_open_ = false;
    }
  }

  bool setup_icu() {
    // Do not bind an empty range through ICU's UTF-8 UText adapter. The
    // Windows ICU build dereferences the adapter's source during this path,
    // even though a zero-length range has no bytes to read. The pool-owned
    // empty UText preserves the same zero-boundary iterator semantics without
    // exposing a one-past source pointer to that adapter.
    if (len_ == 0) {
      // Keep the zero-length value backed by a stable, non-null UText. Some
      // Windows ICU builds dereference the adapter's source even though no
      // byte is in range.
      if (!bind_empty_icu())
        return false;
      brk_->first();
      icu_ready_ = true;
      return true;
    }

    UErrorCode status = U_ZERO_ERROR;
    utext_openUTF8(&text_, src_, len_, &status);
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
  BreakIteratorPool::item_type brk_;

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
  // The raw predicate, WITHOUT constructing an iterator. Constructing one
  // acquires an ICU break iterator from the pool, which builds 32 of them on
  // first touch -- absurd overhead when all you want is a byte scan. Callers
  // that only need the answer (u8_string_is_ascii_cached) use this.
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
    return brk_.operator->();
  }

  static BreakIteratorPool::item_type GetBreakIterator() {
    return pool()->accquire();
  }

  EGCIterator(const char* src, int32_t slen) : src_(src), len_(slen), brk_(GetBreakIterator()) {
    reset(src, slen);
  }

  virtual ~EGCIterator() {
    // BreakIterator retains the input text. Clear that reference before the
    // pooled iterator outlives this object and its source buffer.
    clear_icu();
  }
};
#endif  // FLUFFOS_SRC_BASE_INTERNAL_STRUTILS_CC_EGCSTRINGVIEW_H_
