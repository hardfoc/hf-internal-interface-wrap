/**
 * @file test_can_frame.cpp
 * @brief hf_can_message_t (BaseCan v2): length, DLC, flags, validation and
 *        conversion to the transport-neutral CanFrame layout.
 *
 * @copyright HardFOC
 */

#include "BaseCan.h"
#include "HostTest.h"

#include <cstdint>
#include <cstring>

namespace {

// Same members and types as vortex::contracts::CanFrame (hardfoc-vortex-v1
// contracts/can_frame.hpp). hf-internal-interface-wrap must not depend on that
// repository, so the layout is mirrored here and checked field by field.
struct ContractCanFrame {
  uint32_t id{0};
  uint8_t len{0};
  uint8_t flags{0};
  uint8_t data[64]{};
  uint32_t ts_us{0};
};

constexpr uint8_t kFdLengths[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

} // namespace

// --- compile-time checks ------------------------------------------------------

static_assert(sizeof(hf_can_message_t{}.data) == 64, "payload must be FD-sized");
static_assert(HfCanDlcToLen(15, true) == 64, "FD DLC 15 is 64 bytes");
static_assert(HfCanDlcToLen(15, false) == 8, "classic DLC 15 is 8 bytes");
static_assert(HfCanLenToDlc(9, true) == 9, "9 bytes needs DLC 9 (12 bytes)");
static_assert(HfCanLenToDlc(9, false) == HF_CAN_DLC_INVALID, "classic cannot carry 9");
static_assert(!HfCanIsValidLength(9, true), "9 is not an exact FD length");
// Flag bits must equal vortex::contracts::frame_flag
static_assert(hf_can_flag::EXT == 0x01 && hf_can_flag::RTR == 0x02 && hf_can_flag::FDF == 0x04 &&
                  hf_can_flag::BRS == 0x08 && hf_can_flag::ESI == 0x10,
              "flag bits must match the contracts CanFrame");

// --- DLC / length tables ------------------------------------------------------

HF_TEST(DlcTable_MatchesIso11898) {
  for (uint8_t dlc = 0; dlc < 16; ++dlc) {
    CHECK_EQ(HfCanDlcToLen(dlc, true), kFdLengths[dlc]);
    CHECK_EQ(HfCanDlcToLen(dlc, false), static_cast<uint8_t>(dlc > 8 ? 8 : dlc));
  }
  CHECK_EQ(HfCanDlcToLen(16, true), 0);
  CHECK_EQ(HfCanDlcToLen(255, false), 0);
}

HF_TEST(LenToDlc_RoundsUpAndRejects) {
  for (uint8_t len = 0; len <= 8; ++len) {
    CHECK_EQ(HfCanLenToDlc(len, false), len);
    CHECK_EQ(HfCanLenToDlc(len, true), len);
  }
  CHECK_EQ(HfCanLenToDlc(13, true), 10); // carried in 16 bytes
  CHECK_EQ(HfCanLenToDlc(33, true), 14); // carried in 48 bytes
  CHECK_EQ(HfCanLenToDlc(64, true), 15);
  CHECK_EQ(HfCanLenToDlc(65, true), HF_CAN_DLC_INVALID);
  CHECK_EQ(HfCanLenToDlc(12, false), HF_CAN_DLC_INVALID);
}

HF_TEST(ValidLengths_ExactFdSizesOnly) {
  int fd_valid = 0;
  int classic_valid = 0;
  for (int len = 0; len < 256; ++len) {
    fd_valid += HfCanIsValidLength(static_cast<uint8_t>(len), true) ? 1 : 0;
    classic_valid += HfCanIsValidLength(static_cast<uint8_t>(len), false) ? 1 : 0;
  }
  CHECK_EQ(fd_valid, 16);
  CHECK_EQ(classic_valid, 9);
  for (uint8_t len : kFdLengths) {
    CHECK(HfCanIsValidLength(len, true));
  }
}

// --- length setters (the old SetDLC(64) overrun) -------------------------------

HF_TEST(SetLength_ClassicRejectsAboveEight) {
  hf_can_message_t m;
  CHECK_EQ(m.GetLength(), 8); // default kept from v1
  CHECK(m.SetLength(0));
  CHECK(m.SetLength(8));
  CHECK(!m.SetLength(9));
  CHECK(!m.SetLength(64));
  CHECK(!m.SetDLC(64)); // v1 accepted 64 here although data[] had 8 bytes
  CHECK_EQ(m.GetLength(), 8);
  CHECK_EQ(m.GetMaxDataLength(), 8);
}

HF_TEST(SetLength_FdAcceptsExactLengthsOnly) {
  hf_can_message_t m;
  m.SetFdFrame();
  CHECK_EQ(m.GetMaxDataLength(), 64);
  for (uint8_t len : kFdLengths) {
    CHECK(m.SetLength(len));
    CHECK_EQ(m.GetLength(), len);
  }
  CHECK(!m.SetLength(9));
  CHECK(!m.SetLength(63));
  CHECK(!m.SetLength(65));
  CHECK_EQ(m.GetLength(), 64); // unchanged after the rejected calls
}

HF_TEST(SetData_CopiesAndValidates) {
  hf_can_message_t m;
  const uint8_t bytes[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  CHECK(m.SetData(bytes, 3));
  CHECK_EQ(m.GetLength(), 3);
  CHECK_EQ(m.data[2], 3);
  CHECK(!m.SetData(bytes, 12)); // classic
  CHECK_EQ(m.GetLength(), 3);
  CHECK(!m.SetData(nullptr, 1));
  CHECK(m.SetData(nullptr, 0));
  m.SetFdFrame();
  CHECK(m.SetData(bytes, 12));
  CHECK_EQ(m.data[11], 12);
  CHECK_EQ(m.GetDlcCode(), 9);
}

HF_TEST(DlcCode_RoundTripAndNonCompliantClassic) {
  hf_can_message_t m;
  CHECK(m.SetDlcCode(5));
  CHECK_EQ(m.GetLength(), 5);
  CHECK(!m.dlc_non_comp);
  CHECK(m.SetDlcCode(12)); // classic code 12 means 8 bytes
  CHECK_EQ(m.GetLength(), 8);
  CHECK(m.dlc_non_comp);
  CHECK(!m.SetDlcCode(16));
  m.SetFdFrame();
  for (uint8_t code = 0; code < 16; ++code) {
    CHECK(m.SetDlcCode(code));
    CHECK_EQ(m.GetDlcCode(), code);
  }
  m.dlc = 9; // written directly, invalid for FD
  CHECK_EQ(m.GetDlcCode(), HF_CAN_DLC_INVALID);
}

HF_TEST(SetClassicFrame_RefusesLongPayload) {
  hf_can_message_t m;
  m.SetFdFrame(true);
  CHECK(m.SetLength(16));
  CHECK(!m.SetClassicFrame());
  CHECK(m.is_canfd);
  CHECK(m.SetLength(8));
  CHECK(m.SetClassicFrame());
  CHECK(!m.is_canfd);
  CHECK(!m.is_brs);
}

// --- flags -----------------------------------------------------------------------

HF_TEST(Flags_PackAndUnpack) {
  hf_can_message_t m;
  CHECK_EQ(m.GetFlags(), 0);
  for (uint8_t f = 0; f <= hf_can_flag::ALL; ++f) {
    CHECK(m.SetFlags(f));
    CHECK_EQ(m.GetFlags(), f);
  }
  CHECK(m.SetFlags(hf_can_flag::EXT | hf_can_flag::RTR));
  CHECK(m.is_extended);
  CHECK(m.is_rtr);
  CHECK(!m.is_canfd);
  CHECK(!m.SetFlags(0x20));
  CHECK(!m.SetFlags(0x80 | hf_can_flag::EXT));
  CHECK_EQ(m.GetFlags(), static_cast<uint8_t>(hf_can_flag::EXT | hf_can_flag::RTR));
}

// --- Validate() (mirrors contracts CheckFrame) --------------------------------------

HF_TEST(Validate_Cases) {
  hf_can_message_t m;
  m.id = 0x7FF;
  CHECK(m.IsValid());
  m.id = 0x800;
  CHECK_EQ(m.Validate(), hf_can_frame_error_t::ID_RANGE);
  m.SetExtendedFrame();
  CHECK(m.IsValid());
  m.id = 0x20000000U;
  CHECK_EQ(m.Validate(), hf_can_frame_error_t::ID_RANGE);
  m.id = 0x100;

  m.dlc = 9; // direct write past the setter
  CHECK_EQ(m.Validate(), hf_can_frame_error_t::CLASSIC_TOO_LONG);
  m.is_rtr = true;
  CHECK_EQ(m.Validate(), hf_can_frame_error_t::RTR_WITH_DATA);
  m.is_rtr = false;
  m.dlc = 0;
  CHECK(m.IsValid()); // zero-length data frames are legal (CANopen SYNC)

  m.is_brs = true;
  CHECK_EQ(m.Validate(), hf_can_frame_error_t::FD_BITS_ON_CLASSIC);
  m.is_brs = false;
  m.is_esi = true;
  CHECK_EQ(m.Validate(), hf_can_frame_error_t::FD_BITS_ON_CLASSIC);
  m.is_esi = false;

  m.is_canfd = true;
  m.dlc = 9;
  CHECK_EQ(m.Validate(), hf_can_frame_error_t::FD_LENGTH_NOT_EXACT);
  m.dlc = 65;
  CHECK_EQ(m.Validate(), hf_can_frame_error_t::FD_TOO_LONG);
  m.dlc = 64;
  CHECK(m.IsValid());
  m.is_rtr = true;
  CHECK_EQ(m.Validate(), hf_can_frame_error_t::RTR_ON_FD);
}

HF_TEST(SetFdFrame_ClearsRtr) {
  hf_can_message_t m;
  m.SetRemoteFrame();
  m.SetFdFrame(true);
  CHECK(!m.is_rtr);
  CHECK(m.is_brs);
  CHECK(m.IsValid());
}

// --- conversion to / from the contracts CanFrame layout -----------------------------

HF_TEST(Frame_RoundTripIsLossless) {
  ContractCanFrame f;
  f.id = 0x1ABCDEF0U;
  f.len = 48;
  f.flags = hf_can_flag::EXT | hf_can_flag::FDF | hf_can_flag::BRS | hf_can_flag::ESI;
  for (int i = 0; i < 64; ++i) {
    f.data[i] = static_cast<uint8_t>(0xA5 ^ i);
  }
  f.ts_us = 0xDEADBEEFU;

  const hf_can_message_t m = hf_can_message_t::FromFrame(f);
  CHECK_EQ(m.id, f.id);
  CHECK_EQ(m.GetLength(), 48);
  CHECK(m.is_extended && m.is_canfd && m.is_brs && m.is_esi && !m.is_rtr);
  CHECK_EQ(m.timestamp_us, static_cast<uint64_t>(0xDEADBEEFU));
  CHECK(m.IsValid());

  ContractCanFrame back;
  m.ToFrame(back);
  CHECK_EQ(back.id, f.id);
  CHECK_EQ(back.len, f.len);
  CHECK_EQ(back.flags, f.flags);
  CHECK_EQ(back.ts_us, f.ts_us);
  CHECK(std::memcmp(back.data, f.data, sizeof(f.data)) == 0);
}

HF_TEST(Frame_EveryFlagCombinationRoundTrips) {
  for (uint8_t flags = 0; flags <= hf_can_flag::ALL; ++flags) {
    ContractCanFrame f;
    f.flags = flags;
    f.len = 8;
    ContractCanFrame back;
    hf_can_message_t::FromFrame(f).ToFrame(back);
    CHECK_EQ(back.flags, flags);
    CHECK_EQ(back.len, 8);
  }
}

HF_TEST(Frame_MessageToFrameKeepsLow32BitsOfTimestamp) {
  hf_can_message_t m;
  m.id = 0x181;
  m.timestamp_us = 0x123456789ULL;
  const uint8_t b[2] = {0x11, 0x22};
  CHECK(m.SetData(b, 2));
  ContractCanFrame f;
  m.ToFrame(f);
  CHECK_EQ(f.ts_us, 0x23456789U);
  CHECK_EQ(f.len, 2);
  CHECK_EQ(f.flags, 0);
  CHECK_EQ(f.data[1], 0x22);
}

HF_TEST(Frame_UnknownFlagBitsAreIgnored) {
  ContractCanFrame f;
  f.flags = 0xE0 | hf_can_flag::RTR;
  const hf_can_message_t m = hf_can_message_t::FromFrame(f);
  CHECK(m.is_rtr);
  CHECK_EQ(m.GetFlags(), hf_can_flag::RTR);
}

// --- bus state helper -------------------------------------------------------------

HF_TEST(StateFromCounters_Thresholds) {
  CHECK_EQ(HfCanStateFromCounters(0, 0), hf_can_bus_state_t::ERROR_ACTIVE);
  CHECK_EQ(HfCanStateFromCounters(95, 95), hf_can_bus_state_t::ERROR_ACTIVE);
  CHECK_EQ(HfCanStateFromCounters(96, 0), hf_can_bus_state_t::ERROR_WARNING);
  CHECK_EQ(HfCanStateFromCounters(0, 96), hf_can_bus_state_t::ERROR_WARNING);
  CHECK_EQ(HfCanStateFromCounters(128, 0), hf_can_bus_state_t::ERROR_PASSIVE);
  CHECK_EQ(HfCanStateFromCounters(0, 200), hf_can_bus_state_t::ERROR_PASSIVE);
  CHECK_EQ(HfCanStateFromCounters(255, 0), hf_can_bus_state_t::ERROR_PASSIVE);
  CHECK_EQ(HfCanStateFromCounters(256, 0), hf_can_bus_state_t::BUS_OFF);
}

// --- BaseCan v2 defaults ---------------------------------------------------------

namespace {

/** Smallest BaseCan: records the timeout SendMessage() was called with. */
class RecordingCan : public BaseCan {
public:
  hf_u32_t last_timeout = 12345;
  hf_can_err_t Initialize() noexcept override {
    return hf_can_err_t::CAN_SUCCESS;
  }
  hf_can_err_t Deinitialize() noexcept override {
    return hf_can_err_t::CAN_SUCCESS;
  }
  hf_can_err_t SendMessage(const hf_can_message_t& message, hf_u32_t timeout_ms) noexcept override {
    (void)message;
    last_timeout = timeout_ms;
    return hf_can_err_t::CAN_ERR_QUEUE_FULL;
  }
  hf_can_err_t ReceiveMessage(hf_can_message_t& message, hf_u32_t timeout_ms) noexcept override {
    (void)message;
    (void)timeout_ms;
    return hf_can_err_t::CAN_ERR_QUEUE_EMPTY;
  }
  hf_can_err_t SetReceiveCallback(hf_can_receive_callback_t callback) noexcept override {
    (void)callback;
    return hf_can_err_t::CAN_SUCCESS;
  }
  hf_can_err_t SetAcceptanceFilter(hf_u32_t id, hf_u32_t mask, bool extended) noexcept override {
    (void)id;
    (void)mask;
    (void)extended;
    return hf_can_err_t::CAN_SUCCESS;
  }
  hf_can_err_t GetStatus(hf_can_status_t& status) noexcept override {
    (void)status;
    return hf_can_err_t::CAN_SUCCESS;
  }
  hf_can_err_t Reset() noexcept override {
    return hf_can_err_t::CAN_SUCCESS;
  }
};

} // namespace

HF_TEST(BaseCan_TrySendUsesZeroTimeout) {
  RecordingCan can;
  hf_can_message_t m;
  CHECK_EQ(can.TrySendMessage(m), hf_can_err_t::CAN_ERR_QUEUE_FULL);
  CHECK_EQ(can.last_timeout, 0U);
}

HF_TEST(BaseCan_V2DefaultsAreUnsupported) {
  RecordingCan can;
  hf_can_bus_status_t st;
  CHECK_EQ(can.GetBusStatus(st), hf_can_err_t::CAN_ERR_UNSUPPORTED_OPERATION);
  CHECK_EQ(can.RequestBusRecovery(), hf_can_err_t::CAN_ERR_UNSUPPORTED_OPERATION);
  CHECK_EQ(st.state, hf_can_bus_state_t::STOPPED);
}
