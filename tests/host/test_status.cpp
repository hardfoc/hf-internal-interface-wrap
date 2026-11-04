/**
 * @file test_status.cpp
 * @brief hf::Status, hf::Result and the hf::ToStatus() of every hf_*_err_t.
 *
 * @copyright HardFOC
 */

#include "hf/iiw/Result.h"
#include "hf/iiw/Status.h"

#include "BaseAdc.h"
#include "BaseBluetooth.h"
#include "BaseCan.h"
#include "BaseGpio.h"
#include "BaseI2c.h"
#include "BaseLogger.h"
#include "BaseNvs.h"
#include "BasePeriodicTimer.h"
#include "BasePio.h"
#include "BasePwm.h"
#include "BaseSpi.h"
#include "BaseTemperature.h"
#include "BaseUart.h"
#include "BaseWifi.h"
#include "HostTest.h"

#include <cstdint>
#include <cstring>
#include <type_traits>

using hf::Status;

namespace {

struct Sample {
  float pressure_pa{0.0F};
  std::int32_t raw{0};
};

static_assert(std::is_trivially_copyable_v<hf::Result<int>>);
static_assert(std::is_trivially_copyable_v<hf::Result<Sample>>);
static_assert(std::is_trivially_copyable_v<hf::Result<void>>);
static_assert(sizeof(hf::Result<void>) == 4);
static_assert(hf::Result<int>{}.ok(), "a default Result is a success");
static_assert(!hf::Result<int>{0, Status::Bus, 3}.ok());
static_assert(hf::ToStatus(hf_spi_err_t::SPI_SUCCESS) == Status::Ok);
static_assert(hf::ToStatus(hf_i2c_err_t::I2C_ERR_DEVICE_NACK) == Status::Bus);
static_assert(hf::ToStatus(TEMP_ERR_TIMEOUT) == Status::Timeout);

hf::Result<Sample> Read(bool ok) {
  if (!ok) {
    return {{}, Status::Bus, 42};
  }
  return {{101325.0F, 7}};
}

} // namespace

HF_TEST(status_names_are_distinct_static_strings) {
  for (std::uint8_t i = 0; i < hf::kStatusCount; ++i) {
    const char* name = hf::ToString(static_cast<Status>(i));
    CHECK(name != nullptr);
    for (std::uint8_t j = 0; j < i; ++j) {
      CHECK(std::strcmp(name, hf::ToString(static_cast<Status>(j))) != 0);
    }
  }
  CHECK(std::strcmp(hf::ToString(Status::DependencyNotReady), "DependencyNotReady") == 0);
  CHECK(std::strcmp(hf::ToString(static_cast<Status>(200)), "Unknown") == 0);
}

HF_TEST(result_carries_value_status_and_detail) {
  const auto good = Read(true);
  CHECK(good.ok());
  CHECK(static_cast<bool>(good));
  CHECK(good.value.raw == 7);
  CHECK(good.detail == 0);
  const auto bad = Read(false);
  CHECK(!bad.ok());
  CHECK(bad.status == Status::Bus);
  CHECK(bad.detail == 42);
  CHECK(bad.value.raw == 0);
  const hf::Result<void> v{Status::Timeout, 9};
  CHECK(!v.ok());
  CHECK(hf::Result<void>{}.ok());
}

// Every code of every table: success and only success is Ok, and the Status is a real value.
#define HF_CHECK_CODE(NAME, VALUE, DESC)                                                           \
  {                                                                                                \
    const Status s = hf::ToStatus(static_cast<Enum>(VALUE));                                       \
    CHECK(static_cast<std::uint8_t>(s) < hf::kStatusCount);                                        \
    CHECK((s == Status::Ok) == (static_cast<Enum>(VALUE) == kSuccess));                            \
  }

#define HF_CHECK_TABLE(ENUM, SUCCESS, LIST)                                                        \
  do {                                                                                             \
    using Enum = ENUM;                                                                             \
    constexpr Enum kSuccess = SUCCESS;                                                             \
    LIST(HF_CHECK_CODE)                                                                            \
  } while (0)

