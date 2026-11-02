/**
 * @file quickstart.cpp
 * @brief Documentation snippet: portable code written against the Base* interfaces.
 *
 * Shown on the Quickstart page and in the README. Compiled by the host build
 * (tests/host, target hf_iiw_doc_snippets), so the page cannot drift from the headers.
 */

// --8<-- [start:quickstart]
#include "BaseGpio.h"
#include "BaseI2c.h"

// Reads a sensor's WHO_AM_I register and lights an LED when it matches.
// Nothing here knows which MCU it runs on: pass in EspGpio / EspI2cDevice,
// StmGpio / StmI2cDevice, or a test double.
bool check_sensor(BaseI2c& sensor, BaseGpio& led) {
  constexpr hf_u8_t kWhoAmIReg = 0x0F;
  constexpr hf_u8_t kExpectedId = 0x6C;

  hf_u8_t id = 0;
  if (!sensor.ReadRegister(kWhoAmIReg, id)) {   // write the register address, read one byte
    return false;                               // no ACK: wiring, address or pull-ups
  }
  const bool ok = (id == kExpectedId);
  const hf_gpio_err_t err = ok ? led.SetActive() : led.SetInactive();  // polarity-aware
  return ok && err == hf_gpio_err_t::GPIO_SUCCESS;
}
// --8<-- [end:quickstart]
