#ifndef FJS_ERROR_H
#define FJS_ERROR_H

#include <cstdio>
#include <cstdlib>
#include <print>
#include <utility>

// TODO: __builtin_trap() generates the same `ud2` instruction that is emitted
// at the end if FJS_fiberTrampoline. It might be helpful to use this to have
// the same behavior between AsmContext and Ucontext backends.

namespace FJS::detail {

[[noreturn]] inline void abortInternal(const char *Msg, const char *File,
                                       unsigned Line) {
  std::println(stderr, "[ABORT] {}:{}: {}", File, Line, Msg);
  std::fflush(stderr);
  std::abort();
}

inline void assertInternal(bool Cond, const char *CondStr, const char *Msg,
                           const char *File, unsigned Line) {
  if (!Cond) [[unlikely]] {
    std::println(stderr, "[ASSERTION FAILED] {}:{}: {}: {}", File, Line,
                 CondStr, Msg);
    std::fflush(stderr);
    std::abort();
  }
}

[[noreturn]] inline void unreachableInternal(const char *Msg, const char *File,
                                             unsigned Line) {
  std::println(stderr, "[UNREACHABLE] {}:{}: {}", File, Line, Msg);
  std::fflush(stderr);
  std::abort();
}

} // namespace FJS::detail

/// Unconditional fatal exit: prints \p Msg (with file/line information) and
/// terminates the process. Active in ALL build confiturations. This guards
/// paths reachable by user bugs.
#define FJS_ABORT(Msg) FJS::detail::abortInternal((Msg), __FILE__, __LINE__)

#ifdef NDEBUG

#define FJS_ASSERT(Cond, Msg) ((void)sizeof(Cond))
#define FJS_UNREACHABLE(Msg) std::unreachable()

#else

/// Debug-build invariant check: aborts with \p CondStr, \p Msg and the source
/// location if the \p Cond is false.
///
/// In release (`NDEBUG`) builds it compiles to nothing - but the condition
/// stays *syntactically* checked (unevaluated `sizeof`), so asserts can't
/// silently rot.
///
/// \warning The condition must be side-effect free: it does not execute in
///          release builds.
#define FJS_ASSERT(Cond, Msg)                                                  \
  FJS::detail::assertInternal((Cond), #Cond, (Msg), __FILE__, __LINE__)

/// Marks a point that control flow can never reach.
///
/// Debug builds: aborts loudly with \p Msg and the source location.
/// Release builds: becomes `std::unreachable()` - a *promise* to the
/// optimizer; reaching it is undefined behavior.
#define FJS_UNREACHABLE(Msg)                                                   \
  FJS::detail::unreachableInternal((Msg), __FILE__, __LINE__)

#endif

#endif // FJS_ERROR_H
