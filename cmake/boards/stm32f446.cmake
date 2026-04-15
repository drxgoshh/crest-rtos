# cmake/boards/stm32f446.cmake
#
# Board-specific configuration for the STM32F446 (ARM Cortex-M4 with FPU).
# Included by the root CMakeLists.txt when BOARD=stm32f446.

# Architecture directory under arch/
set(CREST_ARCH "arm/cortex-m4")

# Compiler / linker CPU flags
set(CREST_CPU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
)

# Linker script
set(CREST_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/boards/stm32f446/link.ld")

# Board-specific source files (BSP only — no application code)
set(CREST_BOARD_SOURCES
    "${CMAKE_SOURCE_DIR}/boards/stm32f446/startup.c"
    "${CMAKE_SOURCE_DIR}/boards/stm32f446/uart.c"
)

# Board-specific include directories (public — visible to all targets)
set(CREST_BOARD_INCLUDES
    "${CMAKE_SOURCE_DIR}/boards/stm32f446"
)
