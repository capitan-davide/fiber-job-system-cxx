#ifndef FJS_COUNTER_POOL_H
#define FJS_COUNTER_POOL_H

#include "Counter.h"
#include "SpinLock.h"
#include "ThreadSafety.h"

#include "FJS/Types.h"

#include <array>
#include <vector>

namespace FJS {

/// Fixed stock of counters handed out by runJobs and returned by
/// waitForCountersAndFree. Callers never allocate or free one.
class CounterPool {
public:
  static constexpr U32 MaxCounters = 16;

  CounterPool();

  /// \return A counter armed with \p Value, or nullptr if the stock is out.
  Counter *acquire(U32 Value) FJS_REQUIRES(!Lock);

  /// Return \p C to the pool. The caller's pointer is dead after this call -
  /// the counter may be rearmed and handed to an unrelated batch at any time.
  ///
  /// \pre \p C came from this pool's `acquire`.
  /// \pre No job still holts \p C as its `Done` and no fiber is parked on it:
  ///      releasing a counter that can still be decremented or waited on hands
  ///      the next batch a counter someone else will mutate.
  void release(Counter *C) FJS_REQUIRES(!Lock);

private:
  std::array<Counter, MaxCounters> Counters;
  std::vector<Counter *> Free FJS_GUARDED_BY(Lock); // LIFO
  SpinLock Lock;
};

} // namespace FJS

#endif // FJS_COUNTER_POOL_H
