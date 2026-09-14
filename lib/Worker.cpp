#include "Worker.h"
#include "Fiber.h"
#include "JobScheduler.h"
#include "Shutdown.h"
#include "SpinLock.h"
#include "WorkerContext.h"

#include "FJS/Error.h"
#include "FJS/Tracy.h"

#include <cstring>

#include <pthread.h>
#include <sched.h>

namespace FJS {

namespace {

void workerMain(FJS::JobScheduler *S, U32 Index) {
  ZoneScoped;

  WorkerContext Ctx;
#ifdef TRACY_ENABLE
  Ctx.Bootstrap.TracyName = makeTracyBootName(Index);

  // In short-lived programs, tracy cannot resolve the thread name set with
  // pthread_setname_np.
  tracy::SetThreadName(std::format("fjs-w{}", Index).c_str());
#endif
  setThisWorkerContext(&Ctx);

  while (!isShutdownRequested()) {
    Fiber *F = S->Pool.acquire(FiberStackSize::Small);
    FJS_ASSERT(F, "Fiber pool exhausted");

    Ctx.Current = F;
    switchFiber(Ctx.Bootstrap, *Ctx.Current); // returns when a fiber parks
                                              // back here
    FJS_ASSERT(Ctx.Current, "Nothing parked back to Bootstrap?");

    S->Pool.release(Ctx.Current); // whoever went dry on us, not necessarily F
    FJS_PAUSE(); // don't hammer the queue re-entering instantly
  }

  setThisWorkerContext(nullptr);
  TracyFiberLeave;
}

} // namespace

void startWorkers(WorkerPool &W, JobScheduler &S, U32 Count) {
  FJS_ASSERT(
      areShutdownSignalsBlocked(),
      "Bad sigmask, startWorkers called without blocking shutdown signals");

  const U32 MaxWorkers = std::thread::hardware_concurrency();
  FJS_ASSERT(Count <= MaxWorkers,
             "{} workers requested but only {} cores available", Count,
             MaxWorkers);

  W.Threads.reserve(Count);
  for (U32 I = 0; I < Count; ++I) {
    std::thread &T = W.Threads.emplace_back(workerMain, &S, I);

    cpu_set_t Cpus;
    CPU_ZERO(&Cpus);
    CPU_SET(I, &Cpus);
    int Err = pthread_setaffinity_np(T.native_handle(), sizeof(Cpus), &Cpus);
    FJS_ASSERT(!Err, "Failed to pin worker: {}",
               std::make_error_code(std::errc{Err}).message());

    // Thread names: kernel buffer is 16 bytes INCLUDING the '\0' - 15 usable
    // chars, and longer names fail (ERANGE), not truncate. Shows up in gdb,
    // htop, and TSan reports.
    Err = pthread_setname_np(T.native_handle(),
                             std::format("fjs-w{}", I).c_str());
    FJS_ASSERT(!Err, "Failed to name worker {}: {}", I,
               std::make_error_code(std::errc{Err}).message());
  }
}

void joinWorkers(WorkerPool &W) {
  for (std::thread &T : W.Threads)
    T.join();
  W.Threads.clear();
}

} // namespace FJS
