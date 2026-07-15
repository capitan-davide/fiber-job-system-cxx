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

/// Prepares a fiber so the next `FJS_fiberSwitch` to it runs \p EntryFn(\p Arg).
///
/// Allocates a fresh stack and forgoes a `ContextFrame` as its top that mimics
/// a parked fiber: the cargo slots (R12/R13) carry \p EntryFn and \p Arg, and
/// RIP points at `FJS_fiberTrampoline`. `F.SP` is set to this frame, so to the
/// switch the new fiber is indistinguishable from one that parked itself.
///
/// The fiber does not run yet - execution starts only when some fiber switches
/// to `F.SP`.
///
/// \param F       Fiber to initialize. Any previous state is overwritten without
///                being freed.
/// \param EntryFn Function the fiber must execute on first resume. Must never
///                return: it must switch away (park) instead. Returning lands
///                on `ud2` and aborts the process.
/// \param Arg     Opaque word passed as `EntryFn`'s only argument.
void createFiber(Fiber &F, void (*EntryFn)(void *), void *Arg);

/// Frees a fiber's stack.
///
/// The fiber must not be running and must never be resumed again: its saved
/// context lives on the stack being freed. Safe on parked fibers, including
/// ones parked mid-`EntryFn` that will simply never continue.
///
/// \param F Fiber to destroy. Its fields are reset so a later `createFiber` may
///          reuse the object.
void destroyFiber(Fiber &F);

} // namespace FJS

#endif // FJS_FIBER_H
