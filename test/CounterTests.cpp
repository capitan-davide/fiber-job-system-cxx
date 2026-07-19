#include "Counter.h"
#include "Fiber.h"

#include "FJS/Types.h"

#include <array>
#include <csignal>

#include <catch2/catch_test_macros.hpp>

#include "DeathTest.h"

namespace {
auto *FakeA = reinterpret_cast<FJS::Fiber *>(0x1);
auto *FakeB = reinterpret_cast<FJS::Fiber *>(0x2);
} // namespace

TEST_CASE("addWaiter refuses when already satisfied", "[Counter]") {
  FJS::Counter Done{0};
  REQUIRE_FALSE(Done.addWaiter(FakeA, 0));

  // Target need not be zero: waiting for the *current* value is a no-op too.
  FJS::Counter Pending{2};
  REQUIRE_FALSE(Pending.addWaiter(FakeA, 2));
}

TEST_CASE("addWaiter registers when not yet satisfied", "[Counter]") {
  FJS::Counter C{2};
  REQUIRE(C.addWaiter(FakeA, 0));
  REQUIRE(C.addWaiter(FakeB, 1)); // distinct fibers, distinct targets
}

TEST_CASE("addWaiter aborts when the wait list is full", "[Counter]") {
  FJS::Counter C{1};
  for (FJS::U32 I = 0; I < FJS::Counter::MaxWaiters; ++I)
    REQUIRE(C.addWaiter(FakeA, 0));

  FJS_REQUIRE_DEATH(SIGABRT, [&] { C.addWaiter(FakeB, 0); });
}

TEST_CASE("decrement wakes the waiter as its target and frees the slot",
          "[Counter]") {
  FJS::Counter C{2};
  REQUIRE(C.addWaiter(FakeA, 1));

  std::array<FJS::Fiber *, FJS::Counter::MaxWaiters> Woken;

  REQUIRE(C.decrement(Woken) == 1); // 2 -> 1: FakeA's target
  REQUIRE(Woken[0] == FakeA);

  REQUIRE(C.decrement(Woken) == 0); // 1 -> 0: slot was cleared, nobody is left
}

TEST_CASE("decrement leaves waiters with other targets registered",
          "[Counter]") {
  FJS::Counter C{2};
  REQUIRE(C.addWaiter(FakeA, 0));

  std::array<FJS::Fiber *, FJS::Counter::MaxWaiters> Woken;

  REQUIRE(C.decrement(Woken) == 0); // 2 -> 1: not FakeA's target yet
  REQUIRE(C.decrement(Woken) == 1); // 1 -> 0: now it is
  REQUIRE(Woken[0] == FakeA);
}

TEST_CASE("decrement wakes all waiters sharing a target", "[Counter]") {
  FJS::Counter C{1};
  REQUIRE(C.addWaiter(FakeA, 0));
  REQUIRE(C.addWaiter(FakeB, 0));

  std::array<FJS::Fiber *, FJS::Counter::MaxWaiters> Woken;

  REQUIRE(C.decrement(Woken) == 2);
  // The wait list is a set, not a queue: no order promised.
  REQUIRE(((Woken[0] == FakeA && Woken[1] == FakeB) ||
           (Woken[0] == FakeB && Woken[1] == FakeA)));
}

TEST_CASE("A woken slot can be registered again", "[Counter]") {
  FJS::Counter C{2};

  for (FJS::U32 I = 0; I < FJS::Counter::MaxWaiters; ++I)
    REQUIRE(C.addWaiter(FakeA, 1));

  std::array<FJS::Fiber *, FJS::Counter::MaxWaiters> Woken;
  REQUIRE(C.decrement(Woken) == FJS::Counter::MaxWaiters); // 2 -> 1

  // Every slot must be free again: a full re-fill succeeds (would abort if any
  // slot leaked).
  for (FJS::U32 I = 0; I < FJS::Counter::MaxWaiters; ++I)
    REQUIRE(C.addWaiter(FakeB, 0));
}
