#include "JobQueue.h"

#include "FJS/Job.h"

#include <mutex>

namespace FJS {

void JobQueue::push(const JobDecl &JD) {
  std::lock_guard G{Lock};
  Jobs.push_back(JD);
}

bool JobQueue::pop(JobDecl &Out) {
  std::lock_guard G{Lock};

  if (Jobs.empty())
    return false;
  Out = Jobs.front();
  Jobs.pop_front();
  return true;
}

} // namespace FJS
