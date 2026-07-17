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
  // NOTE: a *failing* REQUIRE throws, and from a fiber the exception would
  // unwind into the asm trampoline (no unwind info) -> std::terminate. Fine
  // here: a loud death still fails the test. Prefer asserting on the main side
  // when possible.
  REQUIRE(TestFiber.SP == nullptr); // invalidated by `FJS::switchFiber`

  SeenArg = Arg;
  Trace = 1;

  Backend::switchFiber(TestFiber, MainFiber); // switch back to main
  FJS_UNREACHABLE("Reached the end of the fiber entrypoint");
}

template <typename Backend> FIBER_ENTRY_POINT(increment) {
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
  REQUIRE(Ret == nullptr);

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
  REQUIRE(diesWithSignal([&] { *Guard = 1; }) == SIGSEGV);

  const long PageSize = ::sysconf(_SC_PAGESIZE);
  volatile char *FirstUsable = Guard + PageSize;
  REQUIRE(diesWithSignal([&] { *FirstUsable = 1; }) == 0);

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
