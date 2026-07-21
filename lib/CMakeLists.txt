add_library(LibFJS STATIC
  Counter.cpp
  CounterPool.cpp
  Fiber.cpp
  FiberPool.cpp
  FiberSwitch.S
  JobQueue.cpp
  JobScheduler.cpp
  Shutdown.cpp
  Worker.cpp
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

target_link_libraries(
  LibFJS
  PUBLIC
    Tracy::TracyClient
  PRIVATE
    FJS::PendingAfterSwitch
    FJS::WorkerContext
)

# CET must stay off: the hand-rolled fiber switch rets to addresses no shadow
# stack has seen. `-fcf-protection=none` omits the SHSTK-compatible marking, so
# the loaded keeps enforcement disabled for the whole process even on systems
# where CET is on by default. Proper support of this option would need a shadow
# stack per fiber. On kernels without CET userspace support this option has no
# runtime effect (the flag only strips the binary's `endbr64` pads and CET
# markings).
target_compile_options(LibFJS
  PUBLIC
    -fcf-protection=none
  PRIVATE
    -fno-exceptions
    -fno-rtti
)

if(FJS_USE_UCONTEXT)
  target_compile_definitions(LibFJS PRIVATE FJS_USE_UCONTEXT)
endif()

# Isolate the opaque accessor to TLS in their own target. To forcefully disable
# IPO/LTO on THIS target.
add_library(PendingAfterSwitch OBJECT
  PendingAfterSwitch.cpp
)

add_library(FJS::PendingAfterSwitch ALIAS PendingAfterSwitch)

target_include_directories(
  PendingAfterSwitch 
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/include
)

target_compile_options(PendingAfterSwitch PRIVATE -fno-lto)

set_target_properties(PendingAfterSwitch
  PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION OFF
    INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF
    INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF
    INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO OFF
)

add_library(WorkerContext OBJECT
  WorkerContext.cpp
)

add_library(FJS::WorkerContext ALIAS WorkerContext)

target_include_directories(
  WorkerContext
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/include
)

target_compile_options(WorkerContext PRIVATE -fno-lto)

set_target_properties(WorkerContext
  PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION OFF
    INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF
    INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF
    INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO OFF
)
