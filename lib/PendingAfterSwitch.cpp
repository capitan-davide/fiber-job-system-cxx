#include "PendingAfterSwitch.h"

#include <utility>

namespace FJS {

namespace {
// One slot per thread: written by the fiber switching away, consumed by the
// fiber being resumed - always the next thing this thread runs.
thread_local AfterSwitch PendingAfterSwitch;
} // namespace

FJS_OPAQUE
void setPendingAfterSwitch(AfterSwitch Pending) noexcept {
  asm volatile("" ::: "memory");                            // no reordering before the store
  PendingAfterSwitch = Pending;
  asm volatile("" : : "r"(&PendingAfterSwitch) : "memory"); // address escapes; store committed
}

FJS_OPAQUE
AfterSwitch takePendingAfterSwitch() noexcept {
  asm volatile("" : : "r"(&PendingAfterSwitch) : "memory"); // force TLS to recompute
  AfterSwitch Tmp = std::exchange(PendingAfterSwitch, {});
  asm volatile("" ::: "memory");                            // no reordering after the load
  return Tmp;
}

} // namespace FJS
