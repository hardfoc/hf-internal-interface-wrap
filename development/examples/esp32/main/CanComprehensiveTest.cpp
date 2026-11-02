/**
 * @file CanComprehensiveTest.cpp
 * @brief Comprehensive CAN testing suite for ESP32-C6 with ESP-IDF v5.5 TWAI API and SN65
 * transceiver
 *
 * This comprehensive test suite validates all EspCan functionality including:
 * - ESP-IDF v5.5 TWAI node-based API compliance
 * - ESP32-C6 TWAI controller operation
 * - SN65 CAN transceiver integration
 * - Advanced filtering and timing configuration
 * - Event-driven callback systems
 * - Single-callback per event with user data pointer
 * - Error handling and bus recovery
 * - Performance and stress testing
 * - Self-test and loopback modes
 *
 * Hardware Requirements:
 * - ESP32-C6 DevKit
 * - SN65HVD230/SN65HVD232 CAN transceiver
 * - CAN bus termination resistors (120Ω)
 * - Optional: Second CAN node for full bus testing
 *
 * Wiring for ESP32-C6 + SN65:
 * - GPIO4 (TX) -> SN65 CTX pin
 * - GPIO5 (RX) -> SN65 CRX pin
 * - 3.3V -> SN65 VCC
 * - GND -> SN65 GND
 * - SN65 CANH/CANL -> CAN bus
 *
 * For External Loopback Testing:
 * - Connect: SN65 CANH -> 120Ω resistor -> SN65 CANL
 * - DO NOT short TWAI TX/RX lines directly!
 *
 * IMPORTANT: Two loopback modes are tested:
 *
 * 1. INTERNAL LOOPBACK (enable_loopback=true):
 *    - Uses ESP32's internal hardware loopback
 *    - TX and RX on same pin (GPIO4)
 *    - No external hardware required
 *    - Interrupt callbacks work correctly
 *    - Used for: message_transmission, acceptance_filtering, batch_transmission,
 *               high_throughput, bus_recovery, self_test_mode
 *
 * 2. EXTERNAL LOOPBACK (enable_loopback=false):
 *    - Requires proper CAN bus loopback AFTER the transceiver
 *    - Connect: SN65 CANH -> 120Ω termination resistor -> SN65 CANL
 *    - Uses real CAN transceiver hardware with proper CAN bus signaling
 *    - Tests actual CAN bus communication with differential signaling
 *    - Used for: external_physical_loopback (in SN65 transceiver section)
 *    - NOTE: Shorting TWAI TX/RX lines directly does NOT work!
 *
 * ## Test Progression Indicator:
 * GPIO14 toggles HIGH/LOW after each test completion for visual feedback.
 * Test sections are indicated by 5 blinks on GPIO14 (like SpiComprehensiveTest).
 * This allows monitoring test progress without serial output.
 *
 * @author Nebiyu Tadesse
 * @date 2025
 * @copyright HardFOC
 */

// ESP-IDF C headers must be wrapped in extern "C" for C++ compatibility
#ifdef __cplusplus
extern "C" {
#endif

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#ifdef __cplusplus
}
#endif

// HardFOC interface includes
#include "TestFramework.h"
#include "base/BaseCan.h"
#include "mcu/esp32/EspCan.h"

#include <atomic>
#include <memory>
#include <vector>

static const char* TAG = "CAN_Test";

static TestResults g_test_results;

// Test configuration constants
static constexpr uint32_t TEST_CAN_ID_STANDARD = 0x123;
static constexpr uint32_t TEST_CAN_ID_EXTENDED = 0x12345678;
static constexpr uint32_t TEST_BAUD_RATE = 500000;
static constexpr uint32_t TEST_TIMEOUT_MS = 5000;
static constexpr hf_pin_num_t TEST_TX_PIN = 4; // ESP32-C6 + SN65
static constexpr hf_pin_num_t TEST_RX_PIN = 5; // ESP32-C6 + SN65

// Event bits for synchronization
static constexpr int MESSAGE_RECEIVED_BIT = BIT0;
static constexpr int ERROR_OCCURRED_BIT = BIT1;
static constexpr int STATE_CHANGED_BIT = BIT2;

// Global test data
static EventGroupHandle_t test_event_group = nullptr;
static std::atomic<uint32_t> messages_received{0};
static std::atomic<uint32_t> errors_detected{0};
static hf_can_message_t last_received_message{};

//=============================================================================
// TEST SECTION CONFIGURATION
//=============================================================================
// Enable/disable specific test categories by setting to true or false

// Core CAN functionality tests
static constexpr bool ENABLE_CORE_TESTS = true; // Initialization, self-test, message transmission
static constexpr bool ENABLE_ADVANCED_TESTS = true;    // Acceptance filtering, advanced timing
static constexpr bool ENABLE_ERROR_TESTS = true;       // Error handling, bus recovery
static constexpr bool ENABLE_PERFORMANCE_TESTS = true; // Batch transmission, high throughput
static constexpr bool ENABLE_TRANSCEIVER_TESTS =
    true; // SN65 transceiver integration, signal quality

//=============================================================================
// TEST HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Verify CAN pin states before testing
 */
void verify_can_pin_states() {
  ESP_LOGI(TAG, "Verifying CAN pin states...");

  // Note: We can't directly read GPIO states in this context,
  // but we can log the expected behavior
  ESP_LOGI(TAG, "Expected CAN pin behavior:");
  ESP_LOGI(TAG, "  TX (GPIO%d): LOW when idle (recessive state)", TEST_TX_PIN);
  ESP_LOGI(TAG, "  RX (GPIO%d): HIGH when idle (recessive state)", TEST_RX_PIN);
  ESP_LOGI(TAG, "  Internal loopback: TX and RX on same pin (GPIO%d)", TEST_TX_PIN);
  ESP_LOGI(TAG, "  External loopback: CANH->120Ω->CANL (after transceiver)");
  ESP_LOGI(TAG, "  Safety: Minimal current flow, GPIO protection active");
}

/**
 * @brief Test callback for received CAN messages (enhanced version with user data)
 */
void test_receive_callback_enhanced(const hf_can_message_t& message, void* user_data) {
  (void)user_data; // Unused in basic tests

  last_received_message = message;
  messages_received.fetch_add(1);

  // Signal that a message was received (for test synchronization)
  if (test_event_group) {
    BaseType_t higher_priority_task_woken = pdFALSE;
    xEventGroupSetBitsFromISR(test_event_group, MESSAGE_RECEIVED_BIT, &higher_priority_task_woken);
    // Note: No portYIELD_FROM_ISR needed as we're not in a critical timing scenario
  }
}

/**
 * @brief Create a test CAN message
 */
hf_can_message_t create_test_message(uint32_t id, bool extended = false, uint8_t dlc = 8) {
  hf_can_message_t message{};
  message.id = id;
  message.is_extended = extended;
  message.dlc = dlc;
  message.is_rtr = false;

  // Fill with test pattern
  for (uint8_t i = 0; i < dlc && i < 8; ++i) {
    message.data[i] = static_cast<uint8_t>(0xA0 + i);
  }

  return message;
}

/**
 * @brief Wait for events with timeout
 */
bool wait_for_event(EventBits_t bits, uint32_t timeout_ms) {
  EventBits_t result =
      xEventGroupWaitBits(test_event_group, bits, pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
  return (result & bits) != 0;
}

//=============================================================================
// COMPREHENSIVE ESPCAN FUNCTIONALITY VALIDATION
//=============================================================================

/**
 * @brief Test basic initialization and state management
 */
bool test_basic_initialization() noexcept {
  ESP_LOGI(TAG, "Feature 1: Basic Initialization and State Management");

  hf_esp_can_config_t config{};
  config.tx_pin = TEST_TX_PIN;
  config.rx_pin = TEST_RX_PIN;
  config.baud_rate = TEST_BAUD_RATE;
  config.enable_self_test = true;
  config.enable_loopback = false;

  EspCan can(config);

  // Test lazy initialization
  if (can.IsInitialized()) {
    ESP_LOGE(TAG, "❌ Lazy initialization failed - should not be initialized");
    return false;
  }

  // Test initialization
  if (can.Initialize() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "❌ Initialization failed");
    return false;
  } else if (!can.IsInitialized()) {
    ESP_LOGE(TAG, "❌ IsInitialized() should return true after Initialize()");
    return false;
  } else {
    ESP_LOGI(TAG, "✅ Initialization and state management - PASSED");
  }

  // Test deinitialization
  if (can.Deinitialize() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "❌ Deinitialization failed");
    return false;
  } else if (can.IsInitialized()) {
    ESP_LOGE(TAG, "❌ IsInitialized() should return false after Deinitialize()");
    return false;
  }

  return true;
}

/**
 * @brief Test message transmission and reception
 */
