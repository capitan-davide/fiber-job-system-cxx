#ifndef FJS_COUNTER_H
#define FJS_COUNTER_H

#include "FJS/Types.h"

#include <atomic>

namespace FJS {

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
  /// \param Value Number of decrements expected before completion - typically
  ///              the job count of the batch.
  explicit Counter(U32 Value) : Value{Value} {}

  /// Records on completed job. Never called more times than the initial
  /// \p Value: the counter would wrap below zero.
  void decrement();

  // Current value; 0 means every expected job has finished.
  U32 load() const;

private:
  std::atomic<U32> Value;
};

} // namespace FJS

#endif // FJS_COUNTER_H
