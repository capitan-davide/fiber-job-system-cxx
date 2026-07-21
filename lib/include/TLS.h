#ifndef FJS_TLS_H
#define FJS_TLS_H

/// Compiler attributes:
///   - noinline: prevents function inlining
///   - noipa (GCC): prevents inlining and other interprocedural optimizations
///   - noclone (GCC): prevents making specialized copies of the same function
///   - optnone (Clang): disables optimizations of the function body
///   - used: prevents DCE from removing an unreferenced symbol
///   - retain: prevents linker from garbage-collect this section
#if defined(__GNUC__) && !defined(__clang__)
#define FJS_OPAQUE __attribute__((noinline, noipa, noclone, used, retain))
#elif defined(__clang__)
#define FJS_OPAQUE __attribute__((noinline, optnone, used, retain))
#else
#error "Compiler not supported"
#endif

#endif // FJS_TLS_H
