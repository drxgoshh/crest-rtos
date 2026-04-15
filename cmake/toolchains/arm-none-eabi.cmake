# cmake/toolchains/arm-none-eabi.cmake
#
# Cross-compilation toolchain for bare-metal ARM targets using the
# arm-none-eabi GCC toolchain.  Set via:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake ..

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TOOLCHAIN_PREFIX arm-none-eabi-)

find_program(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc   REQUIRED)
find_program(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++   REQUIRED)
find_program(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc   REQUIRED)
find_program(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy REQUIRED)
find_program(CMAKE_OBJDUMP      ${TOOLCHAIN_PREFIX}objdump REQUIRED)
find_program(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size    REQUIRED)

# Prevent cmake from trying to link a test executable (it can't run on host)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
