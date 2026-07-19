#ifndef FJS_JOB_H
#define FJS_JOB_H

/// Declares a job entry function with the required signature:
/// `void Name(void *Arg)`.
///
/// Unlike `FIBER_ENTRY_POINT`, a job entry *returns normally* - the scheduler
/// regains control when it does and decrements the job's `Done` counter.
///
/// Exceptions must NOT escape a job: the library is built with
/// `-fno-exceptions`, and unwinding across a fiber switch lands on the forged
/// trampoline frame, where there is nothing valid to unwind through.
#define JOB_ENTRY_POINT(Name) void Name(void *Arg) noexcept

namespace FJS {

/// Signature of a job entry function. See `JOB_ENTRY_POINT`.
using JobEntry = void (*)(void *Param) noexcept;

class Counter;

/// A unit of work handed to the job system.
///
/// A plain aggregate, passed around by value: fill it, submit it (via
/// `RunJobs`), forget it - the job system copies what it needs. Kept trivially
/// copyable on purpose; arrays of decls are submitted in bulk.
struct JobDecl {
  JobEntry EntryFn = nullptr; // the function this job runs
  void *Param = nullptr;      // opaque word passed as EntryFn's argument
  Counter *Done = nullptr;    // decremented when the job finishes; can be null
};

} // namespace FJS

#endif // FJS_JOB_H
