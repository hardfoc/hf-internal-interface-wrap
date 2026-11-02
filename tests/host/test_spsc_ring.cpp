/**
 * @file test_spsc_ring.cpp
 * @brief HfSpscRing: capacity, ordering, overflow counting, wrap-around and a
 *        two-thread producer/consumer run.
 *
 * @copyright HardFOC
 */

#include "BaseCan.h"
#include "HfSpscRing.h"
#include "HostTest.h"

#include <atomic>
#include <cstdint>
#include <thread>

HF_TEST(Ring_InitRoundsUpToPowerOfTwo) {
  HfSpscRing<int> r;
  CHECK(!r.IsReady());
  CHECK(!r.TryPush(1));
  int out = 0;
  CHECK(!r.TryPop(out));
  CHECK(!r.Init(0));
  CHECK(!r.Init(HfSpscRing<int>::kMaxCapacity + 1U));
  CHECK(r.Init(20));
  CHECK_EQ(r.Capacity(), 32U);
  CHECK(r.Init(1));
  CHECK_EQ(r.Capacity(), 1U);
  CHECK(r.Init(64));
  CHECK_EQ(r.Capacity(), 64U);
  CHECK(r.Empty());
}

HF_TEST(Ring_FifoOrder) {
  HfSpscRing<int> r;
  REQUIRE(r.Init(8));
  for (int i = 0; i < 5; ++i) {
    CHECK(r.TryPush(i));
  }
  CHECK_EQ(r.Size(), 5U);
  for (int i = 0; i < 5; ++i) {
    int v = -1;
    CHECK(r.TryPop(v));
    CHECK_EQ(v, i);
  }
  int v = 0;
  CHECK(!r.TryPop(v));
  CHECK(r.Empty());
}

HF_TEST(Ring_OverflowDropsNewestAndCounts) {
  HfSpscRing<int> r;
  REQUIRE(r.Init(4));
  for (int i = 0; i < 4; ++i) {
    CHECK(r.TryPush(i));
  }
  CHECK(!r.TryPush(100));
  CHECK(!r.TryPush(101));
  CHECK(!r.TryPush(102));
  CHECK_EQ(r.Overruns(), 3U);
  CHECK_EQ(r.Peak(), 4U);
  // The oldest data survives; the refused pushes left no trace
  for (int i = 0; i < 4; ++i) {
    int v = -1;
    CHECK(r.TryPop(v));
    CHECK_EQ(v, i);
  }
  CHECK(r.TryPush(7));
  CHECK_EQ(r.Overruns(), 3U);
  r.ResetCounters();
  CHECK_EQ(r.Overruns(), 0U);
  CHECK_EQ(r.Peak(), 0U);
  CHECK_EQ(r.Size(), 1U); // queued elements stay
}

HF_TEST(Ring_ClearDropsQueued) {
  HfSpscRing<int> r;
  REQUIRE(r.Init(4));
  CHECK(r.TryPush(1));
  CHECK(r.TryPush(2));
  r.Clear();
  CHECK(r.Empty());
  CHECK(r.TryPush(3));
  int v = 0;
  CHECK(r.TryPop(v));
  CHECK_EQ(v, 3);
}

HF_TEST(Ring_WrapsManyTimes) {
  HfSpscRing<uint32_t> r;
  REQUIRE(r.Init(4));
  uint32_t next_in = 0;
  uint32_t next_out = 0;
  for (int round = 0; round < 10000; ++round) {
    const int burst = 1 + (round % 4);
    for (int i = 0; i < burst; ++i) {
      CHECK(r.TryPush(next_in++));
    }
    for (int i = 0; i < burst; ++i) {
      uint32_t v = 0;
      CHECK(r.TryPop(v));
      CHECK_EQ(v, next_out++);
    }
  }
  CHECK_EQ(r.Overruns(), 0U);
}

HF_TEST(Ring_CarriesFullFdMessages) {
  HfSpscRing<hf_can_message_t> r;
  REQUIRE(r.Init(2));
  hf_can_message_t m;
  m.id = 0x123;
  m.SetFdFrame(true);
  uint8_t payload[64];
  for (int i = 0; i < 64; ++i) {
    payload[i] = static_cast<uint8_t>(i * 3);
  }
  REQUIRE(m.SetData(payload, 64));
  m.timestamp_us = 42;
  CHECK(r.TryPush(m));
  hf_can_message_t out;
  CHECK(r.TryPop(out));
  CHECK_EQ(out.id, 0x123U);
  CHECK_EQ(out.GetLength(), 64);
  CHECK_EQ(out.data[63], static_cast<uint8_t>(63 * 3));
  CHECK_EQ(out.timestamp_us, 42U);
  CHECK(out.is_canfd && out.is_brs);
}

HF_TEST(Ring_TwoThreadsKeepOrderAndCountEveryLoss) {
  // Producer pushes a sequence as fast as it can (like an RX ISR that never
  // waits); the consumer pops. Every value is either received, in order, or
  // counted as an overrun: received + overruns == produced.
  constexpr uint32_t kCount = 200000;
  HfSpscRing<uint32_t> r;
  REQUIRE(r.Init(64));

  std::atomic<bool> done{false};
  uint32_t received = 0;
  uint32_t last = 0;
  bool ordered = true;
  bool first = true;

  std::thread consumer([&] {
    uint32_t v = 0;
    for (;;) {
      if (r.TryPop(v)) {
        if (!first && v <= last) {
          ordered = false;
        }
        first = false;
        last = v;
        ++received;
      } else if (done.load(std::memory_order_acquire)) {
        if (!r.TryPop(v)) {
          break;
        }
        if (!first && v <= last) {
          ordered = false;
        }
        first = false;
        last = v;
        ++received;
      }
    }
  });

  for (uint32_t i = 0; i < kCount; ++i) {
    (void)r.TryPush(i);
  }
  done.store(true, std::memory_order_release);
  consumer.join();

  CHECK(ordered);
  CHECK_EQ(received + r.Overruns(), kCount);
  CHECK(r.Peak() <= r.Capacity());
}
