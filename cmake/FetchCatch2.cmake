include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2
  GIT_TAG v3.15.2
  GIT_SHALLOW TRUE
  SYSTEM
)

FetchContent_MakeAvailable(Catch2)

# find_package(Catch2) ships the Catch.cmake module via its config package;
# FetchContent does not. Without this, test/CMakeLists.txt's include(Catch)
# cannot find catch_discover_tests.
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
