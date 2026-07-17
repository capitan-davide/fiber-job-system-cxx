#include "JobScheduler.h"

#include "FJS/Error.h"
#include "Fiber.h"

#include "FJS/Counter.h"
#include "FJS/Job.h"

namespace FJS {

namespace {
FIBER_ENTRY_POINT(fiberMain) {
  auto *S = static_cast<JobScheduler *>(Arg);
  while (true) {
    JobDecl JD;
    while (S->Queue.pop(JD)) {
      JD.EntryFn(JD.Param);
      if (JD.Done)
        JD.Done->decrement();
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