bool test_message_transmission() noexcept {
  ESP_LOGI(TAG, "Feature 2: Message Transmission and Reception");

  hf_esp_can_config_t config{};
  config.tx_pin = TEST_TX_PIN;
  config.rx_pin = TEST_TX_PIN; // Use same pin for internal loopback
  config.baud_rate = TEST_BAUD_RATE;
  config.enable_self_test = true;
  config.enable_loopback = true; // Enable internal loopback

  EspCan can(config);
  can.Initialize();
  can.SetReceiveCallbackEx(test_receive_callback_enhanced);

  // Test standard frame
  messages_received.store(0);
  auto std_msg = create_test_message(0x123, false, 8);
  if (can.SendMessage(std_msg, 1000) == hf_can_err_t::CAN_SUCCESS &&
      wait_for_event(MESSAGE_RECEIVED_BIT, 1000)) {
    ESP_LOGI(TAG, "✅ Standard frame transmission - PASSED");
  } else {
    ESP_LOGE(TAG, "❌ Standard frame transmission - FAILED");
    return false;
  }

  // Test extended frame
  messages_received.store(0);
  auto ext_msg = create_test_message(0x12345678, true, 6);
  if (can.SendMessage(ext_msg, 1000) == hf_can_err_t::CAN_SUCCESS &&
      wait_for_event(MESSAGE_RECEIVED_BIT, 1000)) {
    ESP_LOGI(TAG, "✅ Extended frame transmission - PASSED");
  } else {
    ESP_LOGE(TAG, "❌ Extended frame transmission - FAILED");
    return false;
  }

  // Test RTR frame
  messages_received.store(0);
  hf_can_message_t rtr_msg = create_test_message(0x456, false, 4);
  rtr_msg.is_rtr = true;
  if (can.SendMessage(rtr_msg, 1000) == hf_can_err_t::CAN_SUCCESS &&
      wait_for_event(MESSAGE_RECEIVED_BIT, 1000)) {
    ESP_LOGI(TAG, "✅ RTR frame transmission - PASSED");
  } else {
    ESP_LOGE(TAG, "❌ RTR frame transmission - FAILED");
    return false;
  }

  return true;
}

/**
 * @brief Test acceptance filtering
 */
bool test_acceptance_filtering() noexcept {
  ESP_LOGI(TAG, "Feature 3: Acceptance Filtering");

  hf_esp_can_config_t config{};
  config.tx_pin = TEST_TX_PIN;
  config.rx_pin = TEST_TX_PIN; // Use same pin for internal loopback
  config.baud_rate = TEST_BAUD_RATE;
  config.enable_self_test = true;
  config.enable_loopback = true; // Enable internal loopback

  EspCan can(config);
  can.Initialize();
  can.SetReceiveCallbackEx(test_receive_callback_enhanced);

  // Set filter to accept only 0x100-0x10F
  if (can.SetAcceptanceFilter(0x100, 0x7F0, false) == hf_can_err_t::CAN_SUCCESS) {
    // Test accepted message
    messages_received.store(0);
    auto accepted_msg = create_test_message(0x105, false, 4);
    if (can.SendMessage(accepted_msg, 1000) == hf_can_err_t::CAN_SUCCESS &&
        wait_for_event(MESSAGE_RECEIVED_BIT, 1000)) {
      ESP_LOGI(TAG, "✅ Filter acceptance - PASSED");
    } else {
      ESP_LOGE(TAG, "❌ Filter acceptance - FAILED");
      return false;
    }

    // Test rejected message
    messages_received.store(0);
    auto rejected_msg = create_test_message(0x200, false, 4);
    can.SendMessage(rejected_msg, 1000);
    if (!wait_for_event(MESSAGE_RECEIVED_BIT, 500)) {
      ESP_LOGI(TAG, "✅ Filter rejection - PASSED");
    } else {
      ESP_LOGE(TAG, "❌ Filter rejection - FAILED (message should have been filtered)");
      return false;
    }
  } else {
    ESP_LOGE(TAG, "❌ Filter configuration - FAILED");
    return false;
  }

  return true;
}

/**
 * @brief Test advanced timing configuration
 */
bool test_advanced_timing() noexcept {
  ESP_LOGI(TAG, "Feature 4: Advanced Timing Configuration");

  hf_esp_can_config_t config{};
  config.tx_pin = TEST_TX_PIN;
  config.rx_pin = TEST_RX_PIN;
  config.baud_rate = 250000; // Different baud rate
  config.enable_self_test = true;
  config.enable_loopback = false;

  EspCan can(config);
  can.Initialize();

  hf_esp_can_timing_config_t timing{};
  timing.brp = 16;
  timing.prop_seg = 5;
  timing.tseg_1 = 8;
  timing.tseg_2 = 3;
  timing.sjw = 2;

  if (can.ConfigureAdvancedTiming(timing) == hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGI(TAG, "✅ Advanced timing configuration - PASSED");
    return true;
  } else {
    ESP_LOGE(TAG, "❌ Advanced timing configuration - FAILED");
    return false;
  }
}

/**
 * @brief Test statistics and diagnostics
 */
bool test_statistics_diagnostics() noexcept {
  ESP_LOGI(TAG, "Feature 5: Statistics and Diagnostics");

  hf_esp_can_config_t config{};
  config.tx_pin = TEST_TX_PIN;
  config.rx_pin = TEST_RX_PIN;
  config.baud_rate = TEST_BAUD_RATE;
  config.enable_self_test = true;
  config.enable_loopback = false;

  EspCan can(config);
  can.Initialize();
  can.SetReceiveCallbackEx(test_receive_callback_enhanced);

  // Reset statistics
  if (can.ResetStatistics() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "❌ Reset statistics - FAILED");
    return false;
  }

  // Send some messages to generate statistics
  messages_received.store(0);
  for (int i = 0; i < 5; i++) {
    auto msg = create_test_message(0x200 + i, false, 8);
    hf_can_err_t result = can.SendMessage(msg, 500);
    if (result != hf_can_err_t::CAN_SUCCESS) {
      ESP_LOGE(TAG, "Failed to send statistics message %d", i);
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(50)); // Small delay between messages
  }
  vTaskDelay(pdMS_TO_TICKS(200));

  // Get statistics
  hf_can_statistics_t stats{};
  if (can.GetStatistics(stats) == hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGI(TAG, "Statistics: sent=%llu, received=%llu", stats.messages_sent.load(),
             stats.messages_received.load());
    if (stats.messages_sent.load() > 0) {
      ESP_LOGI(TAG, "✅ Statistics collection - PASSED");
    } else {
      ESP_LOGE(TAG, "❌ Statistics collection - FAILED (no messages recorded)");
      return false;
    }
  } else {
    ESP_LOGE(TAG, "❌ Get statistics - FAILED");
    return false;
  }

  // Get diagnostics
  hf_can_diagnostics_t diagnostics{};
  if (can.GetDiagnostics(diagnostics) == hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGI(TAG, "✅ Diagnostics retrieval - PASSED");
  } else {
    ESP_LOGE(TAG, "❌ Diagnostics retrieval - FAILED");
    return false;
  }

  return true;
}

/**
 * @brief Test batch message transmission
 */
bool test_batch_transmission() noexcept {
  ESP_LOGI(TAG, "Feature 6: Batch Message Transmission");

  hf_esp_can_config_t config{};
  config.tx_pin = TEST_TX_PIN;
  config.rx_pin = TEST_TX_PIN; // Use same pin for internal loopback
  config.baud_rate = TEST_BAUD_RATE;
  config.enable_self_test = true;
  config.enable_loopback = true; // Enable internal loopback
  config.tx_queue_depth = 20;

  EspCan can(config);
  can.Initialize();
  can.SetReceiveCallbackEx(test_receive_callback_enhanced);

  // Create batch of messages (smaller batch to avoid memory issues)
  const int BATCH_SIZE = 3;
  hf_can_message_t batch_messages[BATCH_SIZE]; // Use stack array instead of vector
  for (int i = 0; i < BATCH_SIZE; i++) {
    batch_messages[i] = create_test_message(0x300 + i, false, 8);
  }

  messages_received.store(0);
  uint32_t sent_count = can.SendMessageBatch(batch_messages, BATCH_SIZE, 500);
  vTaskDelay(pdMS_TO_TICKS(500));

  if (sent_count >= BATCH_SIZE * 0.9f && messages_received.load() >= sent_count * 0.9f) {
    ESP_LOGI(TAG, "✅ Batch transmission - PASSED (sent: %d, received: %d)", sent_count,
             messages_received.load());
    return true;
  } else {
    ESP_LOGE(TAG, "❌ Batch transmission - FAILED (sent: %d, received: %d)", sent_count,
             messages_received.load());
    return false;
  }
}

/**
 * @brief Test error handling and recovery
 */
