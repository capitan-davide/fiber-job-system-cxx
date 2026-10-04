#ifndef FJS_COUNTER_H
#define FJS_COUNTER_H

#include "FJS/Error.h"
#include "FJS/Types.h"

#include "Fiber.h"
#include "SpinLock.h"
#include "ThreadSafety.h"

#include <array>
#include <atomic>
#include <memory>
#include <span>
#include <string_view>

namespace FJS {

/// One parked fiber waiting for this counter to reach a target value.
struct Waiter {
  Fiber *ParkedFiber = nullptr; // nullptr means the slot is free
  U32 Target = 0;               // value that satiefies this waiter
};

/// Completion tracker for a batch of jobs.
///
/// Created (by `RunJobs`) holding the number of submitted jobs; each finishing
/// job decrements it, so 0 means "all done". Atomic because decrements will
/// land from multiple worker threads.
///
/// Non-copyable by construction (`std::atomic` member): a Counter is an
/// identity that jobs hold pointers to, not a value.
///
/// LIFETIME: reading value 0 does NOT mean decrement() has finished - the
/// decrementer is still inside the lock (waiter scan, unlock) after the value
/// drops. The Counter's storage must outlive that critical section. Pool
/// counters satisfy this by construction: their storage is permanent and any
/// next-life access re-takes the same lock. A stack Counter may only be
/// destroyed once its last decrementer has provably left - polling
/// load() == 0 is not proof.
class Counter {
public:
  static constexpr U32 MaxWaiters = 4;

  /// Pool stock only: an idle counter (value 0, no waiters) awaiting reset()
  Counter() = default;

  /// \param Value Number of decrements expected before completion - typically
  ///              the job count of the batch.
  explicit Counter(U32 Value) : Value{Value} {}

  /// Tripwire for the lifetime rule: dying while a decrement still holds the
  /// lock means the storage is being reused mid-decrement. Only fires when the
  /// overlap actually happens - TSan is the real detector.
  ~Counter() {
    FJS_ASSERT(!Lock.isLocked(), "Counter destroyed mid-decrement");
  }

  /// Records on completed job. When the new value satisfies waiters, they are
  /// removed from the wait list and returned in \p Woken.
  /// \return Number of entries written to \p Woken.
  U32 decrement(std::span<Fiber *, MaxWaiters> Woken) FJS_REQUIRES(!Lock);

  // Current value; 0 means every expected job has finished.
  U32 load() const;

  /// Registers \p F to be woken when the value reaches \p Target.
  /// \return false if the value is ALREADY at Target - the caller must not
  ///         park, nothing was registered.
  bool addWaiter(Fiber *F, U32 Target) FJS_REQUIRES(!Lock);

private:
  friend class CounterPool; // the only code allowed to rearm the counter

  /// Rearms a recycled counter for a new batch of \p Value jobs. Only the pool
  /// calls this - and only on counters nobody can see.
  void reset(U32 NewValue);

  /// Debug tripwire for CounterPool::release. Exempt from thread-safety
  /// analysis.
  void assertNoWaiters(std::string_view ErrorMsg) const
      FJS_NO_THREAD_SAFETY_ANALYSIS {
    for (const Waiter &W : Waiters)
      FJS_ASSERT(!W.ParkedFiber, "{}", ErrorMsg);
  }

private:
  std::atomic<U32> Value;
  SpinLock Lock;
  std::array<Waiter, MaxWaiters> Waiters FJS_GUARDED_BY(Lock);
};

class CounterHandle;

namespace detail {
/// Bridge for code that holds a JobScheduler but isn't one. Not for general use.
CounterHandle makeCounterHandle(Counter *C);
} // namespace detail

/// Owns a batch submitted via `runJobs`/`runIOJobs`: move-only, must be
/// consumed by exactly one `.wait()` call. Destroying (or overwriting) one
/// that was never waited on aborts, rather than leaking or double-freeing
/// the underlying `Counter`.
class [[nodiscard]] CounterHandle {
public:
  // Waiting consumes the handle: only callable through std::move.
  void wait() & = delete;

  /// Blocks until the batch completes, then returns the counter to the pool.
  void wait() &&;

private:
  friend class JobScheduler; // the only class allowed to mint one
  friend CounterHandle detail::makeCounterHandle(Counter *C);

  explicit CounterHandle(Counter *C) : Handle{C} {}

  struct Deleter {
    void operator()(Counter *C) {
      FJS_ABORT("CounterHandle destroyed without wait()");
    }
  };
  std::unique_ptr<Counter, Deleter> Handle;
};

namespace detail {
inline CounterHandle makeCounterHandle(Counter *C) { return CounterHandle{C}; }
} // namespace detail

} // namespace FJS

#endif // FJS_COUNTER_H
