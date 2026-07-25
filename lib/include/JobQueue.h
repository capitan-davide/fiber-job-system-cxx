#ifndef FJS_JOB_QUEUE_H
#define FJS_JOB_QUEUE_H

#include "AdaptiveLock.h"
#include "ThreadSafety.h"

#include "FJS/Job.h"

#include <deque>

namespace FJS {

/// FIFO of pending jobs, feeding the scheduler loop.
class JobQueue {
public:
  /// Enqueues a copy of \p JD at the tail.
  void push(const JobDecl &JD) FJS_REQUIRES(!Lock);

  /// Dequeues the oldest job into /p Out.
  /// \return false if the queue was empty (\p Out is left untouched).
  bool pop(JobDecl &Out) FJS_REQUIRES(!Lock);

private:
  std::deque<JobDecl> Jobs FJS_GUARDED_BY(Lock);
  AdaptiveLock Lock;
};

} // namespace FJS

#endif // FJS_JOB_QUEUE_H
