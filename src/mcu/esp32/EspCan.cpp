/**
 * @file EspCan.cpp
 * @brief ESP32 CAN (TWAI) implementation for the HardFOC system - ESP-IDF v5.5 Compatible.
 *
 * This file provides a comprehensive CAN bus implementation for the ESP32
 * microcontroller family using modern ESP-IDF v5.5+ TWAI node-based APIs.
 * The implementation provides advanced features for ESP32 with external SN65 transceivers.
 *
 * Key Features Implemented:
 * - ESP-IDF v5.5+ handle-based TWAI node API
 * - ESP32 compatible TWAI controller support
 * - Event-driven callback-based message reception
 * - Advanced acceptance filtering (single/dual mask modes)
 * - Comprehensive error detection and bus recovery
 * - Advanced bit timing configuration for various baud rates
 * - Thread-safe operations with proper resource management
 * - Support for external SN65 CAN transceivers
 * - Comprehensive diagnostics and performance monitoring
 *
 * @author Nebiyu Tadesse
 * @date 2025
 * @copyright HardFOC
 *
 * @note This implementation requires ESP-IDF v5.5 or later
 * @note Requires ESP-IDF v5.5 or later for full feature support
 */

#include "EspCan.h"

// C++ standard library headers (must be outside extern "C")
#include <algorithm>
#include <climits>
#include <cstring>

// ESP-IDF C headers must be wrapped in extern "C" for C++ compatibility
#ifdef __cplusplus
extern "C" {
#endif

// ESP32-specific includes via centralized McuSelect.h (included in EspCan.h)
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#ifdef __cplusplus
}
#endif

static const char* TAG = "EspCan";

namespace {

/** @brief Milliseconds to ticks, rounded up so a non-zero timeout never becomes 0 ticks. */
TickType_t MsToTicksCeil(hf_u32_t timeout_ms) noexcept {
  if (timeout_ms == UINT32_MAX) {
    return portMAX_DELAY;
  }
  const uint64_t ticks = (static_cast<uint64_t>(timeout_ms) * configTICK_RATE_HZ + 999U) / 1000U;
  return ticks >= portMAX_DELAY ? (portMAX_DELAY - 1U) : static_cast<TickType_t>(ticks);
}

/** @brief BaseCan timeout to the TWAI driver's int timeout (-1 = forever). */
int ToTwaiTimeout(hf_u32_t timeout_ms) noexcept {
  if (timeout_ms == UINT32_MAX) {
    return -1;
  }
  return timeout_ms > static_cast<hf_u32_t>(INT_MAX) ? INT_MAX : static_cast<int>(timeout_ms);
}

/** @brief TWAI driver error state to hf_can_bus_state_t. */
hf_can_bus_state_t MapTwaiState(twai_error_state_t state) noexcept {
  switch (state) {
  case TWAI_ERROR_ACTIVE:
    return hf_can_bus_state_t::ERROR_ACTIVE;
  case TWAI_ERROR_WARNING:
    return hf_can_bus_state_t::ERROR_WARNING;
  case TWAI_ERROR_PASSIVE:
    return hf_can_bus_state_t::ERROR_PASSIVE;
  case TWAI_ERROR_BUS_OFF:
    return hf_can_bus_state_t::BUS_OFF;
  default:
    return hf_can_bus_state_t::STOPPED;
  }
}

} // namespace

//==============================================================================
// CONSTRUCTOR AND DESTRUCTOR - ESP-IDF v5.5 Compatible
//==============================================================================

EspCan::EspCan(const hf_esp_can_config_t& config) noexcept
    : BaseCan(), config_(config), is_enabled_(false), is_recovering_(false), config_mutex_(),
      stats_mutex_(), callback_mutex_(), twai_node_handle_(nullptr), receive_cb_{},
      receive_ud_(nullptr), error_cb_{}, error_ud_(nullptr), state_cb_{}, state_ud_(nullptr),
      tx_cb_{}, tx_ud_(nullptr), statistics_{}, diagnostics_{}, advanced_timing_{},
      current_filter_{}, filter_configured_(false) {
  // **LAZY INITIALIZATION** - Store configuration but do NOT initialize hardware
  // This follows the same pattern as EspAdc
  ESP_LOGD(TAG, "Creating EspCan for controller %d - LAZY INIT",
           static_cast<int>(config_.controller_id));
}

EspCan::~EspCan() noexcept {
  // Ensure proper cleanup following EspAdc pattern
  if (initialized_) {
    ESP_LOGW(TAG, "EspCan destructor called on initialized instance - performing cleanup");
    Deinitialize();
  }

  // Clear callbacks (single-callback design)
  ClearReceiveCallbackEx();
  ClearErrorCallback();
  ClearStateChangeCallback();
  ClearTxCompleteCallback();

  ESP_LOGD(TAG, "EspCan controller %d destroyed", static_cast<int>(config_.controller_id));
}

//==============================================//
// INITIALIZATION AND CONFIGURATION
//==============================================//

