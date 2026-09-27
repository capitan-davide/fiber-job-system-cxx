#include "FJS/Counter.h"
#include "FJS/Error.h"

#include <mutex>

namespace FJS {

U32 Counter::decrement(std::span<Fiber *, MaxWaiters> Woken) {
  std::scoped_lock G{Lock};

  // release is needed here: even though we own Lock, some other thread can
  // still load(), which does not require Lock to be held.
  const U32 Prev = Value.fetch_sub(1, std::memory_order_release);
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

U32 Counter::load() const { return Value.load(std::memory_order_acquire); }

bool Counter::addWaiter(Fiber *F, U32 Target) {
  std::scoped_lock G{Lock};

  // The whole point of this function: the check and the registration happen
  // under the same lock. A decrement cannot land between them.
  // relaxed is enough: Value is only written under Lock, which we hold - the
  // lock hand-off already gives us both freshness and visibility.
  if (Value.load(std::memory_order_relaxed) == Target)
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

void Counter::reset(U32 NewValue) {
  // relaxed: the counter is unpublished here (pool-private, no waiters).
  // Readers can only reach it via the pointer published though the job queue,
  // and that hand-off is what makes this store visible.
  Value.store(NewValue, std::memory_order_relaxed);
}

} // namespace FJS
