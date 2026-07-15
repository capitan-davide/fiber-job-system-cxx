cmake_minimum_required(VERSION 4.0)
project(fiber-job-system VERSION 0.1.0 LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FJS_BUILD_TESTS "Build the Catch2 test suite" ON)
option(FJS_USE_UCONTEXT "Use ucontext fiber backend as default" OFF)

add_subdirectory(lib)

include(CTest)
if(FJS_BUILD_TESTS)
  find_package(Catch2 CONFIG REQUIRED)
  add_subdirectory(test)
endif()
