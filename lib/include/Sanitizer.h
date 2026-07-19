#ifndef FJS_SANITIZER_H
#define FJS_SANITIZER_H

// GCC defines __SANITIZE_ADDRESS__, Clang uses __has_feature
#if defined(__SANITIZE_ADDRESS__)
#define FJS_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define FJS_ASAN 1
#endif
#endif

#if defined(__SANITIZE_THREAD__)
#define FJS_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define FJS_TSAN 1
#endif
#endif

#ifdef FJS_ASAN
#include <sanitizer/common_interface_defs.h>
#endif

#ifdef FJS_TSAN
#include <sanitizer/tsan_interface.h>
#endif

#endif // FJS_SANITIZER_H
