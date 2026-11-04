/**
 * @file Status.h
 * @brief `hf::Status` and `hf::Result<T>`: the one error model of every layer above the drivers.
 *
 * Each layer keeps its own detailed codes (a driver's `Error`, an interface's `hf_*_err_t`) and
 * maps them once onto `hf::Status`; the original code travels along as `detail`. Code branches on
 * `status`, never on `detail`. The values and their meaning are defined in the HardFOC standard
 * (standards/errors.md in hardfoc/.github) and are never renumbered.
 *
 * No dependencies, no heap, C++17. The `Base*` headers map their `hf_*_err_t` with
 * `hf::ToStatus()`.
 */
#pragma once

#include <cstdint>

namespace hf {

/// Outcome of an operation, coarse enough to act on.
enum class Status : std::uint8_t {
  Ok = 0,              ///< Success.
  NotInitialized = 1,  ///< The object has not been (successfully) initialised.
  InvalidArgument = 2, ///< A parameter is out of range; nothing was done.
  Busy = 3,            ///< The resource is in use; retrying later can succeed.
  Timeout = 4,         ///< A wait expired.
  Bus = 5,             ///< A transfer failed: NACK, CRC, framing, arbitration, transport error.
  NotFound = 6,        ///< The device or resource is absent or answered with the wrong identity.
  Fault = 7,           ///< The device reported a fault, or a check of its state failed.
  Unsupported = 8,     ///< The operation is not available on this chip, board or build.
  DependencyNotReady =
      9,         ///< Something this call needs (a bus, a manager, a handler) failed to start.
  NoMemory = 10, ///< A fixed-capacity pool or table is full.
  Unknown = 11,  ///< Anything that maps to none of the above.
};

/// Number of `Status` values; `Unknown` is the last.
inline constexpr std::uint8_t kStatusCount = 12;

/// Name of @p status (`"Ok"`, `"Timeout"`, ...): a static string, never null.
[[nodiscard]] constexpr const char* ToString(Status status) noexcept {
  switch (status) {
  case Status::Ok:
    return "Ok";
  case Status::NotInitialized:
    return "NotInitialized";
  case Status::InvalidArgument:
    return "InvalidArgument";
  case Status::Busy:
    return "Busy";
  case Status::Timeout:
    return "Timeout";
  case Status::Bus:
    return "Bus";
  case Status::NotFound:
    return "NotFound";
  case Status::Fault:
    return "Fault";
  case Status::Unsupported:
    return "Unsupported";
  case Status::DependencyNotReady:
    return "DependencyNotReady";
  case Status::NoMemory:
    return "NoMemory";
  case Status::Unknown:
    return "Unknown";
  }
  return "Unknown";
}

/**
 * @brief A value and the status of the call that produced it.
 *
 * `value` is meaningful only when `ok()`; otherwise it holds `T{}`. `detail` is the originating
 * layer's own code (a driver `Error`, an `hf_*_err_t`, a manager code), 0 if none, for logs only.
 * An ignored `Result` is a compiler warning.
 *
 * @code
 * hf::Result<float> r = handler.ReadPressurePa();
 * if (!r.ok()) { return r.status; }
 * use(r.value);
 * @endcode
 */
template <typename T>
struct [[nodiscard]] Result {
  T value{};                 ///< The value; `T{}` unless `ok()`.
  Status status{Status::Ok}; ///< What happened.
  std::uint16_t detail{0};   ///< The originating layer's own code; 0 if none.

  /// True when `status` is `Status::Ok`.
  [[nodiscard]] constexpr bool ok() const noexcept {
    return status == Status::Ok;
  }
  /// Same as `ok()`.
  constexpr explicit operator bool() const noexcept {
    return ok();
  }
};

/// The status of a call that returns no value.
template <>
struct [[nodiscard]] Result<void> {
  Status status{Status::Ok}; ///< What happened.
  std::uint16_t detail{0};   ///< The originating layer's own code; 0 if none.

  /// True when `status` is `Status::Ok`.
  [[nodiscard]] constexpr bool ok() const noexcept {
    return status == Status::Ok;
  }
  /// Same as `ok()`.
  constexpr explicit operator bool() const noexcept {
    return ok();
  }
};

} // namespace hf
