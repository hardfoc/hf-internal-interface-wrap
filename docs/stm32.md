# STM32

The state of the STM32 implementations: what exists, how they are meant to be used, and what is not done yet.

!!! warning "Early"
    The STM32 classes are not built or tested by this repository's CI, there is no CMake target for them here,
    and they have not been run on hardware as part of this repository. Treat them as a starting point for a
    port, not a finished platform.

## What exists

Eleven interfaces have an STM32 class under [`inc/mcu/stm32/`](../inc/mcu/stm32/StmTypes.h) and
[`src/mcu/stm32/`](../src/mcu/stm32/StmGpio.cpp):

| Interface | STM32 class |
| --- | --- |
| `BaseAdc` | `StmAdc` |
| `BaseCan` | `StmCan` (bxCAN or FDCAN handle) |
| `BaseGpio` | `StmGpio` |
| `BaseI2c` | `StmI2cDevice` |
| `BaseLogger` | `StmLogger` |
| `BaseNvs` | `StmNvs` (with `StmNvsBackend`) |
| `BasePeriodicTimer` | `StmPeriodicTimer` |
| `BasePwm` | `StmPwm` |
| `BaseSpi` | `StmSpiDevice` |
| `BaseTemperature` | `StmTemperature` |
| `BaseUart` | `StmUart` |

There is no STM32 class for Bluetooth, Wi-Fi, PIO or USB Serial/JTAG.

## How they are used

The classes wrap the handles STM32CubeMX generates; they do not configure clocks, pins or peripherals
themselves. Initialise the peripheral with the generated `MX_*_Init()` code, then pass the handle:

```cpp
extern ADC_HandleTypeDef hadc1;   // from CubeMX
extern CAN_HandleTypeDef hcan1;

StmAdc adc(&hadc1);
StmCan can(&hcan1);               // or StmCan(&hfdcan1) for FDCAN
```

Select the family with `HF_TARGET_MCU_STM32F4` or `HF_TARGET_MCU_STM32H7` (see
[Integration](integration.md#mcu-selection)).

## Not done yet

| Gap | Effect |
| --- | --- |
| `StmCan` has no `GetBusStatus()` / `RequestBusRecovery()` | Both return `CAN_ERR_UNSUPPORTED_OPERATION` (base default) |
| CAN FD data-phase timing | Needs the FDCAN HAL extensions |
| No build or test in CI | Changes to `inc/base/` can break the STM32 classes unnoticed |
| `HfTime.h` includes `Stm32Time.h`, which does not exist yet | `hf_time::MonotonicUs()` does not build for an STM32 target |
| No STM32 example application | Nothing to flash for a first check |

The porting plan for running the whole HardFOC stack on an STM32H7 is in
[hf-core](https://github.com/hardfoc/hf-core) (`STM32H7_PORTING.md`).

Next: [Porting](porting.md).
