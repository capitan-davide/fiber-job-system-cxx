#include "Fiber.h"
#include "FiberSwitch.h"

#include "FJS/Error.h"

#include <catch2/catch_test_macros.hpp>

namespace {
FJS::Fiber MainFiber; // note: NO createFiber for this one
FJS::Fiber TestFiber;
int Trace = 0;
void *SeenArg = nullptr;

FIBER_ENTRY_POINT(entry) {
  SeenArg = Arg;
  Trace = 1;
  FJS::fiberSwitch(TestFiber, MainFiber); // switch back to main
  FJS_UNREACHABLE("Reached the end of the fiber entrypoint");
}
} // namespace

TEST_CASE("First fiber switch", "[Fiber]") {
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
  // argument arrives via the R13 cargo, not v
  void *Ret = FJS::fiberSwitch(MainFiber, TestFiber);

  REQUIRE(Trace == 1);
  REQUIRE(SeenArg == reinterpret_cast<void *>(0x1234));
  REQUIRE(Ret == nullptr);

  // `TestFiber` is still parked at the end on `entry`, here we destroy it.
  FJS::destroyFiber(TestFiber);
}
