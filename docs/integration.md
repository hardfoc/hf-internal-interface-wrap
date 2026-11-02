# Integration

How to add the library to a build: ESP-IDF component, the host test build, include paths and the configuration
macros.

## As a git submodule

```bash
git submodule add https://github.com/hardfoc/hf-internal-interface-wrap.git components/hf-internal-interface-wrap
```

Inside the HardFOC stack you do not add it yourself: [hf-core-drivers](https://github.com/hardfoc/hf-core-drivers)
carries it as `internal/hf-internal-interface-wrap`, and [hf-core](https://github.com/hardfoc/hf-core) brings in
hf-core-drivers.

## ESP-IDF component

The root [`CMakeLists.txt`](../CMakeLists.txt) registers an ESP-IDF component with every source in
`src/mcu/esp32/` and `src/utils/`. Point the project at the folder:

```cmake title="CMakeLists.txt (project)"
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS components/hf-internal-interface-wrap)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_app)
```

and list the component in `main/CMakeLists.txt` (`REQUIRES hf-internal-interface-wrap`; the component name is
the folder name).

| Setting | Value |
| --- | --- |
| Include directories | `inc`, `inc/base`, `inc/mcu/esp32`, `inc/mcu/esp32/utils`, `inc/utils` |
| Language | C++20 (`cxx_std_20`), `-fno-exceptions -fno-rtti` |
| Public requirements | `freertos`, `esp_common`, `esp_hw_support`, `esp_system`, `log`, `soc`, `hal`, `esp_wifi`, `bt`, `nvs_flash`, `esp_driver_i2c`, and `esp_driver_usb_serial_jtag` when ESP-IDF has it |
| Private requirements | `esp_netif`, `esp_event`, `esp_timer`, `lwip` |

Both include styles work: `#include "BaseGpio.h"` and `#include "base/BaseGpio.h"`, `#include "EspGpio.h"` and
`#include "mcu/esp32/EspGpio.h"`.

## Other build systems

Code that only uses the interfaces needs nothing but the headers: add `inc`, `inc/base` and `inc/utils` to the
include path. That is how the host tests build ([`tests/host/CMakeLists.txt`](../tests/host/CMakeLists.txt)).
A platform's sources are added by that platform's build; this repository has no CMake target for the STM32
sources.

## MCU selection

`inc/utils/McuSelect.h` needs exactly one target. Under ESP-IDF it comes from `CONFIG_IDF_TARGET_*`; elsewhere
pass it on the command line (`-DHF_TARGET_MCU_STM32H7`) or uncomment it in the file.

<!-- generated: mcu-targets -->
| Macro | Family | `HF_MCU_NAME` |
| --- | --- | --- |
| `HF_TARGET_MCU_ESP32C6` | `HF_MCU_FAMILY_ESP32` | ESP32-C6 |
| `HF_TARGET_MCU_ESP32` | `HF_MCU_FAMILY_ESP32` | ESP32 |
| `HF_TARGET_MCU_ESP32S2` | `HF_MCU_FAMILY_ESP32` | ESP32-S2 |
| `HF_TARGET_MCU_ESP32S3` | `HF_MCU_FAMILY_ESP32` | ESP32-S3 |
| `HF_TARGET_MCU_ESP32C3` | `HF_MCU_FAMILY_ESP32` | ESP32-C3 |
| `HF_TARGET_MCU_ESP32C2` | `HF_MCU_FAMILY_ESP32` | ESP32-C2 |
| `HF_TARGET_MCU_ESP32H2` | `HF_MCU_FAMILY_ESP32` | ESP32-H2 |
| `HF_TARGET_MCU_STM32F4` | `HF_MCU_FAMILY_STM32` | STM32F4 |
| `HF_TARGET_MCU_STM32H7` | `HF_MCU_FAMILY_STM32` | STM32H7 |
| `HF_TARGET_MCU_RP2040` | `HF_MCU_FAMILY_RP2040` | RP2040 |
| `HF_TARGET_MCU_NONE` | `HF_MCU_FAMILY_NONE` | None (software-only) |
<!-- /generated -->

Headers that need it (`HfTime.h`, the ESP32 type headers) include it themselves; the `Base*` headers do not, so
code that uses only the interfaces builds without a target.

## Configuration macros

| Macro | Default | Effect |
| --- | --- | --- |
| `HF_TARGET_MCU_*` | from ESP-IDF, else none (compile error) | Selects the platform |
| `HF_THREAD_SAFE` | defined in `McuSelect.h` | Reserved: no code tests it today; locking comes from `PlatformMutex` |
| `PLATFORM_MUTEX_BACKEND_CONFIGURED` | set by a `PlatformMutexBackend.h` on the include path | `PlatformMutex` uses a real RTOS backend instead of no-ops |

Next: [Testing and CI](testing.md).
