/**
 * @file utilities.cpp
 * @brief Documentation snippets for the utilities page.
 */

#include "BaseGpio.h"
#include "DigitalOutputGuard.h"
#include "SoftChipSelectGuard.h"

void run_motor_test() noexcept;

// --8<-- [start:dog]
hf_gpio_err_t test_with_driver_enabled(BaseGpio& driver_enable_pin) {
  DigitalOutputGuard enable(driver_enable_pin); // output + active
  if (!enable.IsValid()) {
    return enable.GetLastError();
  }
  run_motor_test();
  return hf_gpio_err_t::GPIO_SUCCESS;
} // inactive here, on every return path
// --8<-- [end:dog]

// --8<-- [start:cs]
void one_frame(BaseGpio& cs_pin) {
  SoftChipSelectGuard cs{[&] { (void)cs_pin.SetActive(); }, [&] { (void)cs_pin.SetInactive(); }};
  // the SPI frame goes here; the caller already holds the bus lock
} // chip select released here
// --8<-- [end:cs]
