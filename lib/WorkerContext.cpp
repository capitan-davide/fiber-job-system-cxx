#include "WorkerContext.h"

namespace FJS {

namespace {
/// This thread's context.
thread_local WorkerContext *ThisWorker = nullptr;
}

FJS_OPAQUE
WorkerContext *getThisWorkerContext() noexcept {
  asm volatile("" : : "r"(&ThisWorker) : "memory"); // force TLS to recompute
  WorkerContext *Tmp = ThisWorker;
  asm volatile("" ::: "memory");                    // no reordering after the load
  return Tmp;
}

FJS_OPAQUE
void setThisWorkerContext(WorkerContext *Ctx) noexcept {
  asm volatile("" ::: "memory");                    // no reordering before the store
  ThisWorker = Ctx;
  asm volatile("" : : "r"(&ThisWorker) : "memory"); // address escapes; store committed
}

} // namespace FJS