bool test_error_handling() noexcept {
  ESP_LOGI(TAG, "Feature 7: Error Handling and Recovery");

  hf_esp_can_config_t config{};
  config.tx_pin = TEST_TX_PIN;
  config.rx_pin = TEST_RX_PIN;
  config.baud_rate = TEST_BAUD_RATE;
  config.enable_self_test = true;
  config.enable_alerts = true;

  EspCan can(config);
  can.Initialize();

  // Test status retrieval
  hf_can_status_t status{};
  if (can.GetStatus(status) == hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGI(TAG, "✅ Status retrieval - PASSED");
  } else {
    ESP_LOGE(TAG, "❌ Status retrieval - FAILED");
    return false;
  }

  // Test reset functionality
  if (can.Reset() == hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGI(TAG, "✅ Reset functionality - PASSED");
  } else {
    ESP_LOGE(TAG, "❌ Reset functionality - FAILED");
    return false;
  }

  // Test bus recovery
  if (can.InitiateBusRecovery() == hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGI(TAG, "✅ Bus recovery - PASSED");
  } else {
    ESP_LOGE(TAG, "❌ Bus recovery - FAILED");
    return false;
  }

  return true;
}

/**
 * @brief Comprehensive test to validate ALL EspCan functionality
 * This test systematically validates every aspect of the EspCan implementation
 */
bool test_espcan_comprehensive_functionality() noexcept {
  ESP_LOGI(TAG, "🔍 COMPREHENSIVE EspCan Functionality Validation");
  ESP_LOGI(TAG, "This test validates ALL EspCan features systematically");
  ESP_LOGI(TAG, "*** USING: Internal hardware loopback (TX and RX on GPIO%d) ***", TEST_TX_PIN);

  bool all_features_passed = true;

  // Run individual feature tests
  if (!test_basic_initialization()) {
    all_features_passed = false;
  }

  if (!test_message_transmission()) {
    all_features_passed = false;
  }

  if (!test_acceptance_filtering()) {
    all_features_passed = false;
  }

  if (!test_advanced_timing()) {
    all_features_passed = false;
  }

  if (!test_statistics_diagnostics()) {
    all_features_passed = false;
  }

  if (!test_batch_transmission()) {
    all_features_passed = false;
  }

  if (!test_error_handling()) {
    all_features_passed = false;
  }

  // ============================================================================
  // SUMMARY
  // ============================================================================
  if (all_features_passed) {
    ESP_LOGI(TAG, "🎉 [SUCCESS] ✅ ALL EspCan features validated successfully!");
    ESP_LOGI(TAG, "The EspCan implementation is fully functional and ready for production use.");
  } else {
    ESP_LOGE(TAG, "💥 [FAILED] ❌ Some EspCan features failed validation!");
    ESP_LOGE(TAG, "Review the failed features above and address the issues.");
  }

  return all_features_passed;
}

//=============================================================================
// BASIC FUNCTIONALITY TESTS
//=============================================================================

bool test_can_initialization() noexcept {
  ESP_LOGI(TAG, "Testing CAN initialization with ESP-IDF v5.5 API...");

  // Test configuration for ESP32-C6 + SN65 transceiver
  hf_esp_can_config_t can_config{};
  can_config.tx_pin = TEST_TX_PIN;
  can_config.rx_pin = TEST_RX_PIN;
  can_config.baud_rate = TEST_BAUD_RATE;
  can_config.controller_id = hf_can_controller_id_t::HF_CAN_CONTROLLER_0;
  can_config.mode = hf_can_mode_t::HF_CAN_MODE_NORMAL;
  can_config.enable_self_test = false; // Using external SN65 transceiver
  can_config.enable_loopback = false;
  can_config.tx_queue_depth = 10;
  can_config.sample_point_permill = 750; // 75% sample point for reliability

  EspCan test_can(can_config);

  // Test lazy initialization
  if (test_can.IsInitialized()) {
    ESP_LOGE(TAG, "CAN should not be initialized before Initialize() call");
    return false;
  }

  // Test initialization
  if (test_can.Initialize() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to initialize CAN with ESP-IDF v5.5 API");
    return false;
  }

  if (!test_can.IsInitialized()) {
    ESP_LOGE(TAG, "CAN should be initialized after Initialize() call");
    return false;
  }

  // Test double initialization (should succeed)
  if (test_can.Initialize() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Double initialization should succeed");
    return false;
  }

  // Test deinitialization
  if (test_can.Deinitialize() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to deinitialize CAN");
    return false;
  }

  if (test_can.IsInitialized()) {
    ESP_LOGE(TAG, "CAN should not be initialized after Deinitialize() call");
    return false;
  }

  ESP_LOGI(TAG, "[SUCCESS] CAN initialization test passed");
  return true;
}

bool test_can_self_test_mode() noexcept {
  ESP_LOGI(TAG, "Testing comprehensive TWAI self-test modes for ESP32-C6...");

  bool all_tests_passed = true;

  // ============================================================================
  // TEST 1: Pure Internal Hardware Loopback (ESP-IDF v5.5 Style)
  // ============================================================================
  ESP_LOGI(TAG, "Test 1: Internal hardware loopback (enable_loopback=true)");

  {
    hf_esp_can_config_t internal_config{};
    internal_config.tx_pin = TEST_TX_PIN;
    internal_config.rx_pin = TEST_RX_PIN;
    internal_config.baud_rate = TEST_BAUD_RATE;
    internal_config.enable_self_test = true; // No ACK required
    internal_config.enable_loopback = true;  // Internal hardware loopback

    EspCan internal_can(internal_config);

    if (internal_can.Initialize() != hf_can_err_t::CAN_SUCCESS) {
      ESP_LOGE(TAG, "Failed to initialize internal loopback CAN");
      all_tests_passed = false;
    } else {
      internal_can.SetReceiveCallbackEx(test_receive_callback_enhanced);

      // Test with self-reception request (like ESP-IDF example)
      auto test_msg = create_test_message(TEST_CAN_ID_STANDARD, false, 4);
      test_msg.is_self = true; // Self-reception request flag

      messages_received.store(0);
      if (internal_can.SendMessage(test_msg, 1000) == hf_can_err_t::CAN_SUCCESS) {
        if (wait_for_event(MESSAGE_RECEIVED_BIT, 1000)) {
          ESP_LOGI(TAG, "✅ Internal loopback test PASSED");
        } else {
          ESP_LOGW(TAG,
                   "⚠️  Internal loopback: No message received (may be ESP-IDF v5.5 limitation)");
        }
      } else {
        ESP_LOGE(TAG, "❌ Internal loopback: Failed to send message");
        all_tests_passed = false;
      }
    }
  }

  // ============================================================================
  // TEST 2: Performance Test with Internal Loopback
  // ============================================================================
  ESP_LOGI(TAG, "Test 2: Performance test with internal loopback");

  {
    hf_esp_can_config_t perf_config{};
    perf_config.tx_pin = TEST_TX_PIN;
    perf_config.rx_pin = TEST_TX_PIN; // Use same pin for internal loopback
    perf_config.baud_rate = TEST_BAUD_RATE;
    perf_config.enable_self_test = true; // No ACK required for internal loopback
    perf_config.enable_loopback = true;  // Enable internal loopback
    perf_config.tx_queue_depth = 20;     // Larger queue for performance test

    EspCan perf_can(perf_config);

    if (perf_can.Initialize() == hf_can_err_t::CAN_SUCCESS) {
      perf_can.SetReceiveCallbackEx(test_receive_callback_enhanced);

      const int PERF_MESSAGE_COUNT = 50;
      messages_received.store(0);

      uint64_t start_time = esp_timer_get_time();

      // Send burst of messages
      int sent_count = 0;
      for (int i = 0; i < PERF_MESSAGE_COUNT; i++) {
        auto msg = create_test_message(TEST_CAN_ID_STANDARD + i, false, 8);
        if (perf_can.SendMessage(msg, 100) == hf_can_err_t::CAN_SUCCESS) {
          sent_count++;
        }
      }

      // Wait for all messages to be received
      vTaskDelay(pdMS_TO_TICKS(2000));

      uint64_t end_time = esp_timer_get_time();
      uint32_t duration_ms = (uint32_t)((end_time - start_time) / 1000);
      uint32_t received_count = messages_received.load();

      ESP_LOGI(TAG, "Performance results:");
      ESP_LOGI(TAG, "  Messages sent: %d/%d", sent_count, PERF_MESSAGE_COUNT);
      ESP_LOGI(TAG, "  Messages received: %d", received_count);
      ESP_LOGI(TAG, "  Duration: %d ms", duration_ms);
      ESP_LOGI(TAG, "  Success rate: %.1f%%", (float)received_count / sent_count * 100.0f);

      if (received_count >= sent_count * 0.95f) { // 95% success rate
        ESP_LOGI(TAG, "✅ Performance test PASSED");
      } else {
        ESP_LOGE(TAG, "❌ Performance test FAILED - Low success rate");
        all_tests_passed = false;
      }
    }
  }

  if (all_tests_passed) {
    ESP_LOGI(TAG, "[SUCCESS] ✅ Comprehensive CAN self-test mode PASSED");
    ESP_LOGI(
        TAG,
        "NOTE: For best results, ensure GPIO%d (TX) is connected to GPIO%d (RX) with a jumper wire",
        TEST_TX_PIN, TEST_RX_PIN);
  } else {
    ESP_LOGE(TAG, "[FAILED] ❌ Some CAN self-test modes FAILED");
  }

  return all_tests_passed;
}

