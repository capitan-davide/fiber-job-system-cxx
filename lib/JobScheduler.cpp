#include "JobScheduler.h"

#include "Counter.h"
#include "Fiber.h"

#include "FJS/Error.h"
#include "FJS/Job.h"

#include <array>

namespace FJS {

namespace {
FIBER_ENTRY_POINT(fiberMain) {
  auto *S = static_cast<JobScheduler *>(Arg);
  std::array<Fiber *, Counter::MaxWaiters> Woken;
  while (true) {
    JobDecl JD;
    while (S->Queue.pop(JD)) {
      JD.EntryFn(JD.Param);
      if (JD.Done) {
        const U32 NumWoken = JD.Done->decrement(Woken);
        // No ready list yet: nothing can be waiting. This assert is the
        // tripwire that fires when waitForCounter exists but the ready-list
        // push doesn't.
        FJS_ASSERT(NumWoken == 0, "Woken fibers with nowhere to go");
      }
    }
    // Queue dry: park until the next runJobs resumes us with more work.
    FJS::switchFiber(*S->Current, S->Bootstrap);
  }
}
} // namespace

void initScheduler(JobScheduler &S, U32 FiberCount) {
  S.Pool.init(FiberCount, fiberMain, &S);
}

void runJobs(JobScheduler &S, std::span<const JobDecl> Jobs) {
  for (const JobDecl &JD : Jobs)
    S.Queue.push(JD);

  Fiber *F = S.Pool.acquire();
  FJS_ASSERT(F != nullptr, "Fiber pool exhausted");

  S.Current = F;
  FJS::switchFiber(S.Bootstrap, *S.Current); // returns when fiberMain parks
  S.Current = nullptr;                       // nothing is running now

  S.Pool.release(F); // parked mid-loop, frame intact - safe to hand out again
}

} // namespace FJS
