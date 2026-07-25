#include "FiberPool.h"
#include "Fiber.h"

#include "FJS/Error.h"
#include "FJS/Tracy.h"
#include "FJS/Types.h"

#include <mutex>

namespace FJS {

void FiberPool::init(const FiberPoolConfig &Config, void (*EntryFn)(void *),
                     void *Arg) {
  initTier(Small, Config.SmallFiberCount, FiberStackSize::Small, EntryFn, Arg);
  initTier(Large, Config.LargeFiberCount, FiberStackSize::Large, EntryFn, Arg);
}

void FiberPool::shutdown() {
  shutdownTier(Small);
  shutdownTier(Large);
}

Fiber *FiberPool::acquire(FiberStackSize Size) {
  ZoneScoped;
  Tier &T = tierFor(Size);
  std::lock_guard G{T.Lock};

  if (T.Free.empty()) {
    TracyPlot("Free Fibers", static_cast<I64>(T.Free.size()));
    return nullptr;
  }
  Fiber *F = T.Free.back();
  T.Free.pop_back();
  TracyPlot("Free Fibers", static_cast<I64>(T.Free.size()));
  return F;
}

void FiberPool::release(Fiber *F) {
  ZoneScoped;
  Tier &T = tierFor(F->Tier);
  std::lock_guard G{T.Lock};
  T.Free.push_back(F);
  TracyPlot("Free Fibers", static_cast<I64>(T.Free.size()));
}

void FiberPool::initTier(Tier &T, U32 Count, FiberStackSize Size,
                         void (*EntryFn)(void *), void *Arg) {
  // No lock is needed, runs before the workers spawn.
  T.Fibers.resize(Count);
  T.Free.reserve(Count);
  for (Fiber &F : T.Fibers) {
    createFiber(F, Size, EntryFn, Arg);
    T.Free.push_back(&F);
  }
}

void FiberPool::shutdownTier(Tier &T) {
  // Every fiber must be back in the pool: a missing one is still parked
  // somewhere and would leak (or worse, resumed later). No lock is needed
  // here, runs after the workers join.
  FJS_ASSERT(T.Free.size() == T.Fibers.size(),
             "Trying to free the pool while fibers are running");
  for (Fiber &F : T.Fibers)
    destroyFiber(F);
  T.Fibers.clear();
  T.Free.clear();
}

FiberPool::Tier &FiberPool::tierFor(FiberStackSize Size) {
  return Size == FiberStackSize::Small ? Small : Large;
}

} // namespace FJS
