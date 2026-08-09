#ifndef FJS_IO_THREAD_H
#define FJS_IO_THREAD_H

#include "JobScheduler.h"

#include "FJS/Types.h"

#include <thread>
#include <vector>

namespace FJS {

/// The OS threads that executes IO operations.
struct IOThreadPool {
  std::vector<std::thread> Threads;
};

/// Spawns \p Count IO threads.
///
/// \pre `blockShutdownSignals()` ran on the calling thread - IO threads
/// inherit our mask, and one born unblocked can wat a Ctrl-C. Asserted.
void startIOThreads(IOThreadPool &P, JobScheduler &S, U32 Count);

/// blocks until every IO thread exits, then empties \p P. IO threads exit when
/// shutdown is requested (signal or `requestShutdown`) - so calling this with
/// no shutdown in sight simply clocks until one arrives. That's the intended
/// way for main to wait.
void joinIOThreads(IOThreadPool &P, JobScheduler &S);

} // namespace FJS

#endif // FJS_IO_THREAD_H
