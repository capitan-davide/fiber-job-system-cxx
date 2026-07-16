#include "Fiber.h"
#include "FiberSwitch.h"

#include "FJS/Error.h"
#include "FJS/Types.h"

#include <cstddef>
#include <cstdlib>

#include <sys/mman.h> // TODO: This is temporary and will be moved into an allocator
#include <ucontext.h>
#include <unistd.h>

namespace FJS {

static constexpr U64 DefaultStackSize = 64 * 1024;

namespace {

/// A parked fiber's saved context, exactly as `FJS_fiberSwitch` lays it out on
/// the stack (see FiberSwitch.S).
///
/// \note The 16-byte alignment is NOT for the members (8 would do) - it is the
///       SysV ABI's requirement on a parked fiber `rsp`, which this struct's
///       address become. Real frames get it for free from the switch's push
///       arithmetic; forget ones (from `createFiber`) must place the frame at
///       `Top - 64` with `Top` 16-aligned.
struct alignas(16) ContextFrame {
  U16 FCW;         // 0x00
  U16 _Pad;        // ...
  U32 MXCSR;       // 0x04
  void *R15, *R14; // 0x08, 0x10
  void *R13;       // 0x18  <- cargo: Arg
  void *R12;       // 0x20  <- cargo: EntryFn
  void *RBP, *RBX; // 0x28, 0x30
  void *RIP;       // 0x38  <- &FJS_fiberTrampoline
};
static_assert(sizeof(ContextFrame) == 64);
static_assert(alignof(ContextFrame) == 16);

extern "C" [[noreturn]] void
FJS_fiberTrampoline(); // never call this - address-only symbol

} // namespace

void AsmBackend::createFiber(Fiber &F, void (*EntryFn)(void *), void *Arg) {
  const long PageSize = ::sysconf(_SC_PAGESIZE);

  // One guard page at the low end, usable stack above it.
  F.StackSize = DefaultStackSize + PageSize;
  F.Stack = ::mmap(nullptr, F.StackSize, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (F.Stack == MAP_FAILED)
    std::abort(); // TODO: Proper error handling

  // The bottom page becomes the guard: stack grows down, so an overflow walks
  // into it and faults immediately instead of corrupting memory.
  if (::mprotect(F.Stack, PageSize, PROT_NONE) != 0)
    std::abort(); // TODO: Proper error handling

  auto *Top = static_cast<std::byte *>(F.Stack) + F.StackSize;
  auto *Frame = reinterpret_cast<ContextFrame *>(Top - sizeof(ContextFrame));

  // TODO: Copy the thread's current values for FCW and MXCSR instead of
  // hard-coding them.

  *Frame = {};           // zero everithing first (R14, R15, RBP, RBX, ...)
  Frame->FCW = 0x037F;   // x87 default: all exceptions masked
  Frame->MXCSR = 0x1F80; // SSE default: same idea
  Frame->R12 = reinterpret_cast<void *>(EntryFn); // cargo
  Frame->R13 = Arg;                               // cargo
  Frame->RIP = reinterpret_cast<void *>(&FJS_fiberTrampoline);

  F.SP = Frame;
}

void AsmBackend::destroyFiber(Fiber &F) {
  ::munmap(F.Stack, F.StackSize);
  F = {};
}

namespace {

struct UcontextHeader {
  ucontext_t Ctx; // must stay first: F.SP points here, cast to ucontext_t *
  void (*EntryFn)(void *);
  void *Arg;
};

[[noreturn]] void ucontextTrampoline(unsigned Hi, unsigned Lo) {
  auto *H = reinterpret_cast<UcontextHeader *>(static_cast<U64>(Hi) << 32 |
                                               static_cast<U64>(Lo));
  H->EntryFn(H->Arg);
  FJS_UNREACHABLE("Reached the end of the fiber entrypoint");
}

} // namespace

void UcontextBackend::createFiber(Fiber &F, void (*EntryFn)(void *),
                                  void *Arg) {
  const long PageSize = ::sysconf(_SC_PAGESIZE);

  // Same stack recipe as AsmBackend: guard page low, usable stack above.
  F.StackSize = DefaultStackSize + PageSize;
  F.Stack = ::mmap(nullptr, F.StackSize, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (F.Stack == MAP_FAILED)
    std::abort(); // TODO: Proper error handling

  if (::mprotect(F.Stack, PageSize, PROT_NONE) != 0)
    std::abort(); // TODO: Proper error handling

  // Header at the (16-aligned) top; the usable stack is everything between the
  // guard page and the header.
  auto *Top = static_cast<std::byte *>(F.Stack) + F.StackSize;

  // Manually align the address to 16-byte - required because the size, and
  // alignment, of UcontextHeader are controlled by ucontext_t which makes no
  // promises about them. Rounding down is safe: the header slides toward the
  // stack interior, never past Top.
  const U64 HiAddr =
      (reinterpret_cast<U64>(Top) - sizeof(UcontextHeader)) & ~U64{15};

  auto *Hi = reinterpret_cast<UcontextHeader *>(HiAddr);
  auto *StackLo = static_cast<std::byte *>(F.Stack) + PageSize;

  Hi->EntryFn = EntryFn;
  Hi->Arg = Arg;

  if (::getcontext(&Hi->Ctx) != 0)
    std::abort(); // TODO: Proper error handling
  Hi->Ctx.uc_stack.ss_sp = StackLo;
  Hi->Ctx.uc_stack.ss_size = reinterpret_cast<std::byte *>(Hi) - StackLo;
  Hi->Ctx.uc_link = nullptr; // no successor: entry must never return anyway

  // makecontext only passes ints - split the 64-bit pointer in two.
  ::makecontext(&Hi->Ctx, reinterpret_cast<void (*)()>(&ucontextTrampoline), 2,
                static_cast<U32>(HiAddr >> 32),
                static_cast<U32>(HiAddr & 0xFFFFFFFF));

  F.SP = &Hi->Ctx;
}

void UcontextBackend::destroyFiber(Fiber &F) {
  ::munmap(F.Stack, F.StackSize);
  F = {};
}

/// One-word message channel across a swapcontext, standing in for the asm
/// backend's RDX->RAX hand-off. thread_local is enough: the resumer writes it
/// immediately before swapcontext, the resumed fiber reads it immediately
/// after, on the same thread - even when fibers migrade threads.
thread_local void *SwitchArg = nullptr;

void *UcontextBackend::switchFiber(Fiber &From, Fiber &To, void *Arg) {
  ucontext_t Saved; // on *this* fiber's stack - same lifetime as an asm frame
  From.SP = &Saved;
  SwitchArg = Arg;
  void *TargetSP = To.SP;
  To.SP = nullptr; // the frame is about to be consumed, safeguard agains double-resume
  if (::swapcontext(&Saved, static_cast<ucontext_t *>(TargetSP)) != 0)
    std::abort(); // TODO: Proper error handling

  return SwitchArg; // planted by whoever resumed us
}

} // namespace FJS