HF_TEST(every_code_maps_and_only_success_is_ok) {
  HF_CHECK_TABLE(hf_adc_err_t, hf_adc_err_t::ADC_SUCCESS, HF_ADC_ERR_LIST);
  HF_CHECK_TABLE(hf_bluetooth_err_t, hf_bluetooth_err_t::BLUETOOTH_SUCCESS, HF_BLUETOOTH_ERR_LIST);
  HF_CHECK_TABLE(hf_can_err_t, hf_can_err_t::CAN_SUCCESS, HF_CAN_ERR_LIST);
  HF_CHECK_TABLE(hf_gpio_err_t, hf_gpio_err_t::GPIO_SUCCESS, HF_GPIO_ERR_LIST);
  HF_CHECK_TABLE(hf_i2c_err_t, hf_i2c_err_t::I2C_SUCCESS, HF_I2C_ERR_LIST);
  HF_CHECK_TABLE(hf_logger_err_t, hf_logger_err_t::LOGGER_SUCCESS, HF_LOGGER_ERR_LIST);
  HF_CHECK_TABLE(hf_nvs_err_t, hf_nvs_err_t::NVS_SUCCESS, HF_NVS_ERR_LIST);
  HF_CHECK_TABLE(hf_timer_err_t, hf_timer_err_t::TIMER_SUCCESS, HF_TIMER_ERR_LIST);
  HF_CHECK_TABLE(hf_pio_err_t, hf_pio_err_t::PIO_SUCCESS, HF_PIO_ERR_LIST);
  HF_CHECK_TABLE(hf_pwm_err_t, hf_pwm_err_t::PWM_SUCCESS, HF_PWM_ERR_LIST);
  HF_CHECK_TABLE(hf_spi_err_t, hf_spi_err_t::SPI_SUCCESS, HF_SPI_ERR_LIST);
  HF_CHECK_TABLE(hf_temp_err_t, TEMP_SUCCESS, HF_TEMP_ERR_LIST);
  HF_CHECK_TABLE(hf_uart_err_t, hf_uart_err_t::UART_SUCCESS, HF_UART_ERR_LIST);
  HF_CHECK_TABLE(hf_wifi_err_t, hf_wifi_err_t::WIFI_SUCCESS, HF_WIFI_ERR_LIST);
}

#undef HF_CHECK_TABLE
#undef HF_CHECK_CODE

HF_TEST(rows_of_the_standard_table) {
  CHECK(hf::ToStatus(hf_spi_err_t::SPI_ERR_NOT_INITIALIZED) == Status::NotInitialized);
  CHECK(hf::ToStatus(hf_spi_err_t::SPI_ERR_INVALID_CONFIGURATION) == Status::InvalidArgument);
  CHECK(hf::ToStatus(hf_spi_err_t::SPI_ERR_NULL_POINTER) == Status::InvalidArgument);
  CHECK(hf::ToStatus(hf_spi_err_t::SPI_ERR_BUS_BUSY) == Status::Busy);
  CHECK(hf::ToStatus(hf_spi_err_t::SPI_ERR_TRANSFER_TIMEOUT) == Status::Timeout);
  CHECK(hf::ToStatus(hf_spi_err_t::SPI_ERR_CS_CONTROL_FAILED) == Status::Bus);
  CHECK(hf::ToStatus(hf_i2c_err_t::I2C_ERR_BUS_ARBITRATION_LOST) == Status::Bus);
  CHECK(hf::ToStatus(hf_can_err_t::CAN_ERR_CRC_ERROR) == Status::Bus);
  CHECK(hf::ToStatus(hf_i2c_err_t::I2C_ERR_DEVICE_NOT_RESPONDING) == Status::NotFound);
  CHECK(hf::ToStatus(hf_spi_err_t::SPI_ERR_DEVICE_NOT_FOUND) == Status::NotFound);
  CHECK(hf::ToStatus(hf_spi_err_t::SPI_ERR_HARDWARE_FAULT) == Status::Fault);
  CHECK(hf::ToStatus(hf_spi_err_t::SPI_ERR_VOLTAGE_OUT_OF_RANGE) == Status::Fault);
  CHECK(hf::ToStatus(hf_spi_err_t::SPI_ERR_CLOCK_ERROR) == Status::Fault);
  CHECK(hf::ToStatus(hf_spi_err_t::SPI_ERR_UNSUPPORTED_OPERATION) == Status::Unsupported);
  CHECK(hf::ToStatus(hf_adc_err_t::ADC_ERR_NOT_SUPPORTED) == Status::Unsupported);
  CHECK(hf::ToStatus(hf_spi_err_t::SPI_ERR_OUT_OF_MEMORY) == Status::NoMemory);
  CHECK(hf::ToStatus(hf_spi_err_t::SPI_ERR_FAILURE) == Status::Unknown);
  CHECK(hf::ToStatus(hf_spi_err_t::SPI_ERR_SYSTEM_ERROR) == Status::Unknown);
  CHECK(hf::ToStatus(hf_gpio_err_t::GPIO_ERR_COUNT) == Status::Unknown);
}
