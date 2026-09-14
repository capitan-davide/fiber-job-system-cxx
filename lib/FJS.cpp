#include "FJS/FJS.h"
#include "FJS/Error.h"
#include "FJS/Job.h"

#include "IOThread.h"
#include "Shutdown.h"
#include "Worker.h"

#include <array>
#include <semaphore>

namespace FJS {
namespace {
struct JobSystemCtx {
  JobScheduler S;
  WorkerPool W;
  IOThreadPool P;
};

JobSystemCtx Sys;

struct EntryCtx {
  JobDecl Entry;
  std::binary_semaphore *Finished;
};

JOB_ENTRY_POINT(entryJob) {
  auto *Ctx = static_cast<EntryCtx *>(Arg);
  Ctx->Entry.EntryFn(Ctx->Entry.Param);
  Ctx->Finished->release();
}
} // namespace

void bigInit(const JobSystemConfig &Config) {
  Sys.S.init({.SmallFiberCount = Config.SmallFiberCount,
              .LargeFiberCount = Config.LargeFiberCount});
  startWorkers(Sys.W, Sys.S, Config.NumWorkers);
  startIOThreads(Sys.P, Sys.S, Config.NumIOThreads);
}

void run(const JobDecl &Entry) {
  FJS_ASSERT(Entry.EntryFn, "Entry job has no entry point");
  FJS_ASSERT(!Entry.Done, "The job system owns the job's completion");

  std::binary_semaphore Finished{0};
  EntryCtx Ctx{.Entry = Entry, .Finished = &Finished};

  const std::array Wrapper{JobDecl{.EntryFn = entryJob, .Param = &Ctx}};
  Sys.S.kickJobs(Wrapper);

  Finished.acquire(); // block this thread until completion
}

void bigShutdown() {
  requestShutdown();
  joinWorkers(Sys.W);
  joinIOThreads(Sys.P, Sys.S);
  Sys.S.shutdown();
}

} // namespace FJS
