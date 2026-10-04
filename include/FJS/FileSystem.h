#ifndef FJS_SUPPORT_FILESYSTEM_H
#define FJS_SUPPORT_FILESYSTEM_H

#include "FJS/Counter.h"
#include "FJS/Error.h"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace FJS::FileSystem {

/// Loads \p Path's contents into \p Result as an IO-thread job. Returns a
/// handle tracking the job; call `.wait()` on it (from a fiber - inside a
/// running job) to block until \p Result is populated.
CounterHandle loadFile(std::filesystem::path Path,
                       Result<std::vector<std::byte>> &Result);

} // namespace FJS::FileSystem

#endif // FJS_SUPPORT_FILESYSTEM_H
