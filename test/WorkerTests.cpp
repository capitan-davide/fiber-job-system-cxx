#include "Shutdown.h"
#include "Worker.h"

#include <algorithm>
#include <thread>

#include <pthread.h>
#include <sched.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Worker start pinned and join on shutdown", "[Worker]") {
  FJS::detail::resetShutdownFlagForTesting();
  FJS::blockShutdownSignals();

  FJS::WorkerPool W;

  const FJS::U32 Count = std::min(4u, std::thread::hardware_concurrency());
  FJS::startWorkers(W, Count);

  for (FJS::U32 I = 0; I < Count; ++I) {
    cpu_set_t Cpus;
    int Err = pthread_getaffinity_np(W.Threads[I].native_handle(), sizeof(Cpus),
                                     &Cpus);
    REQUIRE(!Err);
    REQUIRE(CPU_COUNT(&Cpus) == 1); // pinned to exactly one core...
    REQUIRE(CPU_ISSET(I, &Cpus));   // ... and it's the right one
  }

  FJS::requestShutdown();
  FJS::joinWorkers(W); // hangs forever = test failed
}
