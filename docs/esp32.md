# ESP32

The ESP32 implementations: which class wraps which ESP-IDF driver, what they need, and where they differ from the
interface.

## Requirements

| Item | Value |
| --- | --- |
| ESP-IDF | v5.5 or later (`EspCan` uses the TWAI node API introduced in v5.5) |
| Language | C++20, built with `-fno-exceptions -fno-rtti` |
| Primary target | ESP32-C6 (the example applications build for `esp32c6`) |
| Other targets | ESP32, -S2, -S3, -C3, -C2, -H2 are selected through `McuSelect.h`; peripherals a target lacks are not available (no Bluetooth on the S2: `EspBluetoothStub`) |
| ESP-IDF components | `freertos`, `driver` components, `esp_wifi`, `bt`, `nvs_flash`, `esp_driver_i2c`; `esp_driver_usb_serial_jtag` where the target has it |

## Classes

<!-- generated: esp32-classes -->
| Class | Implements | ESP-IDF driver | Header | Source |
| --- | --- | --- | --- | --- |
| `EspAdc` | `BaseAdc` | `esp_adc` (one-shot and continuous, calibration) | [`EspAdc.h`](../inc/mcu/esp32/EspAdc.h) | [`EspAdc.cpp`](../src/mcu/esp32/EspAdc.cpp) |
| `EspBluetooth` | `BaseBluetooth` | NimBLE host | [`EspBluetooth.h`](../inc/mcu/esp32/EspBluetooth.h) | [`EspBluetooth.cpp`](../src/mcu/esp32/EspBluetooth.cpp) |
| `EspCan` | `BaseCan` | TWAI node API (`esp_twai`) | [`EspCan.h`](../inc/mcu/esp32/EspCan.h) | [`EspCan.cpp`](../src/mcu/esp32/EspCan.cpp) |
| `EspGpio` | `BaseGpio` | `driver/gpio`, RTC I/O, glitch filters | [`EspGpio.h`](../inc/mcu/esp32/EspGpio.h) | [`EspGpio.cpp`](../src/mcu/esp32/EspGpio.cpp) |
| `EspI2cDevice` | `BaseI2c` | `driver/i2c_master` | [`EspI2c.h`](../inc/mcu/esp32/EspI2c.h) | [`EspI2c.cpp`](../src/mcu/esp32/EspI2c.cpp) |
| `EspLogger` | `BaseLogger` | `esp_log` | [`EspLogger.h`](../inc/mcu/esp32/EspLogger.h) | [`EspLogger.cpp`](../src/mcu/esp32/EspLogger.cpp) |
| `EspNvs` | `BaseNvs` | `nvs_flash` | [`EspNvs.h`](../inc/mcu/esp32/EspNvs.h) | [`EspNvs.cpp`](../src/mcu/esp32/EspNvs.cpp) |
| `EspPeriodicTimer` | `BasePeriodicTimer` | `esp_timer` | [`EspPeriodicTimer.h`](../inc/mcu/esp32/EspPeriodicTimer.h) | [`EspPeriodicTimer.cpp`](../src/mcu/esp32/EspPeriodicTimer.cpp) |
| `EspPio` | `BasePio` | RMT (`driver/rmt_tx`, `driver/rmt_rx`) | [`EspPio.h`](../inc/mcu/esp32/EspPio.h) | [`EspPio.cpp`](../src/mcu/esp32/EspPio.cpp) |
| `EspPwm` | `BasePwm` | LEDC (`driver/ledc`) | [`EspPwm.h`](../inc/mcu/esp32/EspPwm.h) | [`EspPwm.cpp`](../src/mcu/esp32/EspPwm.cpp) |
| `EspSpiDevice` | `BaseSpi` | `driver/spi_master` | [`EspSpi.h`](../inc/mcu/esp32/EspSpi.h) | [`EspSpi.cpp`](../src/mcu/esp32/EspSpi.cpp) |
| `EspTemperature` | `BaseTemperature` | `driver/temperature_sensor` | [`EspTemperature.h`](../inc/mcu/esp32/EspTemperature.h) | [`EspTemperature.cpp`](../src/mcu/esp32/EspTemperature.cpp) |
| `EspUart` | `BaseUart` | `driver/uart` | [`EspUart.h`](../inc/mcu/esp32/EspUart.h) | [`EspUart.cpp`](../src/mcu/esp32/EspUart.cpp) |
| `EspUsbSerialJtag` | `BaseUsbSerialJtag` | `driver/usb_serial_jtag` | [`EspUsbSerialJtag.h`](../inc/mcu/esp32/EspUsbSerialJtag.h) | [`EspUsbSerialJtag.cpp`](../src/mcu/esp32/EspUsbSerialJtag.cpp) |
| `EspWifi` | `BaseWifi` | `esp_wifi` | [`EspWifi.h`](../inc/mcu/esp32/EspWifi.h) | [`EspWifi.cpp`](../src/mcu/esp32/EspWifi.cpp) |
<!-- /generated -->

Shared buses are owned by a bus object: `EspSpiBus` creates `EspSpiDevice`s and `EspI2cBus` creates
`EspI2cDevice`s. Call the bus's `Initialize()` before `CreateDevice()`; the bus keeps the devices and hands them
out as `BaseSpi*` / `BaseI2c*`.

## Notes by class

| Class | Note |
| --- | --- |
| `EspCan` | Classic CAN only (the TWAI controller has no FD). Receive interrupt fills a lock-free ring; `GetBusStatus()` and `RequestBusRecovery()` are implemented. See [CAN](can.md) |
| `EspGpio` | Interrupts (`ConfigureInterrupt()`), glitch filters and RTC I/O for sleep are available |
| `EspI2cDevice` | Uses the ESP-IDF v5 `i2c_master` driver; the bus enables the internal pull-ups by default (`flags.enable_internal_pullup`) |
| `EspSpiDevice` | `Transfer()` uses `spi_device_transmit()`; the `timeout_ms` argument is reserved |
| `EspPio` | Symbol trains over RMT, for WS2812 LEDs and similar protocols |
| `EspUsbSerialJtag` | Only on targets with the USB Serial/JTAG controller (S3, C3, C6, H2, P4) |

## Example applications

Every ESP32 class has a test application in [`examples/esp32`](../examples/esp32/app_config.yml); see
[Examples](examples.md).

Next: [STM32](stm32.md).