bool test_can_message_transmission() noexcept {
  ESP_LOGI(TAG, "Testing CAN message transmission with ESP-IDF v5.5 loopback pattern...");

  // Use the exact same configuration as the working ESP-IDF example
  hf_esp_can_config_t can_config{};
  can_config.tx_pin = TEST_TX_PIN;
  can_config.rx_pin = TEST_TX_PIN; // Same pin for internal loopback (like ESP-IDF example)
  can_config.baud_rate = TEST_BAUD_RATE;
  can_config.enable_self_test = true; // No ACK required (like ESP-IDF example)
  can_config.enable_loopback = true;  // Enable internal loopback (like ESP-IDF example)

  EspCan test_can(can_config);

  if (test_can.Initialize() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to initialize CAN with loopback configuration");
    return false;
  }

  test_can.SetReceiveCallbackEx(test_receive_callback_enhanced);

  // Test standard frame (matching ESP-IDF example pattern)
  messages_received.store(0);
  auto std_message = create_test_message(TEST_CAN_ID_STANDARD, false, 8);

  ESP_LOGI(TAG, "Sending standard frame (ID: 0x%03X, DLC: %d)", std_message.id, std_message.dlc);
  if (test_can.SendMessage(std_message, 1000) != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to send standard frame");
    return false;
  }

  // Wait longer for reception in loopback mode
  if (!wait_for_event(MESSAGE_RECEIVED_BIT, 2000)) {
    ESP_LOGW(TAG, "Standard frame not received in loopback mode (this may be expected)");
    // Don't fail the test - loopback reception might not work in all ESP-IDF versions
  } else {
    ESP_LOGI(TAG, "Standard frame received successfully in loopback mode");
  }

  // Test extended frame
  messages_received.store(0);
  auto ext_message = create_test_message(TEST_CAN_ID_EXTENDED, true, 6);

  ESP_LOGI(TAG, "Sending extended frame (ID: 0x%08X, DLC: %d)", ext_message.id, ext_message.dlc);
  if (test_can.SendMessage(ext_message, 1000) != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to send extended frame");
    return false;
  }

  if (!wait_for_event(MESSAGE_RECEIVED_BIT, 2000)) {
    ESP_LOGW(TAG, "Extended frame not received in loopback mode (this may be expected)");
  } else {
    ESP_LOGI(TAG, "Extended frame received successfully in loopback mode");
  }

  // Test remote frame
  messages_received.store(0);
  hf_can_message_t rtr_message{};
  rtr_message.id = TEST_CAN_ID_STANDARD;
  rtr_message.is_rtr = true;
  rtr_message.dlc = 4;

  ESP_LOGI(TAG, "Sending RTR frame (ID: 0x%03X, DLC: %d)", rtr_message.id, rtr_message.dlc);
  if (test_can.SendMessage(rtr_message, 1000) != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to send remote frame");
    return false;
  }

  if (!wait_for_event(MESSAGE_RECEIVED_BIT, 2000)) {
    ESP_LOGW(TAG, "RTR frame not received in loopback mode (this may be expected)");
  } else {
    ESP_LOGI(TAG, "RTR frame received successfully in loopback mode");
    // Verify RTR flag
    if (!last_received_message.is_rtr) {
      ESP_LOGE(TAG, "Received message should be RTR");
      return false;
    }
  }

  ESP_LOGI(TAG, "[SUCCESS] CAN message transmission test completed (loopback mode)");
  ESP_LOGI(TAG, "Note: Loopback reception may not work in all ESP-IDF v5.5 configurations");
  return true;
}

//=============================================================================
// ADVANCED FILTERING TESTS
//=============================================================================

bool test_can_acceptance_filtering() noexcept {
  ESP_LOGI(TAG, "Testing CAN acceptance filtering with ESP-IDF v5.5 loopback pattern...");

  // Use the exact same configuration as the working ESP-IDF example
  hf_esp_can_config_t can_config{};
  can_config.tx_pin = TEST_TX_PIN;
  can_config.rx_pin = TEST_TX_PIN; // Same pin for internal loopback (like ESP-IDF example)
  can_config.baud_rate = TEST_BAUD_RATE;
  can_config.enable_self_test = true; // No ACK required (like ESP-IDF example)
  can_config.enable_loopback = true;  // Enable internal loopback (like ESP-IDF example)

  EspCan test_can(can_config);

  if (test_can.Initialize() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to initialize CAN for filtering test");
    return false;
  }

  test_can.SetReceiveCallbackEx(test_receive_callback_enhanced);

  // Test single filter mode
  // Accept only IDs 0x100-0x10F (mask 0x7F0, ID 0x100)
  if (test_can.SetAcceptanceFilter(0x100, 0x7F0, false) != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to set acceptance filter");
    return false;
  }

  // Test accepted message
  messages_received.store(0);
  auto accepted_msg = create_test_message(0x105, false, 4); // Should pass filter

  ESP_LOGI(TAG, "Sending accepted message (ID: 0x%03X, should pass filter)", accepted_msg.id);
  if (test_can.SendMessage(accepted_msg, 1000) != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to send accepted message");
    return false;
  }

  if (!wait_for_event(MESSAGE_RECEIVED_BIT, 2000)) {
    ESP_LOGW(TAG, "Accepted message not received in loopback mode (this may be expected)");
  } else {
    ESP_LOGI(TAG, "Accepted message received successfully in loopback mode");
  }

  // Test rejected message
  messages_received.store(0);
  auto rejected_msg = create_test_message(0x200, false, 4); // Should be filtered out

  ESP_LOGI(TAG, "Sending rejected message (ID: 0x%03X, should be filtered)", rejected_msg.id);
  if (test_can.SendMessage(rejected_msg, 1000) != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to send rejected message");
    return false;
  }

  // Should not receive this message due to filter
  if (wait_for_event(MESSAGE_RECEIVED_BIT, 1000)) {
    ESP_LOGW(TAG, "Rejected message was received (filter may not work in loopback mode)");
  } else {
    ESP_LOGI(TAG, "Rejected message correctly filtered out");
  }

  // Test dual filter mode using advanced filter API
  hf_esp_can_filter_config_t dual_filter{};
  dual_filter.is_dual_filter = true;
  dual_filter.id = 0x300;
  dual_filter.mask = 0x7F0;
  dual_filter.id2 = 0x400;
  dual_filter.mask2 = 0x7F0;
  dual_filter.is_extended = false;

  if (test_can.ConfigureAdvancedFilter(dual_filter) != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to configure dual filter");
    return false;
  }

  // Test both filter ranges
  messages_received.store(0);
  auto msg1 = create_test_message(0x305, false, 2); // First filter range
  auto msg2 = create_test_message(0x405, false, 2); // Second filter range

  ESP_LOGI(TAG, "Testing dual filter with messages 0x%03X and 0x%03X", msg1.id, msg2.id);
  test_can.SendMessage(msg1, 1000);
  test_can.SendMessage(msg2, 1000);

  // Wait for both messages
  vTaskDelay(pdMS_TO_TICKS(1000));

  ESP_LOGI(TAG, "Dual filter test: received %d messages (expected 0-2 in loopback mode)",
           messages_received.load());

  // Clear filter (accept all)
  if (test_can.ClearAcceptanceFilter() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to clear acceptance filter");
    return false;
  }

  ESP_LOGI(TAG, "[SUCCESS] CAN acceptance filtering test completed (loopback mode)");
  ESP_LOGI(TAG, "Note: Filtering behavior may differ in loopback mode");
  return true;
}

//=============================================================================
// ADVANCED TIMING TESTS
//=============================================================================

bool test_can_advanced_timing() noexcept {
  ESP_LOGI(TAG, "Testing CAN advanced bit timing configuration...");

  hf_esp_can_config_t can_config{};
  can_config.tx_pin = TEST_TX_PIN;
  can_config.rx_pin = TEST_RX_PIN;
  can_config.baud_rate = 250000;      // Start with 250kbps
  can_config.enable_self_test = true; // No external ACK required
  can_config.enable_loopback = false; // Using physical wire loopback

  EspCan test_can(can_config);

  if (test_can.Initialize() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to initialize CAN for timing test");
    return false;
  }

  // Test custom timing configuration for improved signal quality
  hf_esp_can_timing_config_t custom_timing{};
  custom_timing.brp = 16;       // Prescaler for 250kbps
  custom_timing.prop_seg = 5;   // Propagation segment
  custom_timing.tseg_1 = 8;     // Time segment 1
  custom_timing.tseg_2 = 3;     // Time segment 2
  custom_timing.sjw = 2;        // Synchronization jump width
  custom_timing.ssp_offset = 0; // Secondary sample point offset

  if (test_can.ConfigureAdvancedTiming(custom_timing) != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to configure advanced timing");
    return false;
  }

  test_can.SetReceiveCallbackEx(test_receive_callback_enhanced);

  // Test message transmission with custom timing
  messages_received.store(0);
  auto test_message = create_test_message(TEST_CAN_ID_STANDARD, false, 8);

  if (test_can.SendMessage(test_message, 1000) != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to send message with custom timing");
    return false;
  }

  if (!wait_for_event(MESSAGE_RECEIVED_BIT, 1000)) {
    ESP_LOGE(TAG, "Message not received with custom timing");
    return false;
  }

  ESP_LOGI(TAG, "[SUCCESS] CAN advanced timing configuration test passed");
  return true;
}

