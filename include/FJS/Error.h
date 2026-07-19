#ifndef FJS_ERROR_H
#define FJS_ERROR_H

#include <cstdio>
#include <cstdlib>
#include <format>
#include <print>
#include <utility>

// TODO: __builtin_trap() generates the same `ud2` instruction that is emitted
// at the end if FJS_fiberTrampoline. It might be helpful to use this to have
// the same behavior between AsmContext and Ucontext backends.

namespace FJS::detail {

template <class... Ts>
[[noreturn]] inline void abortInternal(const char *File, unsigned Line,
                                       std::format_string<Ts...> Fmt,
                                       Ts &&...Args) {
  std::print(stderr, "[ABORT] {}:{}: ", File, Line);
  std::println(stderr, Fmt, std::forward<Ts>(Args)...);
  std::fflush(stderr);
  std::abort();
}

template <class... Ts>
inline void assertInternal(bool Cond, const char *CondStr, const char *File,
                           unsigned Line, std::format_string<Ts...> Fmt,
                           Ts &&...Args) {
  if (!Cond) [[unlikely]] {
    std::print(stderr, "[ASSERTION FAILED] {}:{}: {}: ", File, Line, CondStr);
    std::println(stderr, Fmt, std::forward<Ts>(Args)...);
    std::fflush(stderr);
    std::abort();
  }
}

template <class... Ts>
[[noreturn]] inline void unreachableInternal(const char *File, unsigned Line,
                                             std::format_string<Ts...> Fmt,
                                             Ts &&...Args) {
  std::print(stderr, "[UNREACHABLE] {}:{}: ", File, Line);
  std::println(stderr, Fmt, std::forward<Ts>(Args)...);
  std::fflush(stderr);
  std::abort();
}

} // namespace FJS::detail

/// Unconditional fatal exit: prints \p Msg (with file/line information) and
/// terminates the process. Active in ALL build confiturations. This guards
/// paths reachable by user bugs.
#define FJS_ABORT(...)                                                         \
  FJS::detail::abortInternal(__FILE__, __LINE__, __VA_ARGS__)

#ifdef NDEBUG

#define FJS_ASSERT(Cond, ...) ((void)sizeof(Cond))
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
#define FJS_ASSERT(Cond, ...)                                                  \
  FJS::detail::assertInternal((Cond), #Cond, __FILE__, __LINE__, __VA_ARGS__)

/// Marks a point that control flow can never reach.
///
/// Debug builds: aborts loudly with \p Msg and the source location.
/// Release builds: becomes `std::unreachable()` - a *promise* to the
/// optimizer; reaching it is undefined behavior.
#define FJS_UNREACHABLE(...)                                                   \
  FJS::detail::unreachableInternal(__FILE__, __LINE__, __VA_ARGS__)

#endif

#endif // FJS_ERROR_H
