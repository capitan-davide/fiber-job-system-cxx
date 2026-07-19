#ifndef FJS_SHUTDOWN_H
#define FJS_SHUTDOWN_H

namespace FJS {

/// Blocks shutdown signals on the calling thread. Main thread, BEFORE any
/// worker spawns: threads inherit the creator's mask at creation time.
void blockShutdownSignals();

/// True once a shutdown signal has arrived. Workers poll this once per
/// sheduler-loop iteration.
bool isShutdownRequested();

/// True if this thread's mask blocks every shutdown signal.
bool areShutdownSignalsBlocked();

/// Spawns a detached signal-wait thread: first shutdown signal sets
/// `isShutdownRequested`, a second force-exits the process.
///
/// \pre `blockShutdownSignals()` already ran on this thread - the waiter
///      inherits our mask, and `sigwait` only consumes blocked signals.
void startSignalWaiter();

/// Request a graceful shutdown programmatically - same flag the signal waiter
/// sets.
void requestShutdown();

namespace testing {
/// Test hook: clears the shutdown flag so a test starts from "running".
void resetShutdownFlagForTesting();
} // namespace testing

} // namespace FJS

#endif // FJS_SHUTDOWN_H
