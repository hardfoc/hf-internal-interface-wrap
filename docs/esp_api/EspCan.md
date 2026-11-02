---
layout: default
title: "🚌 EspCan"
description: "ESP32 CAN (TWAI) implementation: ISR-fed RX ring, non-blocking TX, error counters"
nav_order: 7
parent: "🔧 ESP32 Implementations"
permalink: /docs/esp_api/EspCan/
---

# EspCan API Reference

## Overview

`EspCan` implements [`BaseCan`](../api/BaseCan.md) on the ESP32 TWAI controller with the
ESP-IDF v5.5 node API (`esp_twai.h`, `esp_twai_onchip.h`). One `EspCan` object drives one
TWAI node. On the ESP32-C6 there are two TWAI controllers; both are classic CAN only
(CAN 2.0A/2.0B, no CAN FD).

```cpp
#include "mcu/esp32/EspCan.h"
```

## Configuration

```cpp
hf_esp_can_config_t cfg{};          // defaults: GPIO4/5, 500 kbit/s, 75 % sample point
cfg.tx_pin = 14;
cfg.rx_pin = 15;
cfg.baud_rate = 1000000;
cfg.tx_queue_depth = 16;            // driver TX queue (frames waiting for the controller)
cfg.rx_queue_depth = 64;            // RX ring for ReceiveMessage(), rounded up to a power of 2
cfg.rx_mode = hf_esp_can_rx_mode_t::AUTO;
EspCan can(cfg);                    // lazy: nothing touches the hardware yet
can.Initialize();
```

