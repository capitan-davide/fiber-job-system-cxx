cmake_minimum_required(VERSION 4.0)
project(fiber-job-system VERSION 0.1.0 LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(lib)

include(CTest)
if(BUILD_TESTING)
  find_package(Catch2 CONFIG REQUIRED)
  add_subdirectory(test)
endif()
