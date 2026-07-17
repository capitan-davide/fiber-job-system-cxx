#ifndef FJS_TEST_DEATH_TEST_H
#define FJS_TEST_DEATH_TEST_H

#include <cerrno>
#include <cstring>

#include <sys/wait.h>
#include <unistd.h>

#include <catch2/catch_test_macros.hpp>

/// Runs \p F in a forked child; returns the signal that killed it, or 0 if it
/// exited normally.
template <typename Fn> int diesWithSignal(Fn &&F) {
  pid_t Pid = ::fork();
  if (Pid == -1)
    FAIL("fork: " << std::strerror(errno));

  if (Pid == 0) {
    F();
    ::_exit(0);
  }

  int Status;
  ::waitpid(Pid, &Status, 0);
  return WIFSIGNALED(Status) ? WTERMSIG(Status) : 0;
}

#endif // FJS_TEST_DEATH_TEST_H