| Field | Default | Meaning |
|:--|:--|:--|
| `tx_queue_depth` | 10 | Driver TX queue depth. EspCan keeps `tx_queue_depth + 1` TX slots |
| `rx_queue_depth` | 32 | RX ring capacity (power of 2, at most 65536) |
| `rx_mode` | `AUTO` | See [Receiving](#receiving) |
| `enable_self_test` | false | Transmit without an ACK from another node |
| `enable_loopback` | false | Receive the node's own frames (internal loopback) |
| `fail_retry_cnt` | -1 | -1 retries forever, 0 is single shot |

## Receiving

Frames are read from the controller in the TWAI RX interrupt. From there they go to a
receive callback, to a lock-free single-producer ring buffer, or to both:

| `rx_mode` | Callback registered | Callback not registered |
|:--|:--|:--|
| `AUTO` (default) | callback only | RX ring |
| `QUEUE_ALWAYS` | callback **and** RX ring | RX ring |

### Polling (RX ring)

```cpp
hf_can_message_t m;
switch (can.ReceiveMessage(m, 10)) {     // wait up to 10 ms
case hf_can_err_t::CAN_SUCCESS:          // m.timestamp_us = RX interrupt time (µs)
  Handle(m);
  break;
case hf_can_err_t::CAN_ERR_MESSAGE_TIMEOUT:  // nothing within 10 ms
  break;
default:
  break;
}
can.ReceiveMessage(m, 0);                // never blocks: CAN_ERR_QUEUE_EMPTY when empty
```

- The waiting task is woken by the RX interrupt (a FreeRTOS semaphore), so a frame is handed
  over at once, not at the next tick. Timeouts are rounded up to whole ticks.
- Several tasks may call `ReceiveMessage()`; they are serialised by a mutex.
- When the ring is full, the new frame is dropped and counted (`rx_overruns` in
  `GetBusStatus()`, `rx_queue_overflows` in the statistics). Frames already queued are kept.

### Callbacks

```cpp
void OnFrame(const hf_can_message_t& m, void* ctx) {   // ISR context
  auto* q = static_cast<MyQueue*>(ctx);
  q->PushFromIsr(m);                                    // copy and return
}
can.SetReceiveCallbackEx(OnFrame, &queue);
// or the BaseCan form: can.SetReceiveCallback(+[](const hf_can_message_t& m) { ... });
```

Receive callbacks run **in the TWAI interrupt**: no blocking calls, no logging, no
allocation. The error and state-change callbacks run there too.

## Transmitting

```cpp
hf_can_message_t m;
m.id = 0x181;
const uint8_t pdo[4] = {0x01, 0x02, 0x03, 0x04};
m.SetData(pdo, sizeof(pdo));

hf_can_err_t r = can.TrySendMessage(m);   // == SendMessage(m, 0): queue or fail at once
if (r == hf_can_err_t::CAN_ERR_QUEUE_FULL) {
  // retry later, e.g. from a TX-done path; nothing was queued
}
r = can.SendMessage(m, 5);                // wait up to 5 ms for TX queue space
```

- `SendMessage()` copies the frame into an internal TX slot and returns once the driver has
  queued it. The slot is released by the TX-done interrupt. The TWAI driver keeps a
  *pointer* to the queued frame until it is on the bus, so the frame cannot live on the
  caller's stack; the slots make this safe.
- There is no delay after a send. Earlier versions slept 1 ms after every frame
  (`vTaskDelay(pdMS_TO_TICKS(1))`, which is 0 ticks at 100 Hz) to hide the stack-frame problem
  above.
- `timeout_ms = 0` never blocks. `UINT32_MAX` waits forever.
- Return codes: `CAN_SUCCESS`, `CAN_ERR_QUEUE_FULL`, `CAN_ERR_BUS_OFF`,
  `CAN_ERR_MESSAGE_INVALID_DLC` (length invalid for the frame kind),
  `CAN_ERR_MESSAGE_INVALID_ID`, `CAN_ERR_UNSUPPORTED_OPERATION` (FD frame on TWAI).
- Zero-length data frames (for example CANopen SYNC) are accepted.

## Error state, counters and bus-off

```cpp
hf_can_bus_status_t st;
if (can.GetBusStatus(st) == hf_can_err_t::CAN_SUCCESS) {
  // st.state: STOPPED, ERROR_ACTIVE, ERROR_WARNING, ERROR_PASSIVE or BUS_OFF
  // st.tec / st.rec: controller error counters
  // st.rx_overruns: frames dropped because the RX ring was full
  // st.tx_queue_full: sends refused for lack of TX queue space
  // st.bus_off_count: bus-off events since Initialize() (or ResetStatistics())
}
if (st.state == hf_can_bus_state_t::BUS_OFF) {
  can.RequestBusRecovery();   // starts recovery and returns; poll GetBusStatus()
}
```

While the node is bus-off, `SendMessage()` returns `CAN_ERR_BUS_OFF`. Frames that were queued
before bus-off stay queued and are sent after recovery. Recovery needs 128 occurrences of
11 recessive bits on the bus; `GetBusStatus()` reports `ERROR_ACTIVE` once it is done.
`InitiateBusRecovery()` is the older name of `RequestBusRecovery()`.

`GetStatus()` also fills `rx_missed_count` (RX ring overruns) and `tx_failed_count`.

## Filters and timing

- `SetAcceptanceFilter(id, mask, extended)` and `ConfigureAdvancedFilter()` set the single
  mask filter (or the dual 16-bit split). The node is stopped and restarted around the change.
- `ConfigureAdvancedTiming()` sets BRP / segments directly.

## Threading summary

| Call | Context |
|:--|:--|
| `SendMessage`, `TrySendMessage` | any task (not an ISR) |
| `ReceiveMessage` | any task; callers are serialised |
| receive / error / state callbacks | TWAI ISR |
| `Initialize`, `Deinitialize`, filter and timing changes | one task, while nothing else uses the object |

## Hardware test

The `can_test` example (`examples/esp32/main/CanComprehensiveTest.cpp`) includes
`loopback_stress_rx_ring`: 20000 frames at 1 Mbit/s in self-test + internal loopback, sent
with `TrySendMessage()` (falling back to a short blocking send on `CAN_ERR_QUEUE_FULL`) and
received through the RX ring by a separate task. It fails on any lost, reordered or corrupt
frame, on any RX overrun, or below 5000 frames/s. No transceiver is needed.

## Related Documentation

- [BaseCan API Reference](../api/BaseCan.md), including the BaseCan v2 migration notes
- [HardwareTypes Reference](../api/HardwareTypes.md)
<!-- markdownlint-disable-next-line MD013 -->
- [ESP-IDF TWAI Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/peripherals/twai.html)
- [CAN Comprehensive Tests](../../examples/esp32/docs/README_CAN_TEST.md)
