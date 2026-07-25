#ifndef FJS_THREAD_SAFETY_H
#define FJS_THREAD_SAFETY_H

// Enable thread safety attributes only with Clang. The attributes can be safely
// erased when compiling with other compilers.
#if defined(__clang__) && (!defined(SWIG))
#define THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define THREAD_ANNOTATION_ATTRIBUTE__(x)
#endif

#define FJS_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(capability(x))
#define FJS_ACQUIRE(...)                                                       \
  THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))
#define FJS_RELEASE(...)                                                       \
  THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))
#define FJS_TRY_ACQUIRE(...)                                                   \
  THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_capability(__VA_ARGS__))
#define FJS_GUARDED_BY(...)                                                    \
  THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(__VA_ARGS__))
#define FJS_REQUIRES(...)                                                      \
  THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))
#define FJS_NO_THREAD_SAFETY_ANALYSIS                                          \
  THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

#endif // FJS_THREAD_SAFETY_H