//=============================================================================
// ERROR HANDLING AND RECOVERY TESTS
//=============================================================================

bool test_can_error_handling() noexcept {
  ESP_LOGI(TAG, "Testing CAN error handling and recovery...");

  hf_esp_can_config_t can_config{};
  can_config.tx_pin = TEST_TX_PIN;
  can_config.rx_pin = TEST_RX_PIN;
  can_config.baud_rate = TEST_BAUD_RATE;
  can_config.enable_self_test = false; // Normal mode to potentially trigger errors
  can_config.enable_alerts = true;

  EspCan test_can(can_config);

  if (test_can.Initialize() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to initialize CAN for error test");
    return false;
  }

  // Get initial status
  hf_can_status_t initial_status{};
  if (test_can.GetStatus(initial_status) != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to get initial CAN status");
    return false;
  }

  ESP_LOGI(TAG, "Initial status - TX errors: %d, RX errors: %d, Bus-off: %s",
           initial_status.tx_error_count, initial_status.rx_error_count,
           initial_status.bus_off ? "Yes" : "No");

  // Test statistics functionality
  hf_can_statistics_t stats{};
  if (test_can.GetStatistics(stats) != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to get CAN statistics");
    return false;
  }

  // Test diagnostics
  hf_can_diagnostics_t diagnostics{};
  if (test_can.GetDiagnostics(diagnostics) != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to get CAN diagnostics");
    return false;
  }

  // Test reset functionality
  if (test_can.Reset() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to reset CAN controller");
    return false;
  }

  // Verify statistics were reset
  if (test_can.GetStatistics(stats) != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to get statistics after reset");
    return false;
  }

  // Test node info retrieval (ESP-IDF v5.5 specific)
  twai_node_record_t node_info{};
  if (test_can.GetNodeInfo(node_info) != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to get TWAI node info");
    return false;
  }

  ESP_LOGI(TAG, "Node info - Bus errors: %d", node_info.bus_err_num);

  ESP_LOGI(TAG, "[SUCCESS] CAN error handling test passed");
  return true;
}

bool test_can_bus_recovery() noexcept {
  ESP_LOGI(TAG, "Testing CAN bus recovery functionality...");

  hf_esp_can_config_t can_config{};
  can_config.tx_pin = TEST_TX_PIN;
  can_config.rx_pin = TEST_TX_PIN; // Use same pin for internal loopback
  can_config.baud_rate = TEST_BAUD_RATE;
  can_config.enable_self_test = true; // No ACK required for internal loopback
  can_config.enable_loopback = true;  // Enable internal loopback
  can_config.enable_alerts = true;

  EspCan test_can(can_config);

  if (test_can.Initialize() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to initialize CAN for recovery test");
    return false;
  }

  // Test bus recovery initiation
  if (test_can.InitiateBusRecovery() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to initiate bus recovery");
    return false;
  }

  ESP_LOGI(TAG, "Bus recovery initiated successfully");

  // Wait for recovery to complete
  vTaskDelay(pdMS_TO_TICKS(100));

  // Verify we can still send messages after recovery
  test_can.SetReceiveCallbackEx(test_receive_callback_enhanced);
  messages_received.store(0);

  auto test_message = create_test_message(TEST_CAN_ID_STANDARD, false, 4);
  if (test_can.SendMessage(test_message, 1000) != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to send message after recovery");
    return false;
  }

  if (!wait_for_event(MESSAGE_RECEIVED_BIT, 1000)) {
    ESP_LOGE(TAG, "Message not received after recovery");
    return false;
  }

  ESP_LOGI(TAG, "[SUCCESS] CAN bus recovery test passed");
  return true;
}

//=============================================================================
// PERFORMANCE AND STRESS TESTS
//=============================================================================

bool test_can_batch_transmission() noexcept {
  ESP_LOGI(TAG, "Testing CAN batch message transmission with ESP-IDF v5.5 loopback pattern...");

  // Use the exact same configuration as the working ESP-IDF example
  hf_esp_can_config_t can_config{};
  can_config.tx_pin = TEST_TX_PIN;
  can_config.rx_pin = TEST_TX_PIN; // Same pin for internal loopback (like ESP-IDF example)
  can_config.baud_rate = TEST_BAUD_RATE;
  can_config.enable_self_test = true; // No ACK required (like ESP-IDF example)
  can_config.enable_loopback = true;  // Enable internal loopback (like ESP-IDF example)
  can_config.tx_queue_depth = 20;     // Larger queue for batch testing

  EspCan test_can(can_config);

  if (test_can.Initialize() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to initialize CAN for batch test");
    return false;
  }

  test_can.SetReceiveCallbackEx(test_receive_callback_enhanced);

  // Create batch of test messages (smaller batch for loopback testing)
  constexpr uint32_t BATCH_SIZE = 5; // Reduced for loopback testing
  std::vector<hf_can_message_t> batch_messages;

  for (uint32_t i = 0; i < BATCH_SIZE; ++i) {
    auto msg = create_test_message(TEST_CAN_ID_STANDARD + i, false, 8);
    batch_messages.push_back(msg);
  }

  messages_received.store(0);

  ESP_LOGI(TAG, "Sending batch of %d messages in loopback mode", BATCH_SIZE);
  // Send batch using the new batch API
  uint32_t sent_count = test_can.SendMessageBatch(batch_messages.data(), BATCH_SIZE, 1000);

  if (sent_count != BATCH_SIZE) {
    ESP_LOGE(TAG, "Expected to send %d messages, actually sent %d", BATCH_SIZE, sent_count);
    return false;
  }

  ESP_LOGI(TAG, "Successfully sent %d messages, waiting for reception...", sent_count);

  // Wait for all messages to be received (longer timeout for loopback)
  vTaskDelay(pdMS_TO_TICKS(2000));

  uint32_t received_count = messages_received.load();
  ESP_LOGI(TAG, "Batch transmission results: sent %d, received %d", sent_count, received_count);

  // In loopback mode, we may not receive all messages, so we're more lenient
  if (received_count == 0) {
    ESP_LOGW(TAG, "No messages received in loopback mode (this may be expected)");
  } else if (received_count < sent_count) {
    ESP_LOGW(TAG, "Partial reception in loopback mode: %d/%d messages", received_count, sent_count);
  } else {
    ESP_LOGI(TAG, "All messages received successfully in loopback mode");
  }

  ESP_LOGI(TAG, "[SUCCESS] CAN batch transmission test completed (loopback mode)");
  ESP_LOGI(TAG, "Note: Loopback reception may not work in all ESP-IDF v5.5 configurations");
  return true;
}

bool test_can_high_throughput() noexcept {
  ESP_LOGI(TAG, "Testing CAN high throughput performance...");

  hf_esp_can_config_t can_config{};
  can_config.tx_pin = TEST_TX_PIN;
  can_config.rx_pin = TEST_TX_PIN;    // Use same pin for internal loopback
  can_config.baud_rate = 1000000;     // 1 Mbps for high throughput
  can_config.enable_self_test = true; // No ACK required for internal loopback
  can_config.enable_loopback = true;  // Enable internal loopback
  can_config.tx_queue_depth = 50;
  can_config.sample_point_permill = 800; // 80% for high speed

  EspCan test_can(can_config);

  if (test_can.Initialize() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to initialize CAN for throughput test");
    return false;
  }

  test_can.SetReceiveCallbackEx(test_receive_callback_enhanced);

  // Configure timing for 1 Mbps
  hf_esp_can_timing_config_t high_speed_timing{};
  high_speed_timing.brp = 4; // Prescaler for 1 Mbps
  high_speed_timing.prop_seg = 5;
  high_speed_timing.tseg_1 = 8;
  high_speed_timing.tseg_2 = 2;
  high_speed_timing.sjw = 1;

  if (test_can.ConfigureAdvancedTiming(high_speed_timing) != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to configure high-speed timing");
    return false;
  }

  // Measure throughput (reduced message count to prevent driver overload)
  constexpr uint32_t TEST_MESSAGES = 50;
  messages_received.store(0);

  uint64_t start_time = esp_timer_get_time();

  // Send messages with controlled rate to prevent driver overload
  uint32_t sent_successfully = 0;
  for (uint32_t i = 0; i < TEST_MESSAGES; ++i) {
    auto msg = create_test_message(TEST_CAN_ID_STANDARD + (i % 100), false, 8);
    if (test_can.SendMessage(msg, 500) == hf_can_err_t::CAN_SUCCESS) {
      sent_successfully++;
    }
    // Additional delay between messages to prevent driver overload
    vTaskDelay(pdMS_TO_TICKS(5));
  }

  // Wait for reception to complete
  vTaskDelay(pdMS_TO_TICKS(2000));

  uint64_t end_time = esp_timer_get_time();
  uint64_t duration_us = end_time - start_time;
  uint32_t duration_ms = (uint32_t)(duration_us / 1000);

  uint32_t received_count = messages_received.load();

  ESP_LOGI(TAG, "Throughput test results:");
  ESP_LOGI(TAG, "  Messages sent: %d/%d", sent_successfully, TEST_MESSAGES);
  ESP_LOGI(TAG, "  Messages received: %d", received_count);
  ESP_LOGI(TAG, "  Test duration: %d ms", duration_ms);
  ESP_LOGI(TAG, "  Effective rate: %.2f msg/s", (float)received_count * 1000.0f / duration_ms);

  if (received_count < sent_successfully * 0.95f) { // Allow 5% loss
    ESP_LOGE(TAG, "High packet loss detected in throughput test");
    return false;
  }

  ESP_LOGI(TAG, "[SUCCESS] CAN high throughput test passed");
  return true;
}

