#include "Fiber.h"
#include "FiberPool.h"
#include "FiberSwitch.h"

#include "FJS/Error.h"

#include <cerrno>
#include <csignal>

#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

#include <catch2/catch_test_macros.hpp>

namespace {
template <typename Fn> int diesWithSignal(Fn &&F) {
  pid_t Pid = ::fork();
  if (Pid == -1)
    FAIL("fork: " << std::strerror(errno));

  if (Pid == 0) {
    F();
    ::_exit(0);
  }

  int Status;
  ::waitpid(Pid, &Status, 0);
  return WIFSIGNALED(Status) ? WTERMSIG(Status) : 0;
}

FJS::Fiber MainFiber; // note: NO createFiber for this one
FJS::Fiber TestFiber;
int Trace = 0;
void *SeenArg = nullptr;

FIBER_ENTRY_POINT(entry) {
  // NOTE: a *failing* REQUIRE throws, and from a fiber the exception would
  // unwind into the asm trampoline (no unwind info) -> std::terminate. Fine
  // here: a loud death still fails the test. Prefer asserting on the main side
  // when possible.
  REQUIRE(TestFiber.SP == nullptr); // invalidated by `FJS::fiberSwitch`
  SeenArg = Arg;
  Trace = 1;
  FJS::fiberSwitch(TestFiber, MainFiber); // switch back to main
  FJS_UNREACHABLE("Reached the end of the fiber entrypoint");
}

FIBER_ENTRY_POINT(increment) {
  auto *Cnt = reinterpret_cast<unsigned *>(Arg);
  while (true) {
    ++(*Cnt);
    FJS::fiberSwitch(TestFiber, MainFiber);
  }
  FJS_UNREACHABLE("Reached the end of the fiber entrypoint");
}
} // namespace

TEST_CASE("Fiber switching works", "[Fiber]") {
  FJS::createFiber(TestFiber, entry, reinterpret_cast<void *>(0x1234));

  // `FJS::fiberSwitch` first invalidates TestFiber.SP (the frame is about to be
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
  void *Ret = FJS::fiberSwitch(MainFiber, TestFiber);

  REQUIRE(MainFiber.SP == nullptr); // invalidated by `FJS::fiberSwitch`
  REQUIRE(Trace == 1);
  REQUIRE(SeenArg == reinterpret_cast<void *>(0x1234));
  REQUIRE(Ret == nullptr);

  // `TestFiber` is still parked at the end of `entry`, here we destroy it.
  FJS::destroyFiber(TestFiber);
}

TEST_CASE("Fiber ping-pong works", "[Fiber]") {
  unsigned Cnt = 0;
  FJS::createFiber(TestFiber, increment, &Cnt);

  for (unsigned I = 1; I <= 5; ++I) {
    void *Ret = FJS::fiberSwitch(MainFiber, TestFiber);
    REQUIRE(Cnt == I);
  }

  FJS::destroyFiber(TestFiber);
}

TEST_CASE("Guard page traps stack overflow", "[Fiber]") {
  FJS::createFiber(TestFiber, nullptr);

  auto *Guard = reinterpret_cast<volatile char *>(TestFiber.Stack);
  REQUIRE(diesWithSignal([&] { *Guard = 1; }) == SIGSEGV);

  const long PageSize = ::sysconf(_SC_PAGESIZE);
  volatile char *FirstUsable = Guard + PageSize;
  REQUIRE(diesWithSignal([&] { *FirstUsable = 1; }) == 0);

  FJS::destroyFiber(TestFiber);
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
