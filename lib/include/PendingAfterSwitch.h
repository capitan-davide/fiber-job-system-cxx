#ifndef FJS_PENDING_AFTER_SWITCH_H
#define FJS_PENDING_AFTER_SWITCH_H

#include "TLS.h"

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
