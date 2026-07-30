#include "IOThread.h"
#include "JobScheduler.h"
#include "Shutdown.h"

#include "FJS/Tracy.h"

namespace FJS {

namespace {
void ioMain(JobScheduler *S, U32 Index) {
  ZoneScoped;

  std::array<Fiber *, Counter::MaxWaiters> Woken;

#ifdef TRACY_ENABLE
  // In short-lived programs, tracy cannot resolve the thread name set with
  // pthread_setname_np.
  tracy::SetThreadName(std::format("fjs-io{}", Index).c_str());
#endif

  while (true) {
    {
      ZoneScopedN("IO Sleep");
      S->IOReady.acquire();
    }
    if (isShutdownRequested())
      break;

    JobDecl JD;
    const bool Got = S->IOQueue.pop(JD);
    FJS_ASSERT(Got, "IOReady token with no ready jobs");

    {
      ZoneScopedN("IO");
      JD.EntryFn(JD.Param); // may block - that is the whole point
    }
    if (JD.Done) {
      ZoneScopedN("IO Done");
      const U32 NumWoken = JD.Done->decrement(Woken);
      for (U32 I = 0; I < NumWoken; ++I)
        S->pushReady(Woken[I]);
    }
  }
}
} // namespace

void startIOThreads(IOThreadPool &P, JobScheduler &S, U32 Count) {
  FJS_ASSERT(
      areShutdownSignalsBlocked(),
      "Bad sigmask, startIOThreads called without blocking shutdown signals");

  const U32 MaxWorkers = std::thread::hardware_concurrency();
  FJS_ASSERT(Count <= MaxWorkers,
             "{} workers requested but only {} cores available", Count,
             MaxWorkers);

  P.Threads.reserve(Count);
  for (U32 I = 0; I < Count; ++I) {
    std::thread &T = P.Threads.emplace_back(ioMain, &S, I);

    // Thread names: kernel buffer is 16 bytes INCLUDING the '\0' - 15 usable
    // chars, and longer names fail (ERANGE), not truncate. Shows up in gdb,
    // htop, and TSan reports.
    int Err = pthread_setname_np(T.native_handle(),
                                 std::format("fjs-io{}", I).c_str());
    FJS_ASSERT(!Err, "Failed to name worker {}: {}", I, std::strerror(Err));
  }
}

void joinIOThreads(IOThreadPool &P, JobScheduler &S) {
  // Wake-up each thread so that it can evaluate the shutdown flag.
  S.IOReady.release(static_cast<std::ptrdiff_t>(P.Threads.size()));
  for (std::thread &T : P.Threads)
    T.join();
  P.Threads.clear();
}

} // namespace FJS
