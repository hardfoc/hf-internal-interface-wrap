/**
 * @file BaseCan.h
 * @ingroup can
 * @brief Abstract base class for CAN bus implementations in the HardFOC system.
 *
 * This header-only file defines the abstract base class for CAN bus communication
 * that provides a consistent API across different CAN controller implementations.
 * Concrete implementations for various microcontrollers inherit from this class.
 *
 * ERROR HANDLING:
 * - All CAN error codes are defined in this file using the hf_can_err_t enumeration
 * - Lower-level types (McuTypes_CAN.h) maintain minimal error constants for compatibility
 * - All virtual methods return hf_can_err_t for comprehensive error reporting
 * - No legacy compatibility code - use hf_can_message_t structure for all operations
 *
 * @author Nebiyu Tadesse
 * @date 2025
 * @copyright HardFOC
 *
 * @note This is a header-only abstract base class following the same pattern as BaseAdc.
 * @note Users should program against this interface, not specific implementations.
 * @note All legacy camelCase types and bool-returning methods have been removed.
 */

#pragma once

#include "HardwareTypes.h"
#include <atomic>
#include <cstdint>
#include <string_view>

/**
 * @defgroup can CAN Module
 * @brief All CAN-related types, enums, and functions for Controller Area Network operations.
 */

//--------------------------------------
//  HardFOC CAN Error Codes (Table)
//--------------------------------------
/**
 * @brief HardFOC CAN error codes
 * @ingroup can
 * @details Comprehensive error enumeration for all CAN operations in the system.
 *          This enumeration is used across all CAN-related classes to provide
 *          consistent error reporting and handling.
 */

#define HF_CAN_ERR_LIST(X)                                                                         \
  /* Success codes */                                                                              \
  X(CAN_SUCCESS, 0, "Success")                                                                     \
  /* General errors */                                                                             \
  X(CAN_ERR_FAILURE, 1, "General failure")                                                         \
  X(CAN_ERR_NOT_INITIALIZED, 2, "Not initialized")                                                 \
  X(CAN_ERR_ALREADY_INITIALIZED, 3, "Already initialized")                                         \
  X(CAN_ERR_INVALID_PARAMETER, 4, "Invalid parameter")                                             \
  X(CAN_ERR_NULL_POINTER, 5, "Null pointer")                                                       \
  X(CAN_ERR_OUT_OF_MEMORY, 6, "Out of memory")                                                     \
  /* Bus errors */                                                                                 \
  X(CAN_ERR_BUS_OFF, 7, "Bus off state")                                                           \
  X(CAN_ERR_BUS_ERROR, 8, "Bus error")                                                             \
  X(CAN_ERR_BUS_BUSY, 9, "Bus busy")                                                               \
  X(CAN_ERR_BUS_NOT_AVAILABLE, 10, "Bus not available")                                            \
  X(CAN_ERR_BUS_RECOVERY_FAILED, 11, "Bus recovery failed")                                        \
  X(CAN_ERR_BUS_ARBITRATION_LOST, 12, "Bus arbitration lost")                                      \
  /* Message errors */                                                                             \
  X(CAN_ERR_MESSAGE_TIMEOUT, 13, "Message timeout")                                                \
  X(CAN_ERR_MESSAGE_LOST, 14, "Message lost")                                                      \
  X(CAN_ERR_MESSAGE_INVALID, 15, "Invalid message")                                                \
  X(CAN_ERR_MESSAGE_TOO_LONG, 16, "Message too long")                                              \
  X(CAN_ERR_MESSAGE_INVALID_ID, 17, "Invalid message ID")                                          \
  X(CAN_ERR_MESSAGE_INVALID_DLC, 18, "Invalid data length code")                                   \
  X(CAN_ERR_QUEUE_FULL, 19, "Queue full")                                                          \
  X(CAN_ERR_QUEUE_EMPTY, 20, "Queue empty")                                                        \
  /* Transmission errors */                                                                        \
  X(CAN_ERR_TX_FAILED, 21, "Transmission failed")                                                  \
  X(CAN_ERR_TX_ABORTED, 22, "Transmission aborted")                                                \
  X(CAN_ERR_TX_ERROR_PASSIVE, 23, "Transmit error passive")                                        \
  X(CAN_ERR_TX_ERROR_WARNING, 24, "Transmit error warning")                                        \
  /* Reception errors */                                                                           \
  X(CAN_ERR_RX_OVERRUN, 25, "Receive overrun")                                                     \
  X(CAN_ERR_RX_ERROR_PASSIVE, 26, "Receive error passive")                                         \
  X(CAN_ERR_RX_ERROR_WARNING, 27, "Receive error warning")                                         \
  X(CAN_ERR_RX_FIFO_FULL, 28, "Receive FIFO full")                                                 \
  /* Hardware errors */                                                                            \
  X(CAN_ERR_HARDWARE_FAULT, 29, "Hardware fault")                                                  \
  X(CAN_ERR_COMMUNICATION_FAILURE, 30, "Communication failure")                                    \
  X(CAN_ERR_DEVICE_NOT_RESPONDING, 31, "Device not responding")                                    \
  X(CAN_ERR_VOLTAGE_OUT_OF_RANGE, 32, "Voltage out of range")                                      \
  X(CAN_ERR_CLOCK_ERROR, 33, "Clock error")                                                        \
  X(CAN_ERR_TRANSCEIVER_ERROR, 34, "Transceiver error")                                            \
  /* Configuration errors */                                                                       \
  X(CAN_ERR_INVALID_CONFIGURATION, 35, "Invalid configuration")                                    \
  X(CAN_ERR_UNSUPPORTED_OPERATION, 36, "Unsupported operation")                                    \
  X(CAN_ERR_INVALID_BAUD_RATE, 37, "Invalid baud rate")                                            \
  X(CAN_ERR_INVALID_CONTROLLER_ID, 38, "Invalid controller ID")                                    \
  X(CAN_ERR_FILTER_ERROR, 39, "Filter error")                                                      \
  X(CAN_ERR_FILTER_FULL, 40, "Filter table full")                                                  \
  /* Protocol errors */                                                                            \
  X(CAN_ERR_STUFF_ERROR, 41, "Bit stuffing error")                                                 \
  X(CAN_ERR_FORM_ERROR, 42, "Frame format error")                                                  \
  X(CAN_ERR_CRC_ERROR, 43, "CRC error")                                                            \
  X(CAN_ERR_ACK_ERROR, 44, "Acknowledgment error")                                                 \
  X(CAN_ERR_BIT_ERROR, 45, "Bit error")                                                            \
  /* System errors */                                                                              \
  X(CAN_ERR_SYSTEM_ERROR, 46, "System error")                                                      \
  X(CAN_ERR_PERMISSION_DENIED, 47, "Permission denied")                                            \
  X(CAN_ERR_OPERATION_ABORTED, 48, "Operation aborted")                                            \
                                                                                                   \
  /* Extended CAN errors (for ESP32 compatibility) */                                              \
  X(CAN_ERR_FAIL, 49, "Generic failure")                                                           \
  X(CAN_ERR_RESOURCE_BUSY, 50, "Resource busy")                                                    \
  X(CAN_ERR_INVALID_STATE, 51, "Invalid state")                                                    \
  X(CAN_ERR_NOT_SUPPORTED, 52, "Not supported")                                                    \
  X(CAN_ERR_TIMEOUT_ALT, 53, "Operation timeout")                                                  \
  X(CAN_ERR_UNKNOWN, 54, "Unknown error")

