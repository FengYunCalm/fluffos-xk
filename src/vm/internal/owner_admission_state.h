#pragma once

#include <cstdint>

// Admission outcome of one owner task. The mailbox task and the trace event both
// carry the enum; the string spelling exists only at the LPC boundary, so a new
// outcome cannot be spelled one way in the task and another in its trace.
enum class OwnerAdmissionState : uint8_t {
  kUnset,
  kAccepted,
  kAcceptedDispatchRejected,
  kRejectedBackpressure,
  kRejectedCallbackNotAllowlisted,
  kDroppedCallbackInvalidTarget,
};

inline const char* owner_admission_state_name(OwnerAdmissionState state) {
  switch (state) {
    case OwnerAdmissionState::kAccepted:
      return "accepted";
    case OwnerAdmissionState::kAcceptedDispatchRejected:
      return "accepted_dispatch_rejected";
    case OwnerAdmissionState::kRejectedBackpressure:
      return "rejected_backpressure";
    case OwnerAdmissionState::kRejectedCallbackNotAllowlisted:
      return "rejected_callback_not_allowlisted";
    case OwnerAdmissionState::kDroppedCallbackInvalidTarget:
      return "dropped_callback_invalid_target";
    case OwnerAdmissionState::kUnset:
      break;
  }
  return "";
}