hf_can_err_t EspCan::Initialize() noexcept {
  PlatformMutexLockGuard lock(config_mutex_);

  if (initialized_) {
    ESP_LOGD(TAG, "TWAI node %u already initialized", static_cast<unsigned>(config_.controller_id));
    return hf_can_err_t::CAN_SUCCESS;
  }

  ESP_LOGD(TAG, "Initializing TWAI node %u", static_cast<unsigned>(config_.controller_id));

  // RX ring, TX slot pool and semaphores exist before the first interrupt can fire
  hf_can_err_t queue_result = AllocateQueues();
  if (queue_result != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to allocate RX/TX queues");
    return queue_result;
  }

  // Create TWAI node configuration using ESP-IDF v5.5 API
  twai_onchip_node_config_t node_config = {
      .io_cfg =
          {
              .tx = static_cast<gpio_num_t>(config_.tx_pin),
              .rx = static_cast<gpio_num_t>(config_.rx_pin),
              .quanta_clk_out = GPIO_NUM_NC,
              .bus_off_indicator = GPIO_NUM_NC,
          },
      .clk_src = TWAI_CLK_SRC_DEFAULT,
      .bit_timing =
          {
              .bitrate = config_.baud_rate,
              .sp_permill = static_cast<uint16_t>(config_.sample_point_permill),
              .ssp_permill = static_cast<uint16_t>(config_.secondary_sample_point),
          },
      .data_timing = {},
      .fail_retry_cnt = config_.fail_retry_cnt,
      .tx_queue_depth = config_.tx_queue_depth,
      .intr_priority = config_.intr_priority,
      .flags =
          {
              .enable_self_test = config_.enable_self_test,
              .enable_loopback = config_.enable_loopback,
              .enable_listen_only = config_.enable_listen_only,
              .no_receive_rtr = config_.no_receive_rtr,
          },
  };

  // Create TWAI node using ESP-IDF v5.5 API
  esp_err_t esp_err = twai_new_node_onchip(&node_config, &twai_node_handle_);
  if (esp_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create TWAI node: %s", esp_err_to_name(esp_err));
    ReleaseQueues();
    return ConvertEspError(esp_err);
  }

  // Register event callbacks. on_tx_done returns each frame's TX slot to the pool.
  twai_event_callbacks_t callbacks = {
      .on_tx_done = InternalTxDoneCallback,
      .on_rx_done = InternalReceiveCallback,
      .on_state_change = InternalStateChangeCallback,
      .on_error = InternalErrorCallback,
  };

  esp_err = twai_node_register_event_callbacks(twai_node_handle_, &callbacks, this);
  if (esp_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register event callbacks: %s", esp_err_to_name(esp_err));
    twai_node_delete(twai_node_handle_);
    twai_node_handle_ = nullptr;
    ReleaseQueues();
    return ConvertEspError(esp_err);
  }

  tx_queue_full_.store(0);
  bus_off_count_.store(0);
  bus_state_.store(static_cast<uint8_t>(hf_can_bus_state_t::ERROR_ACTIVE));

  // Enable the TWAI node
  esp_err = twai_node_enable(twai_node_handle_);
  if (esp_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable TWAI node: %s", esp_err_to_name(esp_err));
    twai_node_delete(twai_node_handle_);
    twai_node_handle_ = nullptr;
    ReleaseQueues();
    bus_state_.store(static_cast<uint8_t>(hf_can_bus_state_t::STOPPED));
    return ConvertEspError(esp_err);
  }

  // Initialize statistics (atomic operations - no mutex needed)
  diagnostics_ = hf_can_diagnostics_t{};

  is_enabled_.store(true);
  RefreshBusState();
  initialized_ = true; // Update base class initialization state
  ESP_LOGI(TAG, "TWAI node %u initialized (rx ring %u, tx slots %u)",
           static_cast<unsigned>(config_.controller_id), static_cast<unsigned>(rx_ring_.Capacity()),
           static_cast<unsigned>(tx_slot_count_));
  return hf_can_err_t::CAN_SUCCESS;
}

hf_can_err_t EspCan::Deinitialize() noexcept {
  PlatformMutexLockGuard lock(config_mutex_);

  if (!initialized_) {
    ESP_LOGD(TAG, "TWAI node %u already deinitialized",
             static_cast<unsigned>(config_.controller_id));
    return hf_can_err_t::CAN_SUCCESS;
  }

  ESP_LOGD(TAG, "Deinitializing TWAI node %u", static_cast<unsigned>(config_.controller_id));

  // Clear callbacks
  ClearReceiveCallbackEx();
  ClearErrorCallback();
  ClearStateChangeCallback();
  ClearTxCompleteCallback();
  base_receive_cb_.store(nullptr);

  // Disable and delete TWAI node
  if (twai_node_handle_ != nullptr) {
    if (is_enabled_.load()) {
      twai_node_disable(twai_node_handle_);
      is_enabled_.store(false);
    }

    esp_err_t esp_err = twai_node_delete(twai_node_handle_);
    if (esp_err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to delete TWAI node: %s", esp_err_to_name(esp_err));
    }
    twai_node_handle_ = nullptr;
  }

  // The node (and its interrupt) is gone, so no ISR can touch the queues now
  ReleaseQueues();
  bus_state_.store(static_cast<uint8_t>(hf_can_bus_state_t::STOPPED));

  initialized_ = false; // Update base class initialization state
  filter_configured_ = false;
  ESP_LOGI(TAG, "TWAI node %u deinitialized successfully",
           static_cast<unsigned>(config_.controller_id));
  return hf_can_err_t::CAN_SUCCESS;
}

//==============================================//
// CORE CAN OPERATIONS (From BaseCan interface)
//==============================================//

