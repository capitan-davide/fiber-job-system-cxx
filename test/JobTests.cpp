#include "Counter.h"
#include "DeathTest.h"
#include "JobQueue.h"
#include "JobScheduler.h"
#include "WorkerContext.h"

#include "FJS/Job.h"
#include "FJS/Types.h"

#include <algorithm>
#include <array>
#include <csignal>
#include <span>

#include <span>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>

#include <catch2/catch_test_macros.hpp>
#include <vector>

namespace {
JOB_ENTRY_POINT(jobEntryA) {}
JOB_ENTRY_POINT(jobEntryB) {}
JOB_ENTRY_POINT(countJob) { ++*static_cast<unsigned *>(Arg); }
} // namespace

TEST_CASE("JobQueue pops in FIFO order", "[Job]") {
  FJS::JobQueue Queue;
  int ParamA = 0, ParamB = 0;

  Queue.push({.EntryFn = jobEntryA, .Param = &ParamA});
  Queue.push({.EntryFn = jobEntryB, .Param = &ParamB});

  FJS::JobDecl Out;
  REQUIRE(Queue.pop(Out));
  REQUIRE(Out.EntryFn == jobEntryA);
  REQUIRE(Out.Param == &ParamA);
  REQUIRE(Out.Done == nullptr);

  REQUIRE(Queue.pop(Out));
  REQUIRE(Out.EntryFn == jobEntryB);
  REQUIRE(Out.Param == &ParamB);
  REQUIRE(Out.Done == nullptr);

  REQUIRE_FALSE(Queue.pop(Out));
}

TEST_CASE("Counter counts down to zero", "[Job]") {
  FJS::Counter C(2);
  std::array<FJS::Fiber *, FJS::Counter::MaxWaiters> Woken;

  REQUIRE(C.load() == 2);
  REQUIRE(C.decrement(Woken) == 0);
  REQUIRE(C.load() == 1);
  REQUIRE(C.decrement(Woken) == 0);
  REQUIRE(C.load() == 0);
}

TEST_CASE("Kicked jobs runs a batch to completion", "[Job]") {
  FJS::JobScheduler S({.SmallFiberCount = 2});

  unsigned Ran = 0;
  FJS::Counter C1(5);
  FJS::JobDecl Jobs[5];
  for (auto &JD : Jobs)
    JD = {.EntryFn = countJob, .Param = &Ran, .Done = &C1};

  S.kickJobs(Jobs);

  // FJS::drive acquires the first fiber, this fiber will consume all five
  // jobs - the second fiber is never touched.
  S.drive();
  REQUIRE(Ran == 5);
  REQUIRE(C1.load() == 0);

  FJS::Counter C2(5);
  for (auto &JD : Jobs)
    JD.Done = &C2;

  S.kickJobs(Jobs); // second batch: proves fiber reuse works

  // After the first call to FJS::drive terminates, the first fiber is placed
  // back in to the LIFO queue. The second call to FJS::drive will reuse the
  // same fiber to complete all five jobs again.
  S.drive();
  REQUIRE(Ran == 10);
  REQUIRE(C2.load() == 0);
}

TEST_CASE("waitForCounter returns immediately when already satisfied",
          "[Job]") {
  FJS::JobScheduler S({.SmallFiberCount = 2});

  // Pose as a worker, but no current fiber: the fast path never needs one, and
  // a slow path taken by mistake would abort on the nullptr - the test asserts
  // "no switch happens" structurally, not by luck.
  FJS::WorkerContext Ctx;
  FJS::ThisWorker = &Ctx;

  FJS::Counter Done{0};
  S.waitForCounter(Done); // default Target = 0: the common case

  FJS::Counter Pending{3};
  S.waitForCounter(Pending, 3); // nonzero target, same fast path

  FJS::ThisWorker = nullptr;
  SUCCEED("Fast path returned without switching");
}

