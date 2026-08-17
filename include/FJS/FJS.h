#ifndef FJS_FJS_H
#define FJS_FJS_H

#include "Job.h"
#include "Types.h"

namespace FJS {

struct JobSystemConfig {
  U32 SmallFiberCount;
  U32 LargeFiberCount;
  U32 NumWorkers;
  U32 NumIOThreads;
};

void bigInit(const JobSystemConfig &Config);

void run(const JobDecl &Entry);

void bigShutdown();

} // namespace FJS

#endif // FJS_FJS_H
