cmake_minimum_required(VERSION 4.0)
project(fiber-job-system VERSION 0.1.0 LANGUAGES CXX ASM)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "This project only supports Clang (got ${CMAKE_CXX_COMPILER_ID})")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_LINKER_TYPE LLD)

add_compile_options(-stdlib=libc++)
add_link_options(-stdlib=libc++)

option(FJS_BUILD_TESTS "Build the Catch2 test suite" ON)
option(FJS_USE_UCONTEXT "Use ucontext fiber backend as default" OFF)
option(FJS_ENABLE_TRACY "Enable the Tracy profiler" OFF)

# Comma-separated '-fsanitize' values ('address,undefined', 'thread', ...) or
# 'none'. TSan cannot share a binary with ASan/LSan.
set(FJS_ENABLE_SANITIZERS "none"
  CACHE STRING
  "Sanitizer(s) to enable (comma-separated), or 'none'"
)

if(FJS_ENABLE_TRACY)
  set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
  set(TRACY_FIBERS ON CACHE BOOL "" FORCE)

  # Enable the native architecture of this CPU to leverage expanded instruction
  # sets. Ties the binary to the machine where it was compiled.
  add_compile_options(-march=native -g -fno-omit-frame-pointer)
else()
  set(TRACY_ENABLE OFF CACHE BOOL "" FORCE)
endif()

include(cmake/FetchTracy.cmake)

if(NOT FJS_ENABLE_SANITIZERS STREQUAL "none")
  add_compile_options(-fsanitize=${FJS_ENABLE_SANITIZERS} -fno-omit-frame-pointer)
  add_link_options(-fsanitize=${FJS_ENABLE_SANITIZERS})
endif()

add_subdirectory(lib)

include(CTest)
if(FJS_BUILD_TESTS)
  include(cmake/FetchCatch2.cmake)
  add_subdirectory(test)
endif()
