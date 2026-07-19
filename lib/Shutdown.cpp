#include "Shutdown.h"

#include "FJS/Error.h"

#include <atomic>
#include <bits/types/sigset_t.h>
#include <csignal>
#include <cstring>
#include <format>
#include <thread>

#include <pthread.h>

namespace {

std::atomic_bool ShutdownRequested{false};

constexpr int ShutdownSignals[] = {SIGINT, SIGTERM, SIGHUP};

sigset_t shutdownSigset() {
  sigset_t Set;
  sigemptyset(&Set);
  for (const int Sig : ShutdownSignals)
    sigaddset(&Set, Sig);
  return Set;
}

} // namespace

namespace FJS {

void blockShutdownSignals() {
  sigset_t Set = shutdownSigset();
  const int Err = pthread_sigmask(SIG_BLOCK, &Set, nullptr);
  FJS_ASSERT(!Err, "Failed to set signal mask: {}", std::strerror(Err));
}

bool isShutdownRequested() {
  return ShutdownRequested.load(std::memory_order_acquire);
}

bool areShutdownSignalsBlocked() {
  sigset_t Cur;
  const int Err = pthread_sigmask(SIG_BLOCK, nullptr, &Cur);
  FJS_ASSERT(!Err, "Failed to read signal mask: {}", std::strerror(Err));

  for (const int Sig : ShutdownSignals)
    if (!sigismember(&Cur, Sig))
      return false;
  return true;
}

void startSignalWaiter() {
  std::thread([] {
    sigset_t Set = shutdownSigset();
    int Sig = 0;
    int Err = sigwait(&Set, &Sig);
    FJS_ASSERT(!Err, "Failed to wait for signal: {}", std::strerror(Err));
    ShutdownRequested.store(true, std::memory_order_release);

    // Second signal = the user lost patience with our graceful shutdown.
    Err = sigwait(&Set, &Sig);
    FJS_ASSERT(!Err, "Failed to wait for signal: {}", std::strerror(Err));
    std::_Exit(128 + Sig);
  }).detach();
}

void requestShutdown() {
  ShutdownRequested.store(true, std::memory_order_release);
}

void testing::resetShutdownFlagForTesting() {
  ShutdownRequested.store(false, std::memory_order_release);
}

} // namespace FJS
