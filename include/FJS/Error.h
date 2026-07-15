#ifndef FJS_ERROR_H
#define FJS_ERROR_H

#include <cstdio>
#include <cstdlib>
#include <print>
#include <utility>

namespace FJS::detail {

inline void assertInternal(bool Cond, const char *CondStr, const char *Msg,
                           const char *File, unsigned Line) {
  if (!Cond) [[unlikely]] {
    std::println(stderr, "[ASSERTION FAILED] {}:{}: {}: {}", File, Line, CondStr,
                 Msg);
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

#ifdef NDEBUG
#define FJS_ASSERT(Cond, Msg) ((void)sizeof(Cond))
#define FJS_UNREACHABLE(Msg) std::unreachable()
#else
#define FJS_ASSERT(Cond, Msg)                                                  \
  FJS::detail::assertInternal((Cond), #Cond, (Msg), __FILE__, __LINE__)
#define FJS_UNREACHABLE(Msg)                                                   \
  FJS::detail::unreachableInternal((Msg), __FILE__, __LINE__)
#endif

#endif // FJS_ERROR_H
