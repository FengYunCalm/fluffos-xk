#ifndef FLUFFOS_PACKAGES_SOCKETS_SOCKET_TEXT_H
#define FLUFFOS_PACKAGES_SOCKETS_SOCKET_TEXT_H

#include "base/internal/strutils.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// Accumulates only an incomplete UTF-8 suffix between stream reads. Complete
// input is sanitized immediately; flush() preserves the existing replacement
// character behavior when a peer closes with an incomplete sequence.
class SocketTextAccumulator {
 public:
  std::string consume(std::string_view bytes) {
    std::string input;
    input.reserve(tail_length_ + bytes.size());
    input.append(tail_.data(), tail_length_);
    input.append(bytes);
    tail_length_ = 0;

    const auto tail_length = u8_incomplete_tail(input);
    if (tail_length != 0) {
      tail_length_ = tail_length;
      for (std::size_t i = 0; i < tail_length_; ++i) {
        tail_[i] = input[input.size() - tail_length + i];
      }
      input.resize(input.size() - tail_length);
    }
    return u8_sanitize(input);
  }

  std::string flush() {
    std::string pending(tail_.data(), tail_length_);
    tail_length_ = 0;
    return u8_sanitize(pending);
  }

  void clear() {
    tail_length_ = 0;
  }

  std::size_t pending_size() const {
    return tail_length_;
  }

 private:
  std::array<char, 3> tail_{};
  std::uint8_t tail_length_{0};
};

#endif  // FLUFFOS_PACKAGES_SOCKETS_SOCKET_TEXT_H
