#ifndef FJS_FIBER_POOL_H
#define FJS_FIBER_POOL_H

#include "Fiber.h"
#include "SpinLock.h"

#include "FJS/Types.h"

#include <vector>

namespace FJS {

struct FiberPoolConfig {
  U32 SmallFiberCount = 0;
  U32 LargeFiberCount = 0;
};

/// Fixed pool of pre-created fibers.
///
/// All fiber creation happens once, in `init` - nothing allocates stacks
/// mid-flight. Fibers are recycled LIFO: the most recently released fiber's
/// stack is the one still warm in cache.
class FiberPool {
public:
  /// Creates \p Config.SmallFiberCount + \p Config.LargeFiberCount fibers,
  /// each set up to run \p EntryFn(\p Arg) on first resume - in practice always
  /// the scheduler's fiberMain.
  void init(const FiberPoolConfig &Config, void (*EntryFn)(void *), void *Arg);

  // Destroys every fiber. All of them must have been released back: a fiber
  // still out there is parked state about to be freed.
  void shutdown();

  /// Hands out a parked fiber from the requested tier, or nullptr if that tier
  /// is exhausted - a real runtime condition the scheduler must handle.
  Fiber *acquire(FiberStackSize Size);

  /// Returns \p F to the pool, routed by F->Tier - the caller doesn't need to
  /// remember which tier it came from. \p F must have come from `acquire` and
  /// must be parked.
  void release(Fiber *F);

private:
  struct Tier {
    std::vector<Fiber> Fibers;
    std::vector<Fiber *> Free;
    SpinLock Lock;
  };

  void initTier(Tier &T, U32 Count, FiberStackSize Size,
                void (*EntryFn)(void *), void *Arg);
  void shutdownTier(Tier &T);
  Tier &tierFor(FiberStackSize Size);

  Tier Small, Large;
};

} // namespace FJS

#endif // FJS_FIBER_POOL_H
