#ifndef FJS_JOB_QUEUE_H
#define FJS_JOB_QUEUE_H

#include "SpinLock.h"

#include "FJS/Job.h"

#include <deque>

namespace FJS {

/// FIFO of pending jobs, feeding the scheduler loop.
class JobQueue {
public:
  /// Enqueues a copy of \p JD at the tail.
  void push(const JobDecl &JD);

  /// Dequeues the oldest job into /p Out.
  /// \return false if the queue was empty (\p Out is left untouched).
  bool pop(JobDecl &Out);

private:
  std::deque<JobDecl> Jobs;
  SpinLock Lock;
};

} // namespace FJS

#endif // FJS_JOB_QUEUE_H
