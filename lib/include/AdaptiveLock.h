#ifndef FJS_ADAPTIVE_LOCK_H
#define FJS_ADAPTIVE_LOCK_H

#include "Platform.h"
#include "ThreadSafety.h"

#include "FJS/Types.h"

#include <mutex>

namespace FJS {

class FJS_CAPABILITY("mutex") AdaptiveLock {
public:
  bool tryLock() FJS_TRY_ACQUIRE(true) { return Mutex.try_lock(); }

  void lock() FJS_ACQUIRE() {
    for (U32 I = 0; I < SpinCount; ++I) {
      if (Mutex.try_lock())
        return;
      FJS_PAUSE();
    }
    Mutex.lock(); // actually block this thread
  }

  void unlock() FJS_RELEASE() { Mutex.unlock(); }

  /// Negative-capability support: makes FJS_REQUIRES(!Lock) a valid
  /// expression - the analysis needs operator! on the capability type.
  const AdaptiveLock &operator!() const { return *this; }

private:
  static constexpr U32 SpinCount = 64;

  std::mutex Mutex;
};

} // namespace FJS

#endif // FJS_ADAPTIVE_LOCK_H
