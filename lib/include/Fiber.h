#ifndef FJS_FIBER_H
#define FJS_FIBER_H

#include "FJS/Types.h"

namespace FJS {

struct Fiber {
  void *SP = nullptr;    // parcked stack pointer - only meaningful while parked
  void *Stack = nullptr; // allocation base, kept for freeing
  U64 StackSize = 0;
};

struct ContextFrame {
  U16 FCW;         // 0x00
  U16 _Pad;        // ...
  U32 MXCSR;       // 0x04
  void *R15, *R14; // 0x08, 0x10
  void *R13;       // 0x18  <- cargo: Arg
  void *R12;       // 0x20  <- cargo: EntryFn
  void *RBP, *RBX; // 0x28, 0x30
  void *RIP;       // 0x38  <- &FJS_fiberTrampoline
};
static_assert(sizeof(ContextFrame) == 64);

void createFiber(Fiber &F, void (*EntryFn)(void *), void *Arg);
void destroyFiber(Fiber &F);

} // namespace FJS

#endif // FJS_FIBER_H
