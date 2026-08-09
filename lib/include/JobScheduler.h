#ifndef FJS_JOB_SCHEDULER_H
#define FJS_JOB_SCHEDULER_H

#include "AdaptiveLock.h"
#include "CounterPool.h"
#include "Fiber.h"
#include "FiberPool.h"
#include "JobQueue.h"
#include "ThreadSafety.h"

#include "FJS/Job.h"

#include <semaphore>
#include <span>
#include <vector>

namespace FJS {

struct JobSchedulerConfig {
  U32 SmallFiberCount = 16;
  U32 LargeFiberCount = 4;
};

/// Scheduler state shared by every worker thread. Per-thread state (the
/// bootstrap stack, the currently running fiber) lives in WorkerContext,
/// reached through the thread_local ThisWorker.
class JobScheduler {
public:
  void init(const JobSchedulerConfig &Config = {});

  /// Queues a batch of jobs. Does not run anything. Safe to call from anywhere:
  /// the main thread, or from inside a running job.
  void kickJobs(std::span<const JobDecl> Jobs);

  /// Runs all queued jobs on the calling thread, one after another, and
  /// returns when the queue is empty.
  ///
  /// \pre No worker threads are running (asserted). Workers could steal a
  ///      fiber that parks during a wait and finish it on their own thread;
  ///      drive() assumes that never happens - everything runs here.
  void drive();

  /// Schedules \p Jobs as one batch and returns the counter tracking it: armed
  /// with the batch size, decremented once per finished job. Pass it to
  /// `waitForCounterAndFree` to block on the batch. Safe from anywhere: the
  /// main thread or inside a running job.
  Counter *runJobs(std::span<const JobDecl> Jobs);

  /// Schedules \p Jobs on IO threads as one batch and returns the counter
  /// tracking it: armed with the batch size, decremented once per finished job.
  /// Pass it to `waitForCounterAndFree` to block on the batch. Safe from
  /// anywhere: the main thread or inside a running job.
  Counter *runIOJobs(std::span<const JobDecl> Jobs);

  /// Blocks the CURRENT Fiber (not the thread) until \p C reaches \p Target.
  void waitForCounter(Counter &C, U32 Target = 0);

  /// Block the CURRENT fiber until \p C's batch fully completes, then returns
  /// the counter to the pool. \p C is dead after this call.
  ///
  /// \pre The caller is the counter's SOLE owner: nobody else waits on \p C or
  ///      holds it. Two fibers calling this on one counter is a double-free.
  void waitForCounterAndFree(Counter *C);

  /// Marks \p F as ready to resume: finerMain's loop drains this list before
  /// pulling new jobs. Called when a wait's target is already met by the time
  /// it tries to park (lost the race in registerWaiter), and when
  /// Counter::decrement wakes waiters.
  ///
  /// \pre \p F is parked, not currently in the ready list, the free pool or
  ///      any Counter's wait list.
  void pushReady(Fiber *F) FJS_REQUIRES(!ReadyLock);

  /// Pops a fiber pushed by pushReady - one whose wait is over and ready to
  /// resume. Returns nullptr if the Ready list is empty: not an error,
  /// fiberMain falls through to pulling a new job instead.
  Fiber *popReady() FJS_REQUIRES(!ReadyLock);

  /// Tears the scheduler down and audits for leaks: every fiber back in the
  /// pool, every counter returned. Call after the last worker has been joined
  /// (with workers alive this is a data race, and the audit would lie).
  void shutdown();

private:
  Counter *runJobsOn(JobQueue &Q, std::span<const JobDecl> Jobs);

public:                        // TODO: This has to become private at some point
  JobQueue Queue{"JobQueue"};  // pending jobs - FIFO
  JobQueue IOQueue{"IOQueue"}; // pending IO jobs - FIFO
  FiberPool Pool;              // fibers that run fiberMain when idle
  CounterPool Counters;        // issued by runJobs, reclaimed by
                               // waitForCountersAndFree

  /// One token per job sitting in IOQueue: IO threads sleep in acquire()
  /// instead of spinning. joinIOThreads hands out one extra token per thread
  /// so a sleeper can wake up and notice the shutdown flag.
  std::counting_semaphore<> IOReady{0};

private:
  /// Fibers whose wait is over, ready to be resumed. fiberMain drains this
  /// BEFORE pulling new jobs (finish in-flight work before starting more).
  std::vector<Fiber *> Ready FJS_GUARDED_BY(ReadyLock);
  AdaptiveLock ReadyLock;
};

} // namespace FJS

#endif // FJS_JOB_SCHEDULER_H
