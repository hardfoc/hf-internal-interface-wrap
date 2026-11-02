/**
 * @file api_tasks.cpp
 * @brief Documentation snippets for the API guide, one per task, against the Base* interfaces.
 */

#include "BaseAdc.h"
#include "BaseGpio.h"
#include "BaseNvs.h"
#include "BasePeriodicTimer.h"
#include "BasePwm.h"
#include "BaseSpi.h"
#include "BaseUart.h"

#include <cstdio>

// --8<-- [start:init]
// Construction never touches hardware; the first call (or EnsureInitialized()) does.
bool bring_up(BaseGpio& pin, BaseSpi& spi) {
  return pin.EnsureInitialized() && spi.EnsureInitialized();
}
// --8<-- [end:init]

// --8<-- [start:errors]
// Every interface has an error enum and a constexpr *ErrToString().
void report(hf_spi_err_t err) {
  if (err != hf_spi_err_t::SPI_SUCCESS) {
    const auto text = HfSpiErrToString(err);   // std::string_view
    std::printf("SPI: %.*s\n", static_cast<int>(text.size()), text.data());
  }
}
// --8<-- [end:errors]

// --8<-- [start:spi]
// One full-duplex transfer: chip select is driven by the device for the whole call.
hf_spi_err_t read_id(BaseSpi& spi, hf_u8_t (&rx)[4]) {
  const hf_u8_t tx[4] = {0x9F, 0x00, 0x00, 0x00};   // JEDEC "read ID"
  return spi.Transfer(tx, rx, sizeof(tx), /*timeout_ms=*/10);
}
// --8<-- [end:spi]

// --8<-- [start:adc]
// Average 8 samples of one channel, in volts.
bool read_bus_voltage(BaseAdc& adc, hf_channel_id_t channel, float& volts) {
  return adc.ReadChannelV(channel, volts, /*numOfSamplesToAvg=*/8) == hf_adc_err_t::ADC_SUCCESS;
}
// --8<-- [end:adc]

// --8<-- [start:pwm]
// 25 % duty on one channel; the frequency was set when the channel was configured.
bool set_fan(BasePwm& pwm, hf_channel_id_t channel) {
  return pwm.EnableChannel(channel) == hf_pwm_err_t::PWM_SUCCESS &&
         pwm.SetDutyCycle(channel, 0.25F) == hf_pwm_err_t::PWM_SUCCESS;
}
// --8<-- [end:pwm]

// --8<-- [start:uart]
// Write a line, then read whatever has arrived without waiting.
hf_u16_t echo(BaseUart& uart, hf_u8_t* buf, hf_u16_t size) {
  const hf_u8_t hello[] = {'h', 'i', '\n'};
  if (uart.Write(hello, sizeof(hello), /*timeout_ms=*/10) != hf_uart_err_t::UART_SUCCESS) {
    return 0;
  }
  hf_u16_t n = uart.BytesAvailable();
  if (n > size) {
    n = size;
  }
  return (n > 0U && uart.Read(buf, n, 0) == hf_uart_err_t::UART_SUCCESS) ? n : 0U;
}
// --8<-- [end:uart]

// --8<-- [start:nvs]
// Count boots in non-volatile storage.
hf_u32_t count_boot(BaseNvs& nvs) {
  hf_u32_t boots = 0;
  (void)nvs.GetU32("boots", boots);           // NVS_ERR_KEY_NOT_FOUND on first boot: keep 0
  ++boots;
  if (nvs.SetU32("boots", boots) == hf_nvs_err_t::NVS_SUCCESS) {
    (void)nvs.Commit();
  }
  return boots;
}
// --8<-- [end:nvs]

// --8<-- [start:timer]
// Call a function every millisecond. The callback runs in the timer's context: keep it short.
void on_tick(void* user_data) {
  auto* ticks = static_cast<hf_u32_t*>(user_data);
  ++*ticks;
}

bool start_1khz(BasePeriodicTimer& timer, hf_u32_t& ticks) {
  return timer.SetCallback(on_tick, &ticks) == hf_timer_err_t::TIMER_SUCCESS &&
         timer.Start(/*period_us=*/1000) == hf_timer_err_t::TIMER_SUCCESS;
}
// --8<-- [end:timer]
