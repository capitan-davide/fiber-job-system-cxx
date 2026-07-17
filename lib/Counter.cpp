#include "FJS/Counter.h"
#include "FJS/Error.h"

namespace FJS {

void Counter::decrement() {
  [[maybe_unused]] const U32 Prev =
      Value.fetch_sub(1, std::memory_order_seq_cst);
  FJS_ASSERT(Prev > 0, "Counter decrement underflow");
}

U32 Counter::load() const { return Value.load(std::memory_order_seq_cst); }

} // namespace FJS