TEST_CASE("waitForCounter aborts outside a worker thread", "[Job]") {
  FJS::JobScheduler S({.SmallFiberCount = 2});
  FJS::Counter C{0}; // satisfied - proves even the fast path is fenced

  FJS_REQUIRE_DEATH(SIGABRT, [&] { S.waitForCounter(C); });
}

namespace {
struct WaitTestCtx {
  FJS::JobScheduler *S;
  FJS::Counter *C;
  std::vector<std::string_view> *Log;
};

JOB_ENTRY_POINT(waiter) {
  auto *Ctx = static_cast<WaitTestCtx *>(Arg);
  // A runtime value captured BEFORE parking must survive in this frame.
  const FJS::U32 Before = Ctx->C->load(); // 1
  Ctx->Log->push_back("W:waiting");
  Ctx->S->waitForCounter(*Ctx->C);
  Ctx->Log->push_back(Before == 1 ? "W:resumed" : "W:corrupt");
}

JOB_ENTRY_POINT(worker) {
  auto *Ctx = static_cast<WaitTestCtx *>(Arg);
  Ctx->Log->push_back("D:ran");
}
} // namespace

TEST_CASE("waitForCounter parks the fiber and resumes it on completion",
          "[Job]") {
  // Exactly 2: the batch fiber + the one on the slow path acquires. A third
  // acquire would mean we're leaking fibers per wait - let it abort.
  FJS::JobScheduler S({.SmallFiberCount = 2});

  FJS::Counter C{1};
  std::vector<std::string_view> Log;
  WaitTestCtx Ctx{&S, &C, &Log};

  // W first, so it hits the slow path while C is still 1. D carries Done = &C:
  // finishing it is what wakes W.
  FJS::JobDecl Jobs[] = {
      {.EntryFn = waiter, .Param = &Ctx},
      {.EntryFn = worker, .Param = &Ctx, .Done = &C},
  };
  S.kickJobs(Jobs);
  S.drive();

  REQUIRE(Log.size() == 3);
  REQUIRE(Log[0] == "W:waiting"); // parked BEFORE D ran...
  REQUIRE(Log[1] == "D:ran");     // ... so the wait was real,
  REQUIRE(Log[2] == "W:resumed"); // ... and the wake-up landed.
  REQUIRE(C.load() == 0);

  // Both fibers home again - including the slow path's extra one. This is the
  // assert that catches a forgotten releaseFiber.
  S.shutdown();
}

namespace {
struct AnimateObject {
  FJS::U32 Frame = 0;
};

struct AnimateTestCtx {
  FJS::JobScheduler *S;
  std::span<AnimateObject> Objects;
  bool BatchDone = false;
};

JOB_ENTRY_POINT(animateObject) { ++static_cast<AnimateObject *>(Arg)->Frame; }

JOB_ENTRY_POINT(animateAllObjects) {
  auto *Ctx = static_cast<AnimateTestCtx *>(Arg);

  std::array<FJS::JobDecl, 100> Jobs;
  for (FJS::U32 I = 0; I < Jobs.size(); ++I)
    Jobs[I] = {.EntryFn = animateObject, .Param = &Ctx->Objects[I]};

  FJS::Counter *C = Ctx->S->runJobs(Jobs);
  Ctx->S->waitForCounterAndFree(C); // C is dead past this line

  Ctx->BatchDone = true; // logged strictly AFTER all 100 completed
}
} // namespace

TEST_CASE("AnimateAllObjects: fan out 100 jobs, wait, free the counter",
          "[Job]") {
  FJS::JobScheduler S({.SmallFiberCount = 2}); // 2 again: the outer job's fiber
                                               // + the one its wait acquires.

  std::array<AnimateObject, 100> Objects;
  AnimateTestCtx Ctx{&S, Objects};

  FJS::JobDecl Outer[] = {{.EntryFn = animateAllObjects, .Param = &Ctx}};
  S.kickJobs(Outer);
  S.drive();

  REQUIRE(Ctx.BatchDone);
  REQUIRE(std::ranges::all_of(
      Objects, [](const AnimateObject &Obj) { return Obj.Frame == 1; }));

  S.shutdown();
}