hf_can_err_t EspCan::SendMessage(const hf_can_message_t& message, hf_u32_t timeout_ms) noexcept {
  if (!initialized_ || tx_slot_sem_ == nullptr) {
    return hf_can_err_t::CAN_ERR_NOT_INITIALIZED;
  }

  // TWAI on this target is classic CAN only
  if (message.is_canfd) {
    UpdateStatistics(hf_can_operation_type_t::HF_CAN_OP_SEND, false);
    return hf_can_err_t::CAN_ERR_UNSUPPORTED_OPERATION;
  }
  switch (message.Validate()) {
  case hf_can_frame_error_t::NONE:
    break;
  case hf_can_frame_error_t::ID_RANGE:
    UpdateStatistics(hf_can_operation_type_t::HF_CAN_OP_SEND, false);
    return hf_can_err_t::CAN_ERR_MESSAGE_INVALID_ID;
  default:
    UpdateStatistics(hf_can_operation_type_t::HF_CAN_OP_SEND, false);
    return hf_can_err_t::CAN_ERR_MESSAGE_INVALID_DLC;
  }

  if (bus_state_.load() == static_cast<uint8_t>(hf_can_bus_state_t::BUS_OFF)) {
    UpdateStatistics(hf_can_operation_type_t::HF_CAN_OP_SEND, false);
    return hf_can_err_t::CAN_ERR_BUS_OFF;
  }

  // Wait (or not, for timeout 0) for a free TX slot. The driver keeps a pointer
  // to the frame until transmission completes, so the frame must live in a slot.
  if (xSemaphoreTake(tx_slot_sem_, MsToTicksCeil(timeout_ms)) != pdTRUE) {
    tx_queue_full_.fetch_add(1);
    statistics_.tx_queue_overflows.fetch_add(1);
    UpdateStatistics(hf_can_operation_type_t::HF_CAN_OP_SEND, false);
    return hf_can_err_t::CAN_ERR_QUEUE_FULL;
  }

  TxSlot* slot = nullptr;
  for (uint32_t i = 0; i < tx_slot_count_; ++i) {
    bool expected = false;
    if (tx_slots_[i].in_use.compare_exchange_strong(expected, true)) {
      slot = &tx_slots_[i];
      break;
    }
  }
  if (slot == nullptr) {
    // Cannot happen while the semaphore count matches the free slots
    xSemaphoreGive(tx_slot_sem_);
    UpdateStatistics(hf_can_operation_type_t::HF_CAN_OP_SEND, false);
    return hf_can_err_t::CAN_ERR_RESOURCE_BUSY;
  }

  std::memset(&slot->frame, 0, sizeof(slot->frame));
  slot->frame.buffer = slot->buffer;
  (void)ConvertToTwaiFrame(message, slot->frame); // validated above

  // Every in-flight frame owns a slot and there are tx_queue_depth + 1 slots,
  // so the driver queue normally has room; the timeout only covers a race on
  // dual-core targets.
  esp_err_t esp_err =
      twai_node_transmit(twai_node_handle_, &slot->frame, ToTwaiTimeout(timeout_ms));
  const bool success = (esp_err == ESP_OK);
  if (!success) {
    slot->in_use.store(false);
    xSemaphoreGive(tx_slot_sem_);
  }
  UpdateStatistics(hf_can_operation_type_t::HF_CAN_OP_SEND, success);
  if (success) {
    statistics_.bytes_transmitted.fetch_add(message.is_rtr ? 0U : message.dlc);
  }

  // TX callback: the frame was accepted by (or refused by) the driver queue
  if (tx_cb_ != nullptr) {
    hf_esp_can_tx_info_t tx_info;
    tx_info.message = message;
    tx_info.success = success;
    tx_info.timestamp_us = esp_timer_get_time();
    DispatchTxCompleteCallbacks(tx_info);
  }

  if (!success) {
    if (esp_err == ESP_ERR_TIMEOUT) {
      tx_queue_full_.fetch_add(1);
      return hf_can_err_t::CAN_ERR_QUEUE_FULL;
    }
    if (esp_err == ESP_ERR_INVALID_STATE) {
      return hf_can_err_t::CAN_ERR_BUS_OFF;
    }
    return ConvertEspError(esp_err);
  }
  return hf_can_err_t::CAN_SUCCESS;
}

hf_can_err_t EspCan::ReceiveMessage(hf_can_message_t& message, hf_u32_t timeout_ms) noexcept {
  if (!initialized_ || rx_sem_ == nullptr) {
    return hf_can_err_t::CAN_ERR_NOT_INITIALIZED;
  }

  PlatformMutexLockGuard lock(rx_mutex_);

  TickType_t ticks_left = MsToTicksCeil(timeout_ms);
  TimeOut_t timeout_state;
  vTaskSetTimeOutState(&timeout_state);

  for (;;) {
    if (rx_ring_.TryPop(message)) {
      return hf_can_err_t::CAN_SUCCESS;
    }
    if (timeout_ms == 0U) {
      return hf_can_err_t::CAN_ERR_QUEUE_EMPTY;
    }
    if (xTaskCheckForTimeOut(&timeout_state, &ticks_left) != pdFALSE) {
      return hf_can_err_t::CAN_ERR_MESSAGE_TIMEOUT;
    }
    // A stale give (frame already taken) only costs one more loop
    (void)xSemaphoreTake(rx_sem_, ticks_left);
  }
}

hf_can_err_t EspCan::SetReceiveCallback(hf_can_receive_callback_t callback) noexcept {
  if (callback == nullptr) {
    return hf_can_err_t::CAN_ERR_INVALID_PARAMETER;
  }
  base_receive_cb_.store(callback);
  return hf_can_err_t::CAN_SUCCESS;
}

void EspCan::ClearReceiveCallback() noexcept {
  base_receive_cb_.store(nullptr);
}

//==============================================================================
// SINGLE-CALLBACK PER EVENT MANAGEMENT
//==============================================================================

hf_can_err_t EspCan::SetReceiveCallbackEx(hf_esp_can_receive_callback_t cb,
                                          void* user_data) noexcept {
  if (!cb)
    return hf_can_err_t::CAN_ERR_INVALID_PARAMETER;
  PlatformMutexLockGuard lock(callback_mutex_);
  // The RX ISR reads the callback first, so publish the user data before it
  receive_cb_.store(nullptr);
  receive_ud_.store(user_data);
  receive_cb_.store(cb);
  return hf_can_err_t::CAN_SUCCESS;
}
void EspCan::ClearReceiveCallbackEx() noexcept {
  PlatformMutexLockGuard lock(callback_mutex_);
  receive_cb_.store(nullptr);
  receive_ud_.store(nullptr);
}

hf_can_err_t EspCan::SetErrorCallback(hf_esp_can_error_callback_t cb, void* user_data) noexcept {
  if (!cb)
    return hf_can_err_t::CAN_ERR_INVALID_PARAMETER;
  PlatformMutexLockGuard lock(callback_mutex_);
  error_cb_ = cb;
  error_ud_ = user_data;
  return hf_can_err_t::CAN_SUCCESS;
}
void EspCan::ClearErrorCallback() noexcept {
  PlatformMutexLockGuard lock(callback_mutex_);
  error_cb_ = nullptr;
  error_ud_ = nullptr;
}

hf_can_err_t EspCan::SetStateChangeCallback(hf_esp_can_state_callback_t cb,
                                            void* user_data) noexcept {
  if (!cb)
    return hf_can_err_t::CAN_ERR_INVALID_PARAMETER;
  PlatformMutexLockGuard lock(callback_mutex_);
  state_cb_ = cb;
  state_ud_ = user_data;
  return hf_can_err_t::CAN_SUCCESS;
}
void EspCan::ClearStateChangeCallback() noexcept {
  PlatformMutexLockGuard lock(callback_mutex_);
  state_cb_ = nullptr;
  state_ud_ = nullptr;
}

