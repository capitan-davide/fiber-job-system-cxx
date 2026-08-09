#include "CounterPool.h"
#include "Counter.h"

#include "FJS/Tracy.h"

#include <mutex>

namespace FJS {

void CounterPool::init() {
  Free.reserve(MaxCounters); // capacity is final: Free never reallocates
  for (Counter &C : Counters)
    Free.push_back(&C);
}

Counter *CounterPool::acquire(U32 Value) {
  ZoneScoped;
  std::lock_guard G{Lock};

  if (Free.empty())
    return nullptr; // caller asserts same protocol as FiberPool
  Counter *C = Free.back();
  Free.pop_back();
  C->reset(Value); // rearm + stranded-waiter checkpoint
  TracyPlot("Free Counters", static_cast<I64>(Free.size()));
  return C;
}

void CounterPool::release(Counter *C) {
  ZoneScoped;
  std::lock_guard G{Lock};

  // A counter must release clean: a parked waiter here is a fiber that will
  // never be woken.
  C->assertNoWaiters("Counter released while still has waiters");
  Free.push_back(C);
  TracyPlot("Free Counters", static_cast<I64>(Free.size()));
}

} // namespace FJS
