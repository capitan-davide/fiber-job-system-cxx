add_library(LibFJS STATIC
  Fiber.cpp
  FiberPool.cpp
  FiberSwitch.S
  JobQueue.cpp
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

# CET must stay off: the hand-rolled fiber switch rets to addresses no shadow
# stack has seen. `-fcf-protection=none` omits the SHSTK-compatible marking, so
# the loaded keeps enforcement disabled for the whole process even on systems
# where CET is on by default. Proper support of this option would need a shadow
# stack per fiber. On kernels without CET userspace support this option has no
# runtime effect (the flag only strips the binary's `endbr64` pads and CET
# markings).
target_compile_options(LibFJS PUBLIC "-fcf-protection=none")

if(FJS_USE_UCONTEXT)
  target_compile_definitions(LibFJS PRIVATE FJS_USE_UCONTEXT)
endif()
