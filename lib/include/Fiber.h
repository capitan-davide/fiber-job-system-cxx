#ifndef FJS_FIBER_H
#define FJS_FIBER_H

#include "FiberSwitch.h"

#include "FJS/Error.h"
#include "FJS/Types.h"
#include <utility>

/// Declares a fiber entry function with the required signature:
/// `[[noreturn]] void Name(void *Arg)`.
///
/// `[[noreturn]]` because a fiber has no caller to return to: its stack starts
/// at the forged `ContextFrame`, whose planted return address is
/// `FJS_fiberTrampoline` - below that there's nothing but the guard page. A
/// fiber leaves only by switching away (`switchFiber`); if the entry function
/// ever returns, the trampoline's trailing `ud2` aborts the process. The
/// attribute makes the compiler enforce the contract: any code path that falls
/// off the end becomes a warning at the definition instead of a SIGILL at
/// runtime.
#define FIBER_ENTRY_POINT(Name) [[noreturn]] void Name(void *Arg)

namespace FJS {

using AfterSwitchFn = void (*)(void *);

namespace detail {
struct AfterSwitch {
  AfterSwitchFn Fn = nullptr;
  void *Arg = nullptr;
};

// One slot per thread: written by the fiber switching away, consumed by the
// fiber being resumed - always the next thing this thread runs.
inline thread_local AfterSwitch PendingAfterSwitch;
} // namespace detail

/// Runs the callback the previous fiber attached to the switch that resumed
/// us, if any. Called automatically when resuming inside `switchFiber`; a
/// fiber ENTRY FUNCTION must call it as its first statement (fresh fibers
/// resume at their entry, bypassing the `switchFiber` return path).
inline void runAfterSwitchCallback() {
  detail::AfterSwitch Pending = std::exchange(detail::PendingAfterSwitch, {});
  if (Pending.Fn)
    Pending.Fn(Pending.Arg);
}

struct Fiber {
  void *SP = nullptr;    // parcked stack pointer - only meaningful while parked
  void *Stack = nullptr; // allocation base, kept for freeing
  U64 StackSize = 0;
};

struct AsmBackend {
  static void createFiber(Fiber &F, void (*EntryFn)(void *), void *Arg = {});
  static void destroyFiber(Fiber &F);
  static void *switchFiber(Fiber &From, Fiber &To, void *Arg = {});
};

inline void *AsmBackend::switchFiber(Fiber &From, Fiber &To, void *Arg) {
  void *TargetSP = To.SP;
  To.SP = nullptr; // the frame is about to be consumed, safeguard agains
                   // double-resume
  void *Ret = ::FJS_fiberSwitch(&From.SP, TargetSP, Arg);

  // From.SP == nullptr is set by whomever switches back into this fiber. When
  // switching back, From is now the To argument.
  FJS_ASSERT(From.SP == nullptr, "Fiber resumed without consuming the handle");
  return Ret;
}

struct UcontextBackend {
  static void createFiber(Fiber &F, void (*EntryFn)(void *), void *Arg = {});
  static void destroyFiber(Fiber &F);
  static void *switchFiber(Fiber &From, Fiber &To, void *Arg = {});
};

#ifdef FJS_USE_UCONTEXT
using DefaultBackend = UcontextBackend;
#else
using DefaultBackend = AsmBackend;
#endif

/// Prepares a fiber so the next `FJS_fiberSwitch` to it runs \p EntryFn(\p
/// Arg).
///
/// Allocates a fresh stack and forgoes a `ContextFrame` as its top that mimics
/// a parked fiber: the cargo slots (R12/R13) carry \p EntryFn and \p Arg, and
/// RIP points at `FJS_fiberTrampoline`. `F.SP` is set to this frame, so to the
/// switch the new fiber is indistinguishable from one that parked itself.
///
/// The fiber does not run yet - execution starts only when some fiber switches
/// to `F.SP`.
///
/// \param F       Fiber to initialize. Any previous state is overwritten
/// without
///                being freed.
/// \param EntryFn Function the fiber must execute on first resume. Must never
///                return: it must switch away (park) instead. Returning lands
///                on `ud2` and aborts the process.
/// \param Arg     Opaque word passed as `EntryFn`'s only argument.
inline void createFiber(Fiber &F, void (*EntryFn)(void *), void *Arg = {}) {
  return DefaultBackend::createFiber(F, EntryFn, Arg);
}

/// Frees a fiber's stack.
///
/// The fiber must not be running and must never be resumed again: its saved
/// context lives on the stack being freed. Safe on parked fibers, including
/// ones parked mid-`EntryFn` that will simply never continue.
///
/// \param F Fiber to destroy. Its fields are reset so a later `createFiber` may
///          reuse the object.
inline void destroyFiber(Fiber &F) { return DefaultBackend::destroyFiber(F); }

/// Parks the current fiber (\p From) and resumes \p To.
///
/// This call "returns" twice, on two different stacks:
/// - For \p To: immediately, as the return of the `switchFiber` call *it* made
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
/// \param To   A parked fiber (from a previous `switchFiber`) or a fresh one
///             from `createFiber`. `To.SP` must be non-null.
/// \param Arg  One-word message for \p To: it emerges as the return value of
///             \p To's own `switchFiber` call (or as its EntryFn argument, on
///             first run).
/// \param Callback    Runs st the first thing \p To does after the switch, on
///                    \p To's stack. This is the only safe place to publish
///                    \p From (free list, wait list): \p From's stack is
///                    abandoned by then. Fresh fibers bypass the `switchFiber`
///                    return path, so every entry function MUST call
///                    `runAfterSwitchCallback` as its first statement or the
///                    callback is silently lost.
/// \param CallbackArg Opaque work handed to \p Callback. Fypically \p From,
///                    or a struct holding it.
///
/// \return The \p Arg passed by whichever fiber eventually resumes us.
inline void *switchFiber(Fiber &From, Fiber &To, void *Arg = {},
                         AfterSwitchFn Callback = {}, void *CallbackArg = {}) {
  detail::PendingAfterSwitch = {Callback, CallbackArg};
  void *Ret = DefaultBackend::switchFiber(From, To, Arg);
  runAfterSwitchCallback(); // we were just resumed - run OUR resumer's callback
  return Ret;
}

} // namespace FJS

#endif // FJS_FIBER_H
