#include "FJS/FileSystem.h"
#include "FJS/Error.h"
#include "FJS/FJS.h"
#include "FJS/Job.h"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {
struct LoadFileArg {
  fs::path Path;
  FJS::Result<std::vector<std::byte>> &Result;
};

JOB_ENTRY_POINT(loadFileJob) {
  auto *Ctx = static_cast<LoadFileArg *>(Arg);

  std::error_code EC;
  const std::uintmax_t Size = fs::file_size(Ctx->Path, EC);
  if (EC) {
    Ctx->Result = std::unexpected{EC};
    delete (LoadFileArg *)Arg; // TODO: Just to silence LeakSan, to be removed.
    return;
  }

  std::ifstream File{Ctx->Path, std::ios::binary};
  if (!File) {
    Ctx->Result = std::unexpected{std::make_error_code(std::errc{errno})};
    delete (LoadFileArg *)Arg; // TODO: Just to silence LeakSan, to be removed.
    return;
  }

  Ctx->Result->resize(Size);

  File.read(reinterpret_cast<char *>(Ctx->Result->data()),
            static_cast<std::streamsize>(Size));
  if (File.fail()) {
    Ctx->Result = std::unexpected{std::make_error_code(std::errc{errno})};
    delete (LoadFileArg *)Arg; // TODO: Just to silence LeakSan, to be removed.
    return;
  }
  delete (LoadFileArg *)Arg; // TODO: Just to silence LeakSan, to be removed.
}
} // namespace

namespace FJS::FileSystem {

CounterHandle loadFile(fs::path Path, Result<std::vector<std::byte>> &Result) {
  // TODO: Replace this with custom allocator.
  auto *Arg = new LoadFileArg{.Path = std::move(Path), .Result = Result};
  const std::array Jobs{JobDecl{.EntryFn = loadFileJob, .Param = Arg}};
  return runIOJobs(Jobs);
}

} // namespace FJS::FileSystem
