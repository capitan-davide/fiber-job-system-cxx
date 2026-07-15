#include "FJS/Counter.h"
#include "FJS/Error.h"
#include "FJS/Job.h"
#include "FJS/JobQueue.h"

#include <sys/wait.h>
#include <unistd.h>

#include <catch2/catch_test_macros.hpp>

namespace {
JOB_ENTRY_POINT(jobA) {}
JOB_ENTRY_POINT(jobB) {}
} // namespace

TEST_CASE("JobQueue pops in FIFO order", "[Job]") {
  FJS::JobQueue Queue;
  int PayloadA = 0, PayloadB = 0;

  Queue.push({.EntryFn = jobA, .Param = &PayloadA});
  Queue.push({.EntryFn = jobB, .Param = &PayloadB});

  FJS::JobDecl Out;
  REQUIRE(Queue.pop(Out));
  REQUIRE(Out.EntryFn == jobA);
  REQUIRE(Out.Param == &PayloadA);
  REQUIRE(Out.Done == nullptr);

  REQUIRE(Queue.pop(Out));
  REQUIRE(Out.EntryFn == jobB);
  REQUIRE(Out.Param == &PayloadB);
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
