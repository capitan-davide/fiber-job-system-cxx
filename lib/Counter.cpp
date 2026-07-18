#include "Counter.h"

#include "FJS/Error.h"
#include "SpinLock.h"

namespace FJS {

U32 Counter::decrement(std::span<Fiber *, MaxWaiters> Woken) {
  ScopedLock L(Lock);

  const U32 Prev = Value.fetch_sub(1, std::memory_order_seq_cst);
  FJS_ASSERT(Prev > 0, "Counter decrement underflow");
  const U32 Now = Prev - 1;

  U32 NumWoken = 0;
  for (Waiter &W : Waiters) {
    if (!W.ParkedFiber || W.Target != Now)
      continue;

    Woken[NumWoken++] = W.ParkedFiber;
    W.ParkedFiber = nullptr; // slot is free again
  }

  return NumWoken;
}

U32 Counter::load() const { return Value.load(std::memory_order_seq_cst); }

bool Counter::addWaiter(Fiber *F, U32 Target) {
  ScopedLock L(Lock);

  // The whole point of this function: the check and the registration happen
  // under the same lock. A decrement cannot land between them.
  if (Value.load(std::memory_order_seq_cst) == Target)
    return false; // already satisfied - caller must not park

  for (Waiter &W : Waiters) {
    if (W.ParkedFiber)
      continue;

    W.ParkedFiber = F;
    W.Target = Target;

    return true;
  }

  FJS_ABORT("Counter wait list full");
}

void Counter::reset(U32 NewValue) { Value.store(NewValue); }

} // namespace FJS
