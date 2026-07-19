#include "Fiber.h"
#include "FiberPool.h"

#include "FJS/Error.h"

#include <csignal>

#include <sys/wait.h>
#include <unistd.h>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include "DeathTest.h"

namespace {
FJS::Fiber MainFiber; // note: NO createFiber for this one
FJS::Fiber TestFiber;
int Trace = 0;
void *SeenArg = nullptr;

template <typename Backend> FIBER_ENTRY_POINT(entry) {
  FJS::runAfterSwitchCallback();

  // Update SeenArg and Trace only if TestFiber.SP is the expected value. This
  // way we can indirectly assert TestFiber.SP back in MainFiber.
  if (TestFiber.SP == nullptr) { // invalidated by `switchFiber`
    SeenArg = Arg;
    Trace = 1;
  }

  Backend::switchFiber(TestFiber, MainFiber,
                       reinterpret_cast<void *>(0xABCD)); // switch back to main

  FJS_UNREACHABLE("Reached the end of the fiber entrypoint");
}

template <typename Backend> FIBER_ENTRY_POINT(increment) {
  FJS::runAfterSwitchCallback();
  auto *Cnt = reinterpret_cast<unsigned *>(Arg);
  while (true) {
    ++(*Cnt);
    Backend::switchFiber(TestFiber, MainFiber);
  }
}
} // namespace

TEMPLATE_TEST_CASE("Fiber switching works", "[Fiber]", FJS::AsmBackend,
                   FJS::UcontextBackend) {
  using Backend = TestType;

  Backend::createFiber(TestFiber, entry<Backend>,
                       reinterpret_cast<void *>(0x1234));

  // `FJS::switchFiber` first invalidates TestFiber.SP (the frame is about to be
  // consumed - a second resume must crash, not replay it), then calls
  // `FJS_fiberSwitch`, which does (see FiberSwitch.S):
  //
  // 1: pushes the current state of the callee-saved registers onto the stack,
  //    this creates a real `ContextFrame` at rsp.
  // 2: writes rsp (the address of that frame) to MainFiber.SP - main is now a
  //    parked fiber, indistinguishable from any other.
  // 3: sets rsp to TestFiber's parked SP. This is a forged `ContextFrame` that
  //    holds `entry` in R12, `Arg` in R13, and the address of
  //    `FJS_fiberTrampoline` in RIP.
  // 4: pops the callee-saved registers from the forged `ContextFrame`, copies
  //    RDX (our Arg, `nullptr` - untouched by all of the above) into RAX, and
  //    returns to RIP. Execution now enters `FJS_fiberTrampoline`.
  //
  // The trampoline moves R13 (0x1234) into RDI and calls `entry` via R12 -
  // i.e., entry(0x1234). RAX's nullptr is ignored: on a *first* run the
  // argument arrives via the R13 cargo, not via RDX.
  void *Ret = Backend::switchFiber(MainFiber, TestFiber);

  REQUIRE(MainFiber.SP == nullptr); // invalidated by `FJS::switchFiber`
  REQUIRE(Trace == 1);
  REQUIRE(SeenArg == reinterpret_cast<void *>(0x1234));
  REQUIRE(Ret == reinterpret_cast<void *>(0xABCD));

  // `TestFiber` is still parked at the end of `entry`, here we destroy it.
  Backend::destroyFiber(TestFiber);
}

TEMPLATE_TEST_CASE("Fiber ping-pong works", "[Fiber]", FJS::AsmBackend,
                   FJS::UcontextBackend) {
  using Backend = TestType;

  unsigned Cnt = 0;
  Backend::createFiber(TestFiber, increment<Backend>, &Cnt);

  for (unsigned I = 1; I <= 5; ++I) {
    void *Ret = Backend::switchFiber(MainFiber, TestFiber);
    REQUIRE(Cnt == I);
  }

  Backend::destroyFiber(TestFiber);
}

TEMPLATE_TEST_CASE("Guard page traps stack overflow", "[Fiber]",
                   FJS::AsmBackend, FJS::UcontextBackend) {
  using Backend = TestType;

  Backend::createFiber(TestFiber, nullptr);

  auto *Guard = reinterpret_cast<volatile char *>(TestFiber.Stack);
  FJS_REQUIRE_DEATH(SIGSEGV, [&] { *Guard = 1; });

  const long PageSize = ::sysconf(_SC_PAGESIZE);
  volatile char *FirstUsable = Guard + PageSize;
  FJS_REQUIRE_DEATH(0, [&] { *FirstUsable = 1; });

  Backend::destroyFiber(TestFiber);
}

TEST_CASE("FiberPool hands out and reclaims fibers", "[Fiber]") {
  FJS::FiberPool Pool;
  Pool.init(2, [](void *) {}, nullptr);

  FJS::Fiber *A = Pool.acquire();
  FJS::Fiber *B = Pool.acquire();
  REQUIRE(A != nullptr);
  REQUIRE(B != nullptr);
  REQUIRE(Pool.acquire() == nullptr); // exhausted

  Pool.release(B);
  REQUIRE(Pool.acquire() == B);

  Pool.release(A);
  Pool.release(B);
  Pool.shutdown();
}

namespace {
int CallbackRan = 0;
void *CallbackSeenArg = nullptr;
int RanAtResumePoint = -1; // CallvackRan as seen at the fiber's resume point

void markCallback(void *Arg) {
  ++CallbackRan;
  CallbackSeenArg = Arg;
}

FIBER_ENTRY_POINT(freshEntry) {
  FJS::runAfterSwitchCallback();
  RanAtResumePoint = CallbackRan; // proves it fired before this line
  FJS::switchFiber(TestFiber, MainFiber);
  FJS_UNREACHABLE("Reached the end of the fiber entrypoint");
}

FIBER_ENTRY_POINT(parkedEntry) {
  FJS::runAfterSwitchCallback();
  FJS::switchFiber(TestFiber, MainFiber); // park; resumed WITH callback
  RanAtResumePoint = CallbackRan;         // resume poin: callback already ran
  FJS::switchFiber(TestFiber, MainFiber);
  FJS_UNREACHABLE("Reached the end of the fiber entrypoint");
}
} // namespace

TEST_CASE("After-switch callback fires on fresh-fiber entry", "[Fiber]") {
  CallbackRan = 0;
  RanAtResumePoint = -1;
  int Token;

  FJS::createFiber(TestFiber, freshEntry);
  FJS::switchFiber(MainFiber, TestFiber, nullptr, markCallback, &Token);

  REQUIRE(CallbackRan == 1);
  REQUIRE(CallbackSeenArg == &Token);
  REQUIRE(RanAtResumePoint == 1);

  FJS::destroyFiber(TestFiber);
}

TEST_CASE("After-switch callback fires when resuming a parked fiber",
          "[Fiber]") {
  CallbackRan = 0;
  RanAtResumePoint = -1;
  int Token;

  FJS::createFiber(TestFiber, parkedEntry);
  FJS::switchFiber(MainFiber, TestFiber); // no callback: entry runs, parsk
  REQUIRE(CallbackRan == 0);

  FJS::switchFiber(MainFiber, TestFiber, nullptr, markCallback, &Token);
  REQUIRE(CallbackRan == 1);
  REQUIRE(CallbackSeenArg == &Token);
  REQUIRE(RanAtResumePoint == 1);

  FJS::destroyFiber(TestFiber);
}
