/**
 * @file HfSpscRing.h
 * @brief Lock-free single-producer / single-consumer ring for ISR-to-task hand-off.
 *
 * @details One producer (typically an interrupt handler) calls TryPush(); one
 *          consumer (a task, or several tasks serialised by a mutex) calls
 *          TryPop(). Neither side blocks, takes a lock or allocates. The head
 *          and tail are free-running 32-bit counters, published with
 *          release/acquire ordering, so the ring is safe across cores.
 *
 *          The capacity is rounded up to a power of two so the counters can
 *          wrap at 2^32 without breaking the index mapping. A push into a full
 *          ring drops the new element and counts an overrun (the oldest data
 *          is kept, as a CAN controller FIFO does).
 *
 *          Storage is allocated once by Init() (not from an ISR).
 *
 * @copyright HardFOC
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

template <typename T>
class HfSpscRing {
public:
  /** @brief Largest capacity Init() accepts. */
  static constexpr uint32_t kMaxCapacity = 1U << 16;

  HfSpscRing() noexcept = default;
  ~HfSpscRing() noexcept = default;

  HfSpscRing(const HfSpscRing&) = delete;
  HfSpscRing& operator=(const HfSpscRing&) = delete;
  HfSpscRing(HfSpscRing&&) = delete;
  HfSpscRing& operator=(HfSpscRing&&) = delete;

  /**
   * @brief Allocate storage for at least @p min_capacity elements.
   * @details Not thread-safe; call before the producer and consumer start.
   *          Calling it again replaces the storage and empties the ring.
   * @return false if @p min_capacity is 0, above kMaxCapacity, or allocation failed.
   */
  bool Init(uint32_t min_capacity) noexcept {
    Release();
    if (min_capacity == 0U || min_capacity > kMaxCapacity) {
      return false;
    }
    uint32_t cap = 1U;
    while (cap < min_capacity) {
      cap <<= 1U;
    }
    slots_.reset(new (std::nothrow) T[cap]);
    if (!slots_) {
      return false;
    }
    capacity_ = cap;
    mask_ = cap - 1U;
    return true;
  }

  /** @brief Free the storage. Not thread-safe. */
  void Release() noexcept {
    slots_.reset();
    capacity_ = 0U;
    mask_ = 0U;
    head_.store(0U, std::memory_order_relaxed);
    tail_.store(0U, std::memory_order_relaxed);
    overruns_.store(0U, std::memory_order_relaxed);
    peak_.store(0U, std::memory_order_relaxed);
  }

  /** @brief True after a successful Init(). */
  bool IsReady() const noexcept {
    return capacity_ != 0U;
  }

  /** @brief Number of elements the ring can hold (a power of two, 0 before Init()). */
  uint32_t Capacity() const noexcept {
    return capacity_;
  }

  /**
   * @brief Producer side: append a copy of @p item.
   * @return false when the ring is full (the overrun counter is incremented)
   *         or not initialised.
   */
  bool TryPush(const T& item) noexcept {
    if (capacity_ == 0U) {
      return false;
    }
    const uint32_t head = head_.load(std::memory_order_relaxed);
    const uint32_t tail = tail_.load(std::memory_order_acquire);
    const uint32_t used = head - tail;
    if (used >= capacity_) {
      overruns_.fetch_add(1U, std::memory_order_relaxed);
      return false;
    }
    slots_[head & mask_] = item;
    head_.store(head + 1U, std::memory_order_release);
    const uint32_t now_used = used + 1U;
    if (now_used > peak_.load(std::memory_order_relaxed)) {
      peak_.store(now_used, std::memory_order_relaxed);
    }
    return true;
  }

  /**
   * @brief Consumer side: remove the oldest element into @p out.
   * @return false when the ring is empty.
   */
  bool TryPop(T& out) noexcept {
    if (capacity_ == 0U) {
      return false;
    }
    const uint32_t tail = tail_.load(std::memory_order_relaxed);
    const uint32_t head = head_.load(std::memory_order_acquire);
    if (head == tail) {
      return false;
    }
    out = slots_[tail & mask_];
    tail_.store(tail + 1U, std::memory_order_release);
    return true;
  }

  /**
   * @brief Consumer side: drop every queued element.
   */
  void Clear() noexcept {
    tail_.store(head_.load(std::memory_order_acquire), std::memory_order_release);
  }

  /** @brief Elements queued now (a snapshot when the other side is running). */
  uint32_t Size() const noexcept {
    return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire);
  }

  /** @brief True when nothing is queued (snapshot). */
  bool Empty() const noexcept {
    return Size() == 0U;
  }

  /** @brief Pushes refused because the ring was full, since Init() or ResetCounters(). */
  uint32_t Overruns() const noexcept {
    return overruns_.load(std::memory_order_relaxed);
  }

  /** @brief Highest fill level seen, since Init() or ResetCounters(). */
  uint32_t Peak() const noexcept {
    return peak_.load(std::memory_order_relaxed);
  }

  /** @brief Zero the overrun and peak counters (the queued elements stay). */
  void ResetCounters() noexcept {
    overruns_.store(0U, std::memory_order_relaxed);
    peak_.store(0U, std::memory_order_relaxed);
  }

private:
  std::unique_ptr<T[]> slots_{};
  uint32_t capacity_{0U};
  uint32_t mask_{0U};
  std::atomic<uint32_t> head_{0U};     ///< next slot to write (producer owned)
  std::atomic<uint32_t> tail_{0U};     ///< next slot to read (consumer owned)
  std::atomic<uint32_t> overruns_{0U}; ///< refused pushes
  std::atomic<uint32_t> peak_{0U};     ///< highest fill level
};
