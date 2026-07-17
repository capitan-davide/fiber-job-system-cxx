#ifndef FJS_SPINLOCK_H
#define FJS_SPINLOCK_H

#include "FJS/Error.h"

#include <atomic>

#include <emmintrin.h>

#define FJS_PAUSE() _mm_pause()

namespace FJS {

class SpinLock {
public:
  bool tryLock() {
    bool IsLocked = Flag.test_and_set(std::memory_order_acquire);
    return !IsLocked;
  }

  void lock() {
    while (!tryLock()) {
      // Avoid hammering RMW by spinning on a read. If we tryLock() in the
      // tight loop every attempt is a RMW that grab the cache line in
      // exclusively - even when it fails - so, N spinners will ping-pong
      // the line and slow down the actual owner's unlock. By using .test(),
      // all the spinners share the line read-only; it only gets invalidated
      // on a real unlock. (relaxed is fine here for the inner peek - the
      // authoritative acquire happens in tryLock.)
      while (Flag.test(std::memory_order_relaxed))
        FJS_PAUSE();
    }
  }

  void unlock() { Flag.clear(std::memory_order_release); }

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

template <class LockTy> class ScopedLock {
public:
  explicit ScopedLock(LockTy &Lock) : Lock{Lock} { Lock.lock(); }
  ~ScopedLock() { Lock.unlock(); }
  ScopedLock(const ScopedLock &) = delete;
  ScopedLock &operator=(const ScopedLock &) = delete;

private:
  LockTy &Lock;
};

} // namespace FJS

#endif // FJS_SPINLOCK_H
