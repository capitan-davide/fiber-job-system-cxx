#ifndef FJS_FIBER_SWITCH_H
#define FJS_FIBER_SWITCH_H

/// Switches execution from the current fuber to a parked one.
///
/// Saves the current execution context (callee-saved registers, FP control
/// statem and resume address) as a `ContextFrame` on the current stack,
/// publishes the resulting stack pointer through \p SaveSlot, then adopts
/// \p TargetSP and restores the target fiber's context.
///
/// This call "returns" twice, on two different stacks:
/// - For the calling fiber: not until some other fiber later switches back to
///   the pointer stored in \p SaveSlot - possibly much later.
/// - For the target fiber: immediately, as the return of the `FJS_finerSwitch`
///   call *it* made when it parked itself (or, for a fiber that has never run,
///   as entry into `FJS_fiberTrampoline`).
///
/// \param SaveSlot Where to store the current fiber's parked stack pointer.
///                 Must remain valid until the fiber is resumed.
/// \param TargetSP Parked stack pointer of the fiber to resume. Must point to
///                 a valid `ContextFrame`: either one produced bt a previous
///                 `FJS_fiberSwitch` call, or one forged by the
///                 `FJS::createFiber`. Never null.
/// \param Arg      One-word message for the target fiber: it emerges as the
///                 return value of the target's own `FJS_fiberSwitch` call
///                 (or as the entry argument, for a first resume).
///
/// \return The \p Arg passed by whichever fiber eventually resumes us.
///
/// \warning After this call, \p TargetSP is stale - the target fiber is
///          running as its frame has been consumed. Never resume the same
///          parked pointer twice.
extern "C" void *FJS_fiberSwitch(void **SaveSlot, void *TargetSP, void *Arg);

#endif // FJS_FIBER_SWITCH_H
