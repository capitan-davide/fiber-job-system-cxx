#ifndef FJS_FIBER_POOL_H
#define FJS_FIBER_POOL_H

#include "Fiber.h"

#include "FJS/Types.h"

#include <vector>

namespace FJS {

class FiberPool {
public:
  void init(U32 Count, void (*EntryFn)(void *), void *Arg);
  void shutdown();

  Fiber *acquire();
  void release(Fiber *F);

private:
  std::vector<Fiber> Fibers;
  std::vector<Fiber *> Free;
};

} // namespace FJS

#endif // FJS_FIBER_POOL_H