hf_can_err_t EspCan::SetTxCompleteCallback(hf_esp_can_tx_callback_t cb, void* user_data) noexcept {
  if (!cb)
    return hf_can_err_t::CAN_ERR_INVALID_PARAMETER;
  PlatformMutexLockGuard lock(callback_mutex_);
  tx_cb_ = cb;
  tx_ud_ = user_data;
  return hf_can_err_t::CAN_SUCCESS;
}
void EspCan::ClearTxCompleteCallback() noexcept {
  PlatformMutexLockGuard lock(callback_mutex_);
  tx_cb_ = nullptr;
  tx_ud_ = nullptr;
}

hf_can_err_t EspCan::GetStatus(hf_can_status_t& status) noexcept {
  if (!initialized_) {
    return hf_can_err_t::CAN_ERR_NOT_INITIALIZED;
  }

  // Get TWAI node info using ESP-IDF v5.5 API
  twai_node_status_t node_status;
  twai_node_record_t node_record;
  esp_err_t esp_err = twai_node_get_info(twai_node_handle_, &node_status, &node_record);
  if (esp_err != ESP_OK) {
    return ConvertEspError(esp_err);
  }

  // Convert to CAN bus status - match BaseCan.h structure
  status.tx_error_count = node_status.tx_error_count;
  status.rx_error_count = node_status.rx_error_count;
  status.tx_failed_count = statistics_.send_failures.load();
  status.rx_missed_count = rx_ring_.Overruns();

  // Set state flags based on TWAI state
  switch (node_status.state) {
  case TWAI_ERROR_BUS_OFF:
    status.bus_off = true;
    status.error_warning = false;
    status.error_passive = false;
    break;
  case TWAI_ERROR_ACTIVE:
    status.bus_off = false;
    status.error_warning = false;
    status.error_passive = false;
    break;
  case TWAI_ERROR_WARNING:
    status.bus_off = false;
    status.error_warning = true;
    status.error_passive = false;
    break;
  case TWAI_ERROR_PASSIVE:
    status.bus_off = false;
    status.error_warning = true;
    status.error_passive = true;
    break;
  default:
    status.bus_off = false;
    status.error_warning = false;
    status.error_passive = false;
    break;
  }

  return hf_can_err_t::CAN_SUCCESS;
}

hf_can_err_t EspCan::Reset() noexcept {
  if (!initialized_) {
    return hf_can_err_t::CAN_ERR_NOT_INITIALIZED;
  }

  PlatformMutexLockGuard lock(config_mutex_);

  ESP_LOGI(TAG, "Resetting TWAI node %u", static_cast<unsigned>(config_.controller_id));

  // Reset statistics and the RX/TX/bus-off counters
  (void)ResetStatistics();
  diagnostics_ = hf_can_diagnostics_t{};

  // Check if node is in bus-off state before attempting recovery
  twai_node_status_t node_status;
  twai_node_record_t node_record;
  esp_err_t status_err = twai_node_get_info(twai_node_handle_, &node_status, &node_record);

  if (status_err == ESP_OK && node_status.state == TWAI_ERROR_BUS_OFF) {
    // Node is in bus-off state, recovery is appropriate
    esp_err_t esp_err = twai_node_recover(twai_node_handle_);
    if (esp_err != ESP_OK) {
      ESP_LOGE(TAG, "TWAI node recovery failed: %s", esp_err_to_name(esp_err));
      return ConvertEspError(esp_err);
    }
    ESP_LOGI(TAG, "TWAI node %u recovered from bus-off state",
             static_cast<unsigned>(config_.controller_id));
  } else {
    // Node is not in bus-off state, just reset statistics and continue
    ESP_LOGI(TAG, "TWAI node %u reset (statistics cleared, node operational)",
             static_cast<unsigned>(config_.controller_id));
  }

  return hf_can_err_t::CAN_SUCCESS;
}

hf_can_err_t EspCan::SetAcceptanceFilter(hf_u32_t id, hf_u32_t mask, bool extended) noexcept {
  if (!initialized_) {
    return hf_can_err_t::CAN_ERR_NOT_INITIALIZED;
  }

  hf_esp_can_filter_config_t filter_config;
  filter_config.id = id;
  filter_config.mask = mask;
  filter_config.is_extended = extended;
  filter_config.is_dual_filter = false;

  return ConfigureAdvancedFilter(filter_config);
}

hf_can_err_t EspCan::ClearAcceptanceFilter() noexcept {
  if (!initialized_) {
    return hf_can_err_t::CAN_ERR_NOT_INITIALIZED;
  }

  // Set filter to accept all messages (ID=0, Mask=0)
  return SetAcceptanceFilter(0, 0, false);
}

//==============================================//
// STATISTICS AND DIAGNOSTICS
//==============================================//

hf_can_err_t EspCan::GetStatistics(hf_can_statistics_t& stats) noexcept {
  // Copy atomic values to non-atomic structure for return (atomic operations are thread-safe)
  stats.messages_sent = statistics_.messages_sent.load();
  stats.messages_received = statistics_.messages_received.load();
  stats.bytes_transmitted = statistics_.bytes_transmitted.load();
  stats.bytes_received = statistics_.bytes_received.load();
  stats.send_failures = statistics_.send_failures.load();
  stats.receive_failures = statistics_.receive_failures.load();
  stats.bus_error_count = statistics_.bus_error_count.load();
  stats.arbitration_lost_count = statistics_.arbitration_lost_count.load();
  stats.tx_failed_count = statistics_.tx_failed_count.load();
  stats.bus_off_events = statistics_.bus_off_events.load();
  stats.error_warning_events = statistics_.error_warning_events.load();
  stats.uptime_seconds = statistics_.uptime_seconds.load();
  stats.last_activity_timestamp = statistics_.last_activity_timestamp.load();
  stats.last_error = statistics_.last_error.load();
  stats.tx_queue_peak = statistics_.tx_queue_peak.load();
  stats.rx_queue_peak = statistics_.rx_queue_peak.load();
  stats.tx_queue_overflows = statistics_.tx_queue_overflows.load();
  stats.rx_queue_overflows = statistics_.rx_queue_overflows.load();
  return hf_can_err_t::CAN_SUCCESS;
}

