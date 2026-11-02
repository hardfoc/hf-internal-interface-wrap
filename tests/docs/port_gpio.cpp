/**
 * @file port_gpio.cpp
 * @brief Documentation snippet: the smallest complete BaseGpio implementation, for the Porting
 * page.
 *
 * A real port replaces the register variable with the MCU's GPIO calls.
 */

#include "BaseGpio.h"

// --8<-- [start:port]
#include "BaseGpio.h"

// A GPIO for a new MCU. BaseGpio does the polarity, validation and statistics;
// the port only moves the pin.
class MyMcuGpio : public BaseGpio {
public:
  explicit MyMcuGpio(
      hf_pin_num_t pin, hf_gpio_direction_t dir = hf_gpio_direction_t::HF_GPIO_DIRECTION_OUTPUT,
      hf_gpio_active_state_t active = hf_gpio_active_state_t::HF_GPIO_ACTIVE_HIGH) noexcept
      : BaseGpio(pin, dir, active) {}

  bool Initialize() noexcept override {
    // Configure the pin's mux, direction and pulls with the vendor SDK.
    return SetDirectionImpl(current_direction_) == hf_gpio_err_t::GPIO_SUCCESS;
  }

  bool IsPinAvailable() const noexcept override {
    return pin_ >= 0 && pin_ < GetMaxPins();
  }
  hf_u8_t GetMaxPins() const noexcept override {
    return 32; // pins on this MCU
  }
  const char* GetDescription() const noexcept override {
    return "MyMcu GPIO";
  }

protected:
  hf_gpio_err_t SetDirectionImpl(hf_gpio_direction_t d) noexcept override {
    direction_ = d;
    return hf_gpio_err_t::GPIO_SUCCESS;
  }
  hf_gpio_err_t GetDirectionImpl(hf_gpio_direction_t& d) const noexcept override {
    d = direction_;
    return hf_gpio_err_t::GPIO_SUCCESS;
  }
  hf_gpio_err_t SetOutputModeImpl(hf_gpio_output_mode_t m) noexcept override {
    mode_ = m;
    return hf_gpio_err_t::GPIO_SUCCESS;
  }
  hf_gpio_err_t GetOutputModeImpl(hf_gpio_output_mode_t& m) const noexcept override {
    m = mode_;
    return hf_gpio_err_t::GPIO_SUCCESS;
  }
  hf_gpio_err_t SetPullModeImpl(hf_gpio_pull_mode_t p) noexcept override {
    pull_ = p;
    return hf_gpio_err_t::GPIO_SUCCESS;
  }
  hf_gpio_pull_mode_t GetPullModeImpl() const noexcept override {
    return pull_;
  }
  hf_gpio_err_t SetPinLevelImpl(hf_gpio_level_t level) noexcept override {
    level_ = level; // write the output data register here
    return hf_gpio_err_t::GPIO_SUCCESS;
  }
  hf_gpio_err_t GetPinLevelImpl(hf_gpio_level_t& level) noexcept override {
    level = level_; // read the input data register here
    return hf_gpio_err_t::GPIO_SUCCESS;
  }

private:
  hf_gpio_direction_t direction_{hf_gpio_direction_t::HF_GPIO_DIRECTION_INPUT};
  hf_gpio_output_mode_t mode_{hf_gpio_output_mode_t::HF_GPIO_OUTPUT_MODE_PUSH_PULL};
  hf_gpio_pull_mode_t pull_{hf_gpio_pull_mode_t::HF_GPIO_PULL_MODE_FLOATING};
  hf_gpio_level_t level_{hf_gpio_level_t::HF_GPIO_LEVEL_LOW};
};
// --8<-- [end:port]

// Instantiate it so every override is checked against BaseGpio.
bool port_gpio_compiles() {
  MyMcuGpio led(2);
  return led.EnsureInitialized() && led.SetActive() == hf_gpio_err_t::GPIO_SUCCESS;
}
