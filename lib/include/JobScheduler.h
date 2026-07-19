#ifndef FJS_JOB_SCHEDULER_H
#define FJS_JOB_SCHEDULER_H

#include "CounterPool.h"
#include "Fiber.h"
#include "FiberPool.h"
#include "JobQueue.h"
#include "SpinLock.h"

#include "FJS/Job.h"

#include <span>
#include <vector>

namespace FJS {

struct JobSchedulerConfig {
  U32 FiberCount = 16;
};

/// Single-threaded scheduler state.
class JobScheduler {
public:
  explicit JobScheduler(const JobSchedulerConfig &Config = {});

public: // TODO: This has to become private at some point
  JobQueue Queue; // pending jobs - FIFO
  FiberPool Pool; // fibers that run fiberMain when idle
  CounterPool Counters; // issued by runJobs, reclaimed by waitForCountersAndFree

  /// Fibers whose wait is over, ready to be resumed. finerMain drains this
  /// BEFORE pulling new jobs (finish in-flight work before starting more).
  std::vector<Fiber *> Ready;
  SpinLock ReadyLock;
};

/// Tears the scheduler down and audits for leaks: every fiber back in the
/// pool, every counter returned. Call after the last worker has been joined
/// (with workers alive this is a data race, and the audir would lie).
void shutdownScheduler(JobScheduler &S);

/// Blocks the CURRENT Fiber (not the thread) until \p C reaches \p Target.
void waitForCounter(JobScheduler &S, Counter &C, U32 Target = 0);

/// Queues a batch of jobs. Does not run anything. Safe to call from anywhere:
/// the main thread, or from insize a running job.
void kickJobs(JobScheduler &S, std::span<const JobDecl> Jobs);

/// Runs queued work until none is left. Main thread ONLY - this is the
/// Bootstrap switch, and it requries that the endine is off.
void drive(JobScheduler &S);

/// Schedules \p Jobs as one batch and returns the counter traking it: armed
/// with the batch size, decremented once per finished job. Pass it to
/// `waitForCountersAndFree` to block on the batch. Safe from anywhere: the main
/// thread or inside a running job.
Counter *runJobs(JobScheduler &S, std::span<const JobDecl> Jobs);

/// Block the CURRENT fiber until \p C's batch fully completes, then returns
/// the counter to the pool. \p C is dead after this call.
///
/// \pre The caller is the counter's SOLE owner: nobody else waits on \p C or
///      holds it. Two fibers callind this on one counter is a double-free.
void waitForCounterAndFree(JobScheduler &S, Counter *C);

} // namespace FJS

#endif // FJS_JOB_SCHEDULER_H
