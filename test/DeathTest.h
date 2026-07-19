#ifndef FJS_TEST_DEATH_TEST_H
#define FJS_TEST_DEATH_TEST_H

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <sys/wait.h>
#include <unistd.h>

#include <catch2/catch_test_macros.hpp>

// Sanitizers install their own SIGSEGV/SIGABRT handlers: a sanitizer child
// reports and exits instead of dying BT the signal, so death-by-signal is
// unobserved under then.
#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
#define FJS_SANITIZED 1
#elif __has_feature(thread_sanitizer) || __has_feature(address_sanitizer)
#define FJS_SANITIZED 1
#endif

namespace FJS::testing {

/// Runs \p F in a forked child; returns the signal that killed it, or 0 if it
/// exited normally.
template <typename Fn> int diesWithSignal(Fn &&F) {
  pid_t Pid = ::fork();
  if (Pid == -1)
    FAIL("fork: " << std::strerror(errno));

  if (Pid == 0) {
    F();
    std::_Exit(0);
  }

  int Status;
  ::waitpid(Pid, &Status, 0);
  return WIFSIGNALED(Status) ? WTERMSIG(Status) : 0;
}

} // namespace FJS::testing

#ifdef FJS_SANITIZED
#define FJS_REQUIRE_DEATH(Sig, ...)                                            \
  SKIP("Cannot observe death-by-signal under sanitizers")
#else
#define FJS_REQUIRE_DEATH(Sig, ...)                                            \
  REQUIRE(FJS::testing::diesWithSignal(__VA_ARGS__) == (Sig))
#endif

#endif // FJS_TEST_DEATH_TEST_H
