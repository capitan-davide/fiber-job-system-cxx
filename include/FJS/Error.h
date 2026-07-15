#ifndef FJS_ERROR_H
#define FJS_ERROR_H

#include <cstdlib>
#include <print>
#include <utility>

namespace FJS {

[[noreturn]] inline void unreachableInternal(const char *Msg, const char *File,
                                             unsigned Line) {
  std::println(stderr, "[UNREACHABLE] {}:{}: {}", File, Line, Msg);
  std::fflush(stderr);
  std::abort();
}

} // namespace FJS

#ifdef NDEBUG
#define FJS_UNREACHABLE(Msg) std::unreachable()
#else
#define FJS_UNREACHABLE(Msg) FJS::unreachableInternal(Msg, __FILE__, __LINE__)
#endif

#endif // FJS_ERROR_H
