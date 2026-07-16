#include "FJS/Counter.h"
#include "FJS/Error.h"
#include "FJS/Job.h"
#include "FJS/JobQueue.h"

#include <sys/wait.h>
#include <unistd.h>

#include <catch2/catch_test_macros.hpp>

namespace {
JOB_ENTRY_POINT(jobEntryA) {}
JOB_ENTRY_POINT(jobEntryB) {}
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
  REQUIRE(C.load() == 2);
  C.decrement();
  REQUIRE(C.load() == 1);
  C.decrement();
  REQUIRE(C.load() == 0);
}
