#include "Fiber.h"
#include "FiberSwitch.h"

#include <catch2/catch_test_macros.hpp>

namespace {
FJS::Fiber MainFiber; // note: NO createFiber for this one
FJS::Fiber TestFiber;
int Trace = 0;
void *SeenArg = nullptr;

[[noreturn]] void entry (void *Arg) {
  SeenArg = Arg;
  Trace = 1;
  FJS_fiberSwitch(&TestFiber.SP, MainFiber.SP, nullptr); // switch back to main
}
} // namespace

TEST_CASE("First fiber switch", "[Fiber]") {
  FJS::createFiber(TestFiber, entry, reinterpret_cast<void *>(0x1234));

  // What `FJS_fiberSwitch` does (see FiberSwitch.S):
  //
  // 1: pushes the current state of the callee-saved registers on to the stack,
  //    this creates a real `ContextFrame` at rsp.
  // 2: write the `ContextFrame` to MainFiber.SP
  // 3: set rsp to the TestFiber.SP. This is a fabricated `ContextFrame` that
  //    holds the `EntryFn` in R12, `Arg` in R13, and the address of
  //    `FJS_fiberTrampoline` in RIP.
  // 4: pops the callee-saved registers from the fabricated `ContextFrame`.
  //    Then, will write RDX (i.e., `nullptr`) as the return value and return to
  //    RIP. The execution now passes to `FJS_fiberTrampoline`.
  //
  // The trampoline will now set `Arg` from R13 as the first argument, and
  // invoke `entry` via R12 (i.e., entry(0x1234)).
  void *Ret = FJS_fiberSwitch(&MainFiber.SP, TestFiber.SP, nullptr);

  REQUIRE(Trace == 1);
  REQUIRE(SeenArg == reinterpret_cast<void *>(0x1234));
  REQUIRE(Ret == nullptr);

  // `TestFiber` is still parked at the end on `entry`, here we destroy it.
  FJS::destroyFiber(TestFiber);
}
