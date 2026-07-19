cmake_minimum_required(VERSION 4.0)
project(fiber-job-system VERSION 0.1.0 LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FJS_BUILD_TESTS "Build the Catch2 test suite" ON)
option(FJS_USE_UCONTEXT "Use ucontext fiber backend as default" OFF)

# Comma-separated '-fsanitize' values ('address,undefined', 'thread', ...) or
# 'none'. TSan cannot share a binary with ASan/LSan.
set(FJS_ENABLE_SANITIZERS "none"
  CACHE STRING
  "Sanitizer(s) to enable (comma-separated), or 'none'"
)

if(NOT FJS_ENABLE_SANITIZERS STREQUAL "none")
  add_compile_options(-fsanitize=${FJS_ENABLE_SANITIZERS} -fno-omit-frame-pointer)
  add_link_options(-fsanitize=${FJS_ENABLE_SANITIZERS})
endif()

add_subdirectory(lib)

include(CTest)
if(FJS_BUILD_TESTS)
  find_package(Catch2 CONFIG REQUIRED)
  add_subdirectory(test)
endif()