hf_can_err_t EspCan::ResetStatistics() noexcept {
  // Reset all atomic counters to zero (atomic operations are thread-safe)
  statistics_.messages_sent.store(0);
  statistics_.messages_received.store(0);
  statistics_.bytes_transmitted.store(0);
  statistics_.bytes_received.store(0);
  statistics_.send_failures.store(0);
  statistics_.receive_failures.store(0);
  statistics_.bus_error_count.store(0);
  statistics_.arbitration_lost_count.store(0);
  statistics_.tx_failed_count.store(0);
  statistics_.bus_off_events.store(0);
  statistics_.error_warning_events.store(0);
  statistics_.uptime_seconds.store(0);
  statistics_.last_activity_timestamp.store(0);
  statistics_.last_error.store(hf_can_err_t::CAN_SUCCESS);
  statistics_.tx_queue_peak.store(0);
  statistics_.rx_queue_peak.store(0);
  statistics_.tx_queue_overflows.store(0);
  statistics_.rx_queue_overflows.store(0);
  rx_ring_.ResetCounters();
  tx_queue_full_.store(0);
  bus_off_count_.store(0);
  return hf_can_err_t::CAN_SUCCESS;
}

hf_can_err_t EspCan::GetDiagnostics(hf_can_diagnostics_t& diagnostics) noexcept {
  if (!initialized_) {
    return hf_can_err_t::CAN_ERR_NOT_INITIALIZED;
  }

  // No mutex needed - diagnostics are not updated from ISR context

  // Update diagnostics with current TWAI node status
  twai_node_status_t node_status;
  twai_node_record_t node_record;
  esp_err_t esp_err = twai_node_get_info(twai_node_handle_, &node_status, &node_record);
  if (esp_err == ESP_OK) {
    diagnostics_.tx_error_count = node_status.tx_error_count;
    diagnostics_.rx_error_count = node_status.rx_error_count;
    diagnostics_.last_error_timestamp = esp_timer_get_time() / 1000; // Convert to milliseconds
  }

  diagnostics = diagnostics_;
  return hf_can_err_t::CAN_SUCCESS;
}

//==============================================//
// ESP-IDF v5.5 SPECIFIC ADVANCED FEATURES
//==============================================//

hf_can_err_t EspCan::ConfigureAdvancedTiming(
    const hf_esp_can_timing_config_t& timing_config) noexcept {
  if (!initialized_) {
    return hf_can_err_t::CAN_ERR_NOT_INITIALIZED;
  }

  PlatformMutexLockGuard lock(config_mutex_);

  // ESP-IDF v5.5 requires node to be stopped for timing configuration
  bool was_enabled = is_enabled_.load();
  if (was_enabled) {
    esp_err_t disable_err = twai_node_disable(twai_node_handle_);
    if (disable_err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to disable node for timing config: %s", esp_err_to_name(disable_err));
      return ConvertEspError(disable_err);
    }
    is_enabled_.store(false);
  }

  // Configure advanced timing for improved signal quality.
  // twai_node_reconfig_timing() takes twai_timing_advanced_config_t. In early v5.5.x it is an
  // alias of twai_timing_config_t (clk_src, quanta_resolution_hz, triple_sampling, ...); later
  // v5.5.x releases made it a standalone struct with only the segment fields. Zero-initialise and
  // set the common fields so both layouts build (zeroed extras select the default clock source).
  twai_timing_advanced_config_t esp_timing_config = {};
  esp_timing_config.brp = static_cast<uint32_t>(timing_config.brp);
  esp_timing_config.prop_seg = static_cast<uint8_t>(timing_config.prop_seg);
  esp_timing_config.tseg_1 = static_cast<uint8_t>(timing_config.tseg_1);
  esp_timing_config.tseg_2 = static_cast<uint8_t>(timing_config.tseg_2);
  esp_timing_config.sjw = static_cast<uint8_t>(timing_config.sjw);
  esp_timing_config.ssp_offset = static_cast<uint8_t>(timing_config.ssp_offset);

  esp_err_t esp_err = twai_node_reconfig_timing(twai_node_handle_, &esp_timing_config, nullptr);

  // Re-enable node if it was enabled before
  if (was_enabled) {
    esp_err_t enable_err = twai_node_enable(twai_node_handle_);
    if (enable_err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to re-enable node after timing config: %s",
               esp_err_to_name(enable_err));
      return ConvertEspError(enable_err);
    }
    is_enabled_.store(true);
    RefreshBusState();
  }

  if (esp_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure advanced timing: %s", esp_err_to_name(esp_err));
    return ConvertEspError(esp_err);
  }

  // Store configuration
  advanced_timing_ = timing_config;

  ESP_LOGI(TAG, "Advanced timing configured for TWAI node %u",
           static_cast<unsigned>(config_.controller_id));
  return hf_can_err_t::CAN_SUCCESS;
}

hf_can_err_t EspCan::ConfigureAdvancedFilter(
    const hf_esp_can_filter_config_t& filter_config) noexcept {
  if (!initialized_) {
    return hf_can_err_t::CAN_ERR_NOT_INITIALIZED;
  }

  PlatformMutexLockGuard lock(config_mutex_);

  // ESP-IDF v5.5 requires node to be stopped for filter configuration
  bool was_enabled = is_enabled_.load();
  if (was_enabled) {
    esp_err_t disable_err = twai_node_disable(twai_node_handle_);
    if (disable_err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to disable node for filter config: %s", esp_err_to_name(disable_err));
      return ConvertEspError(disable_err);
    }
    is_enabled_.store(false);
  }

  twai_mask_filter_config_t mask_filter;

  if (filter_config.is_dual_filter) {
    // Configure dual filter mode
    mask_filter = twai_make_dual_filter(filter_config.id, filter_config.mask, filter_config.id2,
                                        filter_config.mask2, filter_config.is_extended);
  } else {
    // Configure single filter mode
    mask_filter.id = filter_config.id;
    mask_filter.mask = filter_config.mask;
    mask_filter.is_ext = filter_config.is_extended;
  }

  esp_err_t esp_err = twai_node_config_mask_filter(twai_node_handle_, 0, &mask_filter);

  // Re-enable node if it was enabled before
  if (was_enabled) {
    esp_err_t enable_err = twai_node_enable(twai_node_handle_);
    if (enable_err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to re-enable node after filter config: %s",
               esp_err_to_name(enable_err));
      return ConvertEspError(enable_err);
    }
    is_enabled_.store(true);
    RefreshBusState();
  }

  if (esp_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure filter: %s", esp_err_to_name(esp_err));
    return ConvertEspError(esp_err);
  }

  // Store configuration
  current_filter_ = filter_config;
  filter_configured_ = true;

  ESP_LOGI(TAG, "Filter configured for TWAI node %u (ID: 0x%X, Mask: 0x%X, Extended: %s, Dual: %s)",
           static_cast<unsigned>(config_.controller_id), filter_config.id, filter_config.mask,
           filter_config.is_extended ? "yes" : "no", filter_config.is_dual_filter ? "yes" : "no");
  return hf_can_err_t::CAN_SUCCESS;
}

