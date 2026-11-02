# Examples

The ESP32 test applications: one per interface, each a self-checking test that prints a pass / fail summary over
the serial monitor.

<!-- generated: examples -->
| App | What it shows | Source | CI |
| --- | --- | --- | --- |
| `ascii_art` :material-star:{ title="featured" } | ASCII art generator example | [`AsciiArtComprehensiveTest.cpp`](../examples/esp32/main/AsciiArtComprehensiveTest.cpp) | yes |
| `gpio_test` :material-star:{ title="featured" } | GPIO peripheral comprehensive testing | [`GpioComprehensiveTest.cpp`](../examples/esp32/main/GpioComprehensiveTest.cpp) | yes |
| `adc_test` :material-star:{ title="featured" } | ADC peripheral comprehensive testing | [`AdcComprehensiveTest.cpp`](../examples/esp32/main/AdcComprehensiveTest.cpp) | yes |
| `uart_test` | UART peripheral comprehensive testing | [`UartComprehensiveTest.cpp`](../examples/esp32/main/UartComprehensiveTest.cpp) | yes |
| `usb_serial_jtag_test` :material-star:{ title="featured" } | Built-in USB Serial/JTAG controller (CDC-ACM + JTAG over native USB) comprehensive testing | [`UsbSerialJtagComprehensiveTest.cpp`](../examples/esp32/main/UsbSerialJtagComprehensiveTest.cpp) | yes |
| `spi_test` | SPI peripheral comprehensive testing | [`SpiComprehensiveTest.cpp`](../examples/esp32/main/SpiComprehensiveTest.cpp) | yes |
| `i2c_test` | I2C peripheral comprehensive testing | [`I2cComprehensiveTest.cpp`](../examples/esp32/main/I2cComprehensiveTest.cpp) | yes |
| `pwm_test` | PWM peripheral comprehensive testing | [`PwmComprehensiveTest.cpp`](../examples/esp32/main/PwmComprehensiveTest.cpp) | yes |
| `can_test` | CAN peripheral comprehensive testing | [`CanComprehensiveTest.cpp`](../examples/esp32/main/CanComprehensiveTest.cpp) | yes |
| `pio_test` :material-star:{ title="featured" } | PIO/RMT comprehensive testing | [`PioComprehensiveTest.cpp`](../examples/esp32/main/PioComprehensiveTest.cpp) | yes |
| `temperature_test` | Temperature sensor comprehensive testing | [`TemperatureComprehensiveTest.cpp`](../examples/esp32/main/TemperatureComprehensiveTest.cpp) | yes |
| `nvs_test` | NVS (Non-Volatile Storage) comprehensive testing | [`NvsComprehensiveTest.cpp`](../examples/esp32/main/NvsComprehensiveTest.cpp) | yes |
| `timer_test` | Timer peripheral comprehensive testing | [`TimerComprehensiveTest.cpp`](../examples/esp32/main/TimerComprehensiveTest.cpp) | yes |
| `logger_test` | Logger system comprehensive testing | [`LoggerComprehensiveTest.cpp`](../examples/esp32/main/LoggerComprehensiveTest.cpp) | yes |
| `wifi_test` | WiFi connectivity comprehensive testing | [`WifiComprehensiveTest.cpp`](../examples/esp32/main/WifiComprehensiveTest.cpp) | yes |
| `bluetooth_test` :material-star:{ title="featured" } | Comprehensive Bluetooth testing suite | [`BluetoothComprehensiveTest.cpp`](../examples/esp32/main/BluetoothComprehensiveTest.cpp) | yes |
| `dog_test` :material-star:{ title="featured" } | DigitalOutputGuard comprehensive testing suite | [`DigitalOutputGuardComprehensiveTest.cpp`](../examples/esp32/main/DigitalOutputGuardComprehensiveTest.cpp) | yes |

Build and flash one (from `examples/esp32`, ESP-IDF environment loaded):

```bash
./scripts/build_app.sh ascii_art Release
./scripts/flash_app.sh flash_monitor ascii_art Release
```

Source of this table: [`examples/esp32/app_config.yml`](../examples/esp32/app_config.yml).
<!-- /generated -->

## One-time setup

The build scripts come from [hf-espidf-project-tools](https://github.com/hardfoc/hf-espidf-project-tools),
checked out as the `examples/esp32/scripts` submodule. They install the ESP-IDF version an app asks for.

```bash
git submodule update --init examples/esp32/scripts
cd examples/esp32
./scripts/build_app.sh list                          # the apps above
./scripts/build_app.sh can_test Release              # build one
./scripts/flash_app.sh flash_monitor can_test Release
```

## What to read in an example

| Part | Where | Shows |
| --- | --- | --- |
| Object construction and configuration | The `test_*` functions | The configuration struct for that `Esp*` class |
| Error checks | The same functions | Return codes compared against `*_SUCCESS` |
| Pins | `constexpr hf_pin_num_t` constants near the top of each file | The ESP32-C6 pins the test expects; change them for your board |
| Test framework | [`examples/esp32/main/TestFramework.h`](../examples/esp32/main/TestFramework.h) | The pass / fail bookkeeping |

The CI workflow builds every app with `ci_enabled: true` for each ESP-IDF version and build type listed in
[`app_config.yml`](../examples/esp32/app_config.yml).

Next: [Testing and CI](testing.md).
