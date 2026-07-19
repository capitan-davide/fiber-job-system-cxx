#ifndef FJS_SANITIZER_H
#define FJS_SANITIZER_H

// GCC defines __SANITIZE_THREAD__, Clang uses __has_feature
#if defined(__SANITIZE_THREAD__)
#define FJS_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define FJS_TSAN 1
#endif
#endif

#ifdef FJS_TSAN
#include <sanitizer/tsan_interface.h>
#endif

#endif // FJS_SANITIZER_H
