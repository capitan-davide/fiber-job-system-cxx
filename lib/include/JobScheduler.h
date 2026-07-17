#ifndef FJS_JOB_SCHEDULER_H
#define FJS_JOB_SCHEDULER_H

#include "FJS/Job.h"
#include "Fiber.h"
#include "FiberPool.h"

#include "FJS/JobQueue.h"

#include <span>

namespace FJS {

/// Single-threaded scheduler state.
struct JobScheduler {
  JobQueue Queue; // pending jobs - FIFO
  FiberPool Pool; // fibers that run fiberMain when idle

  /// The thread's native stack, parked while fibers run. Never created via
  /// `createFiber` - it becomes a fiber the first time something switches away
  /// from it (same trick as MainFiber in the tests). fiberMain switches back
  /// to it when the queue runs dry.
  Fiber Bootstrap;

  /// The fiber currently executing on this thread - whoever is running needs
  /// it to name itself as `From` in a switch.
  Fiber *Current = nullptr;
};

/// TODO: Documentation
void initScheduler(JobScheduler &S, U32 FiberCount = 16);

/// TODO: Documentation
void runJobs(JobScheduler &S, std::span<const JobDecl> Jobs);

/// Blocks the CURRENT Fiber (not the thread) until \p C reaches \p Target.
void waitForCounter(JobScheduler &S, Counter &C, U32 Target = 0);

} // namespace FJS

#endif // FJS_JOB_SCHEDULER_H