hf_can_err_t EspCan::InitiateBusRecovery() noexcept {
  if (!initialized_) {
    return hf_can_err_t::CAN_ERR_NOT_INITIALIZED;
  }

  ESP_LOGI(TAG, "Initiating bus recovery for TWAI node %u",
           static_cast<unsigned>(config_.controller_id));

  // Check if node is in bus-off state before attempting recovery
  twai_node_status_t node_status;
  twai_node_record_t node_record;
  esp_err_t status_err = twai_node_get_info(twai_node_handle_, &node_status, &node_record);

  if (status_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get node status for recovery: %s", esp_err_to_name(status_err));
    return ConvertEspError(status_err);
  }

  if (node_status.state != TWAI_ERROR_BUS_OFF) {
    ESP_LOGD(TAG, "Node is not in bus-off state (current: %d), recovery not needed",
             node_status.state);
    return hf_can_err_t::CAN_SUCCESS; // Not an error, just not needed
  }

  is_recovering_.store(true);

  esp_err_t esp_err = twai_node_recover(twai_node_handle_);
  if (esp_err != ESP_OK) {
    is_recovering_.store(false);
    ESP_LOGE(TAG, "Failed to initiate bus recovery: %s", esp_err_to_name(esp_err));
    return ConvertEspError(esp_err);
  }

  ESP_LOGI(TAG, "Bus recovery initiated successfully for TWAI node %u",
           static_cast<unsigned>(config_.controller_id));
  return hf_can_err_t::CAN_SUCCESS;
}

hf_can_err_t EspCan::GetBusStatus(hf_can_bus_status_t& status) noexcept {
  if (!initialized_) {
    return hf_can_err_t::CAN_ERR_NOT_INITIALIZED;
  }

  twai_node_status_t node_status{};
  esp_err_t esp_err = twai_node_get_info(twai_node_handle_, &node_status, nullptr);
  if (esp_err != ESP_OK) {
    return ConvertEspError(esp_err);
  }

  // The driver reports a disabled node as bus-off; report it as stopped
  status.state = is_enabled_.load() ? MapTwaiState(node_status.state) : hf_can_bus_state_t::STOPPED;
  status.tec = node_status.tx_error_count;
  status.rec = node_status.rx_error_count;
  status.rx_overruns = rx_ring_.Overruns();
  status.tx_queue_full = tx_queue_full_.load();
  status.bus_off_count = bus_off_count_.load();
  return hf_can_err_t::CAN_SUCCESS;
}

hf_can_err_t EspCan::RequestBusRecovery() noexcept {
  return InitiateBusRecovery();
}

hf_can_err_t EspCan::GetNodeInfo(twai_node_record_t& node_info) noexcept {
  if (!initialized_) {
    return hf_can_err_t::CAN_ERR_NOT_INITIALIZED;
  }

  // Get both status and record info
  twai_node_status_t node_status;
  esp_err_t esp_err = twai_node_get_info(twai_node_handle_, &node_status, &node_info);
  if (esp_err != ESP_OK) {
    return ConvertEspError(esp_err);
  }

  return hf_can_err_t::CAN_SUCCESS;
}

uint32_t EspCan::SendMessageBatch(const hf_can_message_t* messages, uint32_t count,
                                  uint32_t timeout_ms) noexcept {
  if (!messages || count == 0 || !initialized_) {
    return 0;
  }

  uint32_t sent_count = 0;
  for (uint32_t i = 0; i < count; ++i) {
    if (SendMessage(messages[i], timeout_ms) == hf_can_err_t::CAN_SUCCESS) {
      sent_count++;
    } else {
      break; // Stop on first failure to maintain order
    }
  }

  return sent_count;
}

//==============================================//
// INTERNAL EVENT CALLBACKS (ESP-IDF v5.5)
//==============================================//
// All of these run in the TWAI interrupt. No logging, no blocking, no
// allocation: only atomics, the lock-free RX ring and FromISR primitives.

bool EspCan::InternalReceiveCallback(twai_node_handle_t handle,
                                     const twai_rx_done_event_data_t* event_data,
                                     void* user_ctx) noexcept {
  (void)event_data;
  EspCan* esp_can = static_cast<EspCan*>(user_ctx);
  if (!esp_can) {
    return false;
  }

  // The driver signals one frame per call; it must be read here, in the callback
  uint8_t recv_buffer[HF_CAN_CLASSIC_MAX_LEN] = {};
  twai_frame_t rx_frame = {};
  rx_frame.buffer = recv_buffer;
  rx_frame.buffer_len = sizeof(recv_buffer);

  if (twai_node_receive_from_isr(handle, &rx_frame) != ESP_OK) {
    esp_can->UpdateStatistics(hf_can_operation_type_t::HF_CAN_OP_RECEIVE, false);
    return false;
  }

  hf_can_message_t message;
  (void)ConvertFromTwaiFrame(rx_frame, message);
  message.timestamp_us = static_cast<hf_u64_t>(esp_timer_get_time());
  message.controller_id = static_cast<hf_u8_t>(esp_can->config_.controller_id);

  esp_can->UpdateStatistics(hf_can_operation_type_t::HF_CAN_OP_RECEIVE, true);
  esp_can->statistics_.bytes_received.fetch_add(message.is_rtr ? 0U : message.dlc);

  return esp_can->DeliverReceivedFromIsr(message);
}

