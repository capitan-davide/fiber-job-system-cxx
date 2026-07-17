#ifndef FJS_FIBER_POOL_H
#define FJS_FIBER_POOL_H

#include "Fiber.h"

#include "FJS/Types.h"

#include <vector>

namespace FJS {

/// Fixed pool of pre-created fibers.
///
/// All fiber creation happens once, in `init` - nothing allocates stacks
/// mid-flight. Fibers are recycled LIFO: the most recently released fiber's
/// stack is the one still warm in cache.
class FiberPool {
public:
  /// Creates \p Count fibers, each set up to run \p EntryFn(\p Arg) on first
  /// resume - in practice always the scheduler's fiberMain.
  void init(U32 Count, void (*EntryFn)(void *), void *Arg);

  // Destroys every fiber. All of them must have been released back: a fiber
  // still out there is parked state about to be freed.
  void shutdown();

  /// Hands out a parked fiber, or nullptr if the pool is exhausted - a real
  /// runtime condition the scheduler must handle, not a bug.
  Fiber *acquire();

  /// Returns \p F to the pool for reuse. \p F must have come from `acquire` and
  /// must be parked (its stack is about to get a new tenant).
  void release(Fiber *F);

private:
  std::vector<Fiber> Fibers;
  std::vector<Fiber *> Free;
};

} // namespace FJS

#endif // FJS_FIBER_POOL_H
