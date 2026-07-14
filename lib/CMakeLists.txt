add_library(LibFJS STATIC
  Fiber.cpp
  FiberSwitch.S
)

add_library(FJS::LibFJS ALIAS LibFJS)

set_target_properties(LibFJS
  PROPERTIES OUTPUT_NAME FJS
)

target_include_directories(
  LibFJS
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/include
)
