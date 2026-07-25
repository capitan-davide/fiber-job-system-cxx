#include "JobScheduler.h"
#include "Counter.h"
#include "Fiber.h"
#include "WorkerContext.h"

#include "FJS/Error.h"
#include "FJS/Job.h"
#include "FJS/Tracy.h"

#include <array>
#include <mutex>

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
  R->S->pushReady(R->Waiter);
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
  fiberEntryProlog();

  auto *S = static_cast<JobScheduler *>(Arg);
  std::array<Fiber *, Counter::MaxWaiters> Woken;

  while (true) {
    // This OS thread's context. Read fresh each iteration: any park may resume
    // us on a different thread, where the old pointer would name another
    // thread's context. Every switchFiber below is followed by `continue`, so
    // stale values die with the iteration. CAREFUL: JD.EntryFn can also park us
    // (a Job calling waitForCounter) and is NOT followed by a re-read - nothing
    // after JD.EntryFn in this loop may use ThisWorker.
    WorkerContext *ThisWorker = getThisWorkerContext();

    // Finish in-flight work before starting new one: a redy fiber holds a
    // half-done job - and a stack we can't recycle until it completes.
    if (Fiber *Next = S->popReady()) {
      Fiber *Me = ThisWorker->Current;
      ThisWorker->Current = Next;

      // We're idle once Next runs, so we go back to the pool - but publishing
      // this fiber must wait until we're off its stack: after-switch callback.
      ReleaseFiberArgs Args = {S, Me};
      switchFiber(*Me, *Next, nullptr, releaseFiber, &Args);
      continue; // resumed: re-start the loop to check for ready fibers before
                // getting a new job
    }

    JobDecl JD;
    if (S->Queue.pop(JD)) {
      JD.EntryFn(JD.Param);
      if (JD.Done) {
        const U32 NumWoken = JD.Done->decrement(Woken);
        for (U32 I = 0; I < NumWoken; ++I)
          S->pushReady(Woken[I]);
      }
      continue;
    }

    // Queue dry: park until the next drive resumes us with more work. There's
    // no need to use a AfterSwitchFn here: Bootstrap is a well known
    // destination - control lands exactly in one place. What Bootstrap does is
    // to release() this fiber as soon as we land there.
    switchFiber(*ThisWorker->Current, ThisWorker->Bootstrap);
  }
}

} // namespace

JobScheduler::JobScheduler(const JobSchedulerConfig &Config) {
  Pool.init({.SmallFiberCount = Config.SmallFiberCount,
             .LargeFiberCount = Config.LargeFiberCount},
            fiberMain, this);
}

void JobScheduler::kickJobs(std::span<const JobDecl> Jobs) {
  ZoneScoped;
  ZoneValue(Jobs.size());
  for (const JobDecl &JD : Jobs) {
    FJS_ASSERT(JD.EntryFn, "Starting a Job with no entry point");
    Queue.push(JD);
  }
}

void JobScheduler::drive() {
  ZoneScoped;
  WorkerContext Ctx; // main volunteers as a worker for the duration
  setThisWorkerContext(&Ctx);

  Fiber *F = Pool.acquire(FiberStackSize::Small);
  FJS_ASSERT(F, "Fiber pool exhausted");

  Ctx.Current = F;
  switchFiber(Ctx.Bootstrap, *Ctx.Current); // returns when fiberMain parks
  FJS_ASSERT(Ctx.Current == F,
             "The wrong fiber reached Bootstrap: some fiber is "
             "stranded on a wait list");

  Pool.release(F); // parked mid-loop, frame intact - safe to hand out again
}

Counter *JobScheduler::runJobs(std::span<const JobDecl> Jobs) {
  ZoneScoped;
  ZoneValue(Jobs.size());
  Counter *C = Counters.acquire(static_cast<U32>(Jobs.size()));
  FJS_ASSERT(C, "Counter pool exhausted");

  for (JobDecl JD : Jobs) {
    FJS_ASSERT(!JD.Done, "runJobs owns Done - don't set it");
    FJS_ASSERT(JD.EntryFn, "Starting a Job with no entry point");
    JD.Done = C;
    Queue.push(JD);
  }
  return C;
}

void JobScheduler::waitForCounter(Counter &C, U32 Target) {
  WorkerContext *ThisWorker = getThisWorkerContext();
  FJS_ASSERT(ThisWorker, "Use of waitForCounter outside a worker thread");

  // load() may race with a concurrent decrement, and that's fine: if it reads
  // a stale "not satisfied", the slow path's `Counter::addWaiter` re-checks
  // under the lock and returns false; if it reads "satisfied" that's genuinely
  // true (a counter never counts back up).
  if (C.load() == Target)
    return; // fast path - no switch, no lock.

  ZoneScoped; // only slow path
  Fiber *Me = ThisWorker->Current;

  // Lives in this frame; the frame is frozen while we're parked, and
  // registerWaiter finishes long before we can be resumed and pop it.
  RegisterWaiterArgs Args = {this, &C, Target, Me};

  // Hand control to a fresh scheduler loop, running fiberMain.
  Fiber *Next = Pool.acquire(FiberStackSize::Small);
  FJS_ASSERT(Next, "Fiber pool exhausted");
  ThisWorker->Current = Next;

  // addWaiter must run *after* the switch: we may not be discoverable on the
  // wait list while anything is still executing on our stack.
  switchFiber(*Me, *Next, nullptr, registerWaiter, &Args);

  // Resumed: the counter hit target.
}

void JobScheduler::waitForCounterAndFree(Counter *C) {
  waitForCounter(*C, 0);
  // Satisfied: every job in the batch as decremented and moved on, and
  // reset()'s assert will vouch that no waiter is still parked here.
  Counters.release(C);
}

void JobScheduler::shutdown() { Pool.shutdown(); }

void JobScheduler::pushReady(Fiber *F) {
  FJS_ASSERT(F->SP, "Pushing a non-parked fiber to the ready list");
  std::lock_guard G{ReadyLock};
  Ready.push_back(F);
}

Fiber *JobScheduler::popReady() {
  std::lock_guard G{ReadyLock};
  if (Ready.empty())
    return nullptr;
  Fiber *F = Ready.back();
  Ready.pop_back();
  return F;
}

} // namespace FJS
