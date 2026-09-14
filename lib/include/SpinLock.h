#ifndef FJS_SPIN_LOCK_H
#define FJS_SPIN_LOCK_H

#include "Platform.h"
#include "ThreadSafety.h"

#include "FJS/Error.h"

#include <atomic>

namespace FJS {

class FJS_CAPABILITY("mutex") SpinLock {
public:
  bool tryLock() FJS_TRY_ACQUIRE(true) {
    bool IsLocked = Flag.test_and_set(std::memory_order_acquire);
    return !IsLocked;
  }

  void lock() FJS_ACQUIRE() {
    while (!tryLock()) {
      // Avoid hammering RMW by spinning on a read. If we tryLock() in the
      // tight loop every attempt is a RMW that grab the cache line in
      // exclusively - even when it fails - so, N spinners will ping-pong
      // the line and slow down the actual owner's unlock. By using .test(),
      // all the spinners share the line read-only; it only gets invalidated
      // on a real unlock. (relaxed is fine here for the inner peek - the
      // authoritative acquire happens in tryLock.)
      while (isLocked())
        FJS_PAUSE();
    }
  }

  void unlock() FJS_RELEASE() { Flag.clear(std::memory_order_release); }

  [[nodiscard]] bool isLocked() const {
    return Flag.test(std::memory_order_relaxed);
  }

  /// Negative-capability support: makes FJS_REQUIRES(!Lock) a valid
  /// expression - the analysis needs operator! on the capability type.
  const SpinLock &operator!() const { return *this; }

private:
  std::atomic_flag Flag;
};

class UnnecessaryLock {
public:
  void lock() {
    // Assert that no one already has the lock.
    FJS_ASSERT(!Flag, "UnnecessaryLock is not unnecessary");

    // Now lock, so we can detect overlapping critical operations if they
    // happen.
    Flag = true;
  }

  void unlock() {
    // Assert correct usage that unlock() is only called after acquire().
    FJS_ASSERT(Flag, "UnnecessaryLock is not unnecessary");
    Flag = false;
  }

private:
  volatile bool Flag;
};

} // namespace FJS

#endif // FJS_SPIN_LOCK_H
