#include "Counter.h"
#include "JobScheduler.h"
#include "Shutdown.h"
#include "Worker.h"

#include "FJS/Job.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <thread>

#include <pthread.h>
#include <sched.h>

#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;

TEST_CASE("Worker start pinned and join on shutdown", "[Worker]") {
  FJS::detail::resetShutdownFlagForTesting();
  FJS::blockShutdownSignals();

  FJS::JobScheduler S;
  FJS::initScheduler(S, /*FiberCount=*/8);

  FJS::WorkerPool W;

  const FJS::U32 Count = std::min(4u, std::thread::hardware_concurrency());
  FJS::startWorkers(W, S, Count);

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

namespace {
JOB_ENTRY_POINT(countRan) {
  auto *Ran = static_cast<std::atomic<FJS::U32> *>(Arg);
  Ran->fetch_add(1, std::memory_order_relaxed);
}
} // namespace

TEST_CASE("Job run on worker threads", "[Worker]") {
  FJS::detail::resetShutdownFlagForTesting();
  FJS::blockShutdownSignals();

  FJS::JobScheduler S;
  FJS::initScheduler(S, /*FiberCount=*/8);

  std::atomic<FJS::U32> Ran{0};
  std::array<FJS::JobDecl, 100> Jobs; // entry bumps an atomic counter
  for (FJS::JobDecl &JD : Jobs)
    JD = {.EntryFn = countRan, .Param = &Ran};

  // Workers first, THEN work: also proves the dry-idle loop picks up jobs that
  // arrive after ignition, not just a pre-filled queue.
  FJS::WorkerPool W;
  const FJS::U32 Count = std::min(4u, std::thread::hardware_concurrency());
  FJS::startWorkers(W, S, Count);

  FJS::Counter *C = FJS::runJobs(S, Jobs);

  // Main can't waitForCounter (worker-only) - deadline poll instead.
  const auto Deadline = std::chrono::steady_clock::now() + 5s;
  while (C->load() != 0) {
    const auto Now = std::chrono::steady_clock::now();
    REQUIRE(Now < Deadline);
    std::this_thread::yield();
  }
  S.Counters.release(C);

  FJS::requestShutdown();
  FJS::joinWorkers(W);

  // relaxed everywhere is enough for Ran: the increments only need atomicity
  // (nobody reads other memory through them), and this final read is ordered
  // after every worker's increments by join() itself - thread join is a
  // synchronization point.
  REQUIRE(Ran.load(std::memory_order_relaxed) == 100);
}
