#include "Counter.h"
#include "JobScheduler.h"
#include "Shutdown.h"
#include "Worker.h"

#include "FJS/Job.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <random>
#include <thread>

#include <pthread.h>
#include <sched.h>

#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;

TEST_CASE("Worker start pinned and join on shutdown", "[Worker]") {
  FJS::testing::resetShutdownFlagForTesting();
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
  FJS::testing::resetShutdownFlagForTesting();
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

namespace {
struct MigrationTestCtx {
  FJS::JobScheduler *S;
  FJS::Counter *Gate;
  std::atomic_bool *Migrated;
};

JOB_ENTRY_POINT(parkAndCompare) {
  auto *Ctx = static_cast<MigrationTestCtx *>(Arg);
  const int Before = sched_getcpu();
  FJS::waitForCounterAndFree(*Ctx->S,
                             Ctx->Gate); // parks (unless it loses the race)
  const int After = sched_getcpu();
  if (Before != After)
    Ctx->Migrated->store(true, std::memory_order_relaxed);
}

JOB_ENTRY_POINT(openGate) {} // its Done = Gate: finishing it wakes the waiter
} // namespace

TEST_CASE("A parked fiber can resume on a different worker", "[Worker]") {
  FJS::testing::resetShutdownFlagForTesting();
  FJS::blockShutdownSignals();

  FJS::JobScheduler S;
  FJS::initScheduler(S, /*FiberCount=*/8);

  FJS::WorkerPool W;
  const FJS::U32 Count = std::min(4u, std::thread::hardware_concurrency());
  if (Count < 2)
    SKIP("Migration test needs at least two workers");
  FJS::startWorkers(W, S, Count);

  std::atomic_bool Migrated{false};
  const auto Deadline = std::chrono::steady_clock::now() + 10s;

  while (!Migrated.load(std::memory_order_relaxed)) {
    REQUIRE(std::chrono::steady_clock::now() < Deadline); // fail = migration
                                                          // never happened

    FJS::Counter *Gate = S.Counters.acquire(1);
    REQUIRE(Gate);
    MigrationTestCtx Ctx{&S, Gate, &Migrated};

    // The waiter goes through runJobs: its completion counter comes from the
    // Pool, whose storage survives the decrement tail.
    const FJS::JobDecl Waiter[] = {{.EntryFn = parkAndCompare, .Param = &Ctx}};
    FJS::Counter *RoundDone = FJS::runJobs(S, Waiter);

    const FJS::JobDecl Opener[] = {{.EntryFn = openGate, .Done = Gate}};
    FJS::kickJobs(S, Opener);

    // Retire the round fully before looping: RoundDone == 0 means the waiter
    // finished, and the waiter released Gate before returning - so both
    // counters are back in the pool when the next round begins.
    while (RoundDone->load() != 0) {
      REQUIRE(std::chrono::steady_clock::now() < Deadline); // fail = waiter
                                                            // is stuck
      std::this_thread::yield();
    }
    S.Counters.release(RoundDone);
  }

  FJS::requestShutdown();
  FJS::joinWorkers(W);
}

namespace {

// Deterministic per-node branching from a seed - REPLICATED in the host
// function below so the test can predict the leaf count.
FJS::U32 branchOf(FJS::U32 Seed) { return 1 + Seed % 3; }
FJS::U32 childSeed(FJS::U32 Seed, FJS::U32 I) { return Seed * 2654435761u + I; }

/// Host-side replay of the tree: how many Depth=0 will this seed's fan-out
/// produce?
FJS::U32 expectedLeaves(FJS::U32 Seed, FJS::U32 Depth) {
  if (Depth == 0)
    return 1;
  FJS::U32 N = 0;
  for (FJS::U32 I = 0; I < branchOf(Seed); ++I)
    N += expectedLeaves(childSeed(Seed, I), Depth - 1);
  return N;
}

struct StressTestCtx {
  FJS::JobScheduler *S;
  std::atomic<FJS::U32> *Leaves;
  FJS::U32 Depth;
  FJS::U32 Seed;
};

JOB_ENTRY_POINT(stressNode) {
  auto *Ctx = static_cast<StressTestCtx *>(Arg);
  if (Ctx->Depth == 0) {
    Ctx->Leaves->fetch_add(1, std::memory_order_relaxed);
    return;
  }

  // Fan out. Children's contexts live on THIS frame - frozen while we're
  // parked below, so handing out pointers to them is safe.
  const FJS::U32 B = branchOf(Ctx->Seed);
  std::array<StressTestCtx, 3> Kids;
  std::array<FJS::JobDecl, 3> Jobs;
  for (FJS::U32 I = 0; I < B; ++I) {
    Kids[I] = {Ctx->S, Ctx->Leaves, Ctx->Depth - 1, childSeed(Ctx->Seed, I)};
    Jobs[I] = {.EntryFn = stressNode, .Param = &Kids[I]};
  }

  FJS::Counter *C = FJS::runJobs(*Ctx->S, std::span{Jobs.data(), B});
  FJS::waitForCounterAndFree(*Ctx->S, C); // fan in
}

} // namespace

TEST_CASE("Stress: randomized fan-out/fan-in trees", "[Worker][Stress]") {
  FJS::testing::resetShutdownFlagForTesting();
  FJS::blockShutdownSignals();

  FJS::JobScheduler S;
  FJS::initScheduler(S, /*FiberCount=*/32);

  FJS::WorkerPool W;
  FJS::startWorkers(W, S, std::min(4u, std::thread::hardware_concurrency()));

  const FJS::U32 BaseSeed = std::random_device{}();
  CAPTURE(BaseSeed); // printed on failure - the repro handle

  for (FJS::U32 Round = 0; Round < 200; ++Round) {
    std::atomic<FJS::U32> Leaves{0};
    const FJS::U32 Seed = BaseSeed + Round;
    StressTestCtx RootCtx{&S, &Leaves, /*Depth=*/3, Seed};

    const FJS::JobDecl Jobs[] = {{.EntryFn = stressNode, .Param = &RootCtx}};
    FJS::Counter *RootDone = FJS::runJobs(S, Jobs);

    const auto Deadline = std::chrono::steady_clock::now() + 30s;
    while (RootDone->load() != 0) {
      REQUIRE(std::chrono::steady_clock::now() < Deadline);
      std::this_thread::yield();
    }
    S.Counters.release(RootDone);

    REQUIRE(Leaves.load() == expectedLeaves(Seed, 3));
  }

  FJS::requestShutdown();
  FJS::joinWorkers(W);
}

namespace {
struct RaceTestCtx {
  FJS::JobScheduler *S;
  FJS::Counter *Gate;
};

JOB_ENTRY_POINT(tightWaiter) {
  auto *Ctx = static_cast<RaceTestCtx *>(Arg);
  // Sole owner of Gate: park (or fast-path) and return it to the pool.
  FJS::waitForCounterAndFree(*Ctx->S, Ctx->Gate);
}

JOB_ENTRY_POINT(tightOpener) {} // its Done = Gate: finish opens the gate
} // namespace

TEST_CASE("Race stress: wait and last decrement collide", "[Worker][Stress]") {
  FJS::testing::resetShutdownFlagForTesting();
  FJS::blockShutdownSignals();

  FJS::JobScheduler S;
  FJS::initScheduler(S, /*FiberCount=*/8);

  FJS::WorkerPool W;
  FJS::startWorkers(W, S, std::min(4u, std::thread::hardware_concurrency()));

  const auto Deadline = std::chrono::steady_clock::now() + 120s;

  for (FJS::U32 I = 0; I < 2000; ++I) {
    // Armed by hand (runJobs would stamp Done itself; here two jobs need two
    // different counters, and Gate must exist before either is kicked.)
    FJS::Counter *Gate = S.Counters.acquire(1);
    REQUIRE(Gate);

    RaceTestCtx Ctx{&S, Gate};
    const FJS::JobDecl Waiter[] = {{.EntryFn = tightWaiter, .Param = &Ctx}};
    const FJS::JobDecl Opener[] = {{.EntryFn = tightOpener, .Done = Gate}};

    // Alternate which side gets the head start - sweeps the interleaving space
    // around the fast-path check and the locked re-check.
    FJS::Counter *RoundDone;
    if (I % 2 == 0) {
      RoundDone = FJS::runJobs(S, Waiter);
      FJS::kickJobs(S, Opener);
    } else {
      FJS::kickJobs(S, Opener);
      RoundDone = FJS::runJobs(S, Waiter);
    }

    while (RoundDone->load() != 0) {
      // A hang here is a lost wakeup: the last decrement scanned the wait list
      // before the waiter registered, so nobody will ever wake it. No data race
      // is involved - no sanitizer can see it. This deadline is the only
      // detector.
      REQUIRE(std::chrono::steady_clock::now() < Deadline);
      std::this_thread::yield();
    }

    S.Counters.release(RoundDone);
  }

  FJS::requestShutdown();
  FJS::joinWorkers(W);
}
