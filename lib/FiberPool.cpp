#include "FiberPool.h"
#include "Fiber.h"

#include "FJS/Error.h"
#include "FJS/Types.h"

namespace FJS {

void FiberPool::init(U32 Count, void (*EntryFn)(void *), void *Arg) {
  Fibers.resize(Count);
  Free.reserve(Count);
  for (Fiber &F : Fibers) {
    createFiber(F, EntryFn, Arg);
    Free.push_back(&F);
  }
}

void FiberPool::shutdown() {
  // Every fiber must be back in the pool: a missing one is still parked
  // somewhere and would leak (or worse, resumed later).
  FJS_ASSERT(Free.size() == Fibers.size(), "");
  for (Fiber &F : Fibers)
    destroyFiber(F);
  Fibers.clear();
  Free.clear();
}

Fiber *FiberPool::acquire() {
  if (Free.empty())
    return nullptr;
  Fiber *F = Free.back();
  Free.pop_back();
  return F;
}

void FiberPool::release(Fiber *F) {
  Free.push_back(F);
}

} // namespace FJS
