#include "CounterPool.h"
#include "Counter.h"

#include <mutex>

namespace FJS {

CounterPool::CounterPool() {
  Free.reserve(MaxCounters); // capacity is final: Free never reallocates
  for (Counter &C : Counters)
    Free.push_back(&C);
}

Counter *CounterPool::acquire(U32 Value) {
  std::lock_guard G{Lock};

  if (Free.empty())
    return nullptr; // caller asserts same protocol as FiberPool
  Counter *C = Free.back();
  Free.pop_back();
  C->reset(Value); // rearm + stranded-waiter checkpoint
  return C;
}

void CounterPool::release(Counter *C) {
  std::lock_guard G{Lock};

  // A counter must release clean: a parked waiter here is a fiber that will
  // never be woken.
  C->assertNoWaiters("Counter released while still has waiters");
  Free.push_back(C);
}

} // namespace FJS