bool EspCan::InternalTxDoneCallback(twai_node_handle_t handle,
                                    const twai_tx_done_event_data_t* event_data,
                                    void* user_ctx) noexcept {
  (void)handle;
  EspCan* esp_can = static_cast<EspCan*>(user_ctx);
  if (!esp_can || !event_data || !esp_can->tx_slots_) {
    return false;
  }

  if (!event_data->is_tx_success) {
    esp_can->statistics_.tx_failed_count.fetch_add(1);
  }

  // Find the slot that owns this frame and return it to the pool
  const auto* done = event_data->done_tx_frame;
  for (uint32_t i = 0; i < esp_can->tx_slot_count_; ++i) {
    TxSlot& slot = esp_can->tx_slots_[i];
    if (&slot.frame == done) {
      slot.in_use.store(false);
      BaseType_t woken = pdFALSE;
      xSemaphoreGiveFromISR(esp_can->tx_slot_sem_, &woken);
      return woken == pdTRUE;
    }
  }
  return false;
}

bool EspCan::InternalErrorCallback(twai_node_handle_t handle,
                                   const twai_error_event_data_t* event_data,
                                   void* user_ctx) noexcept {
  (void)handle;
  EspCan* esp_can = static_cast<EspCan*>(user_ctx);
  if (!esp_can || !event_data) {
    return false;
  }

  if (event_data->err_flags.arb_lost) {
    esp_can->statistics_.arbitration_lost_count.fetch_add(1);
  }
  esp_can->UpdateErrorStatistics(event_data->err_flags.val);
  return false;
}

bool EspCan::InternalStateChangeCallback(twai_node_handle_t handle,
                                         const twai_state_change_event_data_t* event_data,
                                         void* user_ctx) noexcept {
  (void)handle;
  EspCan* esp_can = static_cast<EspCan*>(user_ctx);
  if (!esp_can || !event_data) {
    return false;
  }

  const hf_can_bus_state_t new_state = MapTwaiState(event_data->new_sta);
  esp_can->bus_state_.store(static_cast<uint8_t>(new_state));

  if (event_data->new_sta == TWAI_ERROR_BUS_OFF && event_data->old_sta != TWAI_ERROR_BUS_OFF) {
    esp_can->bus_off_count_.fetch_add(1);
    esp_can->statistics_.bus_off_events.fetch_add(1);
  }
  if (event_data->new_sta == TWAI_ERROR_WARNING) {
    esp_can->statistics_.error_warning_events.fetch_add(1);
  }

  // Bus recovery finished
  if (esp_can->is_recovering_.load() && event_data->new_sta == TWAI_ERROR_ACTIVE) {
    esp_can->is_recovering_.store(false);
  }

  hf_esp_can_state_info_t state_info;
  state_info.previous_state = static_cast<uint32_t>(event_data->old_sta);
  state_info.current_state = static_cast<uint32_t>(event_data->new_sta);
  state_info.timestamp_us = esp_timer_get_time();
  esp_can->DispatchStateChangeCallbacks(state_info);

  return false;
}

//==============================================//
// INTERNAL HELPER METHODS
//==============================================//

hf_can_err_t EspCan::ConvertToTwaiFrame(const hf_can_message_t& hf_message,
                                        twai_frame_t& twai_frame) noexcept {
  if (hf_message.dlc > HF_CAN_CLASSIC_MAX_LEN || hf_message.is_canfd) {
    return hf_can_err_t::CAN_ERR_MESSAGE_INVALID_DLC;
  }
  if (!twai_frame.buffer) {
    return hf_can_err_t::CAN_ERR_NULL_POINTER;
  }

  twai_frame.header.id = hf_message.id;
  twai_frame.header.ide = hf_message.is_extended;
  twai_frame.header.rtr = hf_message.is_rtr;
  twai_frame.header.dlc = hf_message.dlc; // classic: DLC code == byte length

  // A remote frame carries a DLC but no payload
  if (hf_message.is_rtr) {
    twai_frame.buffer_len = 0;
  } else {
    twai_frame.buffer_len = hf_message.dlc;
    if (hf_message.dlc > 0) {
      std::memcpy(twai_frame.buffer, hf_message.data, hf_message.dlc);
    }
  }
  return hf_can_err_t::CAN_SUCCESS;
}

hf_can_err_t EspCan::ConvertFromTwaiFrame(const twai_frame_t& twai_frame,
                                          hf_can_message_t& hf_message) noexcept {
  hf_message = hf_can_message_t{};

  hf_message.id = twai_frame.header.id;
  hf_message.is_extended = twai_frame.header.ide;
  hf_message.is_rtr = twai_frame.header.rtr;
  // Classic only: codes 9..15 mean 8 bytes (dlc_non_comp records it)
  (void)hf_message.SetDlcCode(static_cast<hf_u8_t>(twai_frame.header.dlc & 0x0FU));

  if (!hf_message.is_rtr && hf_message.dlc > 0 && twai_frame.buffer) {
    const size_t n = std::min<size_t>(hf_message.dlc, twai_frame.buffer_len);
    std::memcpy(hf_message.data, twai_frame.buffer, n);
  }
  return hf_can_err_t::CAN_SUCCESS;
}

hf_can_err_t EspCan::ConvertEspError(esp_err_t esp_err) noexcept {
  switch (esp_err) {
  case ESP_OK:
    return hf_can_err_t::CAN_SUCCESS;
  case ESP_ERR_INVALID_ARG:
    return hf_can_err_t::CAN_ERR_INVALID_PARAMETER;
  case ESP_ERR_INVALID_STATE:
    return hf_can_err_t::CAN_ERR_INVALID_STATE;
  case ESP_ERR_TIMEOUT:
    return hf_can_err_t::CAN_ERR_MESSAGE_TIMEOUT;
  case ESP_ERR_NO_MEM:
    return hf_can_err_t::CAN_ERR_OUT_OF_MEMORY;
  case ESP_ERR_NOT_FOUND:
    return hf_can_err_t::CAN_ERR_DEVICE_NOT_RESPONDING;
  case ESP_FAIL:
    return hf_can_err_t::CAN_ERR_FAILURE;
  default:
    return hf_can_err_t::CAN_ERR_SYSTEM_ERROR;
  }
}

