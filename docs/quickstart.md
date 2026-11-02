# Quickstart

Write one function against the interfaces, then give it ESP32 objects: an LED pin and an I2C sensor.

## 1. Add the component

Clone the repository into your ESP-IDF project and list it as an extra component directory (details in
[Integration](integration.md)):

```bash
git submodule add https://github.com/hardfoc/hf-internal-interface-wrap.git components/hf-internal-interface-wrap
```

The component needs ESP-IDF v5.5 or later (the TWAI node API that `EspCan` uses) and C++20.

## 2. Write portable code

The function below takes the interfaces, not ESP32 classes, so it builds unchanged for STM32 or a host test.
It is compiled by the host build from [`tests/docs/quickstart.cpp`](../tests/docs/quickstart.cpp).

--8<-- "tests/docs/quickstart.cpp:quickstart"

## 3. Create the ESP32 objects

```cpp title="main.cpp"
#include "mcu/esp32/EspGpio.h"
#include "mcu/esp32/EspI2c.h"

extern "C" void app_main() {
  EspGpio led(8, hf_gpio_direction_t::HF_GPIO_DIRECTION_OUTPUT);   // GPIO8, active high

  hf_i2c_master_bus_config_t bus_cfg;                              // I2C_NUM_0, internal pull-ups
  bus_cfg.sda_io_num = 6;
  bus_cfg.scl_io_num = 7;
  EspI2cBus bus(bus_cfg);
  if (!bus.Initialize()) {
    return;                                                        // pins or port already in use
  }

  hf_i2c_device_config_t dev_cfg;
  dev_cfg.device_address = 0x6A;                                   // 7-bit address
  dev_cfg.scl_speed_hz = 400000;
  BaseI2c* sensor = bus.GetDevice(bus.CreateDevice(dev_cfg));

  if (sensor == nullptr || !check_sensor(*sensor, led)) {
    // wrong address, no pull-ups, or the sensor did not answer
  }
}
```

Construction only stores the configuration. The bus is set up by `bus.Initialize()`, which must come before
`CreateDevice()`; the pin and the device are set up by the `EnsureInitialized()` calls in `check_sensor()`.

## What each step does

| Step | What happens |
| --- | --- |
| `EspGpio led(...)` | Stores pin, direction and polarity. No register is written yet |
| `EspI2cBus bus(cfg)` | Stores the bus settings: port, pins, glitch filter, pull-ups |
| `bus.Initialize()` | Creates the ESP-IDF I2C master bus |
| `CreateDevice(dev_cfg)` | Adds a device at that address and speed and returns its index, or -1 (bus not initialised, address in use) |
| `GetDevice(index)` | Returns the device as a `BaseI2c*` (`nullptr` for a bad index) |
| `EnsureInitialized()` | Configures the pin, adds the device to the ESP-IDF bus; does nothing the second time |
| `ReadRegister(reg, id)` | `WriteRead()` of one address byte and one data byte |
| `SetActive()` | Drives the pin to its active level (high here, low for an active-low pin) |

## If it does not work

| Symptom | Check |
| --- | --- |
| `CreateDevice()` returns -1 | `bus.Initialize()` was not called or failed, or the address is already on the bus |
| `ReadRegister()` returns false | The 7-bit address (not shifted), SDA / SCL pins swapped, external pull-ups on a long bus |
| The LED never lights | The pin number and `HF_GPIO_ACTIVE_LOW` for an LED wired to 3.3 V |
| Link errors for `Esp*` symbols | The component directory is not in `EXTRA_COMPONENT_DIRS` |
| `#error "No MCU selected!"` | A non-ESP-IDF build must define one `HF_TARGET_MCU_*` (see [Integration](integration.md)) |

Next: [Architecture](architecture.md), or the [API guide](api-guide.md) for the other interfaces.
