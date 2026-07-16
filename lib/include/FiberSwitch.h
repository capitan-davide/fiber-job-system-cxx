#ifndef FJS_FIBER_SWITCH_H
#define FJS_FIBER_SWITCH_H

/// Raw fiber switch (see FiberSwitch.S). Most code should call the
/// `FJS::AsmBackend::switchFiber` wrapper instead, which manages the Fiber
/// handles and poisons the consumed frame; this symbol is the bare mechanism.
///
/// Saves the current context (callee-saved registers, FP control state, resume
/// address) as a `ContextFrame` on the current stack, publishes the resulting
/// stack pointer through \p SaveSlot, then adopts \p TargetSP and restores that
/// fiber's context. Returns twice, on two stacks - see
/// `FJS::AsmBackend::switchFiber` for the full semantics.
///
/// \param SaveSlot Where to store the current fiber's parked stack pointer.
/// \param TargetSP Parked stack pointer to resume. Must point to a valid
///                 `ContextFrame`; consumed by this call - never resume the
///                 same pointer twice.
/// \param Arg      Returned to the target from *its* switch call.
///
/// \return The \p Arg passed by whichever fiber eventually resumes us.
extern "C" void *FJS_fiberSwitch(void **SaveSlot, void *TargetSP, void *Arg);

#endif // FJS_FIBER_SWITCH_H
