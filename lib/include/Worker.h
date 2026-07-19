#ifndef FJS_WORKER_H
#define FJS_WORKER_H

#include <thread>
#include <vector>

#include "FJS/Types.h"

namespace FJS {

/// The OS threads that will drive the scheduler.
struct WorkerPool {
  std::vector<std::thread> Threads;
};

/// Spawns \p Count workers.
///
/// \pre `blockShutdownSignals()` ran on the calling thread - workers
/// inherit our mask, and one born unblocked can wat a Ctrl-C. Asserted.
void startWorkers(WorkerPool &W, U32 Count);

/// blocks until every worker exits, then empties \p W. Workers exit when
/// shutdown is requested (signal or `requestShutdown`) - so calling this with
/// no shutdown in sight simply clocks until one arriges. That's the intended
/// way for main to wait.
void joinWorkers(WorkerPool &W);

} // namespace FJS

#endif // FJS_WORKER_H
