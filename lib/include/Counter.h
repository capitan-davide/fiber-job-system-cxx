#ifndef FJS_COUNTER_H
#define FJS_COUNTER_H

#include "Fiber.h"
#include "SpinLock.h"

#include "FJS/Types.h"

#include <array>
#include <atomic>
#include <span>

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
class Counter {
public:
  static constexpr U32 MaxWaiters = 4;

  /// \param Value Number of decrements expected before completion - typically
  ///              the job count of the batch.
  explicit Counter(U32 Value) : Value{Value} {}

  /// Records on completed job. When the new value satisfies waiters, they are
  /// removed from the wait list and returned in \p Woken.
  /// \return Number of entries written to \p Woken.
  U32 decrement(std::span<Fiber *, MaxWaiters> Woken);

  // Current value; 0 means every expected job has finished.
  U32 load() const;

  /// Registers \p F to be woken when the value reaches \p Target.
  /// \return false if the value is ALREADY at Target - the caller must not
  ///         park, nothing was registered.
  bool addWaiter(Fiber *F, U32 Target);

private:
  std::atomic<U32> Value;
  SpinLock Lock;
  std::array<Waiter, MaxWaiters> Waiters;
};

} // namespace FJS

#endif // FJS_COUNTER_H
