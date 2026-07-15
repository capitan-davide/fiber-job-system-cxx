#ifndef FJS_FIBER_SWITCH_H
#define FJS_FIBER_SWITCH_H

#include "Fiber.h"

namespace FJS {

/// Raw fiber switch (see FiberSwitch.S). Most code should call the
/// `FJS::fiberSwitch` wrapper instead, which manages the Fiber handles and
/// poisons the consumed frame; this symbol is the bare mechanism.
///
/// Saves the current context (callee-saved registers, FP control state, resume
/// address) as a `ContextFrame` on the current stack, publishes the resulting
/// stack pointer through \p SaveSlot, then adopts \p TargetSP and restores that
/// fiber's context. Returns twice, on two stacks - see `FJS::fiberSwitch` for
/// the full semantics.
///
/// \param SaveSlot Where to store the current fiber's parked stack pointer.
/// \param TargetSP Parked stack pointer to resume. Must point to a valid
///                 `ContextFrame`; consumed by this call - never resume the
///                 same pointer twice.
/// \param Arg      Returned to the target from *its* switch call.
///
/// \return The \p Arg passed by whichever fiber eventually resumes us.
extern "C" void *FJS_fiberSwitch(void **SaveSlot, void *TargetSP, void *Arg);

/// Parks the current fiber (\p From) and resumes \p To.
///
/// This call "returns" twice, on two different stacks:
/// - For \p To: immediately, as the return of the `fiberSwitch` call *it* made
///   when it parked itself (or as entry into its EntryFn, if it has never
///   run).
/// - For \p From: not until some other fiber later switches back to it -
///   possibly much later, possibly on a different thread, possibly never.
///
/// \p To's parked state is consumed: its `SP` is nulled before the switch, so
/// an erroneous second resume of the same parked state crashes immediately
/// instead of replaying a stale frame.
///
/// \param From The currently running fiber. Its parked stack pointer is
///             published to `From.SP`, which is how it is later resumed.
/// \param To   A parked fiber (from a previous `fiberSwitch`) or a fresh one
///             from `createFiber`. `To.SP` must be non-null.
/// \param Arg  One-word message for \p To: it emerges as the return value of
///             \p To's own `fiberSwitch` call (or as its EntryFn argument, on
///             first run).
///
/// \return The \p Arg passed by whichever fiber eventually resumes us.
///
/// \see FJS_fiberSwitch for the underlying ABI-level mechanism
inline void *fiberSwitch(Fiber &From, Fiber &To, void *Arg = {}) {
  void *TargetSP = To.SP;
  To.SP = nullptr; // the frame is about to be consumed, safeguard agains double-resume
  return FJS_fiberSwitch(&From.SP, TargetSP, Arg);
}

} // namespace FJS

#endif // FJS_FIBER_SWITCH_H
