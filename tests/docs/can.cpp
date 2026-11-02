/**
 * @file can.cpp
 * @brief Documentation snippets for BaseCan v2: frames, non-blocking send, polling receive, bus
 * status.
 */

#include "BaseCan.h"

#include <cstdint>

// --8<-- [start:frame]
// A classic 11-bit data frame with six bytes, then the same payload as a 12-byte FD frame.
bool build_frames(hf_can_message_t& classic, hf_can_message_t& fd) {
  const hf_u8_t pdo[6] = {1, 2, 3, 4, 5, 6};

  classic.id = 0x181;
  classic.SetStandardFrame();
  if (!classic.SetData(pdo, sizeof(pdo))) { // false: length not valid for this frame kind
    return false;
  }

  fd.id = 0x181;
  fd.SetFdFrame(/*brs=*/true);                              // FDF + bit-rate switch
  if (!fd.SetData(pdo, sizeof(pdo)) || !fd.SetLength(12)) { // 12 bytes: DLC code 9
    return false;
  }
  // fd.SetLength(9) would return false: 9 bytes has no FD DLC code.
  return classic.Validate() == hf_can_frame_error_t::NONE && fd.IsValid();
}
// --8<-- [end:frame]

// --8<-- [start:send]
// Queue a frame from a control loop without ever blocking it.
bool publish(BaseCan& can, const hf_can_message_t& frame) {
  switch (can.TrySendMessage(frame)) { // same as SendMessage(frame, 0)
  case hf_can_err_t::CAN_SUCCESS:
    return true;
  case hf_can_err_t::CAN_ERR_QUEUE_FULL: // TX queue full: drop or retry next cycle
  case hf_can_err_t::CAN_ERR_BUS_OFF:    // controller is bus-off: see recover()
  default:
    return false;
  }
}
// --8<-- [end:send]

// --8<-- [start:receive]
// Drain every frame that arrived since the last call (EspCan: from the RX ring).
hf_u32_t drain(BaseCan& can, void (*handle)(const hf_can_message_t&)) {
  hf_u32_t n = 0;
  hf_can_message_t m;
  while (can.ReceiveMessage(m, /*timeout_ms=*/0) == hf_can_err_t::CAN_SUCCESS) {
    if (m.Validate() == hf_can_frame_error_t::NONE) {
      handle(m); // m.GetLength() bytes are valid in m.data
      ++n;
    }
  }
  return n;
}
// --8<-- [end:receive]

// --8<-- [start:status]
// Watch the error counters and recover from bus-off.
bool recover(BaseCan& can) {
  hf_can_bus_status_t s;
  if (can.GetBusStatus(s) != hf_can_err_t::CAN_SUCCESS) {
    return false; // CAN_ERR_UNSUPPORTED_OPERATION on older drivers
  }
  if (s.rx_overruns > 0U) {
    // Frames were lost: the consumer is too slow or the RX ring is too small.
  }
  if (s.state == hf_can_bus_state_t::BUS_OFF) {
    return can.RequestBusRecovery() == hf_can_err_t::CAN_SUCCESS; // returns at once
  }
  return s.state == hf_can_bus_state_t::ERROR_ACTIVE;
}
// --8<-- [end:status]
