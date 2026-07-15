#include "Fiber.h"

#include <cstddef>
#include <cstdlib>

#include "FJS/Error.h"
#include "FJS/Types.h"

namespace FJS {

extern "C" [[noreturn]] void FJS_fiberTrampoline(); // never call this - address-only symbol

static constexpr U64 DefaultStackSize = 64 * 1024;

void AsmBackend::createFiber(Fiber &F, void (*EntryFn)(void *), void *Arg) {
  F.StackSize = DefaultStackSize;
  F.Stack = std::aligned_alloc(16, F.StackSize);

  auto *Top = static_cast<std::byte *>(F.Stack) + F.StackSize;
  auto *Frame = reinterpret_cast<ContextFrame *>(Top - sizeof(ContextFrame));

  // TODO: Copy the thread's current values for FCW and MXCSR instead of
  // hard-coding them.

  *Frame = {};           // zero everithing first (R14, R15, RBP, RBX, ...)
  Frame->FCW = 0x037F;   // x87 default: all exceptions masked
  Frame->MXCSR = 0x1F80; // SSE default: same idea
  Frame->R12 = reinterpret_cast<void *>(EntryFn); // cargo
  Frame->R13 = Arg;                               // cargo
  Frame->RIP = reinterpret_cast<void *>(&FJS_fiberTrampoline);

  F.SP = Frame;
}

void AsmBackend::destroyFiber(Fiber &F) {
  std::free(F.Stack);
  F = {};
}

void UcontextBackend::createFiber(Fiber &F, void (*EntryFn)(void *),
                                  void *Arg) {
  FJS_UNREACHABLE("Not implemented");
}

void UcontextBackend::destroyFiber(Fiber &F) {
  FJS_UNREACHABLE("Not implemented");
}

} // namespace FJS
