#ifndef FJS_COUNTER_H
#define FJS_COUNTER_H

#include "FJS/Types.h"

#include <atomic>

namespace FJS {

class Counter {
public:
  explicit Counter(U32 Initial) : Value{Initial} {}

  void decrement() { Value.fetch_sub(1, std::memory_order_seq_cst); }
  U32 load() { return Value.load(std::memory_order_seq_cst); }

private:
  std::atomic<U32> Value;
};

} // namespace FJS

#endif // FJS_COUNTER_H
