#include "JobQueue.h"

#include "FJS/Job.h"
#include "FJS/Tracy.h"
#include "FJS/Types.h"

#include <mutex>

namespace FJS {

void JobQueue::push(const JobDecl &JD) {
  ZoneScoped;
  std::scoped_lock G{Lock};
  Jobs.push_back(JD);
  TracyPlot(TracyPlotName, static_cast<I64>(Jobs.size()));
}

bool JobQueue::pop(JobDecl &Out) {
  ZoneScoped;
  std::scoped_lock G{Lock};

  if (Jobs.empty()) {
    TracyPlot(TracyPlotName, static_cast<I64>(Jobs.size()));
    return false;
  }
  Out = Jobs.front();
  Jobs.pop_front();
  TracyPlot(TracyPlotName, static_cast<I64>(Jobs.size()));
  return true;
}

} // namespace FJS
