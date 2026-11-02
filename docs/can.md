# CAN

`BaseCan` v2: one frame type for classic CAN and CAN FD, a send that never blocks, a receive path that works by
polling, and the controller's error counters. Compiled examples from [`tests/docs/can.cpp`](../tests/docs/can.cpp).

## The frame

`hf_can_message_t` carries up to 64 data bytes, so the same type serves classic CAN today and CAN FD
controllers later. `dlc` holds the **length in bytes**, not the 4-bit DLC code.

| Field or call | Meaning |
| --- | --- |
| `id` | 11-bit (`HF_CAN_MAX_STD_ID`, `0x7FF`) or 29-bit (`HF_CAN_MAX_EXT_ID`) identifier |
| `data[64]`, `dlc` | Payload and its length in bytes: 0 to 8 classic; 0 to 8, 12, 16, 20, 24, 32, 48 or 64 FD |
| `is_extended`, `is_rtr`, `is_canfd`, `is_brs`, `is_esi` | Frame format flags |
| `timestamp_us` | Receive time from the driver clock |
| `SetData(src, len)`, `SetLength(len)` | Copy or resize; return `false` and leave the frame unchanged if `len` is not valid for the frame kind |
| `SetStandardFrame()`, `SetExtendedFrame()`, `SetFdFrame(brs)`, `SetClassicFrame()` | Change the format |
| `GetDlcCode()`, `SetDlcCode(code)` | The 4-bit code, at the controller boundary |
| `Validate()`, `IsValid()` | Structural check (identifier range, length, flag combinations): an `hf_can_frame_error_t` |
| `GetFlags()`, `SetFlags()` | Flags packed as `hf_can_flag::EXT`, `RTR`, `FDF`, `BRS`, `ESI` |
| `ToFrame(out)`, `FromFrame(in)` | Convert to and from any struct with `id`, `len`, `flags`, `data[64]`, `ts_us` |

--8<-- "tests/docs/can.cpp:frame"

`HfCanDlcToLen(code, fd)` and `HfCanLenToDlc(len, fd)` convert between byte lengths and DLC codes
(`HF_CAN_DLC_INVALID` when a length cannot be carried).

!!! note "Size"
    `sizeof(hf_can_message_t)` is about 104 bytes since v2 (about 40 before). Size queues and task stacks that
    hold frames by value accordingly.

## Send without blocking

--8<-- "tests/docs/can.cpp:send"

| Call | Blocks | Returns when |
| --- | --- | --- |
| `TrySendMessage(msg)` | Never | Queued, or `CAN_ERR_QUEUE_FULL` / `CAN_ERR_BUS_OFF` at once. Same as `SendMessage(msg, 0)` |
| `SendMessage(msg, timeout_ms)` | Up to `timeout_ms` (default 1000) | Queued, or the timeout passed |
| `SendMessageBatch(msgs, n, timeout_ms)` | Per frame | Returns how many were queued |

Use `TrySendMessage()` from control loops and protocol stacks that must not stall when the bus is busy.

## Receive

Two delivery paths, which can be combined:

- **Polling:** `ReceiveMessage(msg, timeout_ms)` returns the oldest frame. With `timeout_ms = 0` it returns
  `CAN_ERR_QUEUE_EMPTY` at once when nothing is waiting; otherwise it waits up to the timeout
  (`CAN_ERR_MESSAGE_TIMEOUT`).
- **Callback:** `SetReceiveCallback(fn)` calls `fn` for each frame, in interrupt context on ESP32. Keep it to a
  copy and a notification.

--8<-- "tests/docs/can.cpp:receive"

### On ESP32: the RX ring

`EspCan`'s receive interrupt pushes each frame into an `HfSpscRing<hf_can_message_t>`, a lock-free
single-producer, single-consumer ring that `ReceiveMessage()` drains. The interrupt never blocks or allocates;
when the ring is full the new frame is dropped and counted in `rx_overruns`.

| `hf_esp_can_config_t` field | Default | Effect |
| --- | --- | --- |
| `rx_queue_depth` | 32 | Ring capacity, rounded up to a power of two |
| `rx_mode = AUTO` | yes | A registered callback gets each frame; with no callback, frames go to the ring |
| `rx_mode = QUEUE_ALWAYS` | | Every frame goes to the ring, and the callback is also called |
| `tx_queue_depth` | 10 | Frames waiting for the controller; a full queue gives `CAN_ERR_QUEUE_FULL` |

Only one task should call `ReceiveMessage()` on an object at a time (or several, serialised by a mutex): the ring
has one consumer.

## Bus status and recovery

--8<-- "tests/docs/can.cpp:status"

`GetBusStatus()` fills an `hf_can_bus_status_t`:

| Field | Meaning |
| --- | --- |
| `state` | `STOPPED`, `ERROR_ACTIVE`, `ERROR_WARNING`, `ERROR_PASSIVE` or `BUS_OFF` |
| `tec`, `rec` | Transmit and receive error counters |
| `rx_overruns` | Frames lost because the RX ring was full |
| `tx_queue_full` | Sends refused because the TX queue was full |
| `bus_off_count` | Bus-off events since `Initialize()` |

The states follow ISO 11898-1 fault confinement: warning at 96, error-passive at 128, bus-off when TEC passes
255. `HfCanStateFromCounters(tec, rec)` gives the state the counters imply. `RequestBusRecovery()` starts the
recovery sequence (128 occurrences of 11 recessive bits) and returns without waiting; `GetBusStatus()` shows
`ERROR_ACTIVE` once the controller is back. Both return `CAN_ERR_UNSUPPORTED_OPERATION` on an implementation that
does not provide them.

## Filters

`SetAcceptanceFilter(id, mask, extended)` passes frames whose identifier matches `id` in the bits set in `mask`;
`ClearAcceptanceFilter()` accepts everything again.

## Support by platform

| | `EspCan` | `StmCan` |
| --- | --- | --- |
| Classic CAN send and receive | Yes (TWAI, ESP-IDF v5.5 node API) | Yes (bxCAN or FDCAN HAL) |
| CAN FD frames | No: an FD frame returns `CAN_ERR_UNSUPPORTED_OPERATION` | FDCAN handle accepted; data-phase timing needs the FDCAN HAL extensions |
| `ReceiveMessage()` polling | Yes, from the RX ring | Yes, blocking HAL calls |
| `TrySendMessage()` | Yes | Yes (`SendMessage(msg, 0)`) |
| `GetBusStatus()`, `RequestBusRecovery()` | Yes | Not yet: base default `CAN_ERR_UNSUPPORTED_OPERATION` |

## Migrating from v1

| v1 | v2 | Action |
| --- | --- | --- |
| `uint8_t data[8]` | `uint8_t data[64]` | Code that copies `sizeof(m.data)` bytes into an 8-byte buffer must use `m.dlc` |
| `dlc` was a DLC code | `dlc` is a length in bytes | No change for classic frames; FD code that stored a code (9 to 15) stores the length or calls `SetDlcCode()` |
| `canfd_dlc` | removed | Use `dlc` |
| `SetDLC(len)` accepted 0 to 64 on FD frames | exact FD lengths only, 0 to 8 on classic | Check the `bool` result |
| `EspCan::ReceiveMessage()` always unsupported | returns frames | Polling loops start receiving; with a callback, set `rx_mode = QUEUE_ALWAYS` to get both |
| `EspCan::SendMessage()` slept after each frame | no sleep | Callers that relied on the pacing must pace themselves |

Next: [ESP32](esp32.md), or [hf-core](https://github.com/hardfoc/hf-core)'s CANopen utilities, which run over
`BaseCan`.
