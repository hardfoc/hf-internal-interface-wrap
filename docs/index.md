---
hide:
  - navigation
  - toc
---

<div class="hf-hero" markdown>
<span class="hf-kicker">HardFOC core library</span>

# Internal Interface Wrap

<p>Abstract C++ interfaces for the peripherals a motor-control board uses (GPIO, ADC, PWM, I2C, SPI, UART, CAN,
NVS, timers, logging, Wi-Fi, Bluetooth) and their implementations for ESP32, STM32 and the host.</p>

[Quickstart](quickstart.md){ .md-button .md-button--primary }
[API guide](api-guide.md){ .md-button }
[Source on GitHub](https://github.com/hardfoc/hf-internal-interface-wrap){ .md-button }
</div>

Drivers and board code in HardFOC never call ESP-IDF or STM32Cube directly. They take a `BaseGpio&`, a
`BaseI2c&` or a `BaseCan&`, and the board decides at build time which class sits behind it: `EspGpio` on an
ESP32-C6, `StmGpio` on an STM32, a mock in a host test. Porting the stack to a new MCU means implementing these
interfaces once, not editing every driver.

## What you get

- **15 interfaces, one shape.** Each `Base*` class has lazy initialisation (`EnsureInitialized()`), an error enum
  with a `constexpr` to-string, and statistics and diagnostics. See [Architecture](architecture.md).
- **ESP32 implementations for all of them**, on ESP-IDF v5.5: `EspGpio`, `EspAdc`, `EspCan`, `EspSpiDevice`,
  `EspI2cDevice` and the rest. See [ESP32](esp32.md).
- **CAN ready for CANopen and CAN FD.** `BaseCan` v2 carries 64-byte frames, sends without blocking
  (`TrySendMessage()`), reports TEC, REC and bus-off (`GetBusStatus()`); `EspCan` receives into a lock-free ring.
  See [CAN](can.md).
- **STM32 wrappers** over CubeMX-generated HAL handles for eleven interfaces. See [STM32](stm32.md).
- **A small porting surface.** A new MCU implements a handful of `*Impl()` methods per interface; validation,
  polarity and statistics stay in the base class. See [Porting](porting.md).
- **Utilities** for the code above the interfaces: `DigitalOutputGuard`, `SoftChipSelectGuard`, `HfSpscRing`,
  `PlatformMutex`. See [Utilities](utilities.md).

## Used by

| Repository | Uses it for |
| --- | --- |
| [hf-core-drivers](https://github.com/hardfoc/hf-core-drivers) | Carries it as `internal/hf-internal-interface-wrap`, next to the chip drivers |
| [hf-core](https://github.com/hardfoc/hf-core) | Handlers adapt `Base*` objects to the drivers' communication interfaces; the CANopen frame adapter runs over `BaseCan` |
| [hf-hal-vortex-v1](https://github.com/hardfoc/hf-hal-vortex-v1) | Vortex V1 board, through hf-core: its managers create the `Esp*` objects for the board's pins and buses |
| [hf-hal-flux-v1](https://github.com/hardfoc/hf-hal-flux-v1) | Flux V1 board, through hf-core |

## Where to go next

<div class="grid cards" markdown>

-   :material-rocket-launch-outline: **[Quickstart](quickstart.md)**

    ---

    Create an ESP32 pin and I2C device and pass them to portable code.

-   :material-layers-outline: **[Architecture](architecture.md)**

    ---

    The interface model, initialisation, errors, threads and MCU selection.

-   :material-code-braces: **[API guide](api-guide.md)**

    ---

    GPIO, ADC, PWM, SPI, UART, NVS and timers by task.

-   :material-car-electric-outline: **[CAN](can.md)**

    ---

    `BaseCan` v2: FD-sized frames, non-blocking send, RX ring, bus status.

-   :material-chip: **[Porting](porting.md)**

    ---

    Bring the interfaces to a new MCU.

-   :material-test-tube: **[Testing and CI](testing.md)**

    ---

    Host tests, compiled snippets, ESP32 example builds.

</div>
