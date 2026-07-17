#include "Counter.h"
#include "JobScheduler.h"

#include "FJS/Error.h"
#include "FJS/Job.h"
#include "FJS/JobQueue.h"

#include <array>

#include <sys/wait.h>
#include <unistd.h>

#include <catch2/catch_test_macros.hpp>

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

TEST_CASE("runJobs runs a batch to completion", "[Job]") {
  FJS::JobScheduler S;
  FJS::initScheduler(S, /*FiberCount=*/2);

  unsigned Ran = 0;
  FJS::Counter C1(5);
  FJS::JobDecl Jobs[5];
  for (auto &JD : Jobs)
    JD = {.EntryFn = countJob, .Param = &Ran, .Done = &C1};

  // FJS::runJobs acquires the first fiber, this fiber will consume all five
  // jobs - the second fiber is never touched.
  FJS::runJobs(S, Jobs);
  REQUIRE(Ran == 5);
  REQUIRE(C1.load() == 0);

  FJS::Counter C2(5);
  for (auto &JD : Jobs)
    JD.Done = &C2;

  // After the first call to FJS::runJobs terminates, the first fiber is placed
  // back in to the LIFO queue. The second call to FJS::runJobs will reuse the
  // same fiber to complete all five jobs again.
  FJS::runJobs(S, Jobs); // second batch: proves fiber reuse works
  REQUIRE(Ran == 10);
  REQUIRE(C2.load() == 0);
}
