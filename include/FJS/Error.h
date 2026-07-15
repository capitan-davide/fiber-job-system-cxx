#ifndef FJS_ERROR_H
#define FJS_ERROR_H

#include <cstdlib>
#include <ostream>
#include <print>
#include <sstream>
#include <utility>

namespace FJS {

[[noreturn]] inline void unreachableInternal(const char *Msg, const char *File,
                                             unsigned Line) {
  std::ostringstream OSS;
  OSS << "[UNREACHABLE] " << File << ":" << Line << ": " << Msg;
  std::println(stderr, "{}", OSS.str());
  std::fflush(stderr);
  std::abort();
}

} // namespace FJS

#ifdef NDEBUG
#define FJS_UNREACHABLE(msg) std::unreachable()
#else
#define FJS_UNREACHABLE(Msg) FJS::unreachableInternal(Msg, __FILE__, __LINE__)
#endif

#endif // FJS_ERROR_H
