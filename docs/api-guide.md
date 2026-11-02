# API guide

The interfaces by task: what to call, in what order, and what to do with the result. Every block is compiled by
the host build from [`tests/docs/`](../tests/docs/api_tasks.cpp).

## The shape of the API

Fifteen interfaces, each a header in `inc/base/`, and the classes that implement them:

<!-- generated: implementations -->
| Interface | For | ESP32 | STM32 | Host |
| --- | --- | --- | --- | --- |
| [`BaseAdc`](../inc/base/BaseAdc.h) | Analog inputs: counts and volts, averaging, multi-channel reads | [`EspAdc`](../inc/mcu/esp32/EspAdc.h) | [`StmAdc`](../inc/mcu/stm32/StmAdc.h) | - |
| [`BaseBluetooth`](../inc/base/BaseBluetooth.h) | Bluetooth / BLE: scan and connect | [`EspBluetooth`](../inc/mcu/esp32/EspBluetooth.h) | - | - |
| [`BaseCan`](../inc/base/BaseCan.h) | CAN bus: classic and FD-sized frames, filters, bus status | [`EspCan`](../inc/mcu/esp32/EspCan.h) | [`StmCan`](../inc/mcu/stm32/StmCan.h) | - |
| [`BaseGpio`](../inc/base/BaseGpio.h) | Digital pins: direction, polarity, pulls, interrupts | [`EspGpio`](../inc/mcu/esp32/EspGpio.h) | [`StmGpio`](../inc/mcu/stm32/StmGpio.h) | - |
| [`BaseI2c`](../inc/base/BaseI2c.h) | One device on an I2C bus: write, read, write-then-read | [`EspI2cDevice`](../inc/mcu/esp32/EspI2c.h) | [`StmI2cDevice`](../inc/mcu/stm32/StmI2c.h) | - |
| [`BaseLogger`](../inc/base/BaseLogger.h) | Tagged, levelled log output | [`EspLogger`](../inc/mcu/esp32/EspLogger.h) | [`StmLogger`](../inc/mcu/stm32/StmLogger.h) | - |
| [`BaseNvs`](../inc/base/BaseNvs.h) | Key-value non-volatile storage: integers, strings, blobs | [`EspNvs`](../inc/mcu/esp32/EspNvs.h) | [`StmNvs`](../inc/mcu/stm32/StmNvs.h) | [`HostNvs`](../inc/mcu/host/HostNvs.h) |
| [`BasePeriodicTimer`](../inc/base/BasePeriodicTimer.h) | Periodic callback at a microsecond period | [`EspPeriodicTimer`](../inc/mcu/esp32/EspPeriodicTimer.h) | [`StmPeriodicTimer`](../inc/mcu/stm32/StmPeriodicTimer.h) | - |
| [`BasePio`](../inc/base/BasePio.h) | Programmable pulse I/O: symbol trains (WS2812, IR) | [`EspPio`](../inc/mcu/esp32/EspPio.h) | - | - |
| [`BasePwm`](../inc/base/BasePwm.h) | PWM channels: frequency, duty, phase | [`EspPwm`](../inc/mcu/esp32/EspPwm.h) | [`StmPwm`](../inc/mcu/stm32/StmPwm.h) | - |
| [`BaseSpi`](../inc/base/BaseSpi.h) | One device on an SPI bus: full-duplex transfers | [`EspSpiDevice`](../inc/mcu/esp32/EspSpi.h) | [`StmSpiDevice`](../inc/mcu/stm32/StmSpi.h) | - |
| [`BaseTemperature`](../inc/base/BaseTemperature.h) | On-chip or external temperature sensor | [`EspTemperature`](../inc/mcu/esp32/EspTemperature.h) | [`StmTemperature`](../inc/mcu/stm32/StmTemperature.h) | - |
| [`BaseUart`](../inc/base/BaseUart.h) | Serial port: write, read, flush, printf | [`EspUart`](../inc/mcu/esp32/EspUart.h) | [`StmUart`](../inc/mcu/stm32/StmUart.h) | - |
| [`BaseUsbSerialJtag`](../inc/base/BaseUsbSerialJtag.h) | USB Serial/JTAG console | [`EspUsbSerialJtag`](../inc/mcu/esp32/EspUsbSerialJtag.h) | - | - |
| [`BaseWifi`](../inc/base/BaseWifi.h) | Wi-Fi station and access point | [`EspWifi`](../inc/mcu/esp32/EspWifi.h) | - | - |
<!-- /generated -->