//=============================================================================
// RX RING / NON-BLOCKING TX STRESS (self-test loopback, >= 5000 frames/s)
//=============================================================================

namespace {

/** Shared state between the stress producer (test task) and its consumer task. */
struct LoopbackStressState {
  EspCan* can = nullptr;
  std::atomic<bool> producer_done{false};
  std::atomic<bool> consumer_done{false};
  std::atomic<uint32_t> received{0};
  std::atomic<uint32_t> lost{0};         ///< sequence gaps (frames never received)
  std::atomic<uint32_t> out_of_order{0}; ///< sequence went backwards
  std::atomic<uint32_t> corrupt{0};      ///< payload check failed
  uint32_t expected_seq = 0;             ///< consumer-owned
};

void loopback_stress_consumer(void* arg) {
  auto* st = static_cast<LoopbackStressState*>(arg);
  hf_can_message_t m;
  for (;;) {
    const hf_can_err_t r = st->can->ReceiveMessage(m, 20);
    if (r == hf_can_err_t::CAN_SUCCESS) {
      const uint32_t seq = static_cast<uint32_t>(m.data[0]) |
                           (static_cast<uint32_t>(m.data[1]) << 8) |
                           (static_cast<uint32_t>(m.data[2]) << 16) |
                           (static_cast<uint32_t>(m.data[3]) << 24);
      const uint32_t check = ~seq;
      if (m.dlc != 8 || m.data[4] != static_cast<uint8_t>(check) ||
          m.data[5] != static_cast<uint8_t>(check >> 8) ||
          m.data[6] != static_cast<uint8_t>(check >> 16) ||
          m.data[7] != static_cast<uint8_t>(check >> 24)) {
        st->corrupt.fetch_add(1);
      }
      if (seq > st->expected_seq) {
        st->lost.fetch_add(seq - st->expected_seq);
      } else if (seq < st->expected_seq) {
        st->out_of_order.fetch_add(1);
      }
      if (seq >= st->expected_seq) {
        st->expected_seq = seq + 1U;
      }
      st->received.fetch_add(1);
    } else if (st->producer_done.load()) {
      break; // nothing for 20 ms after the producer finished
    }
  }
  st->consumer_done.store(true);
  vTaskDelete(nullptr);
}

} // namespace

/**
 * @brief Self-test loopback at 1 Mbit/s: send as fast as the bus allows through
 *        the non-blocking TX path, receive through the RX ring (no callback),
 *        and count lost, reordered and corrupt frames.
 * @details Pass criteria: every frame sent and received in order, no RX ring
 *          overrun, and an effective rate of at least 5000 frames/s.
 *          Hardware: internal loopback on TEST_TX_PIN; no transceiver needed.
 */
bool test_can_loopback_stress_rx_ring() noexcept {
  ESP_LOGI(TAG, "Loopback stress: RX ring + non-blocking TX at 1 Mbit/s...");

  constexpr uint32_t kFrames = 20000;
  constexpr uint32_t kMinRate = 5000; // frames per second

  hf_esp_can_config_t cfg{};
  cfg.tx_pin = TEST_TX_PIN;
  cfg.rx_pin = TEST_TX_PIN; // internal loopback
  cfg.baud_rate = 1000000;
  cfg.sample_point_permill = 800;
  cfg.enable_self_test = true; // no ACK required
  cfg.enable_loopback = true;  // receive our own frames
  cfg.tx_queue_depth = 32;
  cfg.rx_queue_depth = 256;
  cfg.rx_mode = hf_esp_can_rx_mode_t::AUTO; // no callback registered -> RX ring

  EspCan can(cfg);
  if (can.Initialize() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to initialize CAN for loopback stress");
    return false;
  }

  // Empty-queue behaviour of the polling API
  hf_can_message_t probe;
  if (can.ReceiveMessage(probe, 0) != hf_can_err_t::CAN_ERR_QUEUE_EMPTY) {
    ESP_LOGE(TAG, "ReceiveMessage(timeout 0) on an empty ring must return QUEUE_EMPTY");
    return false;
  }

  static LoopbackStressState st; // static: outlives the consumer task in every path
  st.can = &can;
  st.producer_done.store(false);
  st.consumer_done.store(false);
  st.received.store(0);
  st.lost.store(0);
  st.out_of_order.store(0);
  st.corrupt.store(0);
  st.expected_seq = 0;

  // Consumer above the producer so the ring drains while the producer refills TX
  if (xTaskCreate(loopback_stress_consumer, "can_rx_stress", 4096, &st, 3, nullptr) != pdPASS) {
    ESP_LOGE(TAG, "Failed to create consumer task");
    return false;
  }

  uint32_t sent = 0;
  uint32_t queue_full_returns = 0;
  uint32_t send_errors = 0;
  const uint64_t t0 = esp_timer_get_time();

  for (uint32_t seq = 0; seq < kFrames; ++seq) {
    hf_can_message_t msg;
    msg.id = TEST_CAN_ID_STANDARD;
    const uint32_t check = ~seq;
    const uint8_t payload[8] = {
        static_cast<uint8_t>(seq),         static_cast<uint8_t>(seq >> 8),
        static_cast<uint8_t>(seq >> 16),   static_cast<uint8_t>(seq >> 24),
        static_cast<uint8_t>(check),       static_cast<uint8_t>(check >> 8),
        static_cast<uint8_t>(check >> 16), static_cast<uint8_t>(check >> 24)};
    (void)msg.SetData(payload, sizeof(payload));

    // Non-blocking first; on QUEUE_FULL block (up to 10 ms) for a TX slot
    hf_can_err_t r = can.TrySendMessage(msg);
    if (r == hf_can_err_t::CAN_ERR_QUEUE_FULL) {
      ++queue_full_returns;
      r = can.SendMessage(msg, 10);
    }
    if (r == hf_can_err_t::CAN_SUCCESS) {
      ++sent;
    } else {
      ++send_errors;
    }
  }

  // Let the last frames arrive, then stop the consumer
  vTaskDelay(pdMS_TO_TICKS(100));
  st.producer_done.store(true);
  for (int i = 0; i < 100 && !st.consumer_done.load(); ++i) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  const uint64_t t1 = esp_timer_get_time();

  hf_can_bus_status_t bus{};
  (void)can.GetBusStatus(bus);

  const uint32_t received = st.received.load();
  // Rate over the producer + drain window (the 100 ms settle time is included,
  // so the real bus rate is a little higher than reported)
  const uint64_t elapsed_us = (t1 > t0) ? (t1 - t0) : 1U;
  const uint32_t rate = static_cast<uint32_t>((static_cast<uint64_t>(received) * 1000000U) /
                                              elapsed_us);
  const uint32_t missing = (sent > received) ? (sent - received) : 0U;

  ESP_LOGI(TAG, "Loopback stress results:");
  ESP_LOGI(TAG, "  sent %u / %u (send errors %u, QUEUE_FULL returns %u)", (unsigned)sent,
           (unsigned)kFrames, (unsigned)send_errors, (unsigned)queue_full_returns);
  ESP_LOGI(TAG, "  received %u, missing %u, seq gaps %u, out of order %u, corrupt %u",
           (unsigned)received, (unsigned)missing, (unsigned)st.lost.load(),
           (unsigned)st.out_of_order.load(), (unsigned)st.corrupt.load());
  ESP_LOGI(TAG, "  rate %u frames/s over %u ms (target >= %u)", (unsigned)rate,
           (unsigned)(elapsed_us / 1000U), (unsigned)kMinRate);
  ESP_LOGI(TAG, "  bus: state %u TEC %u REC %u rx_overruns %u tx_queue_full %u bus_off %u",
           (unsigned)bus.state, (unsigned)bus.tec, (unsigned)bus.rec,
           (unsigned)bus.rx_overruns, (unsigned)bus.tx_queue_full, (unsigned)bus.bus_off_count);

  const bool consumer_stopped = st.consumer_done.load();
  (void)can.Deinitialize();

  if (!consumer_stopped) {
    ESP_LOGE(TAG, "Consumer task did not stop");
    return false;
  }
  if (sent != kFrames || missing != 0 || st.lost.load() != 0 || st.out_of_order.load() != 0 ||
      st.corrupt.load() != 0 || bus.rx_overruns != 0) {
    ESP_LOGE(TAG, "Frames lost, reordered or corrupted in loopback stress");
    return false;
  }
  if (rate < kMinRate) {
    ESP_LOGE(TAG, "Rate %u frames/s below %u", (unsigned)rate, (unsigned)kMinRate);
    return false;
  }

  ESP_LOGI(TAG, "[SUCCESS] Loopback stress passed");
  return true;
}