enum class hf_can_err_t : hf_u8_t {
#define X(NAME, VALUE, DESC) NAME = VALUE,
  HF_CAN_ERR_LIST(X)
#undef X
};

/**
 * @brief Convert hf_can_err_t to human-readable string
 * @param err The error code to convert
 * @return String view of the error description
 */
constexpr std::string_view HfCanErrToString(hf_can_err_t err) noexcept {
  switch (err) {
#define X(NAME, VALUE, DESC)                                                                       \
  case hf_can_err_t::NAME:                                                                         \
    return DESC;
    HF_CAN_ERR_LIST(X)
#undef X
  default:
    return HfCanErrToString(hf_can_err_t::CAN_ERR_UNKNOWN);
  }
}

/**
 * @brief Platform-agnostic CAN bus configuration.
 * @details Configuration structure that works across different CAN implementations
 *          without exposing MCU-specific types.
 */
struct hf_can_config_t {
  hf_pin_num_t tx_pin;     ///< CAN TX pin
  hf_pin_num_t rx_pin;     ///< CAN RX pin
  hf_baud_rate_t baudrate; ///< CAN baudrate (bps)
  bool loopback_mode;      ///< Enable loopback mode for testing
  bool silent_mode;        ///< Enable silent mode (listen-only)
  hf_u16_t tx_queue_size;  ///< TX queue size (implementation-dependent)
  hf_u16_t rx_queue_size;  ///< RX queue size (implementation-dependent)

  hf_can_config_t() noexcept
      : tx_pin(HF_INVALID_PIN), rx_pin(HF_INVALID_PIN), baudrate(500000), loopback_mode(false),
        silent_mode(false), tx_queue_size(10), rx_queue_size(10) {}
};

//--------------------------------------
//  CAN / CAN FD frame model (BaseCan v2)
//--------------------------------------

/** @brief Largest classic CAN payload in bytes. */
inline constexpr hf_u8_t HF_CAN_CLASSIC_MAX_LEN = 8U;
/** @brief Largest CAN FD payload in bytes. */
inline constexpr hf_u8_t HF_CAN_FD_MAX_LEN = 64U;
/** @brief Largest 11-bit (base format) identifier. */
inline constexpr hf_u32_t HF_CAN_MAX_STD_ID = 0x7FFU;
/** @brief Largest 29-bit (extended format) identifier. */
inline constexpr hf_u32_t HF_CAN_MAX_EXT_ID = 0x1FFFFFFFU;
/** @brief Returned by HfCanLenToDlc() when a length cannot be carried. */
inline constexpr hf_u8_t HF_CAN_DLC_INVALID = 0xFFU;

/**
 * @brief Packed frame flags (see hf_can_message_t::GetFlags()).
 * @details The bit values are the same as `vortex::contracts::frame_flag`, so a
 *          flags byte can be copied between the two frame types unchanged.
 */
namespace hf_can_flag {
inline constexpr hf_u8_t EXT = 0x01U; ///< 29-bit identifier
inline constexpr hf_u8_t RTR = 0x02U; ///< remote request (classic only)
inline constexpr hf_u8_t FDF = 0x04U; ///< FD frame format
inline constexpr hf_u8_t BRS = 0x08U; ///< bit-rate switch (FD only)
inline constexpr hf_u8_t ESI = 0x10U; ///< error state indicator (FD only)
inline constexpr hf_u8_t ALL = EXT | RTR | FDF | BRS | ESI;
} // namespace hf_can_flag

/**
 * @brief DLC code to payload length in bytes (ISO 11898-1).
 * @param dlc DLC code 0..15
 * @param fd  true for an FD frame
 * @return Length in bytes. Classic codes 9..15 mean 8 bytes. Codes above 15 give 0.
 */
