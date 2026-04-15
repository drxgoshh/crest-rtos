cmake_minimum_required(VERSION 3.16)

# ── Toolchain must be set before project() ────────────────────────────────────
set(CMAKE_TOOLCHAIN_FILE
    "${CMAKE_SOURCE_DIR}/cmake/toolchains/arm-none-eabi.cmake"
    CACHE FILEPATH "Cross-compilation toolchain file")

# ── Board selection ───────────────────────────────────────────────────────────
set(BOARD "stm32f446" CACHE STRING "Target board (must match a file in cmake/boards/)")

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/cmake/boards/${BOARD}.cmake")
    message(FATAL_ERROR "Unknown board '${BOARD}'. "
        "Add cmake/boards/${BOARD}.cmake to support it.")
endif()

include("${CMAKE_SOURCE_DIR}/cmake/boards/${BOARD}.cmake")

# ── Project ───────────────────────────────────────────────────────────────────
project(crest VERSION 0.1.0 LANGUAGES C ASM)

# ── CPU flags interface library ───────────────────────────────────────────────
# All targets link against this so the same flags propagate everywhere without
# repetition.
add_library(crest_cpu INTERFACE)
target_compile_options(crest_cpu INTERFACE ${CREST_CPU_FLAGS})
target_link_options(   crest_cpu INTERFACE ${CREST_CPU_FLAGS})

# ── Kernel library ────────────────────────────────────────────────────────────
# Platform-agnostic RTOS core: task lifecycle, scheduler, allocator, sync
# primitives, and a minimal libc subset.
add_library(kernel STATIC
    kernel/task.c
    kernel/sched.c
    kernel/alloc.c
    kernel/mutex.c
    kernel/semaphore.c
    kernel/libc_stubs.c
)

target_include_directories(kernel
    PUBLIC
        kernel/include
        arch/${CREST_ARCH}      # resolves critical.h through isr.h
)

target_compile_options(kernel PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    -ffunction-sections -fdata-sections
)

target_link_libraries(kernel PUBLIC crest_cpu)

# ── Architecture (port) library ───────────────────────────────────────────────
# Cortex-M4 context switch, PendSV handler, and port_start_first_task.
add_library(arch STATIC
    arch/${CREST_ARCH}/port.c
    arch/${CREST_ARCH}/context_switch.c
)

target_include_directories(arch
    PUBLIC  arch/${CREST_ARCH}
    PRIVATE kernel/include
)

target_compile_options(arch PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    -ffunction-sections -fdata-sections
)

target_link_libraries(arch PUBLIC crest_cpu PRIVATE kernel)

# ── Firmware executable ───────────────────────────────────────────────────────
add_executable(crest
    src/main.c
    ${CREST_BOARD_SOURCES}
)

target_include_directories(crest PRIVATE
    ${CREST_BOARD_INCLUDES}
    kernel/include
    arch/${CREST_ARCH}
)

target_compile_options(crest PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    -ffunction-sections -fdata-sections
)

target_link_options(crest PRIVATE
    -nostdlib -nostartfiles
    -T${CREST_LINKER_SCRIPT}
    -Wl,--gc-sections                   # strip unused sections
    -Wl,-Map=${CMAKE_BINARY_DIR}/crest.map  # generate linker map
)

target_link_libraries(crest PRIVATE kernel arch)

# ── Post-build: .bin / .hex / size report ────────────────────────────────────
add_custom_command(TARGET crest POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary  $<TARGET_FILE:crest> ${CMAKE_BINARY_DIR}/crest.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex    $<TARGET_FILE:crest> ${CMAKE_BINARY_DIR}/crest.hex
    COMMAND ${CMAKE_SIZE}               $<TARGET_FILE:crest>
    COMMENT "Generating crest.bin / crest.hex"
)

# ── Flash target (optional, requires OpenOCD) ─────────────────────────────────
add_custom_target(flash
    DEPENDS crest
    COMMAND openocd
        -f interface/stlink.cfg
        -f target/stm32f4x.cfg
        -c "program ${CMAKE_BINARY_DIR}/crest.bin 0x08000000 verify reset exit"
    COMMENT "Flashing ${BOARD} via ST-Link"
)
