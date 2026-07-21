#ifndef FJS_PENDING_AFTER_SWITCH_H
#define FJS_PENDING_AFTER_SWITCH_H

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

namespace FJS {

using AfterSwitchFn = void (*)(void *);

struct AfterSwitch {
  AfterSwitchFn Fn = nullptr;
  void *Arg = nullptr;
};

FJS_OPAQUE void setPendingAfterSwitch(AfterSwitch Pending) noexcept;
FJS_OPAQUE AfterSwitch takePendingAfterSwitch() noexcept;

} // namespace FJS

#endif // FJS_PENDING_AFTER_SWITCH_H