Every function on this page takes the interface, so it runs on any of the platforms in the table.

## Initialise

```cpp
--8<-- "tests/docs/api_tasks.cpp:init"
```

`EnsureInitialized()` returns `true` at once when the object is already initialised, so calling it at the top of
a function is cheap.

## Handle errors

```cpp
--8<-- "tests/docs/api_tasks.cpp:errors"
```

Compare against the success code of the interface (`SPI_SUCCESS`, `I2C_SUCCESS`, ...). A few convenience
methods return `bool` instead (`BaseI2c::ReadRegister()`, the three-argument `BaseSpi::Transfer()`); use the
status-returning form when you need to know why a call failed.

## Drive and read pins

`BaseGpio` works in logical states: `SetActive()`, `SetInactive()`, `Toggle()` and `IsActive(bool&)` apply the
polarity given at construction (`HF_GPIO_ACTIVE_HIGH` or `HF_GPIO_ACTIVE_LOW`), so the caller never thinks about
electrical levels. `SetDirection()`, `SetOutputMode()` (push-pull or open-drain) and `SetPullMode()` change the
configuration at run time. Interrupts are optional: `ConfigureInterrupt()`, `EnableInterrupt()` and
`WaitForInterrupt()` return `GPIO_ERR_NOT_SUPPORTED` on a platform without them.

## Talk to an I2C device

A `BaseI2c` object is one device at one address; the bus is owned by the platform's bus class.

| Call | Does |
| --- | --- |
| `Write(data, len, timeout_ms)` | One write transaction |
| `Read(data, len, timeout_ms)` | One read transaction |
| `WriteRead(tx, tx_len, rx, rx_len, timeout_ms)` | Write, repeated start, read: the usual register read |
| `WriteRegister(reg, value)` / `ReadRegister(reg, value)` | One-byte register helpers, return `bool` |
| `GetDeviceAddress()` | The device's address |

See the [Quickstart](quickstart.md) for a complete register read.

## Transfer over SPI

```cpp
--8<-- "tests/docs/api_tasks.cpp:spi"
```

`Transfer()` is full duplex; pass `nullptr` for `rx` to write only (`Write()`), or for `tx` to read only
(`Read()`). `TransferChain()` sends several equal-length frames with a gap between them, for daisy-chained parts.

## Read an analog input

```cpp
--8<-- "tests/docs/api_tasks.cpp:adc"
```

`ReadChannelCount()` returns raw counts; `ReadMultipleChannels()` reads several channels in one call.

## Set a PWM duty cycle

```cpp
--8<-- "tests/docs/api_tasks.cpp:pwm"
```

Duty is a fraction from 0.0 to 1.0. `SetDutyCycleRaw()` takes timer counts; `StartAll()`, `StopAll()` and
`UpdateAll()` act on every channel at once.

## Use a serial port

```cpp
--8<-- "tests/docs/api_tasks.cpp:uart"
```

## Store settings

```cpp
--8<-- "tests/docs/api_tasks.cpp:nvs"
```

`SetString()` / `GetString()` and `SetBlob()` / `GetBlob()` store text and binary values; `GetMaxKeyLength()`
and `GetMaxValueSize()` give the platform limits. `Commit()` writes pending changes to storage. The host build has
`HostNvs`, an in-memory implementation for tests.

## Run something periodically

```cpp
--8<-- "tests/docs/api_tasks.cpp:timer"
```

`GetMinPeriod()`, `GetMaxPeriod()` and `GetResolution()` give the platform limits in microseconds.

## Send and receive CAN frames

CAN has its own page: [CAN](can.md).

Next: [CAN](can.md), or [Porting](porting.md) to implement these interfaces on a new MCU.
