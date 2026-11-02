# Utilities

Helpers in [`inc/utils/`](../inc/utils/McuSelect.h) for code that uses the interfaces: what each one is for, how
to use it and what it costs.

| Header | What it is |
| --- | --- |
| [`DigitalOutputGuard.h`](../inc/utils/DigitalOutputGuard.h) | RAII: a `BaseGpio` is active while the guard lives |
| [`SoftChipSelectGuard.h`](../inc/utils/SoftChipSelectGuard.h) | RAII: a software chip select asserted for one SPI frame |
| [`HfSpscRing.h`](../inc/utils/HfSpscRing.h) | Lock-free single-producer, single-consumer ring for ISR-to-task hand-off |
| [`PlatformMutex.h`](../inc/utils/PlatformMutex.h) | Mutex, shared mutex and lock guards with a compile-time backend |
| [`HfTime.h`](../inc/utils/HfTime.h) | Monotonic microsecond clock, selected per MCU |
| [`McuSelect.h`](../inc/utils/McuSelect.h) | Target MCU, family and capability macros |
| [`memory_utils.h`](../inc/utils/memory_utils.h) | `make_unique_nothrow<T>()`: allocation that returns `nullptr` instead of throwing |
| [`AsciiArtGenerator.h`](../inc/utils/AsciiArtGenerator.h) | Text banners for test output |

## DigitalOutputGuard

Sets a pin to output and active on construction, inactive on destruction, including every early return. Use it
for an enable line that must drop when a function exits.

--8<-- "tests/docs/utilities.cpp:dog"

Cost: a `SetDirection()` (when `ensure_output_mode` is true), a `SetActive()` and a `SetInactive()`; no
allocation.

## SoftChipSelectGuard

Runs an assert callable on construction and a deassert callable on destruction; `Release()` deasserts early.
It only owns the chip-select edges: hold the bus lock for the whole frame yourself.

--8<-- "tests/docs/utilities.cpp:cs"

Cost: the two callables, inlined; no allocation.

## HfSpscRing

One producer (usually an interrupt) calls `TryPush()`, one consumer calls `TryPop()`; neither blocks, locks or
allocates. `Init(n)` allocates the storage once, rounded up to a power of two (at most `kMaxCapacity`, 65536). A
push into a full ring drops the new element and counts it in `Overruns()`. `EspCan` uses it for received frames.

Cost: `n` elements of storage; each push or pop is two atomic loads and one atomic store.

## PlatformMutex

`PlatformMutex`, `PlatformSharedMutex`, `PlatformUniqueLock` and `PlatformSharedLock` mirror the standard
library types. With no backend they compile to nothing; with a `PlatformMutexBackend.h` on the include path they
use the RTOS (see [Architecture](architecture.md#threads)).

## HfTime

`hf_time::MonotonicUs()` reads the platform's microsecond counter (`esp_timer` on ESP32), inlined, without a
virtual call. `MonotonicMs()` and `ElapsedUs(t0)` are built on it. Under `HF_TARGET_MCU_NONE` it returns 0;
tests that need time inject their own clock.

Next: [Examples](examples.md).