constexpr hf_u8_t HfCanDlcToLen(hf_u8_t dlc, bool fd) noexcept {
  constexpr hf_u8_t kFdTable[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
  if (dlc > 15U) {
    return 0U;
  }
  if (!fd) {
    return dlc > 8U ? 8U : dlc;
  }
  return kFdTable[dlc];
}

/**
 * @brief Payload length in bytes to the smallest DLC code that carries it.
 * @return HF_CAN_DLC_INVALID when @p len is above 8 (classic) or 64 (FD).
 */
constexpr hf_u8_t HfCanLenToDlc(hf_u8_t len, bool fd) noexcept {
  if (len <= 8U) {
    return len;
  }
  if (!fd || len > HF_CAN_FD_MAX_LEN) {
    return HF_CAN_DLC_INVALID;
  }
  for (hf_u8_t dlc = 9U; dlc < 16U; ++dlc) {
    if (HfCanDlcToLen(dlc, true) >= len) {
      return dlc;
    }
  }
  return HF_CAN_DLC_INVALID;
}

/**
 * @brief Is @p len a payload length a frame of this kind can have exactly?
 * @details Classic: 0..8. FD: 0..8, 12, 16, 20, 24, 32, 48, 64.
 */
constexpr bool HfCanIsValidLength(hf_u8_t len, bool fd) noexcept {
  if (!fd) {
    return len <= HF_CAN_CLASSIC_MAX_LEN;
  }
  const hf_u8_t dlc = HfCanLenToDlc(len, true);
  return dlc != HF_CAN_DLC_INVALID && HfCanDlcToLen(dlc, true) == len;
}

/**
 * @brief Why a frame is not well formed.
 * @details Same cases as `vortex::contracts::FrameError` except `UnknownFlag`,
 *          which cannot occur because hf_can_message_t keeps its flags as bools.
 */
enum class hf_can_frame_error_t : hf_u8_t {
  NONE = 0,
  ID_RANGE,            ///< identifier does not fit 11 / 29 bits
  CLASSIC_TOO_LONG,    ///< classic frame with length > 8
  FD_TOO_LONG,         ///< FD frame with length > 64
  FD_LENGTH_NOT_EXACT, ///< FD length that has no DLC code (for example 9)
  RTR_ON_FD,           ///< FD frames have no remote form
  FD_BITS_ON_CLASSIC,  ///< BRS / ESI without FD format
  RTR_WITH_DATA,       ///< remote frame with a requested length above 8
};

/**
 * @brief Platform-agnostic CAN / CAN FD message (BaseCan v2).
 *
 * @details The payload buffer is FD-sized (64 bytes) and the length is a byte
 *          count, never a DLC code:
 *          - `dlc` holds the **payload length in bytes**: 0..8 for classic
 *            frames, 0..8, 12, 16, 20, 24, 32, 48 or 64 for FD frames. The
 *            name is kept for source compatibility (for classic frames the
 *            byte count and the DLC code are the same number). Use
 *            GetLength() / SetLength() in new code, and GetDlcCode() /
 *            SetDlcCode() when a controller needs the 4-bit wire code.
 *          - SetLength() / SetDLC() reject lengths the frame kind cannot carry
 *            (above 8 on a classic frame, or an FD length without a DLC code),
 *            so `data` can never be over-read or over-written through them.
 *          - Flags: `is_extended`, `is_rtr`, `is_canfd`, `is_brs`, `is_esi`,
 *            also available packed as hf_can_flag bits via GetFlags() /
 *            SetFlags().
 *          - `timestamp_us` is the RX timestamp captured by the driver (ISR
 *            side). It is ignored on transmit.
 *
 *          Code that writes `dlc` directly must keep it valid for the frame
 *          kind; drivers check it with Validate() before touching `data`.
 *
 *          ToFrame() / FromFrame() convert to and from any frame struct with
 *          the members `id`, `len`, `flags`, `data[64]` and `ts_us` (the
 *          layout of `vortex::contracts::CanFrame`). A frame round trip
 *          (frame -> message -> frame) is lossless.
 */
struct hf_can_message_t {
  // === Core CAN Message Fields ===
  hf_u32_t id;                     ///< Message ID (11 or 29-bit)
  hf_u8_t dlc;                     ///< Payload length in BYTES (0..8 classic, 0..64 FD)
  hf_u8_t data[HF_CAN_FD_MAX_LEN]; ///< Payload (64 bytes, FD-sized)

  // === Standard CAN Flags ===
  bool is_extended;  ///< Extended ID flag (29-bit vs 11-bit)
  bool is_rtr;       ///< Remote transmission request flag
  bool is_ss;        ///< Single shot flag (no retransmission)
  bool is_self;      ///< Self reception request flag
  bool dlc_non_comp; ///< Received classic DLC code was 9..15 (length is 8)

  // === Metadata and Diagnostics ===
  hf_u64_t timestamp_us;    ///< RX timestamp in microseconds (driver clock)
  hf_u32_t sequence_number; ///< Message sequence number
  hf_u8_t controller_id;    ///< Originating controller ID
  hf_u8_t retry_count;      ///< Number of transmission retries
  hf_u8_t error_count;      ///< Associated error count

  // === CAN FD Fields ===
  bool is_canfd; ///< FD frame format (FDF)
  bool is_brs;   ///< Bit Rate Switching (FD only)
  bool is_esi;   ///< Error State Indicator (FD only)

  hf_can_message_t() noexcept
      : id(0), dlc(8), data{}, is_extended(false), is_rtr(false), is_ss(false), is_self(false),
        dlc_non_comp(false), timestamp_us(0), sequence_number(0), controller_id(0), retry_count(0),
        error_count(0), is_canfd(false), is_brs(false), is_esi(false) {}

  // --- Length -------------------------------------------------------------

  /** @brief Largest payload for the current frame kind (8 classic, 64 FD). */
  constexpr hf_u8_t GetMaxDataLength() const noexcept {
    return is_canfd ? HF_CAN_FD_MAX_LEN : HF_CAN_CLASSIC_MAX_LEN;
  }

  /** @brief Payload length in bytes. */
  constexpr hf_u8_t GetLength() const noexcept {
    return dlc;
  }

  /**
   * @brief Is @p len a valid payload length for the current frame kind?
   * @param len Length in bytes (not a DLC code)
   */
  constexpr bool IsValidLength(hf_u8_t len) const noexcept {
    return HfCanIsValidLength(len, is_canfd);
  }

  /**
   * @brief Set the payload length in bytes.
   * @return false (and nothing changed) when @p len is above 8 on a classic
   *         frame, above 64, or an FD length without a DLC code.
   */
  bool SetLength(hf_u8_t len) noexcept {
    if (!IsValidLength(len)) {
      return false;
    }
    dlc = len;
    dlc_non_comp = false;
    return true;
  }

  /** @brief Same as IsValidLength(); kept for source compatibility. */
  constexpr bool IsValidDLC(hf_u8_t len) const noexcept {
    return IsValidLength(len);
  }

  /** @brief Same as GetLength(); kept for source compatibility. */
  constexpr hf_u8_t GetEffectiveDLC() const noexcept {
    return dlc;
  }

  /** @brief Same as SetLength(); kept for source compatibility. */
  bool SetDLC(hf_u8_t len) noexcept {
    return SetLength(len);
  }

  /** @brief 4-bit DLC code for the current length (HF_CAN_DLC_INVALID if invalid). */
  constexpr hf_u8_t GetDlcCode() const noexcept {
    return IsValidLength(dlc) ? HfCanLenToDlc(dlc, is_canfd) : HF_CAN_DLC_INVALID;
  }

  /**
   * @brief Set the length from a 4-bit DLC code (as read from a controller).
   * @details Classic codes 9..15 give 8 bytes and set `dlc_non_comp`.
   * @return false when @p code is above 15.
   */
  bool SetDlcCode(hf_u8_t code) noexcept {
    if (code > 15U) {
      return false;
    }
    dlc = HfCanDlcToLen(code, is_canfd);
    dlc_non_comp = !is_canfd && code > 8U;
    return true;
  }

  /**
   * @brief Copy @p len bytes into the payload and set the length.
   * @return false (nothing changed) when @p len is invalid for the frame kind,
   *         or @p src is null with a non-zero length.
   */
  bool SetData(const hf_u8_t* src, hf_u8_t len) noexcept {
    if (!IsValidLength(len) || (src == nullptr && len != 0U)) {
      return false;
    }
    for (hf_u8_t i = 0; i < len; ++i) {
      data[i] = src[i];
    }
    dlc = len;
    dlc_non_comp = false;
    return true;
  }

  // --- Frame kind and flags ---------------------------------------------------

  /** @brief Set standard frame format (11-bit ID). */
  void SetStandardFrame() noexcept {
    is_extended = false;
  }

  /** @brief Set extended frame format (29-bit ID). */
  void SetExtendedFrame() noexcept {
    is_extended = true;
  }

  /** @brief Set data frame (not remote). */
  void SetDataFrame() noexcept {
    is_rtr = false;
  }

  /** @brief Set remote frame. */
  void SetRemoteFrame() noexcept {
    is_rtr = true;
  }

  /** @brief Set single shot transmission. */
  void SetSingleShot() noexcept {
    is_ss = true;
  }

  /** @brief Set self reception request. */
  void SetSelfReception() noexcept {
    is_self = true;
  }

  /**
   * @brief Make this an FD frame.
   * @param brs Bit-rate switch for the data phase
   */
  void SetFdFrame(bool brs = false) noexcept {
    is_canfd = true;
    is_rtr = false;
    is_brs = brs;
  }

  /**
   * @brief Make this a classic frame (clears BRS / ESI).
   * @return false (nothing changed) when the current length is above 8.
   */
  bool SetClassicFrame() noexcept {
    if (dlc > HF_CAN_CLASSIC_MAX_LEN) {
      return false;
    }
    is_canfd = false;
    is_brs = false;
    is_esi = false;
    return true;
  }

  /** @brief Flags packed as hf_can_flag bits. */
  constexpr hf_u8_t GetFlags() const noexcept {
    return static_cast<hf_u8_t>(
        (is_extended ? hf_can_flag::EXT : 0U) | (is_rtr ? hf_can_flag::RTR : 0U) |
        (is_canfd ? hf_can_flag::FDF : 0U) | (is_brs ? hf_can_flag::BRS : 0U) |
        (is_esi ? hf_can_flag::ESI : 0U));
  }

  /**
   * @brief Set the flags from hf_can_flag bits.
   * @return false (nothing changed) when @p flags has a bit outside hf_can_flag::ALL.
   */
  bool SetFlags(hf_u8_t flags) noexcept {
    if ((flags & static_cast<hf_u8_t>(~hf_can_flag::ALL)) != 0U) {
      return false;
    }
    is_extended = (flags & hf_can_flag::EXT) != 0U;
    is_rtr = (flags & hf_can_flag::RTR) != 0U;
    is_canfd = (flags & hf_can_flag::FDF) != 0U;
    is_brs = (flags & hf_can_flag::BRS) != 0U;
    is_esi = (flags & hf_can_flag::ESI) != 0U;
    return true;
  }

  // --- Validation -------------------------------------------------------------

  /** @brief Validate message ID for current frame format. */
  constexpr bool IsValidId() const noexcept {
    return id <= (is_extended ? HF_CAN_MAX_EXT_ID : HF_CAN_MAX_STD_ID);
  }

  /**
   * @brief Structural check drivers apply before transmit and after receive.
   * @details Same rules, in the same order, as `vortex::contracts::CheckFrame`.
   */
  constexpr hf_can_frame_error_t Validate() const noexcept {
    if (!IsValidId()) {
      return hf_can_frame_error_t::ID_RANGE;
    }
    if (is_canfd) {
      if (is_rtr) {
        return hf_can_frame_error_t::RTR_ON_FD;
      }
      if (dlc > HF_CAN_FD_MAX_LEN) {
        return hf_can_frame_error_t::FD_TOO_LONG;
      }
      if (!HfCanIsValidLength(dlc, true)) {
        return hf_can_frame_error_t::FD_LENGTH_NOT_EXACT;
      }
      return hf_can_frame_error_t::NONE;
    }
    if (is_brs || is_esi) {
      return hf_can_frame_error_t::FD_BITS_ON_CLASSIC;
    }
    if (dlc > HF_CAN_CLASSIC_MAX_LEN) {
      return is_rtr ? hf_can_frame_error_t::RTR_WITH_DATA : hf_can_frame_error_t::CLASSIC_TOO_LONG;
    }
    return hf_can_frame_error_t::NONE;
  }

  /** @brief Validate() == NONE. */
  constexpr bool IsValid() const noexcept {
    return Validate() == hf_can_frame_error_t::NONE;
  }

  // --- Conversion to / from a transport frame ---------------------------------

  /**
   * @brief Copy into a frame with members `id`, `len`, `flags`, `data[64]`, `ts_us`.
   * @details `ts_us` receives the low 32 bits of `timestamp_us`. The whole
   *          64-byte buffer is copied. `is_ss`, `is_self`, `sequence_number`
   *          and the other metadata have no place in the frame and are dropped.
   */
  template <typename Frame>
  void ToFrame(Frame& out) const noexcept {
    static_assert(sizeof(out.data) == HF_CAN_FD_MAX_LEN, "frame data must be 64 bytes");
    out.id = id;
    out.len = dlc;
    out.flags = GetFlags();
    for (hf_u8_t i = 0; i < HF_CAN_FD_MAX_LEN; ++i) {
      out.data[i] = data[i];
    }
    out.ts_us = static_cast<decltype(out.ts_us)>(timestamp_us);
  }

  /**
   * @brief Build a message from a frame with members `id`, `len`, `flags`, `data[64]`, `ts_us`.
   * @details Copies the fields as they are; call Validate() to check the result.
   *          Flag bits outside hf_can_flag::ALL are ignored.
   */
  template <typename Frame>
  static hf_can_message_t FromFrame(const Frame& in) noexcept {
    static_assert(sizeof(in.data) == HF_CAN_FD_MAX_LEN, "frame data must be 64 bytes");
    hf_can_message_t m;
    m.id = in.id;
    m.dlc = in.len;
    (void)m.SetFlags(static_cast<hf_u8_t>(in.flags & hf_can_flag::ALL));
    for (hf_u8_t i = 0; i < HF_CAN_FD_MAX_LEN; ++i) {
      m.data[i] = in.data[i];
    }
    m.timestamp_us = static_cast<hf_u64_t>(in.ts_us);
    return m;
  }
};

/**
 * @brief Controller error state (ISO 11898-1 fault confinement).
 * @details Same order as `vortex::contracts::BusState`.
 */
enum class hf_can_bus_state_t : hf_u8_t {
  STOPPED = 0,   ///< not started / in reset
  ERROR_ACTIVE,  ///< TEC and REC < 96
  ERROR_WARNING, ///< TEC or REC >= 96
  ERROR_PASSIVE, ///< TEC or REC >= 128
  BUS_OFF,       ///< TEC > 255
};

/** @brief Error state implied by the counters (warning at 96, passive at 128, bus-off above 255).
 */
constexpr hf_can_bus_state_t HfCanStateFromCounters(hf_u16_t tec, hf_u16_t rec) noexcept {
  if (tec > 255U) {
    return hf_can_bus_state_t::BUS_OFF;
  }
  if (tec >= 128U || rec >= 128U) {
    return hf_can_bus_state_t::ERROR_PASSIVE;
  }
  if (tec >= 96U || rec >= 96U) {
    return hf_can_bus_state_t::ERROR_WARNING;
  }
  return hf_can_bus_state_t::ERROR_ACTIVE;
}

/**
 * @brief Error counters and queue statistics of one controller (BaseCan::GetBusStatus()).
 * @details Same fields as `vortex::contracts::PortStatus`.
 */
struct hf_can_bus_status_t {
  hf_can_bus_state_t state{hf_can_bus_state_t::STOPPED}; ///< controller error state
  hf_u16_t tec{0};                                       ///< transmit error counter
  hf_u16_t rec{0};                                       ///< receive error counter
  hf_u32_t rx_overruns{0};   ///< frames lost because the RX queue was full
  hf_u32_t tx_queue_full{0}; ///< sends refused because the TX queue was full
  hf_u32_t bus_off_count{0}; ///< bus-off events since Initialize()
};

/**
 * @brief CAN bus status information structure.
 * @details Comprehensive status information for CAN bus monitoring and diagnostics.
 */
struct hf_can_status_t {
  hf_u32_t tx_error_count;  ///< Transmit error counter
  hf_u32_t rx_error_count;  ///< Receive error counter
  hf_u32_t tx_failed_count; ///< Failed transmission count
  hf_u32_t rx_missed_count; ///< Missed reception count
  bool bus_off;             ///< Bus-off state
  bool error_warning;       ///< Error warning state
  bool error_passive;       ///< Error passive state

  // CAN-FD specific status
  bool canfd_enabled;        ///< CAN-FD mode is active
  bool canfd_brs_enabled;    ///< Bit Rate Switching is enabled
  hf_u32_t nominal_baudrate; ///< Nominal bit rate (arbitration phase)
  hf_u32_t data_baudrate;    ///< Data bit rate (data phase for CAN-FD)
  hf_u32_t canfd_tx_count;   ///< Number of CAN-FD frames transmitted
  hf_u32_t canfd_rx_count;   ///< Number of CAN-FD frames received
  hf_u32_t brs_tx_count;     ///< Number of BRS frames transmitted
  hf_u32_t brs_rx_count;     ///< Number of BRS frames received
  hf_u32_t form_errors;      ///< CAN-FD form errors
  hf_u32_t stuff_errors;     ///< Stuff errors
  hf_u32_t crc_errors;       ///< CRC errors
  hf_u32_t bit_errors;       ///< Bit errors
  hf_u32_t ack_errors;       ///< Acknowledgment errors

  hf_can_status_t() noexcept
      : tx_error_count(0), rx_error_count(0), tx_failed_count(0), rx_missed_count(0),
        bus_off(false), error_warning(false), error_passive(false), canfd_enabled(false),
        canfd_brs_enabled(false), nominal_baudrate(0), data_baudrate(0), canfd_tx_count(0),
        canfd_rx_count(0), brs_tx_count(0), brs_rx_count(0), form_errors(0), stuff_errors(0),
        crc_errors(0), bit_errors(0), ack_errors(0) {}
};

/**
 * @brief CAN message receive callback function type.
 * @note Updated to use new hf_can_message_t structure
 */
using hf_can_receive_callback_t = void (*)(const hf_can_message_t& message);

/**
 * @brief CAN-FD specific receive callback with enhanced information
 * @param message Received CAN/CAN-FD message
 * @param reception_info Additional reception information (timing, errors, etc.)
 */
struct hf_can_reception_info_t {
  hf_u32_t timestamp_us;      ///< Reception timestamp in microseconds
  hf_u8_t rx_fifo_level;      ///< RX FIFO level when received
  bool data_phase_error;      ///< Error occurred in data phase
  bool arbitration_lost;      ///< Arbitration was lost during transmission
  float bit_timing_tolerance; ///< Measured bit timing tolerance
};
using hf_can_fd_receive_callback_t = void (*)(const hf_can_message_t& message,
                                              const hf_can_reception_info_t& info);

/**
 * @brief CAN bus statistics structure for performance monitoring.
 * @details Provides comprehensive statistics for monitoring CAN bus performance
 *          and identifying potential issues.
 */
struct hf_can_statistics_t {
  // Message counters (atomic for ISR safety)
  std::atomic<hf_u64_t> messages_sent{0};     ///< Total messages successfully sent
  std::atomic<hf_u64_t> messages_received{0}; ///< Total messages successfully received
  std::atomic<hf_u64_t> bytes_transmitted{0}; ///< Total bytes transmitted
  std::atomic<hf_u64_t> bytes_received{0};    ///< Total bytes received

  // Error counters (atomic for ISR safety)
  std::atomic<hf_u32_t> send_failures{0};          ///< Failed send operations
  std::atomic<hf_u32_t> receive_failures{0};       ///< Failed receive operations
  std::atomic<hf_u32_t> bus_error_count{0};        ///< Total bus errors
  std::atomic<hf_u32_t> arbitration_lost_count{0}; ///< Arbitration lost events
  std::atomic<hf_u32_t> tx_failed_count{0};        ///< Transmission failures
  std::atomic<hf_u32_t> bus_off_events{0};         ///< Bus-off occurrences
  std::atomic<hf_u32_t> error_warning_events{0};   ///< Error warning events

  // Performance metrics (atomic for ISR safety)
  std::atomic<hf_u64_t> uptime_seconds{0};                         ///< Total uptime in seconds
  std::atomic<hf_u32_t> last_activity_timestamp{0};                ///< Last activity timestamp
  std::atomic<hf_can_err_t> last_error{hf_can_err_t::CAN_SUCCESS}; ///< Last error encountered

  // Queue statistics (atomic for ISR safety)
  std::atomic<hf_u32_t> tx_queue_peak{0};      ///< Peak TX queue usage
  std::atomic<hf_u32_t> rx_queue_peak{0};      ///< Peak RX queue usage
  std::atomic<hf_u32_t> tx_queue_overflows{0}; ///< TX queue overflow count
  std::atomic<hf_u32_t> rx_queue_overflows{0}; ///< RX queue overflow count

  // Default constructor - all fields are initialized with in-class initialization
  hf_can_statistics_t() noexcept = default;
};

/**
 * @brief CAN diagnostics structure for detailed error analysis.
 * @details Provides detailed diagnostic information for troubleshooting
 *          and monitoring CAN bus health and performance.
 */
struct hf_can_diagnostics_t {
  hf_u32_t tx_error_count;       ///< Transmit error counter
  hf_u32_t rx_error_count;       ///< Receive error counter
  hf_u32_t tx_queue_peak;        ///< Peak TX queue usage
  hf_u32_t rx_queue_peak;        ///< Peak RX queue usage
  hf_u32_t last_error_timestamp; ///< Timestamp of last error
  hf_u32_t controller_resets;    ///< Number of controller resets
  hf_u32_t bus_load_percentage;  ///< Current bus load percentage
  float bit_error_rate;          ///< Bit error rate (errors/bits)

  hf_can_diagnostics_t() noexcept
      : tx_error_count(0), rx_error_count(0), tx_queue_peak(0), rx_queue_peak(0),
        last_error_timestamp(0), controller_resets(0), bus_load_percentage(0),
        bit_error_rate(0.0f) {}
};

/**
 * @class BaseCan
 * @ingroup can
 * @brief Abstract base class defining the unified CAN bus API.
 *
 * This abstract class defines the interface that all CAN controller implementations
 * must provide. It ensures a consistent API across different platforms and CAN
 * controller types, making the system extensible and maintainable.
 *
 * Concrete implementation examples:
 * - Microcontrollers with integrated CAN peripherals
 * - External CAN controllers via SPI (e.g. MCP2515)
 *
 * Features:
 * - Lazy initialization support (initialize on first use)
 * - Comprehensive error handling with detailed error codes
 * - Thread-safe operations (implementation dependent)
 * - Consistent API across different CAN hardware
 *
 * @note This is a header-only abstract base class - instantiate concrete implementations instead.
 */
class BaseCan {
public:
  /**
   * @brief Virtual destructor ensures proper cleanup in derived classes.
   */
  virtual ~BaseCan() noexcept = default;

  // Non-copyable, non-movable (can be changed in derived classes if needed)
  BaseCan(const BaseCan&) = delete;
  BaseCan& operator=(const BaseCan&) = delete;
  BaseCan(BaseCan&&) = delete;
  BaseCan& operator=(BaseCan&&) = delete;

  /**
   * @brief Ensures that the CAN bus is initialized (lazy initialization).
   * @return true if the CAN bus is initialized, false otherwise.
   */
  bool EnsureInitialized() noexcept {
    if (!initialized_) {
      initialized_ = (Initialize() == hf_can_err_t::CAN_SUCCESS);
    }
    return initialized_;
  }

  /**
   * @brief Ensures that the CAN bus is deinitialized.
   * @return true if the CAN bus is deinitialized, false otherwise.
   */
  bool EnsureDeinitialized() noexcept {
    if (initialized_) {
      initialized_ = !(Deinitialize() == hf_can_err_t::CAN_SUCCESS);
    }
    return !initialized_;
  }

  /**
   * @brief Checks if the class is initialized.
   * @return true if initialized, false otherwise
   */
  [[nodiscard]] bool IsInitialized() const noexcept {
    return initialized_;
  }

  //==============================================//
  // PURE VIRTUAL FUNCTIONS [MUST BE OVERRIDDEN]  //
  //==============================================//

  /**
   * @brief Initialize the CAN controller (must be implemented by derived classes).
   * @return hf_can_err_t error code
   */
  virtual hf_can_err_t Initialize() noexcept = 0;

  /**
   * @brief Deinitialize the CAN controller (must be implemented by derived classes).
   * @return hf_can_err_t error code
   */
  virtual hf_can_err_t Deinitialize() noexcept = 0;

  /**
   * @brief Send a CAN message.
   * @param message CAN message to send
   * @param timeout_ms Timeout in milliseconds (0 = non-blocking)
   * @return hf_can_err_t error code
   */
  virtual hf_can_err_t SendMessage(const hf_can_message_t& message,
                                   hf_u32_t timeout_ms = 1000) noexcept = 0;

  /**
   * @brief Receive a CAN message.
   * @param message Reference to store received message
   * @param timeout_ms Timeout in milliseconds (0 = non-blocking)
   * @return hf_can_err_t error code
   */
  virtual hf_can_err_t ReceiveMessage(hf_can_message_t& message,
                                      hf_u32_t timeout_ms = 0) noexcept = 0;

  /**
   * @brief Set callback for received messages.
   * @param callback Callback function to handle received messages
   * @return hf_can_err_t error code
   */
  virtual hf_can_err_t SetReceiveCallback(hf_can_receive_callback_t callback) noexcept = 0;

  /**
   * @brief Set acceptance filter for incoming messages.
   * @param id CAN ID to accept
   * @param mask Acceptance mask (0 = don't care bits)
   * @param extended true for extended frames, false for standard
   * @return hf_can_err_t error code
   */
  virtual hf_can_err_t SetAcceptanceFilter(hf_u32_t id, hf_u32_t mask,
                                           bool extended = false) noexcept = 0;

  /**
   * @brief Get current CAN bus status.
   * @param status Reference to store status information
   * @return hf_can_err_t error code
   */
  virtual hf_can_err_t GetStatus(hf_can_status_t& status) noexcept = 0;

  /**
   * @brief Reset the CAN controller.
   * @return hf_can_err_t error code
   */
  virtual hf_can_err_t Reset() noexcept = 0;

  //==============================================//
  // (OPTIONAL IMPLEMENTATIONS)                   //
  //==============================================//

  /**
   * @brief Clear the receive callback.
   * @note Default implementation does nothing
   */
  virtual void ClearReceiveCallback() noexcept {}

  /**
   * @brief Clear all acceptance filters (accept all messages).
   * @return hf_can_err_t error code
   * @note Default implementation sets filter to accept all (ID=0, Mask=0)
   */
  virtual hf_can_err_t ClearAcceptanceFilter() noexcept {
    return SetAcceptanceFilter(0, 0, false);
  }

  /**
   * @brief Queue a message for transmission without blocking.
   * @details Same as SendMessage(message, 0): the message is queued or the call
   *          fails at once. Drivers return CAN_ERR_QUEUE_FULL when there is no
   *          room and CAN_ERR_BUS_OFF when the controller is bus-off.
   * @return hf_can_err_t error code
   */
  hf_can_err_t TrySendMessage(const hf_can_message_t& message) noexcept {
    return SendMessage(message, 0);
  }

  /**
   * @brief Read the controller error state, TEC/REC and queue counters.
   * @param status Filled on success
   * @return CAN_SUCCESS, or CAN_ERR_UNSUPPORTED_OPERATION (default implementation)
   */
  virtual hf_can_err_t GetBusStatus(hf_can_bus_status_t& status) noexcept {
    (void)status;
    return hf_can_err_t::CAN_ERR_UNSUPPORTED_OPERATION;
  }

  /**
   * @brief Request recovery from bus-off.
   * @details Starts the ISO 11898-1 recovery sequence (128 x 11 recessive bits)
   *          and returns without waiting for it to finish; GetBusStatus() shows
   *          ERROR_ACTIVE once the controller is back. Returns CAN_SUCCESS when
   *          the controller is not bus-off (nothing to do).
   * @return CAN_SUCCESS, or CAN_ERR_UNSUPPORTED_OPERATION (default implementation)
   */
  virtual hf_can_err_t RequestBusRecovery() noexcept {
    return hf_can_err_t::CAN_ERR_UNSUPPORTED_OPERATION;
  }

  /**
   * @brief Set CAN-FD receive callback with enhanced information.
   * @param callback CAN-FD callback function
   * @return hf_can_err_t error code
   * @note Default implementation returns CAN_ERR_UNSUPPORTED_OPERATION
   */
  virtual hf_can_err_t SetReceiveCallbackFD(hf_can_fd_receive_callback_t callback) noexcept {
    (void)callback; // Suppress unused parameter warning
    return hf_can_err_t::CAN_ERR_UNSUPPORTED_OPERATION;
  }

  /**
   * @brief Check if CAN-FD is supported by this controller.
   * @return true if CAN-FD is supported, false otherwise
   * @note Default implementation returns false
   */
  virtual bool SupportsCanFD() const noexcept {
    return false;
  }

  /**
   * @brief Enable or disable CAN-FD mode.
   * @param enable true to enable CAN-FD, false to disable
   * @param data_baudrate Data phase baudrate for CAN-FD (default: 2Mbps)
   * @param timeout_ms Timeout for mode change operation in milliseconds (default: 1000ms)
   * @return true if successful, false otherwise
   * @note Default implementation returns false (not supported)
   */
  virtual bool SetCanFDMode(bool enable, hf_u32_t data_baudrate = 2000000,
                            hf_u32_t timeout_ms = 1000) noexcept {
    (void)enable;
    (void)data_baudrate;
    (void)timeout_ms;
    return false;
  }

  /**
   * @brief Configure CAN-FD timing parameters.
   * @param nominal_prescaler Nominal phase prescaler
   * @param nominal_tseg1 Nominal phase TSEG1
   * @param nominal_tseg2 Nominal phase TSEG2
   * @param data_prescaler Data phase prescaler
   * @param data_tseg1 Data phase TSEG1
   * @param data_tseg2 Data phase TSEG2
   * @param sjw Synchronization jump width
   * @return true if successful, false otherwise
   * @note Default implementation returns false (not supported)
   */
  virtual bool ConfigureCanFDTiming(hf_u16_t nominal_prescaler, hf_u8_t nominal_tseg1,
                                    hf_u8_t nominal_tseg2, hf_u16_t data_prescaler,
                                    hf_u8_t data_tseg1, hf_u8_t data_tseg2,
                                    hf_u8_t sjw = 1) noexcept {
    (void)nominal_prescaler;
    (void)nominal_tseg1;
    (void)nominal_tseg2;
    (void)data_prescaler;
    (void)data_tseg1;
    (void)data_tseg2;
    (void)sjw;
    return false;
  }

  /**
   * @brief Set transmitter delay compensation (CAN-FD feature).
   * @param tdc_offset TDC offset value
   * @param tdc_filter TDC filter value
   * @return true if successful, false otherwise
   * @note Default implementation returns false (not supported)
   */
  virtual bool SetTransmitterDelayCompensation(hf_u8_t tdc_offset, hf_u8_t tdc_filter) noexcept {
    (void)tdc_offset;
    (void)tdc_filter;
    return false;
  }

  /**
   * @brief Send multiple messages in a batch.
   * @param messages Array of messages to send
   * @param count Number of messages to send
   * @param timeout_ms Timeout in milliseconds
   * @return Number of messages successfully sent
   * @note Default implementation sends messages sequentially
   */
  virtual hf_u32_t SendMessageBatch(const hf_can_message_t* messages, hf_u32_t count,
                                    hf_u32_t timeout_ms = 1000) noexcept {
    if (!messages || count == 0) {
      return 0;
    }

    hf_u32_t sent_count = 0;
    for (hf_u32_t i = 0; i < count; ++i) {
      if (SendMessage(messages[i], timeout_ms) == hf_can_err_t::CAN_SUCCESS) {
        sent_count++;
      }
    }
    return sent_count;
  }

  /**
   * @brief Receive multiple messages in a batch.
   * @param messages Array to store received messages
   * @param max_count Maximum number of messages to receive
   * @param timeout_ms Timeout in milliseconds
   * @return Number of messages actually received
   * @note Default implementation receives messages one by one
   */
  virtual hf_u32_t ReceiveMessageBatch(hf_can_message_t* messages, hf_u32_t max_count,
                                       hf_u32_t timeout_ms = 100) noexcept {
    if (!messages || max_count == 0) {
      return 0;
    }

    hf_u32_t received_count = 0;
    for (hf_u32_t i = 0; i < max_count; ++i) {
      if (ReceiveMessage(messages[i], timeout_ms) == hf_can_err_t::CAN_SUCCESS) {
        received_count++;
      } else {
        break; // No more messages available
      }
    }
    return received_count;
  }

  //==============================================//
  // STATISTICS AND DIAGNOSTICS STRUCTURES        //
  //==============================================//

  /**
   * @brief Reset CAN operation statistics.
   * @return hf_can_err_t::CAN_SUCCESS if successful, error code otherwise
   * @note Override this method to provide platform-specific statistics reset
   */
  virtual hf_can_err_t ResetStatistics() noexcept {
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
    return hf_can_err_t::CAN_ERR_UNSUPPORTED_OPERATION;
  }

  /**
   * @brief Reset CAN diagnostic information.
   * @return hf_can_err_t::CAN_SUCCESS if successful, error code otherwise
   * @note Override this method to provide platform-specific diagnostics reset
   */
  virtual hf_can_err_t ResetDiagnostics() noexcept {
    diagnostics_ = hf_can_diagnostics_t{}; // Reset diagnostics to default values
    return hf_can_err_t::CAN_ERR_UNSUPPORTED_OPERATION;
  }

  /**
   * @brief Get CAN operation statistics.
   * @param statistics Reference to statistics structure to fill
   * @return hf_can_err_t::CAN_SUCCESS if successful, error code otherwise
   * @note Override this method to provide platform-specific statistics
   */
  virtual hf_can_err_t GetStatistics(hf_can_statistics_t& statistics) noexcept {
    // Copy atomic values to non-atomic structure for return (atomic operations are thread-safe)
    statistics.messages_sent = statistics_.messages_sent.load();
    statistics.messages_received = statistics_.messages_received.load();
    statistics.bytes_transmitted = statistics_.bytes_transmitted.load();
    statistics.bytes_received = statistics_.bytes_received.load();
    statistics.send_failures = statistics_.send_failures.load();
    statistics.receive_failures = statistics_.receive_failures.load();
    statistics.bus_error_count = statistics_.bus_error_count.load();
    statistics.arbitration_lost_count = statistics_.arbitration_lost_count.load();
    statistics.tx_failed_count = statistics_.tx_failed_count.load();
    statistics.bus_off_events = statistics_.bus_off_events.load();
    statistics.error_warning_events = statistics_.error_warning_events.load();
    statistics.uptime_seconds = statistics_.uptime_seconds.load();
    statistics.last_activity_timestamp = statistics_.last_activity_timestamp.load();
    statistics.last_error = statistics_.last_error.load();
    statistics.tx_queue_peak = statistics_.tx_queue_peak.load();
    statistics.rx_queue_peak = statistics_.rx_queue_peak.load();
    statistics.tx_queue_overflows = statistics_.tx_queue_overflows.load();
    statistics.rx_queue_overflows = statistics_.rx_queue_overflows.load();
    return hf_can_err_t::CAN_ERR_UNSUPPORTED_OPERATION;
  }

  /**
   * @brief Get CAN diagnostic information.
   * @param diagnostics Reference to diagnostics structure to fill
   * @return hf_can_err_t::CAN_SUCCESS if successful, error code otherwise
   * @note Override this method to provide platform-specific diagnostics
   */
  virtual hf_can_err_t GetDiagnostics(hf_can_diagnostics_t& diagnostics) noexcept {
    diagnostics = diagnostics_; // Return diagnostics by default
    return hf_can_err_t::CAN_ERR_UNSUPPORTED_OPERATION;
  }

protected:
  /**
   * @brief Protected constructor
   */
  BaseCan() noexcept : initialized_(false), statistics_{}, diagnostics_{} {}

  //==============================================//
  // VARIABLES                                    //
  //==============================================//

  bool initialized_;                 ///< Initialization status
  hf_can_statistics_t statistics_;   ///< CAN operation statistics
  hf_can_diagnostics_t diagnostics_; ///< CAN diagnostic information
};

//==============================================//
// END OF BASECAN - CLEAN AND MODERN API        //
//==============================================//