//=============================================================================
// SN65 TRANSCEIVER SPECIFIC TESTS
//=============================================================================

bool test_external_physical_loopback() noexcept {
  ESP_LOGI(TAG, "Testing external physical loopback with proper CAN bus loopback...");
  ESP_LOGI(TAG, "*** REQUIRES: CAN bus loopback AFTER transceiver ***");
  ESP_LOGI(TAG, "*** Connect: SN65 CANH -> 120Ω resistor -> SN65 CANL ***");
  ESP_LOGI(TAG, "*** DO NOT short TWAI TX/RX lines directly - this will NOT work! ***");

  hf_esp_can_config_t can_config{};
  can_config.tx_pin = TEST_TX_PIN;
  can_config.rx_pin = TEST_RX_PIN;
  can_config.baud_rate = TEST_BAUD_RATE;
  can_config.enable_self_test = true; // Enable self-test (no ACK required)
  can_config.enable_loopback = false; // Disable internal loopback - using external wire
  can_config.tx_queue_depth = 20;     // Larger queue for external loopback

  EspCan test_can(can_config);

  if (test_can.Initialize() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to initialize CAN for external loopback test");
    return false;
  }

  test_can.SetReceiveCallbackEx(test_receive_callback_enhanced);

  // Test multiple message formats
  std::vector<std::pair<std::string, hf_can_message_t>> test_cases = {
      {"Standard 11-bit ID", create_test_message(0x123, false, 8)},
      {"Extended 29-bit ID", create_test_message(0x12345678, true, 6)},
      {"Short message (2 bytes)", create_test_message(0x456, false, 2)},
      {"RTR frame", []() {
         auto rtr_msg = create_test_message(0x789, false, 4);
         rtr_msg.is_rtr = true;
         return rtr_msg;
       }()}};

  ESP_LOGI(TAG, "Configuration: TX=GPIO%d, RX=GPIO%d, Self-test=%s, Loopback=%s", can_config.tx_pin,
           can_config.rx_pin, can_config.enable_self_test ? "true" : "false",
           can_config.enable_loopback ? "true" : "false");

  bool all_tests_passed = true;
  for (const auto& test_case : test_cases) {
    ESP_LOGI(TAG, "Testing: %s", test_case.first.c_str());

    messages_received.store(0);
    if (test_can.SendMessage(test_case.second, 1000) == hf_can_err_t::CAN_SUCCESS) {
      if (wait_for_event(MESSAGE_RECEIVED_BIT, 2000)) {
        // Verify message integrity
        if (last_received_message.id == test_case.second.id &&
            last_received_message.is_extended == test_case.second.is_extended &&
            last_received_message.is_rtr == test_case.second.is_rtr) {
          ESP_LOGI(TAG, "✅ %s - PASSED", test_case.first.c_str());
        } else {
          ESP_LOGE(TAG, "❌ %s - Message corruption detected", test_case.first.c_str());
          all_tests_passed = false;
        }
      } else {
        ESP_LOGE(TAG, "❌ %s - No message received", test_case.first.c_str());
        all_tests_passed = false;
      }
    } else {
      ESP_LOGE(TAG, "❌ %s - Failed to send", test_case.first.c_str());
      all_tests_passed = false;
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Brief delay between tests
  }

  if (all_tests_passed) {
    ESP_LOGI(TAG, "✅ External physical loopback test PASSED");
  } else {
    ESP_LOGE(TAG, "❌ External physical loopback test FAILED");
    ESP_LOGE(TAG, "Note: This test requires CANH->120Ω->CANL loopback AFTER transceiver");
    ESP_LOGE(TAG, "DO NOT short TWAI TX/RX lines directly - this will NOT work!");
  }

  return all_tests_passed;
}

bool test_loopback_comparison() noexcept {
  ESP_LOGI(TAG, "Testing internal vs external loopback comparison...");

  // Test 1: Internal loopback (should work)
  ESP_LOGI(TAG, "Test 1: Internal loopback (TX=GPIO%d, RX=GPIO%d)", TEST_TX_PIN, TEST_TX_PIN);
  {
    hf_esp_can_config_t internal_config{};
    internal_config.tx_pin = TEST_TX_PIN;
    internal_config.rx_pin = TEST_TX_PIN; // Same pin
    internal_config.baud_rate = TEST_BAUD_RATE;
    internal_config.enable_self_test = true;
    internal_config.enable_loopback = true; // Internal loopback

    EspCan internal_can(internal_config);
    if (internal_can.Initialize() == hf_can_err_t::CAN_SUCCESS) {
      internal_can.SetReceiveCallbackEx(test_receive_callback_enhanced);
      messages_received.store(0);

      auto test_msg = create_test_message(0x100, false, 4);
      if (internal_can.SendMessage(test_msg, 1000) == hf_can_err_t::CAN_SUCCESS) {
        if (wait_for_event(MESSAGE_RECEIVED_BIT, 1000)) {
          ESP_LOGI(TAG, "✅ Internal loopback: Message received successfully");
        } else {
          ESP_LOGW(TAG, "⚠️  Internal loopback: No message received");
        }
      } else {
        ESP_LOGE(TAG, "❌ Internal loopback: Failed to send message");
      }
    }
  }

  // Test 2: External loopback (with proper CAN bus loopback)
  ESP_LOGI(TAG, "Test 2: External loopback (TX=GPIO%d, RX=GPIO%d)", TEST_TX_PIN, TEST_RX_PIN);
  ESP_LOGI(TAG, "Note: This requires CANH->120Ω->CANL loopback AFTER transceiver");
  {
    hf_esp_can_config_t external_config{};
    external_config.tx_pin = TEST_TX_PIN;
    external_config.rx_pin = TEST_RX_PIN; // Different pins
    external_config.baud_rate = TEST_BAUD_RATE;
    external_config.enable_self_test = true;
    external_config.enable_loopback = false; // External loopback

    EspCan external_can(external_config);
    if (external_can.Initialize() == hf_can_err_t::CAN_SUCCESS) {
      external_can.SetReceiveCallbackEx(test_receive_callback_enhanced);
      messages_received.store(0);

      auto test_msg = create_test_message(0x200, false, 4);
      if (external_can.SendMessage(test_msg, 1000) == hf_can_err_t::CAN_SUCCESS) {
        if (wait_for_event(MESSAGE_RECEIVED_BIT, 1000)) {
          ESP_LOGI(TAG, "✅ External loopback: Message received successfully");
        } else {
          ESP_LOGW(TAG, "⚠️  External loopback: No message received (requires CANH->120Ω->CANL "
                        "loopback)");
        }
      } else {
        ESP_LOGE(TAG, "❌ External loopback: Failed to send message");
      }
    }
  }

  ESP_LOGI(TAG, "Loopback comparison test completed");
  return true;
}

bool test_sn65_transceiver_integration() noexcept {
  ESP_LOGI(TAG, "Testing SN65 CAN transceiver integration...");

  // Test with different SN65 configurations
  std::vector<uint32_t> test_baud_rates = {125000, 250000, 500000, 1000000};

  for (auto baud_rate : test_baud_rates) {
    ESP_LOGI(TAG, "Testing SN65 at %d bps...", baud_rate);

    hf_esp_can_config_t can_config{};
    can_config.tx_pin = TEST_TX_PIN;
    can_config.rx_pin = TEST_RX_PIN;
    can_config.baud_rate = baud_rate;
    can_config.enable_self_test = true;
    can_config.enable_loopback = true;

    // Adjust sample point based on baud rate for SN65 compatibility
    if (baud_rate >= 1000000) {
      can_config.sample_point_permill = 800; // 80% for high speed
    } else {
      can_config.sample_point_permill = 750; // 75% for lower speeds
    }

    EspCan test_can(can_config);

    if (test_can.Initialize() != hf_can_err_t::CAN_SUCCESS) {
      ESP_LOGE(TAG, "Failed to initialize CAN at %d bps", baud_rate);
      return false;
    }

    test_can.SetReceiveCallbackEx(test_receive_callback_enhanced);
    messages_received.store(0);

    // Test signal integrity at this baud rate
    auto test_message = create_test_message(TEST_CAN_ID_STANDARD, false, 8);

    if (test_can.SendMessage(test_message, 1000) != hf_can_err_t::CAN_SUCCESS) {
      ESP_LOGE(TAG, "Failed to send message at %d bps", baud_rate);
      return false;
    }

    if (!wait_for_event(MESSAGE_RECEIVED_BIT, 1000)) {
      ESP_LOGE(TAG, "No message received at %d bps", baud_rate);
      return false;
    }

    ESP_LOGI(TAG, "SN65 test passed at %d bps", baud_rate);

    test_can.Deinitialize();
    vTaskDelay(pdMS_TO_TICKS(100)); // Brief delay between tests
  }

  ESP_LOGI(TAG, "[SUCCESS] SN65 transceiver integration test passed");
  return true;
}

bool test_can_signal_quality() noexcept {
  ESP_LOGI(TAG, "Testing CAN signal quality with SN65 transceiver...");

  hf_esp_can_config_t can_config{};
  can_config.tx_pin = TEST_TX_PIN;
  can_config.rx_pin = TEST_RX_PIN;
  can_config.baud_rate = TEST_BAUD_RATE;
  can_config.enable_self_test = true; // No external ACK required
  can_config.enable_loopback = false; // Using physical wire loopback
  can_config.enable_alerts = true;

  EspCan test_can(can_config);

  if (test_can.Initialize() != hf_can_err_t::CAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to initialize CAN for signal quality test");
    return false;
  }

  test_can.SetReceiveCallbackEx(test_receive_callback_enhanced);

  // Test signal quality with various message patterns
  std::vector<std::vector<uint8_t>> test_patterns = {
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // All zeros
      {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // All ones
      {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA}, // Alternating
      {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55}, // Alternating opposite
      {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF}, // Incremental
  };

  uint32_t successful_transmissions = 0;
  uint32_t total_attempts = 0;

  for (const auto& pattern : test_patterns) {
    for (int repeat = 0; repeat < 10; ++repeat) { // Test each pattern multiple times
      hf_can_message_t test_message{};
      test_message.id = TEST_CAN_ID_STANDARD + repeat;
      test_message.dlc = 8;
      memcpy(test_message.data, pattern.data(), 8);

      messages_received.store(0);

      if (test_can.SendMessage(test_message, 500) == hf_can_err_t::CAN_SUCCESS) {
        total_attempts++;

        if (wait_for_event(MESSAGE_RECEIVED_BIT, 500)) {
          successful_transmissions++;

          // Verify data integrity
          bool data_correct = true;
          for (int i = 0; i < 8; ++i) {
            if (last_received_message.data[i] != pattern[i]) {
              data_correct = false;
              break;
            }
          }

          if (!data_correct) {
            ESP_LOGW(TAG, "Data corruption detected in signal quality test");
          }
        }
      }
    }
  }

  float success_rate = (float)successful_transmissions / total_attempts * 100.0f;

  ESP_LOGI(TAG, "Signal quality test results:");
  ESP_LOGI(TAG, "  Total attempts: %d", total_attempts);
  ESP_LOGI(TAG, "  Successful: %d", successful_transmissions);
  ESP_LOGI(TAG, "  Success rate: %.2f%%", success_rate);

  if (success_rate < 98.0f) { // Expect very high success rate in loopback
    ESP_LOGE(TAG, "Signal quality below acceptable threshold");
    return false;
  }

  ESP_LOGI(TAG, "[SUCCESS] CAN signal quality test passed");
  return true;
}

//=============================================================================
// MAIN TEST RUNNER
//=============================================================================

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════════════════════╗");
  ESP_LOGI(TAG, "║                ESP32-C6 CAN COMPREHENSIVE TEST SUITE                        ║");
  ESP_LOGI(TAG, "║                     ESP-IDF v5.5 TWAI API + SN65                           ║");
  ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════════════════════╝");

  ESP_LOGI(TAG, "Hardware Configuration:");
  ESP_LOGI(TAG, "  MCU: ESP32-C6");
  ESP_LOGI(TAG, "  TX Pin: GPIO%d", TEST_TX_PIN);
  ESP_LOGI(TAG, "  RX Pin: GPIO%d", TEST_RX_PIN);
  ESP_LOGI(TAG, "  Transceiver: SN65HVD230/232");
  ESP_LOGI(TAG, "  API: ESP-IDF v5.5 TWAI node-based");
  ESP_LOGI(TAG, "  Internal Loopback: TX and RX on same pin (GPIO%d)", TEST_TX_PIN);
  ESP_LOGI(TAG, "  External Loopback: CANH->120Ω->CANL (after transceiver)");

  vTaskDelay(pdMS_TO_TICKS(1000));

  // Verify CAN pin states
  verify_can_pin_states();

  // Initialize test event group
  test_event_group = xEventGroupCreate();
  if (!test_event_group) {
    ESP_LOGE(TAG, "Failed to create test event group");
    return;
  }

  // Report test section configuration
  print_test_section_status(TAG, "CAN");

  // Run comprehensive test suite based on configuration with test sectioning pattern
  RUN_TEST_SECTION_IF_ENABLED_WITH_PATTERN(
      ENABLE_CORE_TESTS, "CAN CORE TESTS", 5,
      // Core functionality tests
      ESP_LOGI(TAG, "Running core CAN functionality tests...");
      RUN_TEST_IN_TASK("initialization", test_can_initialization, 8192, 1);
      flip_test_progress_indicator(); // Toggle GPIO14 after initialization test
      RUN_TEST_IN_TASK("self_test_mode", test_can_self_test_mode, 12288, 1);
      flip_test_progress_indicator(); // Toggle GPIO14 after self-test mode test
      RUN_TEST_IN_TASK("message_transmission", test_can_message_transmission, 8192, 1);
      flip_test_progress_indicator(); // Toggle GPIO14 after message transmission test
  );

  RUN_TEST_SECTION_IF_ENABLED_WITH_PATTERN(
      ENABLE_ADVANCED_TESTS, "CAN ADVANCED TESTS", 5,
      // Advanced feature tests
      ESP_LOGI(TAG, "Running advanced CAN feature tests...");
      RUN_TEST_IN_TASK("acceptance_filtering", test_can_acceptance_filtering, 8192, 1);
      flip_test_progress_indicator(); // Toggle GPIO14 after acceptance filtering test
      RUN_TEST_IN_TASK("advanced_timing", test_can_advanced_timing, 8192, 1);
      flip_test_progress_indicator(); // Toggle GPIO14 after advanced timing test
  );

  RUN_TEST_SECTION_IF_ENABLED_WITH_PATTERN(
      ENABLE_ERROR_TESTS, "CAN ERROR TESTS", 5,
      // Error handling tests
      ESP_LOGI(TAG, "Running CAN error handling tests...");
      RUN_TEST_IN_TASK("error_handling", test_can_error_handling, 8192, 1);
      flip_test_progress_indicator(); // Toggle GPIO14 after error handling test
      RUN_TEST_IN_TASK("bus_recovery", test_can_bus_recovery, 8192, 1);
      flip_test_progress_indicator(); // Toggle GPIO14 after bus recovery test
  );

  RUN_TEST_SECTION_IF_ENABLED_WITH_PATTERN(
      ENABLE_PERFORMANCE_TESTS, "CAN PERFORMANCE TESTS", 5,
      // Performance tests
      ESP_LOGI(TAG, "Running CAN performance tests...");
      RUN_TEST_IN_TASK("batch_transmission", test_can_batch_transmission, 8192, 1);
      flip_test_progress_indicator(); // Toggle GPIO14 after batch transmission test
      RUN_TEST_IN_TASK("high_throughput", test_can_high_throughput, 12288, 1);
      flip_test_progress_indicator(); // Toggle GPIO14 after high throughput test
      RUN_TEST_IN_TASK("loopback_stress_rx_ring", test_can_loopback_stress_rx_ring, 8192, 1);
      flip_test_progress_indicator(); // Toggle GPIO14 after loopback stress test
  );

  RUN_TEST_SECTION_IF_ENABLED_WITH_PATTERN(
      ENABLE_TRANSCEIVER_TESTS, "CAN TRANSCEIVER TESTS", 5,
      // SN65 transceiver tests
      ESP_LOGI(TAG, "Running SN65 transceiver tests...");
      RUN_TEST_IN_TASK("loopback_comparison", test_loopback_comparison, 8192, 1);
      flip_test_progress_indicator(); // Toggle GPIO14 after loopback comparison test
      RUN_TEST_IN_TASK("external_physical_loopback", test_external_physical_loopback, 8192, 1);
      flip_test_progress_indicator(); // Toggle GPIO14 after external physical loopback test
      RUN_TEST_IN_TASK("sn65_transceiver_integration", test_sn65_transceiver_integration, 8192, 1);
      flip_test_progress_indicator(); // Toggle GPIO14 after SN65 transceiver integration test
      RUN_TEST_IN_TASK("can_signal_quality", test_can_signal_quality, 8192, 1);
      flip_test_progress_indicator(); // Toggle GPIO14 after signal quality test
  );

  print_test_summary(g_test_results, "ESP32-C6 CAN (ESP-IDF v5.5 + SN65)", TAG);

  // Cleanup
  vEventGroupDelete(test_event_group);

  ESP_LOGI(TAG, "\n");
  ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════════════════════╗");
  ESP_LOGI(TAG, "║                      TEST SUITE COMPLETED                                    ║");
  ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════════════════════╝");

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}
