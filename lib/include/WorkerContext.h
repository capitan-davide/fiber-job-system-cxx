#ifndef FJS_WORKER_CONTEXT_H
#define FJS_WORKER_CONTEXT_H

#include "Fiber.h"

namespace FJS {

/// Everything that is a fact about THIS OS thread rather than about the
/// scheduler: the native stack we shut down onto, and who runs on us now.
struct WorkerContext {
  /// The thread's native stack, parked while fibers run. Never created via
  /// `createFiber` - it becomes a fiber the first time something switches away
  /// from it.
  Fiber Bootstrap;

  /// The fiber currently executing on this thread - whoever is running needs
  /// it to name itself as `From` in a switch.
  Fiber *Current = nullptr;
};

/// This thread's context. Set at worker entry (and inside `drive`); nullptr on
/// any thread that isn't currently a worker.
extern thread_local WorkerContext *ThisWorker;

} // namespace FJS

#endif // FJS_WORKER_CONTEXT_H
