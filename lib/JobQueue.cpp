#include "FJS/JobQueue.h"
#include "FJS/Job.h"

namespace FJS {

void JobQueue::push(const JobDecl &JD) { Jobs.push_back(JD); }

bool JobQueue::pop(JobDecl &Out) {
  if (Jobs.empty())
    return false;
  Out = Jobs.front();
  Jobs.pop_front();
  return true;
}

} // namespace FJS
