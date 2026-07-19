#include "Shutdown.h"

#include <csignal>
#include <thread>

#include <unistd.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Signal waiter thread handle signals", "[Shutdown]") {
  FJS::blockShutdownSignals();
  FJS::startSignalWaiter();
  REQUIRE(!FJS::isShutdownRequested());

  kill(getpid(), SIGTERM);
  while (!FJS::isShutdownRequested())
    std::this_thread::yield();
}
