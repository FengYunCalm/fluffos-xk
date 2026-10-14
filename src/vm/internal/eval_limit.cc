#include "base/std.h"

#include <chrono>

#include "eval_limit.h"
#include "posix_timers.h"

FLUFFOS_VM_THREAD_LOCAL volatile std::sig_atomic_t outoftime = 0;
uint64_t max_eval_cost;

#ifndef __linux__
namespace {

using eval_clock = std::chrono::steady_clock;

constexpr uint8_t kEvalPollInterval = 64;
FLUFFOS_VM_THREAD_LOCAL eval_clock::time_point eval_deadline = eval_clock::time_point::max();
FLUFFOS_VM_THREAD_LOCAL uint8_t eval_poll_count = 0;

void set_portable_deadline(uint64_t etime) {
  const auto now = eval_clock::now();
  const auto max_microseconds =
      std::chrono::duration_cast<std::chrono::microseconds>(eval_clock::time_point::max() - now)
          .count();
  if (etime > static_cast<uint64_t>(max_microseconds)) {
    eval_deadline = eval_clock::time_point::max();
  } else {
    eval_deadline = now + std::chrono::microseconds(static_cast<int64_t>(etime));
  }
}

int64_t portable_eval_remaining() {
  const auto now = eval_clock::now();
  if (now >= eval_deadline) {
    outoftime = 1;
    return 0;
  }
  return std::chrono::duration_cast<std::chrono::microseconds>(eval_deadline - now).count();
}

}  // namespace
#endif

void init_eval() {
#ifdef __linux__
  init_posix_timers();
#endif
}

void set_eval(uint64_t etime) {
#ifdef __linux__
  posix_eval_timer_set(etime);
#else
  set_portable_deadline(etime);
  eval_poll_count = 0;
#endif
  outoftime = 0;
}

int64_t get_eval() {
#ifdef __linux__
  return posix_eval_timer_get();
#else
  return portable_eval_remaining();
#endif
}

bool eval_limit_expired() {
#ifdef __linux__
  return outoftime != 0;
#else
  if (outoftime) {
    return true;
  }
  if (++eval_poll_count < kEvalPollInterval) {
    return false;
  }
  eval_poll_count = 0;
  return portable_eval_remaining() == 0;
#endif
}
