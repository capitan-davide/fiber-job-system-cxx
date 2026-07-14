add_executable(tests Tests.cpp)

target_link_libraries(
  tests
  PRIVATE
    Catch2::Catch2WithMain
    FJS::LibFJS
)

target_include_directories(
  tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/include
)

include(Catch)
catch_discover_tests(tests)
