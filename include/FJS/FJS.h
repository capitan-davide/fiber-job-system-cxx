#ifndef FJS_FJS_H
#define FJS_FJS_H

#include "Job.h"
#include "Types.h"

#include "FJS/Counter.h"

#include <span>

namespace FJS {

struct JobSystemConfig {
  U32 SmallFiberCount;
  U32 LargeFiberCount;
  U32 NumWorkers;
  U32 NumIOThreads;
};

void bigInit(const JobSystemConfig &Config);

void run(const JobDecl &Entry);

/// Schedules \p Jobs as one batch on the IO thread pool and returns a handle
/// tracking it. Call `.wait()` on the handle (from a fiber - inside a running
/// job) to block until the batch completes.
CounterHandle runIOJobs(std::span<const JobDecl> Jobs);

/// Internal primitive behind `CounterHandle::wait()`. Prefer `.wait()`.
void waitForCounterAndFree(Counter *C);

void bigShutdown();

} // namespace FJS

#endif // FJS_FJS_H
