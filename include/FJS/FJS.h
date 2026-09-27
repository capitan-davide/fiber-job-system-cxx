#ifndef FJS_FJS_H
#define FJS_FJS_H

#include "Job.h"
#include "Types.h"

#include "FJS/Counter.h"

#include <span>

namespace FJS {

struct JobSystemConfig {
  U32 SmallFiberCount;
  U32 LargeFiberCount;
  U32 NumWorkers;
  U32 NumIOThreads;
};

void bigInit(const JobSystemConfig &Config);

void run(const JobDecl &Entry);

Counter *runIOJobs(std::span<const JobDecl> Jobs);

void waitForCounterAndFree(Counter *C);

void bigShutdown();

} // namespace FJS

#endif // FJS_FJS_H
