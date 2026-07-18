#include "JobScheduler.h"

#include "Counter.h"
#include "Fiber.h"

#include "FJS/Error.h"
#include "FJS/Job.h"

#include <array>

namespace FJS {

namespace {

struct RegisterWaiterArgs {
  JobScheduler *S;
  Counter *C;
  U32 Target;
  Fiber *Waiter;
};

void registerWaiter(void *Arg) {
  auto *R = static_cast<RegisterWaiterArgs *>(Arg);
  if (R->C->addWaiter(R->Waiter, R->Target))
    return; // parked - a future decrement() will hand it back to us.

  // Lost the race: the counter hit Target between the fast-path load and
  // addWaiter's locked re-check. The waiter must not stay parked - queue it
  // for resumption.
  R->S->Ready.push_back(R->Waiter);
}

struct ReleaseFiberArgs {
  JobScheduler *S;
  Fiber *F;
};

void releaseFiber(void *Arg) {
  auto *R = static_cast<ReleaseFiberArgs *>(Arg);
  R->S->Pool.release(R->F);
}

FIBER_ENTRY_POINT(fiberMain) {
  runAfterSwitchCallback();

  auto *S = static_cast<JobScheduler *>(Arg);
  std::array<Fiber *, Counter::MaxWaiters> Woken;

  while (true) {
    // Finish in-flight work before starting new one: a redy fiber holds a
    // half-done job - and a stack we can't recycle until it completes.
    if (!S->Ready.empty()) {
      Fiber *Me = S->Current;
      Fiber *Next = S->Ready.back();
      S->Ready.pop_back();
      S->Current = Next;

      // We're idle once Next runs, so we go back to the pool - but publishing
      // this fiber must wait until we're off its stack: after-switch callback.
      ReleaseFiberArgs Args = {S, Me};
      switchFiber(*Me, *Next, nullptr, releaseFiber, &Args);
      continue; // resumed: someone re-acquired us and switched here.
    }

    JobDecl JD;
    if (S->Queue.pop(JD)) {
      JD.EntryFn(JD.Param);
      if (JD.Done) {
        const U32 NumWoken = JD.Done->decrement(Woken);
        for (U32 I = 0; I < NumWoken; ++I)
          S->Ready.push_back(Woken[I]);
      }
      continue;
    }

    // Queue dry: park until the next runJobs resumes us with more work.
    // There's no need to use a AfterSwitchFn here: Bootstrap is a well known
    // destination - control lands exactly in one place. What Bootstrap does
    // is to release() this fiber as soon as we land there.
    switchFiber(*S->Current, S->Bootstrap);
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
  FJS_ASSERT(F, "Fiber pool exhausted");

  S.Current = F;
  switchFiber(S.Bootstrap, *S.Current); // returns when fiberMain parks
  FJS_ASSERT(S.Current == F, "The wrong fiber reached Bootstrap: some fiber is "
                             "stranded on a wait list");
  S.Current = nullptr; // nothing is running now

  S.Pool.release(F); // parked mid-loop, frame intact - safe to hand out again
}

void waitForCounter(JobScheduler &S, Counter &C, U32 Target) {
  // load() may race with a concurrent decrement, and that's fine: if it reads
  // a stale "not satisfied", the slow path's `Counter::addWaiter` re-checks
  // under the lock and returns false; if it reads "satisfied" that's genuinely
  // true (a counter never counts back up).
  if (C.load() == Target)
    return; // fast path - no switch, no lock.

  Fiber *Me = S.Current;

  // Lives in this frame; the frame is frozen while we're parked, and
  // registerWaiter finishes long before we can be resumed and pop it.
  RegisterWaiterArgs Args = {&S, &C, Target, Me};

  // Hand control to a fresh scheduler loop, running fiberMain.
  Fiber *Next = S.Pool.acquire();
  FJS_ASSERT(Next, "Fiber pool exhausted");
  S.Current = Next;

  // addWaiter must run *after* the switch: we may not be discoverable on the
  // wait list while anything is still executing on our stack.
  switchFiber(*Me, *Next, nullptr, registerWaiter, &Args);

  // Resumed: the counter hit target.
}

} // namespace FJS
