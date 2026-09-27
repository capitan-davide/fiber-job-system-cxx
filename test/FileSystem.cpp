#include "FJS/FileSystem.h"
#include "FJS/Error.h"
#include "FJS/FJS.h"
#include "FJS/Job.h"

#include "Shutdown.h"

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace fs = std::filesystem;

namespace {
struct LoadTestCtx {
  fs::path Path;
  FJS::Result<std::vector<std::byte>> &Result;
};

JOB_ENTRY_POINT(loadSomething) {
  auto *Ctx = static_cast<LoadTestCtx *>(Arg);

  FJS::Counter *C = FJS::FileSystem::loadFile(Ctx->Path, Ctx->Result);
  FJS::waitForCounterAndFree(C);
}
}; // namespace

TEST_CASE("loadFile reads a file's contents", "[FileSysmte]") {
  constexpr std::string_view Contents = "hello, io!";
  const fs::path TmpPath = fs::temp_directory_path() / "fjs_loadfile_test.bin";
  {
    std::ofstream OS{TmpPath, std::ios::binary};
    OS.write(Contents.data(), Contents.size());
  }

  FJS::testing::resetShutdownFlag();
  FJS::bigInit({.SmallFiberCount = 4, .NumWorkers = 2, .NumIOThreads = 1});

  FJS::Result<std::vector<std::byte>> Result;
  LoadTestCtx Ctx{.Path = TmpPath, .Result = Result};

  FJS::run({.EntryFn = loadSomething, .Param = &Ctx});

  FJS::bigShutdown();
  fs::remove(TmpPath);

  REQUIRE(Result.has_value());
  REQUIRE(Result->size() == Contents.size());
  REQUIRE(std::memcmp(Result->data(), Contents.data(), Contents.size()) == 0);
}
