#ifndef FJS_ADAPTIVE_LOCK_H
#define FJS_ADAPTIVE_LOCK_H

#include "Platform.h"
#include "ThreadSafety.h"

#include "FJS/Tracy.h"
#include "FJS/Types.h"

#include <atomic>
#include <mutex>

namespace FJS {

class FJS_CAPABILITY("mutex") AdaptiveLock {
public:
  bool tryLock() FJS_TRY_ACQUIRE(true) {
    if (!Mutex.try_lock())
      return false;
    IsLocked.store(true, std::memory_order_relaxed);
    return true;
  }

  void lock() FJS_ACQUIRE() {
    for (U32 I = 0; I < SpinCount; ++I) {
      if (!isLocked() && tryLock())
        return;
      FJS_PAUSE();
    }
    blockingLock();
  }

  void unlock() FJS_RELEASE() {
    IsLocked.store(false, std::memory_order_relaxed);
    Mutex.unlock();
  }

  /// Hint only, ownership is decided be Mutex alone. A stale read costs one
  /// failed tryLock, never correctness.
  bool isLocked() const { return IsLocked.load(std::memory_order_relaxed); }

  /// Negative-capability support: makes FJS_REQUIRES(!Lock) a valid
  /// expression - the analysis needs operator! on the capability type.
  const AdaptiveLock &operator!() const { return *this; }

private:
  void blockingLock() FJS_ACQUIRE() {
    ZoneScoped;
    Mutex.lock(); // actually block the thread, remove it from the core
    IsLocked.store(true, std::memory_order_relaxed);
  }

  static constexpr U32 SpinCount = 64;

  std::mutex Mutex;
  std::atomic<bool> IsLocked{false};
};

} // namespace FJS

#endif // FJS_ADAPTIVE_LOCK_H
