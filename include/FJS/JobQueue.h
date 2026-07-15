#ifndef FJS_JOB_QUEUE_H
#define FJS_JOB_QUEUE_H

#include "FJS/Job.h"

#include <deque>

namespace FJS {

class JobQueue {
public:
  void push(const JobDecl &JD);
  bool pop(JobDecl &Out);

private:
  std::deque<JobDecl> Jobs;
};

} // namespace FJS

#endif // FJS_JOB_QUEUE_H
