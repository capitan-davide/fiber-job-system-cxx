#include "FiberPool.h"
#include "Fiber.h"

#include "FJS/Error.h"
#include "FJS/Types.h"

#include <mutex>

namespace FJS {

void FiberPool::init(U32 Count, void (*EntryFn)(void *), void *Arg) {
  // No lock is needed, runs before the workers spawn.
  Fibers.resize(Count);
  Free.reserve(Count);
  for (Fiber &F : Fibers) {
    createFiber(F, EntryFn, Arg);
    Free.push_back(&F);
  }
}

void FiberPool::shutdown() {
  // Every fiber must be back in the pool: a missing one is still parked
  // somewhere and would leak (or worse, resumed later). No lock is needed
  // here, runs after the workers join.
  FJS_ASSERT(Free.size() == Fibers.size(),
             "Trying to free the pool while fibers are running");
  for (Fiber &F : Fibers)
    destroyFiber(F);
  Fibers.clear();
  Free.clear();
}

Fiber *FiberPool::acquire() {
  std::lock_guard G{Lock};

  if (Free.empty())
    return nullptr;
  Fiber *F = Free.back();
  Free.pop_back();
  return F;
}

void FiberPool::release(Fiber *F) {
  std::lock_guard G{Lock};
  Free.push_back(F);
}

} // namespace FJS