void EspCan::UpdateStatistics(hf_can_operation_type_t operation_type, bool success) noexcept {
  // No mutex lock needed - using atomic operations for ISR safety
  switch (operation_type) {
  case hf_can_operation_type_t::HF_CAN_OP_SEND:
    if (success) {
      statistics_.messages_sent.fetch_add(1);
    } else {
      statistics_.send_failures.fetch_add(1);
    }
    break;
  case hf_can_operation_type_t::HF_CAN_OP_RECEIVE:
    if (success) {
      statistics_.messages_received.fetch_add(1);
    } else {
      statistics_.receive_failures.fetch_add(1);
    }
    break;
  default:
    break;
  }

  statistics_.last_activity_timestamp.store(esp_timer_get_time() / 1000); // Convert to milliseconds
}

//==============================================================================
// ENHANCED CALLBACK DISPATCH METHODS
//==============================================================================

bool EspCan::DeliverReceivedFromIsr(const hf_can_message_t& message) noexcept {
  bool delivered_to_callback = false;

  hf_esp_can_receive_callback_t cb = receive_cb_.load();
  if (cb != nullptr) {
    cb(message, receive_ud_.load());
    delivered_to_callback = true;
  }
  hf_can_receive_callback_t base_cb = base_receive_cb_.load();
  if (base_cb != nullptr) {
    base_cb(message);
    delivered_to_callback = true;
  }

  const bool queue =
      (config_.rx_mode == hf_esp_can_rx_mode_t::QUEUE_ALWAYS) || !delivered_to_callback;
  if (!queue) {
    return false;
  }

  if (!rx_ring_.TryPush(message)) {
    // Ring full: the frame is dropped and counted (rx_ring_.Overruns())
    statistics_.rx_queue_overflows.fetch_add(1);
    return false;
  }
  const uint32_t fill = rx_ring_.Size();
  if (fill > statistics_.rx_queue_peak.load()) {
    statistics_.rx_queue_peak.store(fill);
  }

  BaseType_t woken = pdFALSE;
  if (rx_sem_ != nullptr) {
    xSemaphoreGiveFromISR(rx_sem_, &woken);
  }
  return woken == pdTRUE;
}

void EspCan::RefreshBusState() noexcept {
  twai_node_status_t node_status{};
  if (twai_node_handle_ != nullptr &&
      twai_node_get_info(twai_node_handle_, &node_status, nullptr) == ESP_OK) {
    bus_state_.store(static_cast<uint8_t>(MapTwaiState(node_status.state)));
  }
}

hf_can_err_t EspCan::AllocateQueues() noexcept {
  ReleaseQueues();

  const uint32_t rx_depth = config_.rx_queue_depth == 0U ? 1U : config_.rx_queue_depth;
  if (!rx_ring_.Init(rx_depth)) {
    return hf_can_err_t::CAN_ERR_OUT_OF_MEMORY;
  }
  rx_sem_ = xSemaphoreCreateBinary();

  // One slot per driver queue entry plus the frame in the controller
  tx_slot_count_ = (config_.tx_queue_depth == 0U ? 1U : config_.tx_queue_depth) + 1U;
  tx_slots_.reset(new (std::nothrow) TxSlot[tx_slot_count_]);
  if (tx_slots_) {
    for (uint32_t i = 0; i < tx_slot_count_; ++i) {
      std::memset(&tx_slots_[i].frame, 0, sizeof(tx_slots_[i].frame));
      tx_slots_[i].in_use.store(false);
    }
    tx_slot_sem_ = xSemaphoreCreateCounting(tx_slot_count_, tx_slot_count_);
  }

  if (rx_sem_ == nullptr || !tx_slots_ || tx_slot_sem_ == nullptr) {
    ReleaseQueues();
    return hf_can_err_t::CAN_ERR_OUT_OF_MEMORY;
  }
  return hf_can_err_t::CAN_SUCCESS;
}

void EspCan::ReleaseQueues() noexcept {
  if (rx_sem_ != nullptr) {
    vSemaphoreDelete(rx_sem_);
    rx_sem_ = nullptr;
  }
  if (tx_slot_sem_ != nullptr) {
    vSemaphoreDelete(tx_slot_sem_);
    tx_slot_sem_ = nullptr;
  }
  tx_slots_.reset();
  tx_slot_count_ = 0;
  rx_ring_.Release();
}

void EspCan::DispatchErrorCallbacks(const hf_esp_can_error_info_t& error_info) noexcept {
  if (error_cb_) {
    error_cb_(error_info, error_ud_);
  }
}

void EspCan::DispatchStateChangeCallbacks(const hf_esp_can_state_info_t& state_info) noexcept {
  if (state_cb_) {
    state_cb_(state_info, state_ud_);
  }
}

void EspCan::DispatchTxCompleteCallbacks(const hf_esp_can_tx_info_t& tx_info) noexcept {
  if (tx_cb_) {
    tx_cb_(tx_info, tx_ud_);
  }
}

// (No registry helpers in single-callback design)

void EspCan::UpdateErrorStatistics(uint32_t error_type) noexcept {
  // No mutex lock needed - using atomic operations for ISR safety
  // Update error counters
  switch (error_type) {
  case 0x01: // TX error
    statistics_.bus_error_count.fetch_add(1);
    break;
  case 0x02: // RX error
    statistics_.bus_error_count.fetch_add(1);
    break;
  case 0x04: // Bus error
    statistics_.bus_error_count.fetch_add(1);
    break;
  default:
    statistics_.bus_error_count.fetch_add(1);
    break;
  }

  statistics_.last_activity_timestamp.store(esp_timer_get_time() / 1000);

  // Create error info and dispatch to callbacks
  hf_esp_can_error_info_t error_info;
  error_info.error_type = error_type;
  error_info.tx_error_count = 0;    // Not available in statistics structure
  error_info.rx_error_count = 0;    // Not available in statistics structure
  error_info.bus_off_state = false; // Would need to query actual state
  error_info.error_warning = false; // Would need to query actual state
  error_info.error_passive = false; // Would need to query actual state
  error_info.timestamp_us = esp_timer_get_time();

  DispatchErrorCallbacks(error_info);
}
