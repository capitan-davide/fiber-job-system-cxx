#ifndef FJS_FJS_H
#define FJS_FJS_H

#include "Job.h"
#include "Types.h"

#include "FJS/Counter.h"

#include <span>

namespace FJS {

struct JobSystemConfig {
  U32 SmallFiberCount; // fiber pool: small-stack tier
  U32 LargeFiberCount; // fiber pool: large-stack tier
  U32 NumWorkers;      // CPU worker threads to start
  U32 NumIOThreads;    // threads dedicated to blocking IO jobs
};

/// Brings up the job system: fiber pool, worker threads, IO threads, and
/// SIGINT/SIGTERM/SIGHUP handling.
/// \pre Called once, from the main thread, before any other FJS function.
void bigInit(const JobSystemConfig &Config);

/// Runs \p Entry on the job system and blocks the calling thread until it
/// returns. \p Entry may itself submit further jobs and wait on them, like
/// any other job.
/// \pre \p Entry.Done is null - the job system owns this job's completion.
void run(const JobDecl &Entry);

/// Schedules \p Jobs as one batch on the IO thread pool and returns a handle
/// tracking it. Call `.wait()` on the handle (from a fiber - inside a running
/// job) to block until the batch completes.
CounterHandle runIOJobs(std::span<const JobDecl> Jobs);

/// Internal primitive behind `CounterHandle::wait()`. Prefer `.wait()`.
void waitForCounterAndFree(Counter *C);

/// Requests shutdown, joins every worker and IO thread, and tears down the
/// scheduler.
/// \pre Every outstanding batch (`run`/`runIOJobs`) has already finished.
void bigShutdown();

} // namespace FJS

#endif // FJS_FJS_H
