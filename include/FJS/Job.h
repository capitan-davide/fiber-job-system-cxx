#ifndef FJS_JOB_H
#define FJS_JOB_H

namespace FJS {

#define JOB_ENTRY_POINT(Name) void Name(void *Arg)

using JobEntry = void (*)(void *Param);

class Counter;

struct JobDecl {
  JobEntry EntryFn = nullptr;
  void *Param = nullptr;
  Counter *Done = nullptr;
};

} // namespace FJS

#endif // FJS_JOB_H
