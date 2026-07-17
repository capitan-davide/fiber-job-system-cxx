#include "Counter.h"
#include "Fiber.h"

#include "FJS/Types.h"

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

TEST_CASE("addWaiter abords when the wait list is full", "[Counter]") {
  FJS::Counter C{1};
  for (FJS::U32 I = 0; I < FJS::MaxWaiters; ++I)
    REQUIRE(C.addWaiter(FakeA, 0));

  REQUIRE(diesWithSignal([&] { C.addWaiter(FakeB, 0); }) == SIGABRT);
}
